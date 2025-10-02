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

#ifdef VANILLA_POLKIT_AGENT
#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

typedef struct {
	PolkitAgentListener parent;
} VanillaPolkitListener;

typedef struct {
	PolkitAgentListenerClass parent_class;
} VanillaPolkitListenerClass;

G_DEFINE_TYPE(VanillaPolkitListener, vanilla_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

static void on_request(PolkitAgentSession *s, const gchar *prompt, gboolean echo_on, gpointer user_data)
{
	vpilog("Request for password auth!");
	polkit_agent_session_response(s, "cum!");
}

static void on_show_info(PolkitAgentSession *s, const gchar *text, gpointer user_data)
{
	if (text) g_printerr("%s\n", text);
}

static void on_show_error(PolkitAgentSession *s, const gchar *text, gpointer user_data)
{
	if (text) g_printerr("Error: %s\n", text);
}

static void on_complete(PolkitAgentSession *s, gboolean gained, gpointer data)
{
	GTask *task = G_TASK(data);
	g_task_return_boolean(task, gained);
	g_object_unref(task);
}

static void vanilla_polkit_initiate_authentication(PolkitAgentListener  *listener,
													const gchar          *action_id,
													const gchar          *message,
													const gchar          *icon_name,
													PolkitDetails        *details,
													const gchar          *cookie,
													GList                *identities,
													GCancellable         *cancellable,
													GAsyncReadyCallback   callback,
													gpointer              user_data)
{
	// Choose the first identity proposed by polkit. Real agents offer a chooser.
	PolkitIdentity *id = (PolkitIdentity *) identities->data;

	PolkitAgentSession *session = polkit_agent_session_new(id, cookie);
	g_signal_connect(session, "request",    G_CALLBACK(on_request),   NULL);
	g_signal_connect(session, "show-info",  G_CALLBACK(on_show_info), NULL);
	g_signal_connect(session, "show-error", G_CALLBACK(on_show_error),NULL);

	// When session completes, call the async callback with success/failure.
	g_signal_connect(session, "completed", G_CALLBACK(on_complete), g_task_new(listener, NULL, NULL, NULL));

	// Start PAM conversation
	polkit_agent_session_initiate(session);
}

static gboolean vanilla_polkit_initiate_authentication_finish(PolkitAgentListener *listener,
                                                  GAsyncResult        *res,
                                                  GError             **error)
{
	// We returned via GTask above; propagate boolean result.
	return g_task_propagate_boolean(G_TASK(res), error);
}

static void vanilla_polkit_listener_class_init(VanillaPolkitListenerClass *self) {
	PolkitAgentListenerClass *lklass = POLKIT_AGENT_LISTENER_CLASS(self);
	lklass->initiate_authentication        = vanilla_polkit_initiate_authentication;
	lklass->initiate_authentication_finish = vanilla_polkit_initiate_authentication_finish;
}

static void vanilla_polkit_listener_init(VanillaPolkitListener *self)
{

}
#endif

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

struct sigaction old_sigint_action;
struct sigaction old_sigterm_action;
void sigint_handler(int signal)
{
    // On the Steam Deck, if the user selects "exit game", we get handed a
    // SIGINT and SIGTERM, but then get SIGKILL'd shortly afterwards, before we
    // can properly clean everything up and send `vanilla-pipe` a "quit" signal.
    // This means `vanilla-pipe` often gets stuck in limbo, and even worse, the
    // Deck's Wi-Fi interface remains under its control until reboot.
    //
    // To resolve this, we make sure the first thing we do upon receiving SIGINT
    // or SIGTERM is to tell the pipe to quit, so hopefully even if we are killed,
    // it can clean itself up without us.
    vpi_stop_pipe();

    // vpi_stop_pipe should have restored the old sigaction (SDL's), so let's
    // follow that through
    raise(signal);
}

int vpi_start_pipe()
{
    if (pipe_pid != -1) {
        // Pipe is already active
        return VANILLA_SUCCESS;
    }

#ifdef VANILLA_POLKIT_AGENT
	gpointer polkit_handle = 0;
	{
		g_autoptr(PolkitSubject) subject = polkit_unix_session_new_for_process_sync(getpid(), NULL, NULL);
		g_autoptr(GError) error = NULL;

		VanillaPolkitListener *listener = g_object_new(vanilla_polkit_listener_get_type(), NULL);
		polkit_handle = polkit_agent_listener_register(
			POLKIT_AGENT_LISTENER(listener),
			POLKIT_AGENT_REGISTER_FLAGS_NONE,
			subject,
			NULL, NULL, &error
		);
		if (!polkit_handle) {
			g_printerr("Failed to register Polkit listener: %s\n", error->message);
		}
	}
#endif

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

        struct sigaction sa;
        sa.sa_handler = sigint_handler;
        sigemptyset(&sa.sa_mask);
        sa.sa_flags = 0;
        sigaction(SIGINT, &sa, &old_sigint_action);
        sigaction(SIGTERM, &sa, &old_sigterm_action);

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

#ifdef VANILLA_POLKIT_AGENT
		if (polkit_handle) {
			polkit_agent_listener_unregister(polkit_handle);
		}
#endif

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

        // Restore old sigaction
        sigaction(SIGINT, &old_sigint_action, NULL);
        sigaction(SIGTERM, &old_sigterm_action, NULL);
    }
}

#else

// Empty function
void vpi_stop_pipe()
{
}

#endif
