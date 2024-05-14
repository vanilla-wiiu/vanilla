#include "sync.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"
#include "wpa.h"

/*void ensure_device_is_reenabled(int signum)
{

}
*/

int sync_with_console_internal(const char *wireless_interface, uint16_t code)
{
    int ret = VANILLA_ERROR;
    static const char *wireless_conf_file = "/tmp/vanilla_wpa.conf";
    FILE *config = fopen(wireless_conf_file, "wb");
    if (!config) {
        print_info("FAILED TO WRITE TEMP CONFIG");
        goto die;
    }

    // Check status of interface with NetworkManager
    int is_managed;
    if (is_networkmanager_managing_device(wireless_interface, &is_managed) != VANILLA_SUCCESS) {
        print_info("FAILED TO DETERMINE MANAGED STATE OF WIRELESS INTERFACE");
        goto die;
    }

    // If NetworkManager is managing this device, temporarily stop it from doing so
    if (is_managed) {
        if (disable_networkmanager_on_device(wireless_interface) != VANILLA_SUCCESS) {
            print_info("FAILED TO SET %s TO UNMANAGED, RESULTS MAY BE UNPREDICTABLE");
        } else {
            print_info("TEMPORARILY SET %s TO UNMANAGED", wireless_interface);
        }
    }

    static const char *ctrl_interface = "/var/run/wpa_supplicant_drc";
    fprintf(config, "ctrl_interface=%s\nupdate_config=1", ctrl_interface);
    fclose(config);

    // Start modified WPA supplicant
    //pthread_mutex_lock(&wpa_response_mtx);
    pid_t pid;
    int err = start_wpa_supplicant(wireless_interface, wireless_conf_file, &pid);
    if (err != VANILLA_SUCCESS) {
        print_info("FAILED TO START WPA SUPPLICANT");
        goto unlock;
    }

    // Get control interface
    size_t buf_len = 1048576;
    char *buf = malloc(buf_len);
    snprintf(buf, buf_len, "%s/%s", ctrl_interface, wireless_interface);
    struct wpa_ctrl *ctrl;// = wpa_ctrl_open(buf);
    while (!(ctrl = wpa_ctrl_open(buf))) {
        print_info("WAITING FOR CTRL INTERFACE");
        sleep(1);
    }

    if (wpa_ctrl_attach(ctrl) < 0) {
        print_info("FAILED TO ATTACH TO WPA");
        goto die_and_close;
    }

    int found_console = 0;
    do {
        size_t actual_buf_len;

        while (1) {
            //print_info("SCANNING");
            actual_buf_len = buf_len;
            wpa_ctrl_command(ctrl, "SCAN", buf, &actual_buf_len);

            if (!memcmp(buf, "FAIL-BUSY", 9)) {
                //print_info("DEVICE BUSY, RETRYING");
                sleep(5);
            } else if (!memcmp(buf, "OK", 2)) {
                break;
            } else {
                print_info("UNKNOWN SCAN RESPONSE: %.*s (RETRYING)", actual_buf_len, buf);
                sleep(5);
            }
        }

        //print_info("WAITING FOR SCAN RESULTS");
        actual_buf_len = buf_len;
        wpa_ctrl_command(ctrl, "SCAN_RESULTS", buf, &actual_buf_len);
        print_info("RECEIVED SCAN RESULTS");

        const char *line = strtok(buf, "\n");
        while (line) {
            if (strstr(line, "WiiU")) {
                print_info("FOUND WII U, TESTING WPS PIN");

                char wps_buf[100];
                snprintf(wps_buf, sizeof(wps_buf), "WPS_PIN %.*s %04d5678", 17, "9c:e6:35:89:f8:13", code);

                print_info("SENDING COMMAND: %s", wps_buf);
                
                size_t actual_buf_len = buf_len;
                wpa_ctrl_command(ctrl, wps_buf, buf, &actual_buf_len);

                static const int max_wait = 20;
                int wait_count = 0;
                int cred_received = 0;

                while (1) {
                    while (wait_count < max_wait && !wpa_ctrl_pending(ctrl)) {
                        print_info("STILL WAITING FOR RESPONSE");
                        sleep(1);
                        wait_count++;
                    }

                    if (wait_count == max_wait) {
                        print_info("GIVING UP, RETURNING TO SCANNING");
                        break;
                    }

                    actual_buf_len = buf_len;
                    wpa_ctrl_recv(ctrl, buf, &actual_buf_len);
                    print_info("CRED RECV: %.*s", buf_len, buf);

                    if (!memcmp("<3>WPS-CRED-RECEIVED", buf, 20)) {
                        print_info("RECEIVED AUTHENTICATION FROM CONSOLE");
                        cred_received = 1;
                        break;
                    }
                }

                if (cred_received) {
                    // Tell wpa_supplicant to save config
                    actual_buf_len = buf_len;
                    print_info("SAVING CONFIG", actual_buf_len, buf);
                    wpa_ctrl_command(ctrl, "SAVE_CONFIG", buf, &actual_buf_len);

                    found_console = 1;
                }
            }
            line = strtok(NULL, "\n");
        }
    } while (!found_console);

    free(buf);

    ret = found_console ? VANILLA_SUCCESS : VANILLA_ERROR;

die_and_detach:
    wpa_ctrl_detach(ctrl);

die_and_close:
    wpa_ctrl_close(ctrl);

die_and_kill:
    kill(pid, SIGINT);

unlock:
    //pthread_mutex_unlock(&wpa_response_mtx);

reenable_managed:
    if (is_managed) {
        enable_networkmanager_on_device(wireless_interface);
    }

die:
    return ret;
}