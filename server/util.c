#include "util.h"

#include <signal.h>
#include <stdlib.h>
#include <sys/prctl.h>
#include <sys/stat.h>
#include <unistd.h>

#include "status.h"
#include "vanilla.h"

// TODO: Static variables are undesirable
char wireless_authenticate_config_filename[1024] = {0};
char wireless_connect_config_filename[1024] = {0};
int interrupted = 0;

const char *get_wireless_connect_config_filename()
{
    if (wireless_connect_config_filename[0] == 0) {
        // Not initialized yet, do this now
        get_home_directory_file("vanilla_wpa_connect.conf", wireless_connect_config_filename, sizeof(wireless_connect_config_filename));
    }
    return wireless_connect_config_filename;
}

const char *get_wireless_authenticate_config_filename()
{
    if (wireless_authenticate_config_filename[0] == 0) {
        // Not initialized yet, do this now
        get_home_directory_file("vanilla_wpa_key.conf", wireless_authenticate_config_filename, sizeof(wireless_authenticate_config_filename));
    }
    return wireless_authenticate_config_filename;
}

size_t read_line_from_fd(int pipe, char *output, size_t max_output_size)
{
    size_t i = 0;
    while (i < max_output_size && read(pipe, output, 1) > 0) {
        int newline = (*output == '\n');
        output++;
        i++;
        if (newline) {
            break;
        }
    }
    *output = 0;
    return i;
}

size_t read_line_from_file(FILE *file, char *output, size_t max_output_size)
{
    return read_line_from_fd(fileno(file), output, max_output_size);
}

size_t get_home_directory(char *buf, size_t buf_size)
{
    size_t ret = snprintf(buf, buf_size, "%s/%s", getenv("HOME"), ".vanilla");
    if (ret <= buf_size) {
        mkdir(buf, 0755);
    }
    return ret;
}

size_t get_home_directory_file(const char *filename, char *buf, size_t buf_size)
{
    size_t max_path_length = get_max_path_length();
    char *dir = malloc(max_path_length);
    get_home_directory(dir, max_path_length);

    size_t ret = snprintf(buf, buf_size, "%s/%s", dir, filename);
    free(dir);
    return ret;
}

size_t get_max_path_length()
{
    return pathconf(".", _PC_PATH_MAX);;
}

void interrupt_handler(int signum)
{
    print_info("INTERRUPT SIGNAL RECEIVED, CANCELLING...");
    interrupted = 1;
}

int is_interrupted()
{
    return interrupted;
}

void force_interrupt()
{
    interrupted = 1;
}

void install_interrupt_handler()
{
    interrupted = 0;
    signal(SIGINT, interrupt_handler);
}

void uninstall_interrupt_handler()
{
    signal(SIGINT, SIG_DFL);
}

int start_process(const char **argv, pid_t *pid_out, int *stdout_pipe)
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
        r = execvp(argv[0], (char * const *) argv);

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
