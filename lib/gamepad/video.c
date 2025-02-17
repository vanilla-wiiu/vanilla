#include "video.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <sys/socket.h>
#endif // _WIN32

#include <assert.h>
#include <math.h>
#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <sys/time.h>
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

static size_t generated_sps_params_size = 0;

static const uint8_t VANILLA_PPS_PARAMS[] = {
    0x00, 0x00, 0x00, 0x01, 0x68, 0xee, 0x06, 0x0c, 0xe8
};

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
    print_info("SENDING IDR");
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
            // print_info("GOT IDR");

            is_idr = 1;

            // pthread_mutex_lock(&video_mutex);
            // if (idr_is_queued) {
            //     idr_is_queued = 0;
            // }
            // pthread_mutex_unlock(&video_mutex);
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
            uint8_t *nals_current = video_packet + generated_sps_params_size + sizeof(VANILLA_PPS_PARAMS);

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
            uint8_t *nals = (is_idr) ? video_packet : (video_packet + generated_sps_params_size + sizeof(VANILLA_PPS_PARAMS));
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

    static const char *frame_start_word = "\x00\x00\x00\x01";
    memcpy(nals_current, frame_start_word, 4);
    nals_current += 4;
    generated_sps_params_size += 4;

    size_t generated_sps = generate_sps_params(nals_current, sizeof(video_packet));
    nals_current += generated_sps;
    generated_sps_params_size += generated_sps;

    memcpy(nals_current, VANILLA_PPS_PARAMS, sizeof(VANILLA_PPS_PARAMS));
    nals_current += sizeof(VANILLA_PPS_PARAMS);

    pthread_mutex_init(&video_mutex, NULL);

    do {
        VideoPacket *vp = &video_packet_cache[video_packet_cache_index % VIDEO_PACKET_CACHE_MAX];
        video_packet_cache_index++;
        void *data = vp;

        size = recv(info->socket_vid, data, sizeof(VideoPacket), 0);
        if (size > 0) {
            handle_video_packet(info, vp, size, info->socket_msg);
        }
    } while (!is_interrupted());

    pthread_mutex_destroy(&video_mutex);

    pthread_exit(NULL);

    return NULL;
}

void write_bits(void *data, size_t buffer_size, size_t *bit_index, uint8_t value, size_t bit_width)
{
    const size_t size_of_byte = 8;

    assert(bit_width <= size_of_byte);
    
    // Calculate offsets
    size_t offset = *bit_index;
    uint8_t *bytes = (uint8_t *) data;
    size_t byte_offset = offset / size_of_byte;

    // NOTE: If this was big endian, the value would need to be shifted to the upper most 8 bits manually

    // Shift to the right for placing into buffer
    size_t local_bit_offset = offset - (byte_offset * size_of_byte);
    size_t remainder = size_of_byte - local_bit_offset;

    // Put bits into the right place in the uint16
    // Promote value to a 16-bit buffer (in case it crosses an 8-bit alignment boundary)
    uint16_t shifted = value << (size_of_byte - bit_width) >> local_bit_offset;

    // Clear any non-zero bits if necessary
    bytes[byte_offset] = (bytes[byte_offset] >> remainder) << remainder;

    // Put bits into buffer
    if (buffer_size - byte_offset > 1) {
        bytes[byte_offset + 1] = 0;
        uint16_t *words = (uint16_t *) (&bytes[byte_offset]);
        *words |= shifted;
    } else {
        uint8_t shifted_byte = shifted;
        // NOTE: If this was big endian, we'd need to get the upper 8 bits manually
        bytes[byte_offset] |= shifted_byte;
    }

    // Increment bit counter by bits
    offset += bit_width;
    *bit_index = offset;
}

void write_exp_golomb(void *data, size_t buffer_size, size_t *bit_index, uint64_t value)
{
    const size_t size_of_byte = 8;

    // x + 1
    uint64_t exp_golomb_value = value + 1;

    // Count how many bits are in this byte
    int leading_zeros = 0;
    const size_t num_bits = sizeof(value)*size_of_byte;
    for (size_t i = 0; i < num_bits; i++) {
        if (exp_golomb_value & (1ULL << (num_bits - i - 1))) {
            break;
        } else {
            leading_zeros++;
        }
    }

    int bit_width = ((sizeof(value) * size_of_byte) - leading_zeros);

    int exp_golomb_leading_zeros = bit_width - 1;
    
    for (int i = 0; i < exp_golomb_leading_zeros; i += size_of_byte) {
        write_bits(data, buffer_size, bit_index, 0, MIN(size_of_byte, exp_golomb_leading_zeros - i));
    }

    int total_bytes = (bit_width / size_of_byte);
    if (bit_width % size_of_byte != 0) {
        total_bytes++;
    }
    for (int i = 0; i < total_bytes; i++) {
        int write_count;
        if (i == 0 && bit_width % size_of_byte != 0) {
            write_count = bit_width % size_of_byte;
        } else {
            write_count = MIN(bit_width, size_of_byte);
        }

        uint8_t byte = exp_golomb_value >> ((total_bytes - 1 - i) * size_of_byte);

        write_bits(data, buffer_size, bit_index, byte, write_count);
    }
}

size_t generate_sps_params(void *data, size_t size)
{
    //
    // Reference: https://www.cardinalpeak.com/blog/the-h-264-sequence-parameter-set
    //

    size_t bit_index = 0;

    // forbidden_zero_bit
    write_bits(data, size, &bit_index, 0, 1);

    // nal_ref_idc = 3 (important/SPS)
    write_bits(data, size, &bit_index, 3, 2);

    // nal_unit_type = 7 (SPS)
    write_bits(data, size, &bit_index, 7, 5);

    // profile_idc = 100 (not sure if this is correct, seems to work)
    write_bits(data, size, &bit_index, 100, 8);

    // constraint_set0_flag
    write_bits(data, size, &bit_index, 0, 1);

    // constraint_set1_flag
    write_bits(data, size, &bit_index, 0, 1);

    // constraint_set2_flag
    write_bits(data, size, &bit_index, 0, 1);

    // constraint_set3_flag
    write_bits(data, size, &bit_index, 0, 1);

    // constraint_set4_flag
    write_bits(data, size, &bit_index, 0, 1);

    // constraint_set5_flag
    write_bits(data, size, &bit_index, 0, 1);

    // reserved_zero_2bits
    write_bits(data, size, &bit_index, 0, 2);

    // level_idc (not sure if this is correct, seems to work)
    write_bits(data, size, &bit_index, 0x20, 8);

    // seq_parameter_set_id
    write_exp_golomb(data, size, &bit_index, 0);

    // chroma_format_idc
    write_exp_golomb(data, size, &bit_index, 1);

    // bit_depth_luma_minus8
    write_exp_golomb(data, size, &bit_index, 0);

    // bit_depth_chroma_minus8
    write_exp_golomb(data, size, &bit_index, 0);

    // qpprime_y_zero_transform_bypass_flag
    write_bits(data, size, &bit_index, 0, 1);

    // seq_scaling_matrix_present_flag
    write_bits(data, size, &bit_index, 0, 1);
    
    // log2_max_frame_num_minus4
    write_exp_golomb(data, size, &bit_index, 4);

    // pic_order_cnt_type
    write_exp_golomb(data, size, &bit_index, 2);

    // max_num_ref_frames
    write_exp_golomb(data, size, &bit_index, 1);

    // gaps_in_frame_num_value_allowed_flag
    write_bits(data, size, &bit_index, 0, 1);

    // pic_width_in_mbs_minus1
    write_exp_golomb(data, size, &bit_index, 53);

    // pic_height_in_map_units_minus1
    write_exp_golomb(data, size, &bit_index, 29);

    // frame_mbs_only_flag
    write_bits(data, size, &bit_index, 1, 1);

    // direct_8x8_inference_flag
    write_bits(data, size, &bit_index, 1, 1);

    // frame_cropping_flag
    write_bits(data, size, &bit_index, 1, 1);

    // frame_crop_left_offset
    write_exp_golomb(data, size, &bit_index, 0);

    // frame_crop_right_offset
    write_exp_golomb(data, size, &bit_index, 5);

    // frame_crop_top_offset
    write_exp_golomb(data, size, &bit_index, 0);

    // frame_crop_bottom_offset
    write_exp_golomb(data, size, &bit_index, 0);

    // vui_parameters_present_flag
    int enable_vui = 1;
    write_bits(data, size, &bit_index, enable_vui, 1);

    if (enable_vui) {
        // aspect_ratio_info_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // overscan_info_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // video_signal_type_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // chroma_loc_info_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // timing_info_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // nal_hrd_parameters_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // vcl_hrd_parameters_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // pic_struct_present_flag
        write_bits(data, size, &bit_index, 0, 1);

        // bitstream_restriction_flag
        write_bits(data, size, &bit_index, 1, 1);

        // bitstream_restriction:
        {
            // motion_vectors_over_pic_boundaries_flag
            write_bits(data, size, &bit_index, 1, 1);

            // max_bytes_per_pic_denom
            write_exp_golomb(data, size, &bit_index, 2);

            // max_bits_per_mb_denom
            write_exp_golomb(data, size, &bit_index, 1);

            // log2_max_mv_length_horizontal
            write_exp_golomb(data, size, &bit_index, 16);

            // log2_max_mv_length_vertical
            write_exp_golomb(data, size, &bit_index, 16);

            // max_num_reorder_frames
            write_exp_golomb(data, size, &bit_index, 0);

            // max_dec_frame_buffering
            write_exp_golomb(data, size, &bit_index, 1);
        }
    }

    // RBSP trailing stop bit
    write_bits(data, size, &bit_index, 1, 1);
    
    // Alignment
    const int align = 8;
    if (bit_index % align != 0) {
        write_bits(data, size, &bit_index, 0, align - (bit_index % align));
    }

    return bit_index / align;
}

size_t generate_pps_params(void *data, size_t size)
{
    memcpy(data, VANILLA_PPS_PARAMS, MIN(sizeof(VANILLA_PPS_PARAMS), size));
    return sizeof(VANILLA_PPS_PARAMS);
}
