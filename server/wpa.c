#include "wpa.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len)
{
    wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, NULL);
}

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    // Set up pipes so child stdout can be read by the parent process
    int pipes[2];
    pipe(pipes);

    // Get parent pid (allows us to check if parent was terminated immediately after fork)
    pid_t ppid_before_fork = getpid();

    // Fork into parent/child processes
    (*pid) = fork();

    if ((*pid) == 0) {
        // We are in the child. Set child to terminate when parent does.
        int r = prctl(PR_SET_PDEATHSIG, SIGHUP);
        if (r == -1) {
            perror(0);
            exit(1);
        }

        // See if parent pid is still the same. If not, it must have been terminated, so we will exit too.
        if (getppid() != ppid_before_fork) {
            exit(1);
        }

        // Set up pipes so our stdout can be read by the parent process
        dup2(pipes[1], STDOUT_FILENO);
        dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);

        // Get path to `wpa_supplicant_drc` (assumes it's in the same path as us - presumably /usr/bin/ or equivalent)
        long path_size = pathconf(".", _PC_PATH_MAX);
        char *path_buf = malloc(path_size);
        char *wpa_buf = malloc(path_size);
        if (!path_buf || !wpa_buf) {
            // Failed to allocate buffer, terminate
            _exit(1);
        }

        // Get current working directory
        if (!getcwd(path_buf, path_size)) {
            _exit(1);
        }

        // Merge current working directory with wpa_supplicant name
        snprintf(wpa_buf, path_size, "%s/%s", path_buf, "wpa_supplicant_drc");
        free(path_buf);

        // Execute process (this will replace the running code)
        r = execl(wpa_buf, wpa_buf, "-Dnl80211", "-i", wireless_interface, "-c", config_file, 0);
        free(wpa_buf);

        // Handle failure to execute, use _exit so we don't interfere with the host
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