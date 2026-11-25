#include "platform_nx_imu.h"

#include <dirent.h>
#include <errno.h>
#include <fcntl.h>
#include <linux/input.h>
#include <math.h>
#include <poll.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/ioctl.h>
#include <time.h>
#include <unistd.h>

#include <iio.h>
#include <vanilla.h>

#include "ui/ui_util.h"
#include "platform.h"

// Reads accelerometer and gyroscope from either the Switch's onboard IMU (Lite)
// or joycon serial IMU (original/OLED) using libiio's buffer interface.
// For some reason, the raw attribute reads appear broken for the gyro on this
// kernel, but the buffer interface works correctly (same as iio_oscilloscope uses)

#define BUFFER_SAMPLES   1             // We just need the latest sample
#define RAIL_SCAN_INTERVAL_MS 1000
#define RAIL_POLL_TIMEOUT_MS 100
#define RAIL_STALL_TIMEOUT_MS 2000
#define NINTENDO_VENDOR_ID 0x057e
#define JOYCON_LEFT_PRODUCT_ID 0x20061
#define JOYCON_RIGHT_PRODUCT_ID 0x2007
#define STANDARD_GRAVITY 9.80665f

typedef struct {
    struct iio_device *dev;
    struct iio_buffer *buf;
    struct iio_channel *ch[3];         // x, y, z channels
    float scale;
    float mount[3][3];                 // mount matrix transform
    int valid;
} sensor_t;

typedef enum {
    IMU_SOURCE_AUTO,
    IMU_SOURCE_RAIL,
    IMU_SOURCE_IIO,
} imu_source_mode_t;

typedef enum {
    RAIL_TRANSFORM_GAMEPAD,
    RAIL_TRANSFORM_LEGACY,
} rail_transform_mode_t;

static struct iio_context *s_ctx = NULL;
static sensor_t s_accel;
static sensor_t s_gyro;
static int s_iio_initialized;
static imu_source_mode_t s_source_mode;
static rail_transform_mode_t s_rail_transform_mode;

static int s_rail_fd = -1;
static unsigned short s_rail_product;
static char s_rail_path[256];
static int s_rail_accel_resolution;
static int s_rail_gyro_resolution;
static int s_rail_values[6];
static unsigned int s_rail_value_mask;

static pthread_t s_thread;
static atomic_int s_run = 0;

// Allows environment variable to set IMU source
static void select_source_mode(void)
{
    const char *source = getenv("VANILLA_NX_IMU_SOURCE");
    const char *rail_transform = getenv("VANILLA_NX_IMU_RAIL_TRANSFORM");
    s_source_mode = IMU_SOURCE_AUTO;
    s_rail_transform_mode = RAIL_TRANSFORM_GAMEPAD;

    if (!source || !source[0] || strcmp(source, "auto") == 0) {
        vpilog("nx-imu: source mode auto (rail preferred, IIO fallback)\n");
    } else if (strcmp(source, "rail") == 0) {
        s_source_mode = IMU_SOURCE_RAIL;
        vpilog("nx-imu: source mode rail-only\n");
    } else if (strcmp(source, "iio") == 0) {
        s_source_mode = IMU_SOURCE_IIO;
        vpilog("nx-imu: source mode onboard-IIO-only\n");
    } else {
        vpilog("nx-imu: unknown VANILLA_NX_IMU_SOURCE=%s - using auto\n",
            source);
    }

    if (!rail_transform || !rail_transform[0] ||
        strcmp(rail_transform, "gamepad") == 0) {
        vpilog("nx-imu: rail transform gamepad\n");
    } else if (strcmp(rail_transform, "legacy") == 0) {
        s_rail_transform_mode = RAIL_TRANSFORM_LEGACY;
        vpilog("nx-imu: rail transform legacy (Switch chassis frame)\n");
    } else {
        vpilog("nx-imu: unknown VANILLA_NX_IMU_RAIL_TRANSFORM=%s; "
            "using gamepad\n", rail_transform);
    }
}

static int open_rail_candidate(const char *path, unsigned short wanted_product, int *accel_resolution, int *gyro_resolution)
{
    struct input_absinfo accel_info;
    struct input_absinfo gyro_info;
    struct input_id id;
    char name[256] = {0};
    int fd = open(path, O_RDONLY | O_NONBLOCK | O_CLOEXEC);
    if (fd < 0) {
        return -1;
    }

    if (ioctl(fd, EVIOCGID, &id) < 0 ||
        ioctl(fd, EVIOCGNAME(sizeof(name)), name) < 0 ||
        id.vendor != NINTENDO_VENDOR_ID || id.product != wanted_product ||
        !strstr(name, "Joy-Con Serial IMU") ||
        ioctl(fd, EVIOCGABS(ABS_X), &accel_info) < 0 ||
        ioctl(fd, EVIOCGABS(ABS_RX), &gyro_info) < 0 ||
        accel_info.resolution <= 0 || gyro_info.resolution <= 0) {
        close(fd);
        return -1;
    }

    *accel_resolution = accel_info.resolution;
    *gyro_resolution = gyro_info.resolution;
    return fd;
}

static int find_rail_imu(unsigned short wanted_product)
{
    struct dirent *entry;
    DIR *dir = opendir("/dev/input");
    if (!dir) {
        return -1;
    }

    int found_fd = -1;
    while ((entry = readdir(dir))) {
        if (strncmp(entry->d_name, "event", 5) != 0) {
            continue;
        }

        char path[sizeof(s_rail_path)];
        int accel_resolution;
        int gyro_resolution;
        snprintf(path, sizeof(path), "/dev/input/%s", entry->d_name);
        int fd = open_rail_candidate(path, wanted_product, &accel_resolution, &gyro_resolution);
        if (fd < 0) {
            continue;
        }

        s_rail_product = wanted_product;
        s_rail_accel_resolution = accel_resolution;
        s_rail_gyro_resolution = gyro_resolution;
        snprintf(s_rail_path, sizeof(s_rail_path), "%s", path);
        found_fd = fd;
        break;
    }

    closedir(dir);
    return found_fd;
}

static int open_preferred_rail_imu(void)
{
    int fd = find_rail_imu(JOYCON_LEFT_PRODUCT_ID);
    if (fd < 0) {
        fd = find_rail_imu(JOYCON_RIGHT_PRODUCT_ID);
    }
    if (fd >= 0) {
        vpilog("nx-imu: using rail IMU %s (accel_res=%d gyro_res=%d)\n",
            s_rail_path, s_rail_accel_resolution, s_rail_gyro_resolution);
    }
    return fd;
}

static void close_rail_imu(void)
{
    if (s_rail_fd >= 0) {
        close(s_rail_fd);
        s_rail_fd = -1;
    }
    s_rail_product = 0;
    s_rail_path[0] = '\0';
    s_rail_value_mask = 0;
}

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

static void apply_mount_matrix(const float mount[3][3], const float in[3], float out[3])
{
    for (int i = 0; i < 3; i++) {
        out[i] = mount[i][0]*in[0] + mount[i][1]*in[1] + mount[i][2]*in[2];
    }
}

static int setup_sensor(struct iio_device *dev, const char *ch_prefix, sensor_t *sensor)
{
    memset(sensor, 0, sizeof(*sensor));

    // Initialize mount matrix to identity
    for (int i = 0; i < 3; i++) {
        sensor->mount[i][i] = 1.0f;
    }

    const char *name = iio_device_get_name(dev);
    const char *id = iio_device_get_id(dev);
    const char *dev_name = name ? name : id;

    // Find channels
    static const char suffixes[3] = {'x', 'y', 'z'};
    char ch_id[32];

    for (int i = 0; i < 3; i++) {
        snprintf(ch_id, sizeof(ch_id), "%s_%c", ch_prefix, suffixes[i]);
        sensor->ch[i] = iio_device_find_channel(dev, ch_id, false);
        if (!sensor->ch[i]) {
            vpilog("nx-imu: %s channel %s not found\n", dev_name, ch_id);
            return -1;
        }

        // Enable channel for buffer capture
        iio_channel_enable(sensor->ch[i]);
        vpilog("nx-imu: %s enabled channel %s\n", dev_name, ch_id);
    }

    // Read scale from first channel
    const char *scale_attr = iio_channel_find_attr(sensor->ch[0], "scale");
    if (scale_attr) {
        double scale_val = 0;
        if (iio_channel_attr_read_double(sensor->ch[0], scale_attr, &scale_val) == 0) {
            sensor->scale = (float)scale_val;
            vpilog("nx-imu: %s scale=%.6g\n", dev_name, sensor->scale);
        }
    }

    // Read mount matrix from device
    const char *mount_attr = iio_device_find_attr(dev, "mount_matrix");
    if (mount_attr) {
        char mount_buf[128];
        if (iio_device_attr_read(dev, mount_attr, mount_buf, sizeof(mount_buf)) > 0) {
            if (parse_mount_matrix(mount_buf, sensor->mount) == 0) {
                vpilog("nx-imu: %s mount_matrix=%s\n", dev_name, mount_buf);
            }
        }
    }

    // Create buffer for reading samples
    sensor->buf = iio_device_create_buffer(dev, BUFFER_SAMPLES, false);
    if (!sensor->buf) {
        vpilog("nx-imu: %s failed to create buffer: %s\n", dev_name, strerror(errno));
        return -1;
    }

    sensor->dev = dev;
    sensor->valid = 1;
    vpilog("nx-imu: %s sensor ready (buffer interface)\n", dev_name);
    return 0;
}

static int read_sensor(sensor_t *sensor, float out[3])
{
    if (!sensor->valid) return -1;

    // Refill buffer to get latest samples
    ssize_t ret = iio_buffer_refill(sensor->buf);
    if (ret < 0) {
        return -1;
    }

    // Read raw values from buffer
    float raw[3];
    for (int i = 0; i < 3; i++) {
        // Get pointer to channel data in buffer
        void *start = iio_buffer_first(sensor->buf, sensor->ch[i]);
        void *end = iio_buffer_end(sensor->buf);

        if (!start || start >= end) {
            return -1;
        }

        // Data format is le:S16/16 (signed 16-bit little-endian)
        int16_t sample = *(int16_t *)start;
        raw[i] = (float)sample * sensor->scale;
    }

    // Apply mount matrix transform
    apply_mount_matrix(sensor->mount, raw, out);
    return 0;
}

static void publish_motion(const float a[3], const float g[3])
{
    vanilla_set_button(VANILLA_SENSOR_ACCEL_X, pack_float(a[0]));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Y, pack_float(a[1]));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Z, pack_float(a[2]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_PITCH, pack_float(g[0]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_YAW,   pack_float(g[1]));
    vanilla_set_button(VANILLA_SENSOR_GYRO_ROLL,  pack_float(g[2]));
}

static void chassis_to_gamepad(const float in[3], float out[3])
{
    out[0] = -in[0];
    out[1] = in[2];
    out[2] = in[1];
}

static void publish_iio(const float a[3], const float g[3])
{
    float transformed_a[3];
    float transformed_g[3];
    chassis_to_gamepad(a, transformed_a);
    chassis_to_gamepad(g, transformed_g);
    publish_motion(transformed_a, transformed_g);

    static int log_throttle = 0;
    if ((++log_throttle % 40) == 1) {
        vpilog("nx-imu: a=(%.2f %.2f %.2f) m/s^2  g=(%.2f %.2f %.2f) rad/s\n",
            transformed_a[0], transformed_a[1], transformed_a[2],
            transformed_g[0], transformed_g[1], transformed_g[2]);
    }
}

static void publish_rail(void)
{
    float accel_scale = STANDARD_GRAVITY / s_rail_accel_resolution;
    float gyro_scale = (float)M_PI /
        (180.0f * s_rail_gyro_resolution);

    // Kernel exposes joycon IMU as if Switch is held portrait, remap
    const float chassis_a[3] = {
        -s_rail_values[1] * accel_scale,
         s_rail_values[0] * accel_scale,
         s_rail_values[2] * accel_scale,
    };
    const float chassis_g[3] = {
        -s_rail_values[4] * gyro_scale,
         s_rail_values[3] * gyro_scale,
         s_rail_values[5] * gyro_scale,
    };

    // Remap to SDL (what Vanilla is written around)
    const float gamepad_a[3] = {
        chassis_a[0],
        chassis_a[2],
        -chassis_a[1],
    };
    const float gamepad_g[3] = {
        chassis_g[0],
        chassis_g[2],
        -chassis_g[1],
    };

    const float *a = s_rail_transform_mode == RAIL_TRANSFORM_LEGACY ? chassis_a : gamepad_a;
    const float *g = s_rail_transform_mode == RAIL_TRANSFORM_LEGACY ? chassis_g : gamepad_g;
    publish_motion(a, g);

    // static int log_throttle = 0;
    // if ((++log_throttle % 200) == 1) {
    //     vpilog("nx-imu: rail raw a=(%d %d %d) g=(%d %d %d); "
    //         "published a=(%.2f %.2f %.2f) g=(%.2f %.2f %.2f)\n",
    //         s_rail_values[0], s_rail_values[1], s_rail_values[2],
    //         s_rail_values[3], s_rail_values[4], s_rail_values[5],
    //         a[0], a[1], a[2], g[0], g[1], g[2]);
    // }
}

static int read_rail_imu(void)
{
    struct pollfd poll_fd = {
        .fd = s_rail_fd,
        .events = POLLIN,
    };
    int poll_result = poll(&poll_fd, 1, RAIL_POLL_TIMEOUT_MS);
    if (poll_result < 0) {
        return errno == EINTR ? 0 : -1;
    }
    if (poll_result == 0) {
        return 0;
    }
    if (poll_fd.revents & (POLLERR | POLLHUP | POLLNVAL)) {
        return -1;
    }

    struct input_event events[64];
    ssize_t size = read(s_rail_fd, events, sizeof(events));
    if (size < 0) {
        return errno == EAGAIN || errno == EINTR ? 0 : -1;
    }
    if (size == 0 || size % sizeof(events[0]) != 0) {
        return -1;
    }

    int published = 0;
    size_t event_count = size / sizeof(events[0]);
    for (size_t i = 0; i < event_count; i++) {
        const struct input_event *event = &events[i];
        if (event->type == EV_ABS) {
            int value_index = -1;
            switch (event->code) {
            case ABS_X:  value_index = 0; break;
            case ABS_Y:  value_index = 1; break;
            case ABS_Z:  value_index = 2; break;
            case ABS_RX: value_index = 3; break;
            case ABS_RY: value_index = 4; break;
            case ABS_RZ: value_index = 5; break;
            }
            if (value_index >= 0) {
                s_rail_values[value_index] = event->value;
                s_rail_value_mask |= 1U << value_index;
            }
        } else if (event->type == EV_SYN && event->code == SYN_REPORT) {
            if (s_rail_value_mask == 0x3f) {
                publish_rail();
                published = 1;
            }
            s_rail_value_mask = 0;
        }
    }
    return published;
}

static void scan_devices(void)
{
    memset(&s_accel, 0, sizeof(s_accel));
    memset(&s_gyro, 0, sizeof(s_gyro));

    unsigned int nb_devices = iio_context_get_devices_count(s_ctx);
    vpilog("nx-imu: found %u IIO devices\n", nb_devices);

    for (unsigned int i = 0; i < nb_devices; i++) {
        struct iio_device *dev = iio_context_get_device(s_ctx, i);
        const char *name = iio_device_get_name(dev);
        const char *id = iio_device_get_id(dev);

        vpilog("nx-imu: device %u: id=%s name=%s\n",
            i, id ? id : "(null)", name ? name : "(null)");

        // Check for accel device
        if (!s_accel.valid && iio_device_find_channel(dev, "accel_x", false)) {
            // Accel scale from IIO is already in m/s^2 per LSB
            setup_sensor(dev, "accel", &s_accel);
        }

        // Check for gyro device
        if (!s_gyro.valid && iio_device_find_channel(dev, "anglvel_x", false)) {
            // Gyro scale from IIO is already in rad/s per LSB
            setup_sensor(dev, "anglvel", &s_gyro);
        }
    }
}

static int initialize_iio(void)
{
    if (s_iio_initialized) {
        return s_accel.valid || s_gyro.valid ? 0 : -1;
    }
    s_iio_initialized = 1;

    s_ctx = iio_create_local_context();
    if (!s_ctx) {
        vpilog("nx-imu: failed to create IIO context: %s\n", strerror(errno));
        return -1;
    }

    vpilog("nx-imu: IIO context created (%s)\n", iio_context_get_name(s_ctx));
    scan_devices();
    if (!s_accel.valid && !s_gyro.valid) {
        vpilog("nx-imu: no onboard IMU sensors found\n");
        iio_context_destroy(s_ctx);
        s_ctx = NULL;
        return -1;
    }

    vpilog("nx-imu: onboard fallback ready (accel=%d gyro=%d)\n",
        s_accel.valid, s_gyro.valid);
    return 0;
}

static void cleanup_sensor(sensor_t *sensor)
{
    if (sensor->buf) {
        iio_buffer_destroy(sensor->buf);
        sensor->buf = NULL;
    }
    if (sensor->valid) {
        for (int i = 0; i < 3; i++) {
            if (sensor->ch[i]) {
                iio_channel_disable(sensor->ch[i]);
            }
        }
    }
    memset(sensor, 0, sizeof(*sensor));
}

static uint64_t monotonic_milliseconds(void)
{
    struct timespec time;
    clock_gettime(CLOCK_MONOTONIC, &time);
    return (uint64_t)time.tv_sec * 1000 + time.tv_nsec / 1000000;
}

static void *imu_thread(void *arg)
{
    (void)arg;
    uint64_t next_rail_scan = monotonic_milliseconds();
    uint64_t last_rail_sample = next_rail_scan;

    while (atomic_load(&s_run)) {
        uint64_t now = monotonic_milliseconds();

        if (s_source_mode != IMU_SOURCE_IIO) {
            if (s_rail_fd < 0 && now >= next_rail_scan) {
                s_rail_fd = open_preferred_rail_imu();
                next_rail_scan = now + RAIL_SCAN_INTERVAL_MS;
                if (s_rail_fd >= 0) {
                    last_rail_sample = now;
                }
            } else if (s_rail_product == JOYCON_RIGHT_PRODUCT_ID &&
                now >= next_rail_scan) {
                int left_fd = find_rail_imu(JOYCON_LEFT_PRODUCT_ID);
                next_rail_scan = now + RAIL_SCAN_INTERVAL_MS;
                if (left_fd >= 0) {
                    close(s_rail_fd);
                    s_rail_fd = left_fd;
                    s_rail_value_mask = 0;
                    last_rail_sample = now;
                    vpilog("nx-imu: promoted motion source to left rail %s\n",
                        s_rail_path);
                }
            }

            if (s_rail_fd >= 0) {
                int read_result = read_rail_imu();
                now = monotonic_milliseconds();
                if (read_result > 0) {
                    last_rail_sample = now;
                } else if (read_result < 0) {
                    vpilog("nx-imu: rail IMU %s disconnected\n", s_rail_path);
                    close_rail_imu();
                    next_rail_scan = now + RAIL_SCAN_INTERVAL_MS;
                } else if (now - last_rail_sample >= RAIL_STALL_TIMEOUT_MS) {
                    vpilog("nx-imu: rail IMU %s stalled%s\n", s_rail_path,
                        s_source_mode == IMU_SOURCE_AUTO ?
                            ", using fallback" : "");
                    close_rail_imu();
                    next_rail_scan = now + RAIL_STALL_TIMEOUT_MS;
                }
                continue;
            }
        }

        if (s_source_mode == IMU_SOURCE_RAIL) {
            usleep(RAIL_POLL_TIMEOUT_MS * 1000);
            continue;
        }

        if (!s_iio_initialized) {
            initialize_iio();
        }

        float a[3] = {0, 0, 0};
        float g[3] = {0, 0, 0};
        int ok = 1;

        if (s_accel.valid) {
            if (read_sensor(&s_accel, a) < 0) {
                ok = 0;
            }
        }

        if (ok && s_gyro.valid) {
            if (read_sensor(&s_gyro, g) < 0) {
                ok = 0;
            }
        }

        if (ok && (s_accel.valid || s_gyro.valid)) {
            publish_iio(a, g);
        } else if (!ok) {
            static int err_count = 0;
            if ((++err_count % 100) == 1) {
                vpilog("nx-imu: read error\n");
            }
        } else {
            usleep(RAIL_POLL_TIMEOUT_MS * 1000);
        }

        // iio_buffer_refill blocks while onboard fallback is active
    }

    return NULL;
}

int vpi_nx_imu_init(void)
{
    if (atomic_load(&s_run)) {
        return 0;
    }

    select_source_mode();
    if (s_source_mode != IMU_SOURCE_IIO) {
        s_rail_fd = open_preferred_rail_imu();
    }
    if (s_source_mode == IMU_SOURCE_IIO) {
        initialize_iio();
    } else if (s_source_mode == IMU_SOURCE_AUTO && s_rail_fd < 0) {
        vpilog("nx-imu: no rail IMU found, trying onboard fallback\n");
        initialize_iio();
    }

    atomic_store(&s_run, 1);
    if (pthread_create(&s_thread, NULL, imu_thread, NULL) != 0) {
        atomic_store(&s_run, 0);
        close_rail_imu();
        cleanup_sensor(&s_accel);
        cleanup_sensor(&s_gyro);
        if (s_ctx) {
            iio_context_destroy(s_ctx);
            s_ctx = NULL;
        }
        s_iio_initialized = 0;
        vpilog("nx-imu: pthread_create failed\n");
        return -1;
    }

    vpilog("nx-imu: motion reader started\n");
    return 0;
}

void vpi_nx_imu_quit(void)
{
    if (!atomic_load(&s_run)) {
        return;
    }

    atomic_store(&s_run, 0);
    pthread_join(s_thread, NULL);

    close_rail_imu();
    cleanup_sensor(&s_accel);
    cleanup_sensor(&s_gyro);

    if (s_ctx) {
        iio_context_destroy(s_ctx);
        s_ctx = NULL;
    }
    s_iio_initialized = 0;

    vpilog("nx-imu: stopped\n");
}
