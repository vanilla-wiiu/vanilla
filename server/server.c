#include "server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    int pipes[2];
    pipe(pipes);

    pid_t ppid_before_fork = getpid();

    (*pid) = fork();

    if ((*pid) == 0) {
        int r = prctl(PR_SET_PDEATHSIG, SIGHUP);
        if (r == -1) {
            perror(0);
            exit(1);
        }

        if (getppid() != ppid_before_fork) {
            exit(1);
        }

        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);

        // Currently in the child
        long path_size = pathconf(".", _PC_PATH_MAX);
        char *path_buf = malloc(path_size);
        char *wpa_buf = malloc(path_size);
        if (!path_buf) {
            // Failed to allocate buffer
            _exit(1);
        }

        if (!getcwd(path_buf, path_size)) {
            // Failed to get current working directory
            _exit(1);
        }

        snprintf(wpa_buf, path_size, "%s/%s", path_buf, "wpa_supplicant_drc");
        free(path_buf);

        r = execl(wpa_buf, wpa_buf, "-Dnl80211", "-i", wireless_interface, "-c", config_file, NULL);
        free(wpa_buf);
        if (r < 0) {
            _exit(1);
        }
    } else if ((*pid) < 0) {
        // Fork error

        return VANILLA_ERROR;
    } else {
        // Continuation of parent
        close(pipes[1]);

        // Wait for WPA supplicant to start
        static const int max_attempts = 5;
        int nbytes, attempts = 0, success = 0, total_nbytes = 0;
        static const char *expected = "Successfully initialized wpa_supplicant";
        static const int expected_len = 39; //strlen(expected)
        char buf[expected_len];
        do {
            // Read string from child process
            do {
                nbytes = read(pipes[0], buf + total_nbytes, expected_len - total_nbytes);
                total_nbytes += nbytes;
            } while (total_nbytes < expected_len);

            attempts++;

            // We got success message!
            if (!strncmp(buf, expected, expected_len)) {
                success = 1;
                break;
            }

            // Haven't gotten success message (yet), wait and try again
            sleep(1);
        } while (attempts < max_attempts);

        if (success) {
            // WPA initialized correctly! Continue with action...
            return VANILLA_SUCCESS;
        } else {
            // Give up
            kill((*pid), SIGINT);
            return VANILLA_ERROR;
        }
    }
}

void wpa_message_callback(char *msg, size_t len)
{
    printf("wpa message: %s\n", msg);
}

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len)
{
    wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, wpa_message_callback);
}

int vanilla_sync_with_console(const char *wireless_interface, uint16_t code)
{
    static const char *wireless_conf_file = "/tmp/vanilla_wpa.conf";
    FILE *config = fopen(wireless_conf_file, "wb");
    if (!config) {
        print_info("FAILED TO WRITE TEMP CONFIG");
        return VANILLA_ERROR;
    }

    static const char *ctrl_interface = "/var/run/wpa_supplicant_drc";
    fprintf(config, "ctrl_interface=%s\nupdate_config=1", ctrl_interface);
    fclose(config);

    // Start modified WPA supplicant
    pid_t pid;
    int err = start_wpa_supplicant(wireless_interface, wireless_conf_file, &pid);
    if (err != VANILLA_SUCCESS) {
        print_info("FAILED TO START WPA SUPPLICANT");
        return VANILLA_ERROR;
    }

    // Get control interface
    int ret = VANILLA_ERROR;
    char buf[16384];
    snprintf(buf, sizeof(buf), "%s/%s", ctrl_interface, wireless_interface);
    printf("%s\n", buf);
    struct wpa_ctrl *ctrl = wpa_ctrl_open(buf);
    while (!ctrl) {
        print_info("FAILED TO ATTACH TO WPA");
        goto die;
    }

    if (wpa_ctrl_attach(ctrl) < 0) {
        print_info("FAILED TO ATTACH TO WPA");
        goto die_and_close;
    }

    int found_console = 0;
    do {
        size_t actual_buf_len = sizeof(buf);
        wpa_ctrl_command(ctrl, "SCAN", buf, &actual_buf_len);

        printf("%.*s", actual_buf_len, buf);
        // printf("sleeping for 10 seconds...\n");
        sleep(10);
        // printf("retrieving scan results...\n");

        actual_buf_len = sizeof(buf);
        wpa_ctrl_command(ctrl, "SCAN_RESULTS", buf, &actual_buf_len);

        const char *line = strtok(buf, "\n");
        while (line) {
            if (strstr(line, "WiiU")) {
                printf("Found a Wii U! Determine if WPS pin is correct...");

                int attempts = 0;

                do {
                    if (attempts > 0) {
                        sleep(5);
                    }
                    
                    char wps_buf[100];
                    snprintf(wps_buf, sizeof(wps_buf), "WPS_PIN %.*s %04d5678", 17, line, code);
                    
                    actual_buf_len = sizeof(buf);
                    wpa_ctrl_command(ctrl, wps_buf, buf, &actual_buf_len);

                    attempts++;
                } while (!strncmp(buf, "FAIL_BUSY", 9));
                
                printf("%.*s", actual_buf_len, buf);
                //printf("%s\n", wps_buf);

                printf("saving config...\n");

                actual_buf_len = sizeof(buf);
                wpa_ctrl_command(ctrl, "SAVE_CONFIG", buf, &actual_buf_len);
            }
            //printf("%s\n", line);
            line = strtok(NULL, "\n");
        }
    } while (!found_console);

    /*if (found_bssid) {
        

        ret = VANILLA_ERROR_SUCCESS;
    } else {
        ret = VANILLA_ERROR_CANCELLED;
    }*/
    ret = VANILLA_SUCCESS;

die_and_detach:
    wpa_ctrl_detach(ctrl);

die_and_close:
    wpa_ctrl_close(ctrl);

die:
    kill(pid, SIGINT);
    return ret;
}
