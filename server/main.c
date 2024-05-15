#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>

#include "status.h"
#include "vanilla.h"

void console_event_handler(void *context, int type, const char *data, size_t data_size)
{
    switch (type) {
    case VANILLA_EVENT_VIDEO:
        print_info("RECEIVED VIDEO PACKET OF SIZE %llu", data_size);
        break;
    case VANILLA_EVENT_AUDIO:
        print_info("RECEIVED AUDIO PACKET OF SIZE %llu", data_size);
        break;
    }
}

int main(int argc, char **argv)
{
    print_status(VANILLA_READY);

    char *line = NULL;
    size_t size;

    while (1) {
        ssize_t sz = getline(&line, &size, stdin);
        if (sz == -1) {
            break;
        }

        static const int cmd_max_args = 10;
        static const int cmd_max_arg_length = 20;
        int cmd_nb_arg = 0;
        char cmd_args[cmd_max_args][cmd_max_arg_length];

        static const char *delim = " \n";
        char *token = strtok(line, delim);
        while (token && cmd_nb_arg < cmd_max_args) {
            strncpy(cmd_args[cmd_nb_arg], token, cmd_max_arg_length);
            token = strtok(NULL, delim);
            cmd_nb_arg++;
        }

        if (cmd_nb_arg == 0) {
            print_status(VANILLA_UNKNOWN_COMMAND);
            continue;
        }

        if (!strcmp("SYNC", cmd_args[0])) {
            if (cmd_nb_arg != 3) {
                print_status(VANILLA_INVALID_ARGUMENT);
                continue;
            }

            const char *wireless_interface = cmd_args[1];
            const char *code = cmd_args[2];

            int sync_status = vanilla_sync_with_console(wireless_interface, atoi(code));
            if (sync_status == VANILLA_SUCCESS) {
                print_status(VANILLA_SUCCESS);
            } else {
                print_status(VANILLA_ERROR);
            }
        } else if (!strcmp("CONNECT", cmd_args[0])) {
            if (cmd_nb_arg != 2) {
                print_status(VANILLA_INVALID_ARGUMENT);
                continue;
            }

            const char *wireless_interface = cmd_args[1];

            int status = vanilla_connect_to_console(wireless_interface, console_event_handler, NULL);
            if (status == VANILLA_SUCCESS) {
                print_status(VANILLA_SUCCESS);
            } else {
                print_status(VANILLA_ERROR);
            }
        } else if (!strcmp("EXIT", cmd_args[0])) {
            break;
        } else {
            print_status(VANILLA_UNKNOWN_COMMAND);
        }
    }

    return 0;
}