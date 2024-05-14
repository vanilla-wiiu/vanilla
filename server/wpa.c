#include "wpa.h"

#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <wpa_ctrl.h>

#include "status.h"
#include "util.h"

void wpa_msg(char *msg, size_t len)
{
    print_info("%.*s", len, msg);
}

void wpa_ctrl_command(struct wpa_ctrl *ctrl, const char *cmd, char *buf, size_t *buf_len)
{
    wpa_ctrl_request(ctrl, cmd, strlen(cmd), buf, buf_len, NULL /*wpa_msg*/);
}

int start_process(const char *path, const char **argv, pid_t *pid_out, int *stdout_pipe)
{
    // Set up pipes so child stdout can be read by the parent process
    int pipes[2];
    pipe(pipes);

    // Get parent pid (allows us to check if parent was terminated immediately after fork)
    pid_t ppid_before_fork = getpid();

    // Fork into parent/child processes
    pid_t pid = fork();

    if (pid == 0) {
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
        //dup2(pipes[1], STDERR_FILENO);
        close(pipes[0]);
        close(pipes[1]);

        // Execute process (this will replace the running code)
        r = execvp(path, (char * const *) argv);

        // Handle failure to execute, use _exit so we don't interfere with the host
        _exit(1);
    } else if (pid < 0) {
        // Fork error
        return VANILLA_ERROR;
    } else {
        // Continuation of parent
        close(pipes[1]);
        if (!stdout_pipe) {
            // Caller is not interested in the stdout
            close(pipes[0]);
        } else {
            // Caller is interested so we'll hand them the pipe
            *stdout_pipe = pipes[0];
        }

        // If caller wants the pid, send it to them
        if (pid_out) {
            *pid_out = pid;
        }

        return VANILLA_SUCCESS;
    }
}

int start_wpa_supplicant(const char *wireless_interface, const char *config_file, pid_t *pid)
{
    // Get path to `wpa_supplicant_drc` (assumes it's in the same path as us - presumably /usr/bin/ or equivalent)
    size_t path_size = get_max_path_length();
    char *path_buf = malloc(path_size);
    char *wpa_buf = malloc(path_size);
    if (!path_buf || !wpa_buf) {
        // Failed to allocate buffer, terminate
        return VANILLA_ERROR;
    }

    // Get current working directory
    if (!getcwd(path_buf, path_size)) {
        return VANILLA_ERROR;
    }

    // Merge current working directory with wpa_supplicant name
    snprintf(wpa_buf, path_size, "%s/%s", path_buf, "wpa_supplicant_drc");
    free(path_buf);

    const char *argv[] = {wpa_buf, "-Dnl80211", "-i", wireless_interface, "-c", config_file, NULL};
    int pipe;

    int r = start_process(wpa_buf, argv, pid, &pipe);
    free(wpa_buf);

    if (r != VANILLA_SUCCESS) {
        return r;
    }

    // Wait for WPA supplicant to start
    static const int max_attempts = 5;
    int nbytes, attempts = 0, success = 0, total_nbytes = 0;
    static const char *expected = "Successfully initialized wpa_supplicant";
    static const int expected_len = 39; //strlen(expected)
    char buf[expected_len];
    do {
        // Read string from child process
        do {
            nbytes = read(pipe, buf + total_nbytes, expected_len - total_nbytes);
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

    // I'm not sure why, but closing this pipe breaks wpa_supplicant in subtle ways, so just leave it.
    //close(pipe);

    if (success) {
        // WPA initialized correctly! Continue with action...
        return VANILLA_SUCCESS;
    } else {
        // Give up
        kill((*pid), SIGINT);
        return VANILLA_ERROR;
    }
    
}

static const char *nmcli = "nmcli";
int is_networkmanager_managing_device(const char *wireless_interface, int *is_managed)
{
    pid_t nmcli_pid;
    int pipe;

    const char *argv[] = {nmcli, "device", "show", wireless_interface, NULL};

    int r = start_process(nmcli, argv, &nmcli_pid, &pipe);
    if (r != VANILLA_SUCCESS) {
        // Assume nmcli is not installed so the host is not using NetworkManager
        print_info("FAILED TO LAUNCH NMCLI, RESULTS MAY BE UNPREDICTABLE");
        *is_managed = 0;
        return VANILLA_SUCCESS;
    }

    int status;
    waitpid(nmcli_pid, &status, 0);

    if (!WIFEXITED(status)) {
        // Something went wrong
        print_info("NMCLI DID NOT EXIT NORMALLY");
        return VANILLA_ERROR;
    }

    char buf[100];
    int ret = VANILLA_ERROR;
    while (read_line_from_fd(pipe, buf, sizeof(buf))) {
        if (memcmp(buf, "GENERAL.STATE", 13) == 0) {
            *is_managed = !strstr(buf, "unmanaged");
            ret = VANILLA_SUCCESS;
            goto exit;
        }
    }

exit:
    close(pipe);
    return ret;
}

int set_networkmanager_on_device(const char *wireless_interface, int on)
{
    const char *argv[] = {nmcli, "device", "set", wireless_interface, "managed", on ? "on" : "off", NULL};

    pid_t nmcli_pid;
    int r = start_process(nmcli, argv, &nmcli_pid, NULL);
    if (r != VANILLA_SUCCESS) {
        return r;
    }

    int status;
    waitpid(nmcli_pid, &status, 0);
    if (WIFEXITED(status)) {
        return VANILLA_SUCCESS;
    } else {
        return VANILLA_ERROR;
    }
}

int disable_networkmanager_on_device(const char *wireless_interface)
{
    return set_networkmanager_on_device(wireless_interface, 0);
}

int enable_networkmanager_on_device(const char *wireless_interface)
{
    return set_networkmanager_on_device(wireless_interface, 1);
}