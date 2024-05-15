#include "gamepad.h"

#include <arpa/inet.h>
#include <errno.h>
#include <netinet/in.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"
#include "util.h"
#include "wpa.h"

static const uint16_t PORT_MSG = 50110;
static const uint16_t PORT_VID = 50120;
static const uint16_t PORT_AUD = 50121;
static const uint16_t PORT_HID = 50122;
static const uint16_t PORT_CMD = 50123;

unsigned int reverse_bits(unsigned int b, int bit_count)
{
    unsigned int result = 0;

    for (int i = 0; i < bit_count; i++) {
        result |= ((b >> i) & 1) << (bit_count - 1 -i );
    }

    return result;
}

typedef struct
{
    unsigned magic : 4;
    unsigned packet_type : 2;
    unsigned seq_id : 10;
    unsigned init : 1;
    unsigned frame_begin : 1;
    unsigned chunk_end : 1;
    unsigned frame_end : 1;
    unsigned has_timestamp : 1;
    unsigned payload_size : 11;
    unsigned timestamp : 32;
    uint8_t extended_header[8];
    uint8_t payload[2048];
} VideoPacket;

int h264_nal_encapsulate(int is_idr, uint8_t *frame, size_t frame_size, uint8_t *nals);

void send_to_console(int fd, const void *data, size_t data_size, int port)
{
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = inet_addr("192.168.1.10");
    address.sin_port = htons((uint16_t) (port - 100));
    ssize_t sent = sendto(fd, data, data_size, 0, (const struct sockaddr *) &address, sizeof(address));
    if (sent == -1) {
        print_info("Failed to send to Wii U socket: fd - %d; port - %d", fd, port);
    }
}

void handle_video_packet(vanilla_event_handler_t event_handler, void *context, unsigned char *data, size_t size, int socket_msg)
{
    // TODO: This is all really weird. Copied from drc-sim-c but I feel like there's probably a better way.

    for (size_t i = 0; i < size; i++) {
        data[i] = reverse_bits(data[i], 8);
    }

    VideoPacket *vp = (VideoPacket *) data;

    vp->magic = reverse_bits(vp->magic, 4);
    vp->packet_type = reverse_bits(vp->packet_type, 2);
    vp->seq_id = reverse_bits(vp->seq_id, 10);
    vp->payload_size = reverse_bits(vp->payload_size, 11);
    vp->timestamp = reverse_bits(vp->timestamp, 32);
    for (int byte = 0; byte < sizeof(vp->extended_header); ++byte)
        vp->extended_header[byte] = (unsigned char) reverse_bits(vp->extended_header[byte], 8);
    for (int byte = 0; byte < vp->payload_size; ++byte)
        vp->payload[byte] = (unsigned char) reverse_bits(vp->payload[byte], 8);
    
    // Check is_idr_packet (what the fuck is 'idr'?)
    int is_idr = 0;
    for (int i = 0; i < sizeof(vp->extended_header); i++) {
        if (vp->extended_header[i] == 0x80) {
            is_idr = 1;
            break;
        }
    }

    // Check seq ID
    static int seq_id_expected = -1;
    int seq_matched = 1;
    if (seq_id_expected == -1) {
        seq_id_expected = vp->seq_id;
    } else if (seq_id_expected != vp->seq_id) {
        seq_matched = 0;
    }
    seq_id_expected = (vp->seq_id + 1) & 0x3ff;  // 10 bit number
    static int is_streaming = 0;
    if (!seq_matched) {
        is_streaming = 0;
    }

    // Check if this is the beginning of the packet
    static char video_packet[100000];
    static size_t video_packet_size = 0;

    if (vp->frame_begin) {
        video_packet_size = 0;
        if (!is_streaming) {
            if (is_idr) {
                is_streaming = 1;
            } else {
                // Make an IDR request to the Wii U?
                unsigned char idr_request[] = {1, 0, 0, 0}; // Undocumented
                send_to_console(socket_msg, idr_request, sizeof(idr_request), PORT_MSG);
                return;
            }
        }
    }

    memcpy(video_packet + video_packet_size, vp->payload, vp->payload_size);
    video_packet_size += vp->payload_size;

    if (vp->frame_end) {
        if (is_streaming) {
            // Encapsulate packet data into NAL unit
            static int frame_decode_num = 0;
            size_t nals_sz = video_packet_size * 2;
            uint8_t *nals = malloc(nals_sz);
            uint8_t *nals_current = nals;
            int slice_header = is_idr ? 0x25b804ff : (0x21e003ff | ((frame_decode_num & 0xff) << 13));
            frame_decode_num++;

            uint8_t params[] = {
                    // sps
                    0x00, 0x00, 0x00, 0x01, 0x67, 0x64, 0x00, 0x20, 0xac, 0x2b, 0x40, 0x6c, 0x1e, 0xf3, 0x68,
                    // pps
                    0x00, 0x00, 0x00, 0x01, 0x68, 0xee, 0x06, 0x0c, 0xe8
            };

            if (is_idr) {
                memcpy(nals_current, params, sizeof(params));
                nals_current += sizeof(params);
            }

            // begin slice nalu
            uint8_t slice[] = {0x00, 0x00, 0x00, 0x01,
                            (uint8_t) ((slice_header >> 24) & 0xff),
                            (uint8_t) ((slice_header >> 16) & 0xff),
                            (uint8_t) ((slice_header >> 8) & 0xff),
                            (uint8_t) (slice_header & 0xff)
            };
            memcpy(nals_current, slice, sizeof(slice));
            nals_current += sizeof(slice);

            // Frame
            memcpy(nals_current, video_packet, 2);
            nals_current += 2;

            // Escape codes
            for (int byte = 2; byte < video_packet_size; ++byte) {
                if (video_packet[byte] <= 3 && *(nals_current - 2) == 0 && *(nals_current - 1) == 0) {
                    *nals_current = 3;
                    nals_current++;
                }
                *nals_current = video_packet[byte];
                nals_current++;
            }

            event_handler(context, VANILLA_EVENT_VIDEO, nals, (nals_current - nals));

            free(nals);
        } else {
            // We didn't receive the complete frame so we'll skip it here
        }
    }
}

#pragma pack(push, 1)

typedef struct {
    // Little endian
    int16_t x;
    int16_t y;
    int16_t z;
} InputPacketAccelerometerWiiU;

typedef struct {
    // Little endian
    signed roll : 24;
    signed yaw : 24;
    signed pitch : 24;
} InputPacketGyroscopeWiiU;

typedef struct {
    signed char unknown[6];
} InputPacketMagnetWiiU;

typedef struct {
    // Big endian
    struct {
        struct {
            unsigned pad : 1;
            unsigned extra : 3;
            unsigned value : 12;
        } coords[2];
    } points[10];
} InputPacketTouchscreenWiiU;

pthread_mutex_t button_mtx;
uint16_t current_buttons = 0;
uint8_t current_extra_buttons = 0;
float current_stick_left_x = 0;
float current_stick_left_y = 0;
float current_stick_right_x = 0;
float current_stick_right_y = 0;

typedef struct {
    // Big endian
    uint16_t seq_id;
    uint16_t buttons;
    uint8_t power_status;
    uint8_t battery_charge;
    uint16_t stick_left_x;
    uint16_t stick_left_y;
    uint16_t stick_right_x;
    uint16_t stick_right_y;
    uint8_t audio_volume;
    InputPacketAccelerometerWiiU accelerometer;
    InputPacketGyroscopeWiiU gyroscope;
    InputPacketMagnetWiiU magnet;
    InputPacketTouchscreenWiiU touchscreen; // byte 36 - 76
    unsigned char unknown_0[4];
    uint8_t extra_buttons;
    unsigned char unknown_1[46];
    uint8_t fw_version_neg;
} InputPacket;

#pragma pack(pop)

InputPacket global_input_packet;

void set_button_state_bit(uint16_t bit_flag, int pressed)
{
    pthread_mutex_lock(&button_mtx);
    if (pressed) {
        current_buttons |= bit_flag;
    } else {
        current_buttons &= ~bit_flag;
    }
    pthread_mutex_unlock(&button_mtx);
}

void set_extra_button_state_bit(uint8_t bit_flag, int pressed)
{
    pthread_mutex_lock(&button_mtx);
    if (pressed) {
        current_extra_buttons |= bit_flag;
    } else {
        current_extra_buttons &= ~bit_flag;
    }
    pthread_mutex_unlock(&button_mtx);
}

void set_button_state(int button_id, int pressed)
{
    switch (button_id) {
    case VANILLA_BTN_A:
        set_button_state_bit(0x8000, pressed);
        break;
    case VANILLA_BTN_B:
        set_button_state_bit(0x4000, pressed);
        break;
    case VANILLA_BTN_X:
        set_button_state_bit(0x2000, pressed);
        break;
    case VANILLA_BTN_Y:
        set_button_state_bit(0x1000, pressed);
        break;
    case VANILLA_BTN_L:
        set_button_state_bit(0x0020, pressed);
        break;
    case VANILLA_BTN_R:
        set_button_state_bit(0x0010, pressed);
        break;
    case VANILLA_BTN_ZL:
        set_button_state_bit(0x0080, pressed);
        break;
    case VANILLA_BTN_ZR:
        set_button_state_bit(0x0040, pressed);
        break;
    case VANILLA_BTN_MINUS:
        set_button_state_bit(0x0004, pressed);
        break;
    case VANILLA_BTN_PLUS:
        set_button_state_bit(0x0008, pressed);
        break;
    case VANILLA_BTN_HOME:
        set_button_state_bit(0x0002, pressed);
        break;
    case VANILLA_BTN_LEFT:
        set_button_state_bit(0x800, pressed);
        break;
    case VANILLA_BTN_RIGHT:
        set_button_state_bit(0x400, pressed);
        break;
    case VANILLA_BTN_DOWN:
        set_button_state_bit(0x100, pressed);
        break;
    case VANILLA_BTN_UP:
        set_button_state_bit(0x200, pressed);
        break;

    // "Extra" buttons
    case VANILLA_BTN_L3:
        set_extra_button_state_bit(0x08, pressed);
        break;
    case VANILLA_BTN_R3:
        set_extra_button_state_bit(0x04, pressed);
        break;
    }
}

void set_axis_state(int axis_id, float value)
{
    pthread_mutex_lock(&button_mtx);
    switch (axis_id) {
    case VANILLA_AXIS_L_X:
        current_stick_left_x = value;
        break;
    case VANILLA_AXIS_L_Y:
        current_stick_left_y = value;
        break;
    case VANILLA_AXIS_R_X:
        current_stick_right_x = value;
        break;
    case VANILLA_AXIS_R_Y:
        current_stick_right_y = value;
        break;
    }
    pthread_mutex_unlock(&button_mtx);
}

uint16_t transform_axis_value(float real)
{
    static const uint16_t min = 900;
    static const uint16_t max = 3200;
    static const uint16_t center = (max - min) / 2;

    return ((int) (real * (max - min))) + center;
}

void send_input(int socket_hid)
{
    static uint16_t seq_id = 0;

    pthread_mutex_lock(&button_mtx);

    global_input_packet.buttons = htons(current_buttons);
    global_input_packet.extra_buttons = htons(current_extra_buttons);

    // Range appears to be:
    // 

    // print_info("sending X axis: %f -> %i", current_stick_left_x, transform_axis_value(current_stick_left_x));
    // global_input_packet.stick_left_x = htons(transform_axis_value(current_stick_left_x));
    // global_input_packet.stick_left_y = htons(transform_axis_value(-current_stick_left_y));
    // global_input_packet.stick_right_x = htons(transform_axis_value(current_stick_right_x));
    // global_input_packet.stick_right_y = htons(transform_axis_value(-current_stick_right_y));

    pthread_mutex_unlock(&button_mtx);

    global_input_packet.seq_id = htons(seq_id);
    seq_id++;

    global_input_packet.fw_version_neg = 215;

    send_to_console(socket_hid, &global_input_packet, sizeof(global_input_packet), PORT_HID);
}

int main_loop(vanilla_event_handler_t event_handler, void *context)
{
    int socket_msg, socket_cmd, socket_aud, socket_vid, socket_hid;

    // Open port for video input
    struct sockaddr_in address;
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = INADDR_ANY;

    // TODO: Limit these sockets to one interface?

    int ret = VANILLA_ERROR;

    // Video feed
    address.sin_port = htons(PORT_VID);
    socket_vid = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(socket_vid, (const struct sockaddr *) &address, sizeof(address)) == -1) {
        print_info("FAILED TO BIND VID PORT: %i", errno);
        goto exit;
    }

    // Message feed
    address.sin_port = htons(PORT_MSG);
    socket_msg = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(socket_msg, (const struct sockaddr *) &address, sizeof(address)) == -1) {
        print_info("FAILED TO BIND MSG PORT: %i", errno);
        goto exit_vid;
    }

    // Controller input feed
    address.sin_port = htons(PORT_HID);
    memset(&global_input_packet, 0, sizeof(global_input_packet));
    socket_hid = socket(AF_INET, SOCK_DGRAM, 0);
    if (bind(socket_hid, (const struct sockaddr *) &address, sizeof(address)) == -1) {
        print_info("FAILED TO BIND HID PORT: %i", errno);
        goto exit_msg;
    }

    pthread_mutex_init(&button_mtx, NULL);

    unsigned char data[2048];
    uint32_t size;

    // TODO: Perhaps these should be on separate threads?
    while (!is_interrupted()) {
        // Receive video
        size = recv(socket_vid, &data, sizeof(data), 0);
        if (size > 0) {
            handle_video_packet(event_handler, context, data, size, socket_msg);
        }

        // Receive message
        /*size = recv(socket_msg, &data, sizeof(data), 0);
        if (size > 0) {
            // TODO: Implement
        }*/
        send_input(socket_hid);
    }

    ret = VANILLA_SUCCESS;

    pthread_mutex_destroy(&button_mtx);

exit_msg:
    close(socket_msg);

exit_vid:
    close(socket_vid);

exit:
    return ret;
}

int connect_as_gamepad_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, vanilla_event_handler_t event_handler, void *context)
{
    while (1) {
        while (!wpa_ctrl_pending(ctrl)) {
            sleep(2);
            print_info("WAITING FOR CONNECTION");

            if (is_interrupted()) return VANILLA_ERROR;
        }

        char buf[1024];
        size_t actual_buf_len = sizeof(buf);
        wpa_ctrl_recv(ctrl, buf, &actual_buf_len);

        if (memcmp(buf, "<3>CTRL-EVENT-CONNECTED", 23) == 0) {
            break;
        }

        if (is_interrupted()) return VANILLA_ERROR;
    }

    print_info("CONNECTED TO CONSOLE");

    // Use DHCP on interface
    /*sleep(5);
    int r = call_dhcp(wireless_interface);
    if (r != VANILLA_SUCCESS) {
        print_info("FAILED TO RUN DHCP ON %s", wireless_interface);
        return r;
    } else {
        print_info("DHCP SEEMS TO HAVE WORKED?");
    }*/

    // Remove as default
    
    // ip route del default via 192.168.1.1 dev wlp4s0

    // Set region

    return main_loop(event_handler, context);
}