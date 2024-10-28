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

static pthread_mutex_t video_mutex;
static int idr_is_queued = 0;

#define VIDEO_PACKET_CACHE_MAX 1024
static VideoPacket video_packet_cache[VIDEO_PACKET_CACHE_MAX];
static size_t video_packet_cache_index = 0;
static uint8_t video_packet[100000];

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

void handle_video_packet(gamepad_context_t *ctx, VideoPacket *vp, size_t size, int socket_msg)
{
    //
    // === IMPORTANT NOTE! ===
    //
    // This for loop skips vp->magic, vp->packet_type, and vp->timestamp to save processing.
    // If you want those, you'll have to adjust this loop.
    //
    uint8_t *data = (uint8_t *) vp;
    for (size_t i = 0; i < 4; i++) {
        data[i] = reverse_bits(data[i], 8);
    }

    // vp->magic = reverse_bits(vp->magic, 4);
    // vp->packet_type = reverse_bits(vp->packet_type, 2);
    // vp->timestamp = reverse_bits(vp->timestamp, 32);
    vp->seq_id = reverse_bits(vp->seq_id, 10);
    vp->payload_size = reverse_bits(vp->payload_size, 11);

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
    static VideoPacket *video_segments[1024];
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

    video_segments[vp->seq_id] = vp;

    if (vp->frame_end) {
        video_packet_seq_end = vp->seq_id;
    }

    if (video_packet_seq_end != -1) {
        // int complete_frame = 1;
        if (is_streaming) {
            // Encapsulate packet data into NAL unit
            static int frame_decode_num = 0;
            uint8_t *nals_current = video_packet + sizeof(VANILLA_SPS_PARAMS) + sizeof(VANILLA_PPS_PARAMS);

            int slice_header = is_idr ? 0x25b804ff : (0x21e003ff | ((frame_decode_num & 0xff) << 13));
            frame_decode_num++;

            // begin slice nalu
            uint8_t slice[] = {0x00, 0x00, 0x00, 0x01,
                            (uint8_t) ((slice_header >> 24) & 0xff),
                            (uint8_t) ((slice_header >> 16) & 0xff),
                            (uint8_t) ((slice_header >> 8) & 0xff),
                            (uint8_t) (slice_header & 0xff)
            };
            memcpy(nals_current, slice, sizeof(slice));
            nals_current += sizeof(slice);

            // Get pointer to first packet's payload
            int current_index = video_packet_seq;
            uint8_t *from = video_segments[current_index]->payload;

            memcpy(nals_current, from, 2);
            nals_current += 2;

            // Escape codes
            int byte = 2;
            while (1) {
                uint8_t *data = video_segments[current_index]->payload;
                size_t pkt_size = video_segments[current_index]->payload_size;
                while (byte < pkt_size) {
                    if (data[byte] <= 3 && *(nals_current - 2) == 0 && *(nals_current - 1) == 0) {
                        *nals_current = 3;
                        nals_current++;
                    }
                    *nals_current = data[byte];
                    nals_current++;
                    byte++;
                }

                if (current_index == video_packet_seq_end) {
                    break;
                }

                byte = 0;
                current_index = (current_index + 1) % VIDEO_PACKET_CACHE_MAX;
            }

            // Skip IDR parameters if not an IDR frame
            uint8_t *nals = (is_idr) ? video_packet : (video_packet + sizeof(VANILLA_SPS_PARAMS) + sizeof(VANILLA_PPS_PARAMS));
            push_event(ctx->event_loop, VANILLA_EVENT_VIDEO, nals, (nals_current - nals));
        } else {
            // We didn't receive the complete frame so we'll skip it here
        }
    }
}

void *listen_video(void *x)
{
    // Receive video
    gamepad_context_t *info = (gamepad_context_t *) x;
    ssize_t size;

    // Set up IDR nals on video_packet
    uint8_t *nals_current = video_packet;
    memcpy(nals_current, VANILLA_SPS_PARAMS, sizeof(VANILLA_SPS_PARAMS));
    nals_current += sizeof(VANILLA_SPS_PARAMS);
    memcpy(nals_current, VANILLA_PPS_PARAMS, sizeof(VANILLA_PPS_PARAMS));
    nals_current += sizeof(VANILLA_PPS_PARAMS);

    pthread_mutex_init(&video_mutex, NULL);

    do {
        VideoPacket *vp = &video_packet_cache[video_packet_cache_index % VIDEO_PACKET_CACHE_MAX];
        video_packet_cache_index++;
        void *data = vp;

        size = recv(info->socket_vid, data, sizeof(VideoPacket), 0);
        if (size > 0) {
            if (is_stop_code(data, size)) break;
            handle_video_packet(info, vp, size, info->socket_msg);
        }
    } while (!is_interrupted());

    pthread_mutex_destroy(&video_mutex);

    pthread_exit(NULL);

    return NULL;
}
