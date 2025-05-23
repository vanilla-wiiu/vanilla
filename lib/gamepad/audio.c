#include "audio.h"

#ifdef _WIN32
#include <winsock2.h>
#else
#include <arpa/inet.h>
#endif

#include <pthread.h>
#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include <sys/time.h>
#include <unistd.h>

#include "gamepad.h"
#include "vanilla.h"
#include "util.h"

static unsigned char queued_audio[8192];
static size_t queued_audio_start = 0;
static size_t queued_audio_end = 0;
static pthread_mutex_t queued_audio_mutex;
static pthread_cond_t queued_audio_cond;

int send_audio_packet(const void *data, size_t len)
{
    pthread_mutex_lock(&queued_audio_mutex);

	for (size_t i = 0; i < len; ) {
        size_t phys = queued_audio_end % sizeof(queued_audio);
        size_t max_write = MIN(sizeof(queued_audio) - phys, len - i);
        memcpy(queued_audio + phys, ((const unsigned char *) data) + i, max_write);

        i += max_write;
        queued_audio_end += max_write;

		// Skip start ahead if necessary
		if (queued_audio_end > queued_audio_start + sizeof(queued_audio)) {
			queued_audio_start = queued_audio_end - sizeof(queued_audio);
		}
    }

	pthread_cond_broadcast(&queued_audio_cond);

    pthread_mutex_unlock(&queued_audio_mutex);

    return VANILLA_SUCCESS;
}

static void *handle_queued_audio(void *data)
{
    // Mic only ever sends 512 bytes at a time
	const size_t MIC_PAYLOAD_SIZE = 512;

	gamepad_context_t *ctx = (gamepad_context_t *) data;

    pthread_mutex_lock(&queued_audio_mutex);

	while (!is_interrupted()) {
		while (!is_interrupted() && queued_audio_end < (queued_audio_start + MIC_PAYLOAD_SIZE)) {
			// Wait for more data
			pthread_cond_wait(&queued_audio_cond, &queued_audio_mutex);
		}

		if (is_interrupted()) {
			break;
		}

		AudioPacket ap;

		// Copy data into our packet
		for (size_t i = 0; i < MIC_PAYLOAD_SIZE; ) {
			size_t phys = queued_audio_start % sizeof(queued_audio);
			size_t max_write = MIN(sizeof(queued_audio) - phys, MIC_PAYLOAD_SIZE - i);
			memcpy(ap.payload + i, queued_audio + phys, max_write);

			i += max_write;
			queued_audio_start += max_write;
		}

		// Don't need access to the buffer while we set up the packet
		pthread_mutex_unlock(&queued_audio_mutex);

		// Set up remaining default parameters
		ap.format = 6;
		ap.mono = 1;
		ap.vibrate = 0;
		ap.type = TYPE_AUDIO; // Audio data
		ap.timestamp = 0; // Gamepad actually sends no timestamp
		ap.payload_size = MIC_PAYLOAD_SIZE;

		static unsigned int seq_id = 0;
		ap.seq_id = seq_id++;

		// Reverse bits on params
		ap.format = reverse_bits(ap.format, 3);
		ap.seq_id = reverse_bits(ap.seq_id, 10);
		ap.payload_size = reverse_bits(ap.payload_size, 16);//ntohs(ap.payload_size);
		// ap.timestamp = reverse_bits(ap.timestamp, 32); // Not necessary because timestamp is 0

		// Further reverse bits
		unsigned char *bytes = (unsigned char *) &ap;
		const size_t header_sz = sizeof(AudioPacket) - sizeof(ap.payload);
		for (int i = 0; i < 4; i++) { // 4 instead of 8 because ap.timestamp == 0
			bytes[i] = (unsigned char) reverse_bits(bytes[i], 8);
		}

		// Console expects 512 bytes every 16 ms so make sure we achieve that interval
		static struct timeval last;
		struct timeval now;
		gettimeofday(&now, 0);
		long diff = (now.tv_sec - last.tv_sec) * 1000000 + (now.tv_usec - last.tv_usec);
		static const long target_delta = 16000;
		if (diff < target_delta) {
			usleep(target_delta - diff);
		}
		gettimeofday(&last, 0);

		// Send packet to console
		send_to_console(ctx->socket_aud, &ap, header_sz + MIC_PAYLOAD_SIZE, PORT_AUD);

    	pthread_mutex_lock(&queued_audio_mutex);
	}

    pthread_mutex_unlock(&queued_audio_mutex);

	return 0;
}

void handle_audio_packet(gamepad_context_t *ctx, unsigned char *data, size_t len)
{
    //
    // === IMPORTANT NOTE! ===
    //
    // This for loop skips ap->format, ap->seq_id, and ap->timestamp to save processing.
    // If you want those, you'll have to adjust this loop.
    //
    for (int byte = 0; byte < 2; byte++) {
        data[byte] = (unsigned char) reverse_bits(data[byte], 8);
    }

    AudioPacket *ap = (AudioPacket *) data;

    // ap->format = reverse_bits(ap->format, 3);
    // ap->seq_id = reverse_bits(ap->seq_id, 10);
    ap->payload_size = ntohs(ap->payload_size);
    // ap->timestamp = reverse_bits(ap->timestamp, 32);

    if (ap->type == TYPE_VIDEO) {
        AudioPacketVideoFormat *avp = (AudioPacketVideoFormat *) ap->payload;
        avp->timestamp = ntohl(avp->timestamp);
        avp->video_format = ntohl(avp->video_format);

        // TODO: Implement
        return;
    }

    if (ap->payload_size) {
        push_event(ctx->event_loop, VANILLA_EVENT_AUDIO, ap->payload, ap->payload_size);
    }

    uint8_t vibrate_val = ap->vibrate;
    push_event(ctx->event_loop, VANILLA_EVENT_VIBRATE, &vibrate_val, sizeof(vibrate_val));
}

void *listen_audio(void *x)
{
    gamepad_context_t *info = (gamepad_context_t *) x;
    unsigned char data[2048];
    ssize_t size;

    queued_audio_start = 0;
    queued_audio_end = 0;

    pthread_mutex_init(&queued_audio_mutex, 0);
	pthread_cond_init(&queued_audio_cond, 0);

	pthread_t mic_thread;
	int mic_thread_created = 1;
	if (pthread_create(&mic_thread, 0, handle_queued_audio, info) != 0) {
		vanilla_log("Failed to create mic thread");
		mic_thread_created = 0;
	}

    do {
        size = recv(info->socket_aud, data, sizeof(data), 0);
        if (size > 0) {
            handle_audio_packet(info, data, size);
        }
    } while (!is_interrupted());

	if (mic_thread_created) {
		// Tell thread to exit
    	pthread_mutex_lock(&queued_audio_mutex);
		pthread_cond_broadcast(&queued_audio_cond);
    	pthread_mutex_unlock(&queued_audio_mutex);
		pthread_join(mic_thread, 0);
	}

    pthread_cond_destroy(&queued_audio_cond);
	pthread_mutex_destroy(&queued_audio_mutex);

    pthread_exit(NULL);

    return NULL;
}
