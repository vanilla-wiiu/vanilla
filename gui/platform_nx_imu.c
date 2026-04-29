#include "platform_nx_imu.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <unistd.h>

#include <vanilla.h>

#include "ui/ui_util.h"
#include "platform.h"

// Reads accelerometer and gyroscope from the Switch's onboard IMU on
// Switchroot/L4T. The IMU sits on SPI3.0 and the kernel exposes it as
// two separate IIO devices under /sys/bus/iio/devices: one carries
// in_accel_*_raw, the other carries in_anglvel_*_raw. Both share the
// same parent device, so they describe the same physical sensor.
//
// In handheld mode the Joy-Cons are rigidly attached to the chassis and
// move with it, so the console IMU captures the same motion the user is
// imparting on their controllers — exactly what we want for forwarding
// motion to the Wii U gamepad. (Detached Joy-Cons in a grip would not
// produce motion here, but handheld is the primary use case.)
//
// CONFIG_IIO_TRIGGER is off in the Switchroot kernel, so the buffered
// /dev/iio:deviceN interface isn't usable. We poll sysfs at ~100Hz; six
// raw fds are kept open and re-read with lseek+read, which is cheap.

#define IIO_BASE         "/sys/bus/iio/devices"
#define IIO_MAX_DEVS     8
#define POLL_INTERVAL_MS 5            // ~200 Hz sampling
#define TARGET_SAMPLE_HZ 200.0f       // chip ODR; LSM6DSL → 208 Hz
#define MAX_SAMPLE_HZ    416.0f
#define G_TO_MS2         9.80665f
#define DEG_TO_RAD       (float)(M_PI / 180.0f)

typedef struct {
    char path[160];
    char name[64];
    char parent[256];      // realpath of underlying device, for pairing accel+gyro halves

    int accel_raw_fd[3];   // all -1 if device doesn't expose accel
    int gyro_raw_fd[3];    // all -1 if device doesn't expose gyro

    float accel_scale;     // m/s^2 per raw count (Linux IIO convention)
    float gyro_scale;      // rad/s per raw count

    // Sensor-frame to chassis-frame transform from sysfs `mount_matrix`.
    // out[i] = sum_j mount[i][j] * raw_axis[j]. Identity if not provided.
    float mount[3][3];

    int has_accel;
    int has_gyro;
    int valid;
} iio_dev_t;

static pthread_t s_thread;
static atomic_int s_run = 0;
static int s_pipe_w = -1;
static int s_pipe_r = -1;

static int sysfs_read(const char *path, char *buf, size_t buf_size)
{
    int fd = open(path, O_RDONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = read(fd, buf, buf_size - 1);
    close(fd);
    if (n <= 0) return -1;
    buf[n] = 0;
    while (n > 0 && (buf[n-1] == '\n' || buf[n-1] == ' ' || buf[n-1] == '\t')) {
        buf[--n] = 0;
    }
    return 0;
}

static int sysfs_read_at(const char *dir, const char *file, char *buf, size_t buf_size)
{
    char p[320];
    snprintf(p, sizeof(p), "%s/%s", dir, file);
    return sysfs_read(p, buf, buf_size);
}

static int sysfs_write_at(const char *dir, const char *file, const char *value)
{
    char p[320];
    snprintf(p, sizeof(p), "%s/%s", dir, file);
    int fd = open(p, O_WRONLY | O_CLOEXEC);
    if (fd < 0) return -1;
    ssize_t n = write(fd, value, strlen(value));
    close(fd);
    return n < 0 ? -1 : 0;
}

// Pick the highest rate from a sysfs "_available" list that's <= max_hz,
// then write it to the corresponding setting file. Many IMU drivers leave
// sampling_frequency near zero by default to save power, which would
// otherwise cap our update rate at ~1 Hz.
static void set_sampling_rate(const char *iio_path)
{
    char available[256];
    if (sysfs_read_at(iio_path, "sampling_frequency_available", available, sizeof(available)) < 0) {
        // No constraint advertised; just write the target.
        char val[32];
        snprintf(val, sizeof(val), "%g", TARGET_SAMPLE_HZ);
        if (sysfs_write_at(iio_path, "sampling_frequency", val) == 0) {
            vpilog("nx-imu: %s sampling_frequency <- %s\n", iio_path, val);
        } else {
            vpilog("nx-imu: %s sampling_frequency write failed: %s\n", iio_path, strerror(errno));
        }
        return;
    }

    // Parse the space-separated list, pick best rate close to target.
    // Prefer the largest rate <= MAX_SAMPLE_HZ; among those, the closest
    // to TARGET_SAMPLE_HZ wins.
    float best = 0;
    char *p = available;
    while (*p) {
        char *end = p;
        float v = strtof(p, &end);
        if (end == p) break;
        if (v <= MAX_SAMPLE_HZ
            && (best == 0 || fabsf(v - TARGET_SAMPLE_HZ) < fabsf(best - TARGET_SAMPLE_HZ))) {
            best = v;
        }
        p = end;
        while (*p == ' ' || *p == '\t' || *p == '\n') p++;
    }

    if (best <= 0) {
        vpilog("nx-imu: %s couldn't pick a rate from \"%s\"\n", iio_path, available);
        return;
    }

    char val[32];
    snprintf(val, sizeof(val), "%g", best);
    if (sysfs_write_at(iio_path, "sampling_frequency", val) == 0) {
        vpilog("nx-imu: %s sampling_frequency <- %s Hz (available: %s)\n",
            iio_path, val, available);
    } else {
        vpilog("nx-imu: %s sampling_frequency write failed: %s\n",
            iio_path, strerror(errno));
    }
}

static int sysfs_read_int_fd(int fd, long *out)
{
    char buf[32];
    if (lseek(fd, 0, SEEK_SET) < 0) return -1;
    ssize_t n = read(fd, buf, sizeof(buf) - 1);
    if (n <= 0) return -1;
    buf[n] = 0;
    char *end = NULL;
    long v = strtol(buf, &end, 10);
    if (end == buf) return -1;
    *out = v;
    return 0;
}

static void open_axis_fds(const char *iio_path, const char *names[3], int fds[3])
{
    for (int i = 0; i < 3; i++) {
        char p[320];
        snprintf(p, sizeof(p), "%s/%s", iio_path, names[i]);
        fds[i] = open(p, O_RDONLY | O_CLOEXEC);
    }
}

static int all_open(const int fds[3])
{
    return fds[0] >= 0 && fds[1] >= 0 && fds[2] >= 0;
}

static void close_fds(int fds[3])
{
    for (int i = 0; i < 3; i++) {
        if (fds[i] >= 0) { close(fds[i]); fds[i] = -1; }
    }
}

static void close_iio(iio_dev_t *d)
{
    close_fds(d->accel_raw_fd);
    close_fds(d->gyro_raw_fd);
    d->valid = 0;
    d->has_accel = 0;
    d->has_gyro = 0;
}

// Parse a 3x3 mount matrix from a sysfs mount_matrix string of the form
// "a, b, c; d, e, f; g, h, i" into out[3][3]. Returns 0 on success.
static int parse_mount_matrix(const char *s, float out[3][3])
{
    float m[9];
    int n = sscanf(s, "%f , %f , %f ; %f , %f , %f ; %f , %f , %f",
        &m[0], &m[1], &m[2], &m[3], &m[4], &m[5], &m[6], &m[7], &m[8]);
    if (n != 9) return -1;
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out[i][j] = m[3*i + j];
        }
    }
    return 0;
}

static int open_iio(const char *iio_path, iio_dev_t *out)
{
    memset(out, 0, sizeof(*out));
    for (int i = 0; i < 3; i++) {
        out->accel_raw_fd[i] = -1;
        out->gyro_raw_fd[i]  = -1;
    }
    // Default to identity transform; override below if sysfs provides one.
    for (int i = 0; i < 3; i++) {
        for (int j = 0; j < 3; j++) {
            out->mount[i][j] = (i == j) ? 1.0f : 0.0f;
        }
    }
    snprintf(out->path, sizeof(out->path), "%s", iio_path);

    if (sysfs_read_at(iio_path, "name", out->name, sizeof(out->name)) < 0) {
        out->name[0] = 0;
    }

    // Resolve the underlying device so we can pair accel+gyro halves of the
    // same physical sensor. /sys/bus/iio/devices/iio:deviceN is a symlink
    // into /sys/devices/...; the parent of that target is the bus device.
    char real[PATH_MAX];
    if (realpath(iio_path, real)) {
        char *slash = strrchr(real, '/');
        if (slash) *slash = 0;
        snprintf(out->parent, sizeof(out->parent), "%s", real);
    }

    // Apply the kernel's reported sensor-to-chassis mount transform if any.
    char mount_buf[128];
    if (sysfs_read_at(iio_path, "mount_matrix", mount_buf, sizeof(mount_buf)) == 0) {
        if (parse_mount_matrix(mount_buf, out->mount) == 0) {
            vpilog("nx-imu: %s mount_matrix=%s\n", iio_path, mount_buf);
        } else {
            vpilog("nx-imu: %s couldn't parse mount_matrix \"%s\"\n", iio_path, mount_buf);
        }
    }

    static const char *accel_files[3] = { "in_accel_x_raw", "in_accel_y_raw", "in_accel_z_raw" };
    static const char *gyro_files[3]  = { "in_anglvel_x_raw", "in_anglvel_y_raw", "in_anglvel_z_raw" };

    open_axis_fds(iio_path, accel_files, out->accel_raw_fd);
    out->has_accel = all_open(out->accel_raw_fd);
    if (!out->has_accel) close_fds(out->accel_raw_fd);

    open_axis_fds(iio_path, gyro_files, out->gyro_raw_fd);
    out->has_gyro = all_open(out->gyro_raw_fd);
    if (!out->has_gyro) close_fds(out->gyro_raw_fd);

    if (!out->has_accel && !out->has_gyro) {
        return -1;
    }

    char buf[64];
    if (out->has_accel) {
        if (sysfs_read_at(iio_path, "in_accel_scale", buf, sizeof(buf)) == 0
            || sysfs_read_at(iio_path, "in_accel_x_scale", buf, sizeof(buf)) == 0) {
            out->accel_scale = strtof(buf, NULL);
        }
        if (out->accel_scale == 0) {
            out->accel_scale = G_TO_MS2 / 4096.0f;  // factory LSM6DSL fallback
            vpilog("nx-imu: %s missing accel scale, using fallback %.6g\n",
                iio_path, out->accel_scale);
        }
    }
    if (out->has_gyro) {
        if (sysfs_read_at(iio_path, "in_anglvel_scale", buf, sizeof(buf)) == 0
            || sysfs_read_at(iio_path, "in_anglvel_x_scale", buf, sizeof(buf)) == 0) {
            out->gyro_scale = strtof(buf, NULL);
        }
        if (out->gyro_scale == 0) {
            out->gyro_scale = DEG_TO_RAD * 0.07f;  // 0.07 dps/LSB factory default
            vpilog("nx-imu: %s missing gyro scale, using fallback %.6g\n",
                iio_path, out->gyro_scale);
        }

        // The Linux IIO contract is that gyro scale is in rad/s per LSB,
        // but some drivers (notably Switchroot's) publish it in deg/s per
        // LSB. Detect by implied full-scale magnitude: any real gyro in
        // rad/s would have full_scale < ~50 even at extreme ranges, while
        // dps would always exceed 100. Convert if needed.
        float full_scale = out->gyro_scale * 32768.0f;
        if (full_scale > 50.0f) {
            out->gyro_scale *= DEG_TO_RAD;
            vpilog("nx-imu: %s gyro scale appears to be deg/s (full=%.1f); converting to rad/s\n",
                iio_path, full_scale);
        }
    }

    out->valid = 1;
    vpilog("nx-imu: IIO %s name=\"%s\" accel=%d gyro=%d accel_scale=%.6g gyro_scale=%.6g\n",
        iio_path, out->name, out->has_accel, out->has_gyro,
        out->accel_scale, out->gyro_scale);

    // Force runtime PM off so the chip stays awake. Some IIO drivers will
    // auto-suspend the bus device between reads, and waking from suspend
    // can interleave with the chip's sample register updates and yield
    // torn / random reads when sysfs is polled at high rate.
    if (sysfs_write_at(iio_path, "power/control", "on") == 0) {
        vpilog("nx-imu: %s power/control <- on\n", iio_path);
    } else if (errno != ENOENT) {
        vpilog("nx-imu: %s power/control write failed: %s\n", iio_path, strerror(errno));
    }

    set_sampling_rate(iio_path);
    return 0;
}

static void scan_iio(iio_dev_t *devs, int *count)
{
    DIR *d = opendir(IIO_BASE);
    if (!d) {
        vpilog("nx-imu: opendir(%s) failed: %s\n", IIO_BASE, strerror(errno));
        return;
    }
    struct dirent *e;
    while ((e = readdir(d)) != NULL && *count < IIO_MAX_DEVS) {
        if (e->d_name[0] == '.') continue;
        if (strncmp(e->d_name, "iio:device", 10) != 0) continue;

        char path[200];
        snprintf(path, sizeof(path), "%s/%s", IIO_BASE, e->d_name);

        int dup = 0;
        for (int i = 0; i < *count; i++) {
            if (devs[i].valid && !strcmp(devs[i].path, path)) { dup = 1; break; }
        }
        if (dup) continue;

        iio_dev_t tmp;
        if (open_iio(path, &tmp) == 0) {
            devs[(*count)++] = tmp;
        }
    }
    closedir(d);
}

static void publish(const float a[3], const float g[3])
{
    vanilla_set_button(VANILLA_SENSOR_ACCEL_X, pack_float(a[0]));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Y, pack_float(a[1]));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Z, pack_float(a[2]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_PITCH, pack_float(g[0]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_YAW,   pack_float(g[1]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_ROLL,  pack_float(g[2]));

    static int log_throttle = 0;
    if ((++log_throttle % 20) == 1) {
        // Values are already in chassis frame (mount_matrix applied at read).
        vpilog("nx-imu: a=(%.2f %.2f %.2f) m/s^2  g=(%.2f %.2f %.2f) rad/s\n",
            a[0], a[1], a[2], g[0], g[1], g[2]);
    }
}

static int read_axes(int fds[3], float scale, const float mount[3][3], float out[3])
{
    long raw[3];
    float chip[3];
    for (int i = 0; i < 3; i++) {
        if (sysfs_read_int_fd(fds[i], &raw[i]) < 0) return -1;
        chip[i] = (float)raw[i] * scale;
    }
    // Apply mount transform: out_i = sum_j mount[i][j] * chip[j]
    for (int i = 0; i < 3; i++) {
        out[i] = mount[i][0]*chip[0] + mount[i][1]*chip[1] + mount[i][2]*chip[2];
    }
    return 0;
}

// Pick which device to read accel from, and which to read gyro from.
// Prefer pairing accel and gyro that share a parent device — that means
// they're two halves of the same physical IMU and stay phase-aligned.
static void select_sources(iio_dev_t *devs, int count,
                           iio_dev_t **accel_src, iio_dev_t **gyro_src)
{
    *accel_src = NULL;
    *gyro_src = NULL;

    iio_dev_t *combined = NULL;
    for (int i = 0; i < count; i++) {
        if (devs[i].valid && devs[i].has_accel && devs[i].has_gyro) {
            combined = &devs[i];
            break;
        }
    }
    if (combined) {
        *accel_src = combined;
        *gyro_src = combined;
        return;
    }

    iio_dev_t *a = NULL, *g = NULL;
    for (int i = 0; i < count; i++) {
        if (devs[i].valid && devs[i].has_accel && !a) a = &devs[i];
        if (devs[i].valid && devs[i].has_gyro && !g)  g = &devs[i];
    }
    // Prefer matching parent if multiple options exist
    if (a && g && strcmp(a->parent, g->parent) != 0) {
        for (int i = 0; i < count; i++) {
            if (devs[i].valid && devs[i].has_gyro && !strcmp(devs[i].parent, a->parent)) {
                g = &devs[i];
                break;
            }
        }
    }
    *accel_src = a;
    *gyro_src = g;
}

static void *imu_thread(void *arg)
{
    (void)arg;

    iio_dev_t devs[IIO_MAX_DEVS];
    memset(devs, 0, sizeof(devs));
    int count = 0;

    scan_iio(devs, &count);

    iio_dev_t *accel_src = NULL;
    iio_dev_t *gyro_src = NULL;
    select_sources(devs, count, &accel_src, &gyro_src);
    vpilog("nx-imu: initial scan: %d device(s); accel=%s gyro=%s\n",
        count,
        accel_src ? accel_src->path : "(none)",
        gyro_src ? gyro_src->path : "(none)");

    int rescan_ticks = 0;
    struct pollfd pfd = { .fd = s_pipe_r, .events = POLLIN };

    // Gyro filtering / calibration intentionally disabled for diagnostic
    // builds — passing raw chassis-frame values through so other Switchroot
    // users can compare their chip output against ours. Restore this block
    // and the matching post-read processing further down before shipping.
    //
    // const int   GYRO_CALIB_SAMPLES = 1000 / POLL_INTERVAL_MS;  // ~1 second
    // const float GYRO_FILTER_ALPHA  = 0.30f;   // ~17 ms time constant at 200 Hz
    // const float GYRO_DEADZONE      = 0.50f;   // rad/s, ~stationary noise sd
    // float gyro_bias[3] = {0, 0, 0};
    // float gyro_sum[3]  = {0, 0, 0};
    // int   gyro_calib_count = 0;
    // int   gyro_calibrated = 0;
    // float gyro_filt[3] = {0, 0, 0};

    while (atomic_load(&s_run)) {
        int pr = poll(&pfd, 1, POLL_INTERVAL_MS);
        if (pr < 0 && errno != EINTR) break;
        if (pfd.revents & POLLIN) {
            char buf[16];
            while (read(s_pipe_r, buf, sizeof(buf)) > 0) {}
        }

        if (!accel_src || !gyro_src) {
            if (++rescan_ticks >= 200) {
                rescan_ticks = 0;
                scan_iio(devs, &count);
                select_sources(devs, count, &accel_src, &gyro_src);
            }
            continue;
        }

        float a[3] = {0}, g[3] = {0};
        if (read_axes(accel_src->accel_raw_fd, accel_src->accel_scale, accel_src->mount, a) < 0
            || read_axes(gyro_src->gyro_raw_fd, gyro_src->gyro_scale, gyro_src->mount, g) < 0) {
            vpilog("nx-imu: sample failed, will rescan\n");
            close_iio(accel_src);
            if (gyro_src != accel_src) close_iio(gyro_src);
            accel_src = gyro_src = NULL;
            continue;
        }

        // Diagnostic build: publish raw mount-matrix-transformed values
        // directly. Calibration / deadzone / EMA filter are commented out
        // above for testing on other Switchroot consoles.
        //
        // if (!gyro_calibrated) {
        //     for (int i = 0; i < 3; i++) gyro_sum[i] += g[i];
        //     gyro_calib_count++;
        //     if (gyro_calib_count >= GYRO_CALIB_SAMPLES) {
        //         for (int i = 0; i < 3; i++) {
        //             gyro_bias[i] = gyro_sum[i] / (float)gyro_calib_count;
        //             gyro_filt[i] = 0;
        //         }
        //         gyro_calibrated = 1;
        //         vpilog("nx-imu: gyro calibrated, bias=(%.4f %.4f %.4f) rad/s\n",
        //             gyro_bias[0], gyro_bias[1], gyro_bias[2]);
        //     }
        //     float zero[3] = {0, 0, 0};
        //     publish(a, zero);
        //     continue;
        // }
        //
        // for (int i = 0; i < 3; i++) {
        //     g[i] -= gyro_bias[i];
        //     if (g[i] > -GYRO_DEADZONE && g[i] < GYRO_DEADZONE) {
        //         g[i] = 0;
        //     } else if (g[i] > 0) {
        //         g[i] -= GYRO_DEADZONE;
        //     } else {
        //         g[i] += GYRO_DEADZONE;
        //     }
        //     gyro_filt[i] = GYRO_FILTER_ALPHA * g[i]
        //                  + (1.0f - GYRO_FILTER_ALPHA) * gyro_filt[i];
        // }
        //
        // publish(a, gyro_filt);

        publish(a, g);
    }

    for (int i = 0; i < count; i++) close_iio(&devs[i]);
    return NULL;
}

int vpi_nx_imu_init(void)
{
    if (atomic_load(&s_run)) {
        return 0;
    }

    int pfds[2];
    if (pipe(pfds) < 0) {
        vpilog("nx-imu: pipe failed: %s\n", strerror(errno));
        return -1;
    }
    fcntl(pfds[0], F_SETFL, O_NONBLOCK);
    fcntl(pfds[1], F_SETFL, O_NONBLOCK);
    fcntl(pfds[0], F_SETFD, FD_CLOEXEC);
    fcntl(pfds[1], F_SETFD, FD_CLOEXEC);
    s_pipe_r = pfds[0];
    s_pipe_w = pfds[1];

    atomic_store(&s_run, 1);
    if (pthread_create(&s_thread, NULL, imu_thread, NULL) != 0) {
        atomic_store(&s_run, 0);
        close(s_pipe_r); close(s_pipe_w);
        s_pipe_r = s_pipe_w = -1;
        vpilog("nx-imu: pthread_create failed\n");
        return -1;
    }

    vpilog("nx-imu: started (IIO sysfs poller)\n");
    return 0;
}

void vpi_nx_imu_quit(void)
{
    if (!atomic_load(&s_run)) {
        return;
    }
    atomic_store(&s_run, 0);
    if (s_pipe_w >= 0) {
        char b = 0;
        write(s_pipe_w, &b, 1);
    }
    pthread_join(s_thread, NULL);
    if (s_pipe_r >= 0) close(s_pipe_r);
    if (s_pipe_w >= 0) close(s_pipe_w);
    s_pipe_r = s_pipe_w = -1;
}
