#include "pipemgmt.h"

#ifdef VANILLA_PIPE_AVAILABLE

#include <errno.h>
#include <libgen.h>
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/prctl.h>
#include <sys/types.h>
#include <sys/wait.h>
#include <unistd.h>
#include <vanilla.h>

#include "config.h"
#include "platform.h"

static pid_t pipe_pid = -1;
static int pipe_input;
static int pipe_err;
static pthread_t pipe_log_thread = 0;

void *vpi_pipe_log_thread(void *data)
{
    char buf[512];
    int pipe_err = (intptr_t) data;
    size_t buf_read = 0;
    while (1) {
        ssize_t this_read = read(pipe_err, buf + buf_read, 1);
        if (this_read == 1) {
            if (buf[buf_read] == '\n') {
                // Print line and reset
                buf[buf_read + 1] = '\0';
                vpilog(buf);

                buf_read = 0;
            } else {
                buf_read += this_read;
            }
        } else {
            // Encountered error, pipe likely exited so break here
            break;
        }
    }

    return 0;
}

int vpi_start_pipe()
{
    if (pipe_pid != -1) {
        // Pipe is already active
        return VANILLA_SUCCESS;
    }

    // Set up pipes so child stdout can be read by the parent process
    int err_pipes[2], in_pipes[2];
    pipe(in_pipes);
    pipe(err_pipes);

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
        dup2(err_pipes[1], STDERR_FILENO);
        dup2(in_pipes[0], STDIN_FILENO);
        close(err_pipes[0]);
        close(err_pipes[1]);
        close(in_pipes[0]);
        close(in_pipes[1]);

        setsid();

        // Execute process (this will replace the running code)
        const char *pkexec = "pkexec";

        // Get current working directory
        char exe[4096];
        ssize_t link_len = readlink("/proc/self/exe", exe, sizeof(exe));
        exe[link_len] = 0;
        dirname(exe);
        strcat(exe, "/vanilla-pipe");

        r = execlp(pkexec, pkexec, exe, "-local", vpi_config.wireless_interface, (const char *) 0);

        // Handle failure to execute, use _exit so we don't interfere with the host
        _exit(1);
    } else if (pid < 0) {
        // Fork error
        return VANILLA_ERR_GENERIC;
    } else {
        // Continuation of parent
        int ret = VANILLA_ERR_GENERIC;

        char ready_buf[500];
        memset(ready_buf, 0, sizeof(ready_buf));
        size_t read_count = 0;
        while (read_count < sizeof(ready_buf)) {
            ssize_t this_read = read(err_pipes[0], ready_buf + read_count, sizeof(ready_buf));
            if (this_read > 0) {
                read_count += this_read;
                break;
            }

            if (this_read <= 0) {
                break;
            }
        }
        if (!memcmp(ready_buf, "READY", 5)) {
            ret = VANILLA_SUCCESS;
            pipe_pid = pid;

            pipe_input = in_pipes[1];
            pipe_err = err_pipes[0];

            pthread_create(&pipe_log_thread, 0, vpi_pipe_log_thread, (void *) (intptr_t) pipe_err);
        } else {
            vpilog("GOT INVALID SIGNAL: %.*s\n", sizeof(ready_buf), ready_buf);

            // Kill seems to break a lot of things so I guess we'll just leave it orphaned
            // kill(pipe_pid, SIGKILL);
            close(in_pipes[1]);
            close(err_pipes[0]);
        }

        close(err_pipes[1]);
        close(in_pipes[0]);

        return ret;
    }
}

void vpi_stop_pipe()
{
    if (pipe_pid != -1) {
        // Signal to pipe to quit. We must send it through stdin because the pipe
        // runs under root, so we have no permission to send it SIGINT or SIGTERM.
        ssize_t s = write(pipe_input, "QUIT\n", 5);
        close(pipe_input);

        // Wait for our log thread to quit
        pthread_join(pipe_log_thread, 0);

        close(pipe_err);

        waitpid(pipe_pid, 0, 0);
        pipe_pid = -1;
    }
}

#else

// Empty function
void vpi_stop_pipe()
{
}

#endif
