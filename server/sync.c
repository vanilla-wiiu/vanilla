#include "sync.h"

#include <signal.h>
#include <stdio.h>
#include <string.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "gamepad.h"
#include "status.h"
#include "util.h"
#include "wpa.h"

int create_connect_config(const char *input_config, const char *bssid)
{
    FILE *in_file = fopen(input_config, "r");
    if (!in_file) {
        print_info("FAILED TO OPEN INPUT CONFIG FILE");
        return VANILLA_ERROR;
    }
    
    FILE *out_file = fopen(get_wireless_connect_config_filename(), "w");
    if (!out_file) {
        print_info("FAILED TO OPEN OUTPUT CONFIG FILE");
        return VANILLA_ERROR;
    }

    int len;
    char buf[150];
    while (len = read_line_from_file(in_file, buf, sizeof(buf))) {
        if (memcmp("\tssid=", buf, 6) == 0) {
            fprintf(out_file, "\tscan_ssid=1\n\tbssid=%s\n", bssid);
        }

        fwrite(buf, len, 1, out_file);

        if (memcmp(buf, "update_config=1", 15) == 0) {
            static const char *ap_scan_line = "ap_scan=1\n";
            fwrite(ap_scan_line, strlen(ap_scan_line), 1, out_file);
        }
    }

    fclose(in_file);
    fclose(out_file);

    return VANILLA_SUCCESS;
}

int sync_with_console_internal(struct wpa_ctrl *ctrl, const char *wireless_interface, uint16_t code)
{
    char buf[16384];
    const size_t buf_len = sizeof(buf);

    int found_console = 0;
    char bssid[18];
    do {
        size_t actual_buf_len;

        if (is_interrupted()) goto exit_loop;

        // Request scan from hardware
        while (1) {
            if (is_interrupted()) goto exit_loop;

            // print_info("SCANNING");
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
            if (is_interrupted()) goto exit_loop;

            if (strstr(line, "WiiU")) {
                print_info("FOUND WII U, TESTING WPS PIN");

                // Make copy of bssid for later
                strncpy(bssid, line, sizeof(bssid));
                bssid[17] = '\0';

                char wps_buf[100];
                snprintf(wps_buf, sizeof(wps_buf), "WPS_PIN %.*s %04d5678", 17, bssid, code);

                size_t actual_buf_len = buf_len;
                wpa_ctrl_command(ctrl, wps_buf, buf, &actual_buf_len);

                static const int max_wait = 20;
                int wait_count = 0;
                int cred_received = 0;

                while (!is_interrupted()) {
                    while (wait_count < max_wait && !wpa_ctrl_pending(ctrl)) {
                        if (is_interrupted()) goto exit_loop;
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

                    // Create connect config which needs a couple more parameters
                    create_connect_config(get_wireless_authenticate_config_filename(), bssid);

                    found_console = 1;
                }
            }
            line = strtok(NULL, "\n");
        }
    } while (!found_console);

exit_loop:
    return found_console ? VANILLA_SUCCESS : VANILLA_ERROR;
}