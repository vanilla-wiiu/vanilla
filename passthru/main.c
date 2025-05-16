#include <arpa/inet.h>
#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <sys/un.h>
#include <unistd.h>

#include "../pipe/def.h"

void string_to_hex(unsigned char *output, size_t length, const char *input)
{
    char c[3];
    c[2] = 0;
    for (size_t i = 0; i < length; i++) {
        memcpy(c, input + i * 2, 2);
        output[i] = strtol(c, 0, 16);
    }
}

int main(int argc, const char **argv)
{
	if (argc != 3) {
		fprintf(stderr, "Usage: %s <bssid> <psk>\n", argv[0]);
		return 1;
	}

	// Prepare command
	vanilla_pipe_command_t cmd;
	cmd.control_code = VANILLA_PIPE_CC_PASSTHRU;
	string_to_hex(cmd.connection.bssid.bssid, sizeof(cmd.connection.bssid), argv[1]);
	string_to_hex(cmd.connection.psk.psk, sizeof(cmd.connection.psk), argv[2]);

	// Create client socket
	struct sockaddr_un addr = {0};
	addr.sun_family = AF_UNIX;
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, VANILLA_PIPE_LOCAL_SOCKET, VANILLA_PIPE_CMD_CLIENT_PORT);
	unlink(addr.sun_path);

	int ret = 1;

    int skt = socket(AF_UNIX, SOCK_DGRAM, 0);
    if (skt == -1) {
        fprintf(stderr, "FAILED TO CREATE SOCKET: %i\n", errno);
		goto exit;
    }

    if (bind(skt, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
        fprintf(stderr, "FAILED TO BIND PORT %u: %i\n", VANILLA_PIPE_CMD_CLIENT_PORT, errno);
        goto close_socket;
    }

	// Prepare target for server/pipe
	snprintf(addr.sun_path, sizeof(addr.sun_path) - 1, VANILLA_PIPE_LOCAL_SOCKET, VANILLA_PIPE_CMD_SERVER_PORT);

	// Send code to pipe
	if (sendto(skt, (const char *) &cmd, sizeof(cmd.control_code) + sizeof(cmd.connection), 0, (const struct sockaddr *) &addr, sizeof(addr)) == -1) {
		fprintf(stderr, "Failed to write control code to socket\n");
		goto close_socket;
	}

	ret = 0;

close_socket:
	close(skt);

exit:
	return ret;
}
