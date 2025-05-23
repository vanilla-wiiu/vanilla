#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#include "gamepad/audio.h"
#include "util.h"

int main(int argc, const char **argv)
{
	if (argc != 2) {
		printf("Usage: %s <hex-string>\n", argv[0]);
		return 1;
	}

	const char *s = argv[1];
	uint8_t bytes[8];
	if (strlen(s) != sizeof(bytes) * 2) {
		printf("ERROR: Invalid hex data\n");
		return 1;
	}

	char tmp_hex[3];
	tmp_hex[2] = 0;
	for (size_t i = 0; i < sizeof(bytes); i++) {
		memcpy(tmp_hex, s + i * 2, 2);
		bytes[i] = strtol(tmp_hex, 0, 16);
	}

	for (int i = 0; i < sizeof(bytes); i++) {
        bytes[i] = (unsigned char) reverse_bits(bytes[i], 8);
    }

    AudioPacket *ap = (AudioPacket *) bytes;

    ap->format = reverse_bits(ap->format, 3);
    ap->seq_id = reverse_bits(ap->seq_id, 10);
    ap->payload_size = reverse_bits(ap->payload_size, 16);//ntohs(ap->payload_size);
    ap->timestamp = reverse_bits(ap->timestamp, 32);

	printf("Format: %u\n", ap->format);
	printf("Mono: %u\n", ap->mono);
	printf("Vibrate: %u\n", ap->vibrate);
	printf("Type: %u\n", ap->type);
	printf("Seq ID: %u\n", ap->seq_id);
	printf("Payload Size: %u\n", ap->payload_size);
	printf("Timestamp: %u\n", ap->timestamp);

	return 0;
}
