#include "video.h"

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/socket.h>
#include <sys/types.h>
#include <unistd.h>

#include "gamepad.h"
#include "vanilla.h"
#include "status.h"
#include "util.h"

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

pthread_mutex_t video_mutex;
int idr_is_queued = 0;

void request_idr()
{
    pthread_mutex_lock(&video_mutex);
    idr_is_queued = 1;
    pthread_mutex_unlock(&video_mutex);
}

void send_idr_request_to_console(int socket_msg)
{
    // Make an IDR request to the Wii U?
    unsigned char idr_request[] = {1, 0, 0, 0}; // Undocumented
    send_to_console(socket_msg, idr_request, sizeof(idr_request), PORT_MSG);
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
    
    // Check if packet is IDR (instantaneous decoder refresh)
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
    static char video_segments[1024][2048];
    static size_t video_segment_size[1024];
    static int video_packet_seq = -1;
    static int video_packet_seq_end = -1;

    if (vp->frame_begin) {
        video_packet_seq = vp->seq_id;
        video_packet_seq_end = -1;

        if (!is_streaming) {
            if (is_idr) {
                is_streaming = 1;
            } else {
                send_idr_request_to_console(socket_msg);
                return;
            }
        }
    }

    pthread_mutex_lock(&video_mutex);
    if (idr_is_queued) {
        send_idr_request_to_console(socket_msg);
        idr_is_queued = 0;
    }
    pthread_mutex_unlock(&video_mutex);

    memcpy(video_segments[vp->seq_id], vp->payload, vp->payload_size);
    video_segment_size[vp->seq_id] = vp->payload_size;

    if (vp->frame_end) {
        video_packet_seq_end = vp->seq_id;
    }

    if (video_packet_seq_end != -1) {
        // int complete_frame = 1;
        if (is_streaming) {
            // Combine segments
            char video_packet[100000];
            size_t video_packet_size = 0;
            for (int i = video_packet_seq; ; i = (i + 1) % 1024) {
                memcpy(video_packet + video_packet_size, video_segments[i], video_segment_size[i]);
                video_packet_size += video_segment_size[i];
                if (i == video_packet_seq_end) {
                    break;
                }
            }

            // Encapsulate packet data into NAL unit
            static int frame_decode_num = 0;
            size_t nals_sz = video_packet_size * 2;
            uint8_t *nals = malloc(nals_sz);
            uint8_t *nals_current = nals;
            int slice_header = is_idr ? 0x25b804ff : (0x21e003ff | ((frame_decode_num & 0xff) << 13));
            frame_decode_num++;

            if (is_idr) {
                memcpy(nals_current, sps_pps_params, sizeof(sps_pps_params));
                nals_current += sizeof(sps_pps_params);
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

void *listen_video(void *x)
{
    // Receive video
    struct gamepad_thread_context *info = (struct gamepad_thread_context *) x;
    unsigned char data[2048];
    ssize_t size;

    pthread_mutex_init(&video_mutex, NULL);

    do {
        size = recv(info->socket_vid, data, sizeof(data), 0);
        if (size > 0) {
            if (is_stop_code(data, size)) break;
            handle_video_packet(info->event_handler, info->context, data, size, info->socket_msg);
        }
    } while (!is_interrupted());

    pthread_mutex_destroy(&video_mutex);

    pthread_exit(NULL);

    return NULL;
}
