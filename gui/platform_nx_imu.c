#include "platform_nx_imu.h"

#include <errno.h>
#include <math.h>
#include <pthread.h>
#include <stdbool.h>
#include <stdatomic.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <unistd.h>

#include <iio.h>
#include <vanilla.h>

#include "ui/ui_util.h"
#include "platform.h"

// Reads accelerometer and gyroscope from the Switch's onboard IMU using
// libiio's buffer interface. The raw attribute reads appear broken for
// the gyro on this kernel, but the buffer interface works correctly
// (same as iio_oscilloscope uses).

#define BUFFER_SAMPLES   1             // We just need the latest sample

typedef struct {
    struct iio_device *dev;
    struct iio_buffer *buf;
    struct iio_channel *ch[3];         // x, y, z channels
    float scale;
    float mount[3][3];                 // mount matrix transform
    int valid;
} sensor_t;

static struct iio_context *s_ctx = NULL;
static sensor_t s_accel;
static sensor_t s_gyro;

static pthread_t s_thread;
static atomic_int s_run = 0;

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

static void publish(const float a[3], const float g[3])
{
    // Transform from Switch chassis coords to Wii U GamePad coords.
    // Switch upright (screen facing user) should map to GamePad upright.
    float ax = -a[0];   // Negate X for correct roll
    float ay = a[2];   // GamePad Y = -Switch Z
    float az = a[1];   // GamePad Z = -Switch Y

    float gx = -g[0];   // Negate X for correct roll
    float gy = g[2];
    float gz = g[1];

    vanilla_set_button(VANILLA_SENSOR_ACCEL_X, pack_float(ax));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Y, pack_float(ay));
    vanilla_set_button(VANILLA_SENSOR_ACCEL_Z, pack_float(az));
    vanilla_set_button(VANILLA_SENSOR_GYRO_PITCH, pack_float(gx));
    vanilla_set_button(VANILLA_SENSOR_GYRO_YAW,   pack_float(gy));
    vanilla_set_button(VANILLA_SENSOR_GYRO_ROLL,  pack_float(gz));

    static int log_throttle = 0;
    if ((++log_throttle % 40) == 1) {
        vpilog("nx-imu: a=(%.2f %.2f %.2f) m/s^2  g=(%.2f %.2f %.2f) rad/s\n",
            ax, ay, az, gx, gy, gz);
    }
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

static void *imu_thread(void *arg)
{
    (void)arg;

    while (atomic_load(&s_run)) {
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
            publish(a, g);
        } else if (!ok) {
            static int err_count = 0;
            if ((++err_count % 100) == 1) {
                vpilog("nx-imu: read error\n");
            }
        }

        // No sleep needed - iio_buffer_refill blocks until data is ready
    }

    return NULL;
}

int vpi_nx_imu_init(void)
{
    if (atomic_load(&s_run)) {
        return 0;
    }

    s_ctx = iio_create_local_context();
    if (!s_ctx) {
        vpilog("nx-imu: failed to create IIO context: %s\n", strerror(errno));
        return -1;
    }

    vpilog("nx-imu: IIO context created (%s)\n", iio_context_get_name(s_ctx));

    scan_devices();

    if (!s_accel.valid && !s_gyro.valid) {
        vpilog("nx-imu: no IMU sensors found\n");
        iio_context_destroy(s_ctx);
        s_ctx = NULL;
        return -1;
    }

    vpilog("nx-imu: sensors ready (accel=%d gyro=%d)\n", s_accel.valid, s_gyro.valid);

    atomic_store(&s_run, 1);
    if (pthread_create(&s_thread, NULL, imu_thread, NULL) != 0) {
        atomic_store(&s_run, 0);
        cleanup_sensor(&s_accel);
        cleanup_sensor(&s_gyro);
        iio_context_destroy(s_ctx);
        s_ctx = NULL;
        vpilog("nx-imu: pthread_create failed\n");
        return -1;
    }

    vpilog("nx-imu: started (libiio buffer interface)\n");
    return 0;
}

void vpi_nx_imu_quit(void)
{
    if (!atomic_load(&s_run)) {
        return;
    }

    atomic_store(&s_run, 0);
    pthread_join(s_thread, NULL);

    cleanup_sensor(&s_accel);
    cleanup_sensor(&s_gyro);

    if (s_ctx) {
        iio_context_destroy(s_ctx);
        s_ctx = NULL;
    }

    vpilog("nx-imu: stopped\n");
}
