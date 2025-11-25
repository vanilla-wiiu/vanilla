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

static pthread_mutex_t idr_mutex;
static int idr_is_queued = 0;

#define VIDEO_PACKET_QUEUE_MAX 1024
static VideoPacket video_packet_queue[VIDEO_PACKET_QUEUE_MAX];
static size_t video_packet_min = 0;
static size_t video_packet_max = 0;
static pthread_mutex_t video_packet_mutex;
static pthread_cond_t video_packet_cond;

static const uint8_t VANILLA_PPS_PARAMS[] = {
    0x00, 0x00, 0x00, 0x01, 0x68, 0xee, 0x06, 0x0c, 0xe8
};

void request_idr()
{
    pthread_mutex_lock(&idr_mutex);
    idr_is_queued = 1;
    pthread_mutex_unlock(&idr_mutex);
}

void send_idr_request_to_console(int socket_msg)
{
    // Make an IDR request to the Wii U?
    unsigned char idr_request[] = {1, 0, 0, 0}; // Undocumented
    vanilla_log("SENDING IDR");
    send_to_console(socket_msg, idr_request, sizeof(idr_request), PORT_MSG);
}

static uint8_t *write_slice_nal(int is_idr, int frame_decode_num, uint8_t *out)
{
	uint32_t slice_header = is_idr ? 0x25b804ff : (0x21e003ff | ((frame_decode_num & 0xff) << 13));
    *((uint32_t *)out) = htobe32(slice_header);
    return out + sizeof(uint32_t);

    static uint8_t idr_num = 0;

    uint8_t slice_nal[200];
    size_t bit_index = 0;

    // forbidden_zero_bit
    write_bits(slice_nal, sizeof(slice_nal), &bit_index, 0, 1);

    // nal_ref_idc
    write_bits(slice_nal, sizeof(slice_nal), &bit_index, is_idr ? 3 : 1, 2);

    // nal_unit_type
    write_bits(slice_nal, sizeof(slice_nal), &bit_index, is_idr ? 5 : 1, 5);

    // first_mb_in_slice
    write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);

    // slice_type
    write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, is_idr ? 2 : 0);

    // pic_parameter_set_id
    write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);

    // frame_num
    const int fn_bits = 8;
    write_bits(slice_nal, sizeof(slice_nal), &bit_index, frame_decode_num, fn_bits);

    if (is_idr) {
        // idr_pic_id
        write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, idr_num);
        idr_num++;

        // dec_ref_pic_marking() for IDR:
        write_bits(slice_nal, sizeof(slice_nal), &bit_index, 0, 1);     // no_output_of_prior_pics_flag
        write_bits(slice_nal, sizeof(slice_nal), &bit_index, 0, 1);     // long_term_reference_flag
    } else {
        // num_ref_idx_active_override_flag
        write_bits(slice_nal, sizeof(slice_nal), &bit_index, 1, 1);

        // num_ref_idx_l0_active_minus1
        write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);

        // ref_pic_list_modification_flag_l0 = 0 (no modifications)
        write_bits(slice_nal, sizeof(slice_nal), &bit_index, 0, 1);

        // dec_ref_pic_marking() for non-IDR:
        write_bits(slice_nal, sizeof(slice_nal), &bit_index, 0, 1);     // adaptive_ref_pic_marking_mode_flag = 0

        // CABAC: entropy_coding_mode_flag==1 and P-slice → must write cabac_init_idc
        write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);          // cabac_init_idc = 0 (good default)
    }

    // slice_qp_delta
    write_signed_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);

    // deblocking_filter_control_present_flag == 1 → must write
    write_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);          // disable_deblocking_filter_idc = 0
    write_signed_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);          // slice_alpha_c0_offset_div2
    write_signed_exp_golomb(slice_nal, sizeof(slice_nal), &bit_index, 0);          // slice_beta_offset_div2

    size_t bytes = (bit_index + 7) >> 3;
    memcpy(out, slice_nal, bytes);
    out += bytes;

    return out;
}

void handle_video_packet(gamepad_context_t *ctx, VideoPacket *vp)
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

    // Check if this is the beginning of the packet
    static VideoPacket *video_segments[VIDEO_PACKET_QUEUE_MAX];
    static int video_packet_seq = -1;
    static int video_packet_seq_end = -1;
    static int video_complete_frame = 0;

	static uint8_t frame_decode_num = 0;

    if (vp->frame_begin) {
        video_packet_seq = vp->seq_id;
        video_packet_seq_end = -1;

        memset(video_segments, 0, sizeof(video_segments));

		frame_decode_num++;

        if (!video_complete_frame && !is_idr) {
            send_idr_request_to_console(ctx->socket_msg);
            return;
        }

        video_complete_frame = 0;
    }

    pthread_mutex_lock(&idr_mutex);
    if (idr_is_queued) {
        send_idr_request_to_console(ctx->socket_msg);
        idr_is_queued = 0;
    }
    pthread_mutex_unlock(&idr_mutex);

    // vanilla_log("set seq_id %i = %p", vp->seq_id, vp);
    video_segments[vp->seq_id] = vp;

	if (vp->frame_end)
        video_packet_seq_end = vp->seq_id;

    if (video_packet_seq != -1 && video_packet_seq_end != -1) {
        int complete_frame = 1;
        {
            int current_index = video_packet_seq;
            while (1) {
                if (!video_segments[current_index]) {
                    complete_frame = 0;
                    vanilla_log("damn, incomplete frame (missing %i)", current_index);
                    break;
                }

                if (current_index == video_packet_seq_end) {
                    break;
                }

                current_index = (current_index + 1) % VIDEO_PACKET_QUEUE_MAX;
            }
        }

        if (complete_frame) {
            video_complete_frame = 1;

			// Encapsulate packet data into NAL unit
			vanilla_event_t *event;
			int ret = acquire_event(ctx->event_loop, &event);

			event->type = VANILLA_EVENT_VIDEO;

			uint8_t *video_packet = event->data;

			const int OLD_CODE = 1;
			if (OLD_CODE) {
				uint8_t *nals_current = video_packet;
                static const char *frame_start_word = "\x00\x00\x00\x01";

				if (is_idr) {
					uint8_t sps[200], pps[200];
					size_t sps_size = generate_sps_params(sps, sizeof(sps));
					size_t pps_size = generate_pps_params(pps, sizeof(pps));

					memcpy(nals_current, frame_start_word, 4);
					nals_current += 4;

					memcpy(nals_current, sps, sps_size);
					nals_current += sps_size;

					memcpy(nals_current, pps, pps_size);
					nals_current += pps_size;
				}

                memcpy(nals_current, frame_start_word, 4);
                nals_current += 4;

                nals_current = write_slice_nal(is_idr, frame_decode_num, nals_current);

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
					current_index = (current_index + 1) % VIDEO_PACKET_QUEUE_MAX;
				}

				event->size = (nals_current - video_packet);
			} else {
				uint8_t *out = video_packet;
				out += 4; // Make room for size

                out = write_slice_nal(is_idr, frame_decode_num, out);

				// vanilla_log_no_newline("generated slice NAL header:");
				// for (size_t i = 0; i < bytes; i++) {
				// 	vanilla_log_no_newline(" %02x", slice_nal[i] & 0xFF);
				// }
				// vanilla_log_no_newline("\n");

				// uint32_t slice_header = is_idr ? 0x25b804ff : (0x21e003ff | ((0 & 0xff) << 13));
				// *(uint32_t*)out = htobe32(slice_header);
				// out += sizeof(uint32_t);

				int i = video_packet_seq;

				size_t offset = 0;
				while (1) {
					uint8_t *data = video_segments[i]->payload;
					size_t sz = video_segments[i]->payload_size - offset;

					memcpy(out, data + offset, sz);
					out += sz;

					offset = 0;

					if (i == video_packet_seq_end) {
						break;
					}

					i = (i + 1) % VIDEO_PACKET_QUEUE_MAX;
				}

				// Go back and write size
				*((uint32_t *)video_packet) = htobe32(out - (video_packet + 4));

				event->size = (out - video_packet);
			}

			// vanilla_log_no_newline("a few bytes from the packet:");
			// for (size_t i = 0; i < 64; i++) {
			// 	// vanilla_log_no_newline(" %02x", video_segments[video_packet_seq]->payload[i] & 0xFF);
			// 	vanilla_log_no_newline(" %02x", video_packet[i] & 0xFF);
			// }
			// vanilla_log_no_newline("\n");

			release_event(ctx->event_loop);
        } else {
            // We didn't receive the complete frame so we'll skip it here
        }
    }
}

void *consume_video_packets(void *data)
{
    gamepad_context_t *ctx = (gamepad_context_t *) data;

    pthread_mutex_lock(&video_packet_mutex);

    while (!is_interrupted()) {
        while (video_packet_min < video_packet_max) {
            VideoPacket *vp = &video_packet_queue[video_packet_min % VIDEO_PACKET_QUEUE_MAX];

            pthread_mutex_unlock(&video_packet_mutex);
            handle_video_packet(ctx, vp);
            pthread_mutex_lock(&video_packet_mutex);

            video_packet_min++;
        }
        pthread_cond_wait(&video_packet_cond, &video_packet_mutex);
    }

    pthread_mutex_unlock(&video_packet_mutex);
}

size_t generate_h264_header(void *data, size_t size)
{
    uint8_t *output = (uint8_t *) data;

    static const char *frame_start_word = "\x00\x00\x00\x01";
    memcpy(output, frame_start_word, 4);
    output += 4;

    size_t sps = generate_sps_params(output, size - 4);
    output += sps;

    size_t pps = generate_pps_params(output, size - 4 - sps);
    output += pps;

    return (uintptr_t) output - (uintptr_t) data;
}

void *listen_video(void *x)
{
    // Receive video
    gamepad_context_t *info = (gamepad_context_t *) x;
    ssize_t size;

    pthread_mutex_init(&idr_mutex, NULL);
    pthread_mutex_init(&video_packet_mutex, NULL);
    pthread_cond_init(&video_packet_cond, NULL);

    pthread_t video_consumer_thread;
    pthread_create(&video_consumer_thread, 0, consume_video_packets, info);

    do {
        VideoPacket *vp = &video_packet_queue[video_packet_max % VIDEO_PACKET_QUEUE_MAX];
        size = recv(info->socket_vid, (void *) vp, sizeof(VideoPacket), 0);
        if (size > 0) {
            pthread_mutex_lock(&video_packet_mutex);
            video_packet_max++;
            if (video_packet_max == video_packet_min + VIDEO_PACKET_QUEUE_MAX) {
                vanilla_log("WARNING: ROLLED OVER VIDEO PACKET QUEUE");
            }
            pthread_cond_broadcast(&video_packet_cond);
            pthread_mutex_unlock(&video_packet_mutex);
            // (info, vp, size, info->socket_msg);
        }
    } while (!is_interrupted());

    pthread_join(video_consumer_thread, 0);

    pthread_cond_destroy(&video_packet_cond);
    pthread_mutex_destroy(&video_packet_mutex);
    pthread_mutex_destroy(&idr_mutex);

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

// void write_signed_exp_golomb(void *data, size_t buffer_size, size_t *bit_index, int64_t value)
// {
//     uint64_t codeNum = (value <= 0) ? (uint64_t)(-value*2) : (uint64_t)(value*2-1);
//     write_exp_golomb(data, buffer_size, bit_index, codeNum-1);
// }

void write_signed_exp_golomb(void *data, size_t buffer_size, size_t *bit_index, int64_t value)
{
    uint64_t codeNum;

    if (value > 0) {
        codeNum = ((uint64_t)value << 1) - 1;  // 2*v - 1
    } else {
        // value <= 0
        codeNum = (uint64_t)(-value) << 1;     // -2*v
    }

    write_exp_golomb(data, buffer_size, bit_index, codeNum);
}

size_t generate_sps_params(void *data, size_t size)
{
    // memcpy(data, VANILLA_SPS_PARAMS, MIN(sizeof(VANILLA_SPS_PARAMS), size));
    // return sizeof(VANILLA_SPS_PARAMS);

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
    write_bits(data, size, &bit_index, 1, 1);

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

	// size_t bit_index = 0;

    // // forbidden_zero_bit
    // write_bits(data, size, &bit_index, 0, 1);

    // // nal_ref_idc = 3 (important)
    // write_bits(data, size, &bit_index, 3, 2);

    // // nal_unit_type = 8 (PPS)
    // write_bits(data, size, &bit_index, 7, 5);

	// // pic_parameter_set_id
	// write_exp_golomb(data, size, &bit_index, 0);

	// // seq_parameter_set_id
	// write_exp_golomb(data, size, &bit_index, 0);

	// // entropy_coding_mode_flag
	// write_exp_golomb(data, size, &bit_index, 0);
}
