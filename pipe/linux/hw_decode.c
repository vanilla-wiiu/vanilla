#include "hw_decode.h"

#include <fcntl.h>
#include <linux/videodev2.h>
#include <poll.h>
#include <stdio.h>
#include <string.h>
#include <sys/ioctl.h>
#include <sys/mman.h>
#include <unistd.h>

#define NUM_BUFFERS 4
#define WIDTH 854
#define HEIGHT 480
#define INPUT_BUF_SIZE (1024 * 1024)  // 1MB input buffer

struct buffer {
    void* start;
    size_t length;
};

static int fd = -1;
static struct buffer* input_buffers;
static struct buffer* output_buffers;
static int num_input_buffers;
static int num_output_buffers;

static int init_device() {
    struct v4l2_capability cap;
    if (ioctl(fd, VIDIOC_QUERYCAP, &cap) < 0) {
        perror("VIDIOC_QUERYCAP");
        return -1;
    }

    // Set input format (H.264)
    struct v4l2_format fmt = {0};
    fmt.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    fmt.fmt.pix_mp.width = WIDTH;
    fmt.fmt.pix_mp.height = HEIGHT;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_H264;
    fmt.fmt.pix_mp.field = V4L2_FIELD_NONE;
    fmt.fmt.pix_mp.num_planes = 1;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = INPUT_BUF_SIZE;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT(INPUT)");
        return -1;
    }

    // Set output format (YUV420)
    fmt.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    fmt.fmt.pix_mp.pixelformat = V4L2_PIX_FMT_YUV420;
    fmt.fmt.pix_mp.plane_fmt[0].sizeimage = WIDTH * HEIGHT * 3 / 2;

    if (ioctl(fd, VIDIOC_S_FMT, &fmt) < 0) {
        perror("VIDIOC_S_FMT(OUTPUT)");
        return -1;
    }

    return 0;
}

static int init_buffers() {
    // Request input buffers
    struct v4l2_requestbuffers reqbuf = {0};
    reqbuf.count = NUM_BUFFERS;
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    reqbuf.memory = V4L2_MEMORY_MMAP;

    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        perror("VIDIOC_REQBUFS(INPUT)");
        return -1;
    }
    num_input_buffers = reqbuf.count;

    // Allocate input buffers
    input_buffers = calloc(reqbuf.count, sizeof(*input_buffers));
    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane planes[1] = {{0}};
        buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = 1;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF(INPUT)");
            return -1;
        }

        input_buffers[i].length = buf.m.planes[0].length;
        input_buffers[i].start = mmap(NULL, buf.m.planes[0].length,
                                    PROT_READ | PROT_WRITE, MAP_SHARED,
                                    fd, buf.m.planes[0].m.mem_offset);
        if (input_buffers[i].start == MAP_FAILED) {
            perror("mmap(INPUT)");
            return -1;
        }
    }

    // Similar process for output buffers
    reqbuf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_REQBUFS, &reqbuf) < 0) {
        perror("VIDIOC_REQBUFS(OUTPUT)");
        return -1;
    }
    num_output_buffers = reqbuf.count;

    output_buffers = calloc(reqbuf.count, sizeof(*output_buffers));
    for (int i = 0; i < reqbuf.count; i++) {
        struct v4l2_buffer buf = {0};
        struct v4l2_plane planes[1] = {{0}};
        buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
        buf.memory = V4L2_MEMORY_MMAP;
        buf.index = i;
        buf.m.planes = planes;
        buf.length = 1;

        if (ioctl(fd, VIDIOC_QUERYBUF, &buf) < 0) {
            perror("VIDIOC_QUERYBUF(OUTPUT)");
            return -1;
        }

        output_buffers[i].length = buf.m.planes[0].length;
        output_buffers[i].start = mmap(NULL, buf.m.planes[0].length,
                                     PROT_READ | PROT_WRITE, MAP_SHARED,
                                     fd, buf.m.planes[0].m.mem_offset);
        if (output_buffers[i].start == MAP_FAILED) {
            perror("mmap(OUTPUT)");
            return -1;
        }
    }

    return 0;
}

int hw_decode_init() {
    // Open the V4L2 device
    fd = open("/dev/video10", O_RDWR);
    if (fd < 0) {
        perror("Failed to open decoder device");
        return -1;
    }

    if (init_device() < 0 || init_buffers() < 0) {
        close(fd);
        fd = -1;
        return -1;
    }

    // Start streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON(INPUT)");
        return -1;
    }

    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_STREAMON, &type) < 0) {
        perror("VIDIOC_STREAMON(OUTPUT)");
        return -1;
    }

    return 0;
}

int hw_decode_frame(const uint8_t* input, size_t input_size, 
                   uint8_t* output, size_t output_size) {
    if (fd < 0) return -1;

    // Queue input buffer
    struct v4l2_buffer buf = {0};
    struct v4l2_plane planes[1] = {{0}};
    buf.type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    buf.memory = V4L2_MEMORY_MMAP;
    buf.m.planes = planes;
    buf.length = 1;

    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("VIDIOC_DQBUF(INPUT)");
        return -1;
    }

    memcpy(input_buffers[buf.index].start, input, input_size);
    buf.m.planes[0].bytesused = input_size;

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF(INPUT)");
        return -1;
    }

    // Get decoded frame
    buf.type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    if (ioctl(fd, VIDIOC_DQBUF, &buf) < 0) {
        perror("VIDIOC_DQBUF(OUTPUT)");
        return -1;
    }

    size_t frame_size = buf.m.planes[0].bytesused;
    if (frame_size > output_size) {
        return -1;
    }

    memcpy(output, output_buffers[buf.index].start, frame_size);

    if (ioctl(fd, VIDIOC_QBUF, &buf) < 0) {
        perror("VIDIOC_QBUF(OUTPUT)");
        return -1;
    }

    return frame_size;
}

void hw_decode_cleanup() {
    if (fd < 0) return;

    // Stop streaming
    enum v4l2_buf_type type = V4L2_BUF_TYPE_VIDEO_OUTPUT_MPLANE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);
    type = V4L2_BUF_TYPE_VIDEO_CAPTURE_MPLANE;
    ioctl(fd, VIDIOC_STREAMOFF, &type);

    // Unmap and free buffers
    for (int i = 0; i < num_input_buffers; i++) {
        munmap(input_buffers[i].start, input_buffers[i].length);
    }
    for (int i = 0; i < num_output_buffers; i++) {
        munmap(output_buffers[i].start, output_buffers[i].length);
    }

    free(input_buffers);
    free(output_buffers);

    close(fd);
    fd = -1;
} 