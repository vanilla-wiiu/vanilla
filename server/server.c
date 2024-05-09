#include "server.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpa_ctrl.h>

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    int pipes[2];
    pipe(pipes);

    (*pid) = fork();

    if ((*pid) == 0) {
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);

        // Currently in the child
        // char *app = "/home/matt/src/hostap/wpa_supplicant/wpa_supplicant";
        char *app = "/usr/local/bin/wpa_supplicant_drc";
        if (execl(app, app, "-Dnl80211", "-i", wireless_interface, "-c", config_file, NULL) < 0) {
            _exit(1);
        }
    } else if ((*pid) < 0) {
        // Fork error
        return VANILLA_ERROR_BAD_FORK;
    } else {
        // Continuation of parent
        close(pipes[1]);

        // Give WPA supplicant some time to start (TODO: probably should wait for success message)
        sleep(1);

        /*char buf[4096];
        int nbytes = read(pipes[0], buf, sizeof(buf));
        printf("Output: (%.*s)\n", nbytes, buf);*/
        return VANILLA_ERROR_SUCCESS;
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
    static const char *wireless_conf_file = "/tmp/wireless.conf";
    FILE *config = fopen(wireless_conf_file, "wb");
    if (!config) {
        return VANILLA_ERROR_CONFIG_FILE;
    }

    static const char *ctrl_interface = "/var/run/wpa_supplicant_drc";
    fprintf(config, "ctrl_interface=%s\nupdate_config=1", ctrl_interface);
    fclose(config);

    // Start modified WPA supplicant
    pid_t pid;
    int err = start_wpa_supplicant(wireless_interface, wireless_conf_file, &pid);
    if (err != VANILLA_ERROR_SUCCESS) {
        return err;
    }

    // Get control interface
    int ret;
    char buf[16384];
    sprintf(buf, "%s/%s", ctrl_interface, wireless_interface);
    struct wpa_ctrl *ctrl = wpa_ctrl_open(buf);
    if (!ctrl) {
        ret = VANILLA_ERROR_UNKNOWN;
        goto die;
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
                    sprintf(wps_buf, "WPS_PIN %.*s %04d5678", 17, line, code);
                    
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
            printf("%s\n", line);
            line = strtok(NULL, "\n");
        }
    } while (!found_console);

    /*if (found_bssid) {
        

        ret = VANILLA_ERROR_SUCCESS;
    } else {
        ret = VANILLA_ERROR_CANCELLED;
    }*/
    ret = VANILLA_ERROR_SUCCESS;

die:
    kill(pid, SIGINT);
    return ret;




    // Scan
    // Check results for WiiU
    // Send WPS Pin
    // Save config
    // Close
}