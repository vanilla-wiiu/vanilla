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

#define POLKIT_AGENT_I_KNOW_API_IS_SUBJECT_TO_CHANGE
#include <polkit/polkit.h>
#include <polkitagent/polkitagent.h>

#include "config.h"
#include "platform.h"

static pid_t pipe_pid = -1;
static pid_t potential_pipe_pid = -1;
static int pipe_input;
static int pipe_err;
static pthread_t pipe_log_thread = 0;

static int err_pipes[2], in_pipes[2];

static gpointer polkit_handle = 0;
static GMainContext *gmainctx = 0;
static PolkitAgentSession *pk_session = 0;

typedef struct {
	PolkitAgentListener parent;
} VanillaPolkitListener;

typedef struct {
	PolkitAgentListenerClass parent_class;
} VanillaPolkitListenerClass;

static VanillaPolkitListener *listener = 0;
static vpi_pw_callback pk_pw_callback = 0;
static void *pk_pw_callback_userdata = 0;
static PolkitIdentity *pk_identity = 0;
static GTask *pk_task = 0;
static int pk_attempts = 0;
static int pk_ignore_complete = 0;
static char pk_cookie[256]; // TODO: No actual idea how long a Polkit cookie usually is

G_DEFINE_TYPE(VanillaPolkitListener, vanilla_polkit_listener, POLKIT_AGENT_TYPE_LISTENER)

static void try_again();

static void on_request(PolkitAgentSession *s, const gchar *prompt, gboolean echo_on, gpointer user_data)
{
	vpilog("Polkit authentication requested\n");
	pk_session = s;
}

static void on_show_info(PolkitAgentSession *s, const gchar *text, gpointer user_data)
{
	vpilog("Info\n");
	if (text) vpilog("%s\n", text);
}

static void on_show_error(PolkitAgentSession *s, const gchar *text, gpointer user_data)
{
	vpilog("Error\n");
	if (text) vpilog("Error: %s\n", text);
}

static void on_complete(PolkitAgentSession *s, gboolean gained, gpointer data)
{
	vpilog("Complete: %i\n", gained);

	g_object_unref(pk_session);
	pk_session = 0;

	if (!pk_ignore_complete) {
		pk_attempts++;

		if (pk_pw_callback) {
			int cb_ret;
			if (gained) {
				cb_ret = VPI_POLKIT_RESULT_SUCCESS;
				g_task_return_boolean(pk_task, gained);
				g_object_unref(pk_task);
				pk_task = 0;
			} else if (pk_attempts < 3) {
				cb_ret = VPI_POLKIT_RESULT_RETRY;
				try_again();
			} else {
				cb_ret = VPI_POLKIT_RESULT_FAIL;
				g_task_return_error(pk_task, g_error_new_literal(G_IO_ERROR, G_IO_ERROR_FAILED, "Maximum retries exceeded"));
				// g_task_return_boolean(pk_task, gained);
				g_object_unref(pk_task);
				pk_task = 0;
			}
			pk_pw_callback(cb_ret, pk_pw_callback_userdata);
		}
	}
}

static void try_again()
{
	pk_ignore_complete = 0;

	g_main_context_push_thread_default(gmainctx);

	PolkitAgentSession *session = polkit_agent_session_new(pk_identity, pk_cookie);
	g_signal_connect(session, "request",    G_CALLBACK(on_request), NULL);
	g_signal_connect(session, "show-info",  G_CALLBACK(on_show_info), NULL);
	g_signal_connect(session, "show-error", G_CALLBACK(on_show_error), NULL);

	// When session completes, call the async callback with success/failure.
	g_signal_connect(session, "completed", G_CALLBACK(on_complete), NULL);

	// Start PAM conversation
	polkit_agent_session_initiate(session);

	vpilog("Polkit session initiated\n");

	g_main_context_pop_thread_default(gmainctx);
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
	g_main_context_push_thread_default(gmainctx);

	pk_identity = identities->data;
	g_object_ref(pk_identity);

	pk_task = g_task_new(listener, cancellable, callback, user_data);

	strncpy(pk_cookie, cookie, sizeof(pk_cookie));

	try_again();

	g_main_context_pop_thread_default(gmainctx);
}

static gboolean vanilla_polkit_initiate_authentication_finish(PolkitAgentListener *listener,
                                                  GAsyncResult        *res,
                                                  GError             **error)
{
	return g_task_propagate_boolean(G_TASK(res), error);
}

static void vanilla_polkit_listener_finalize(GObject *object)
{
	g_return_if_fail(object != NULL);
	g_return_if_fail(G_TYPE_CHECK_INSTANCE_TYPE((object), vanilla_polkit_listener_get_type()));
	G_OBJECT_CLASS(vanilla_polkit_listener_parent_class)->finalize(object);
}

static void vanilla_polkit_listener_class_init(VanillaPolkitListenerClass *self)
{
	GObjectClass *g_object_class = G_OBJECT_CLASS(self);
	g_object_class->finalize = vanilla_polkit_listener_finalize;

	PolkitAgentListenerClass *lklass = POLKIT_AGENT_LISTENER_CLASS(self);
	lklass->initiate_authentication        = vanilla_polkit_initiate_authentication;
	lklass->initiate_authentication_finish = vanilla_polkit_initiate_authentication_finish;
}

static void vanilla_polkit_listener_init(VanillaPolkitListener *self)
{
}

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

void vpi_close_polkit_session()
{
	if (polkit_handle) {
		pk_ignore_complete = 1;
		if (pk_session) {
			polkit_agent_session_cancel(pk_session);
			pk_session = 0;
		}

		if (pk_task) {
			g_task_return_boolean(pk_task, 0);
			g_object_unref(pk_task);
			pk_task = 0;
		}

		vpilog("Unregistering Polkit listener\n");
		polkit_agent_listener_unregister(polkit_handle);
		polkit_handle = 0;

		g_object_unref(listener);
		listener = 0;

		g_main_context_unref(gmainctx);
		gmainctx = 0;

		if (pk_identity) {
			g_object_unref(pk_identity);
			pk_identity = 0;
		}
	}
}

void vpi_cancel_pw()
{
	// Close existing pkexec process
	if (potential_pipe_pid != -1) {
		kill(potential_pipe_pid, SIGTERM);
		potential_pipe_pid = -1;
		close(in_pipes[1]);
		close(err_pipes[0]);

		close(err_pipes[1]);
		close(in_pipes[0]);
	}
	vpi_close_polkit_session();
}

void vpi_submit_pw(const char *s, vpi_pw_callback callback, void *userdata)
{
	pk_pw_callback = callback;
	pk_pw_callback_userdata = userdata;
	polkit_agent_session_response(pk_session, s);
}

int vpi_start_epilog()
{
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

		pipe_input = in_pipes[1];
		pipe_err = err_pipes[0];

		pthread_create(&pipe_log_thread, 0, vpi_pipe_log_thread, (void *) (intptr_t) pipe_err);

		pipe_pid = potential_pipe_pid;
	} else {
		vpilog("GOT INVALID SIGNAL: %.*s\n", sizeof(ready_buf), ready_buf);

		// Kill seems to break a lot of things so I guess we'll just leave it orphaned
		// kill(pipe_pid, SIGKILL);
		pipe_pid = -1;
		close(in_pipes[1]);
		close(err_pipes[0]);
	}

	close(err_pipes[1]);
	close(in_pipes[0]);

	vpi_close_polkit_session();

	return ret;
}

int vpi_start_pipe()
{
    if (pipe_pid != -1) {
        // Pipe is already active
        return VANILLA_SUCCESS;
    }

	{
		PolkitAuthority *auth = polkit_authority_get_sync(NULL, NULL);
		PolkitSubject *sub = polkit_unix_process_new_for_owner(getpid(), 0, -1);
		PolkitAuthorizationResult *res =
			polkit_authority_check_authorization_sync(
				auth, sub, "com.mattkc.vanilla", NULL,
				POLKIT_CHECK_AUTHORIZATION_FLAGS_NONE, NULL, NULL);

		gboolean ok = res && polkit_authorization_result_get_is_authorized(res);

		if (res) g_object_unref(res);
		g_object_unref(sub);
		g_object_unref(auth);

		// gboolean ok = polkit_authorization_result_get_is_authorized(res);
		// gboolean chall = polkit_authorization_result_get_is_challenge(res);
		if (!ok) {
			g_autoptr(PolkitSubject) subject = polkit_unix_session_new_for_process_sync(getpid(), NULL, NULL);
			g_autoptr(GError) error = NULL;

			pk_session = 0;
			pk_attempts = 0;

			gmainctx = g_main_context_new();
			g_main_context_push_thread_default(gmainctx);

			listener = g_object_new(vanilla_polkit_listener_get_type(), NULL);
			vpilog("Registered Polkit listener\n");
			polkit_handle = polkit_agent_listener_register(
				POLKIT_AGENT_LISTENER(listener),
				POLKIT_AGENT_REGISTER_FLAGS_NONE,
				subject,
				NULL, NULL, &error
			);
			g_main_context_pop_thread_default(gmainctx);
			if (!polkit_handle) {
				g_main_context_unref(gmainctx);
				gmainctx = 0;

				vpilog("Failed to register Polkit listener: %s\n", error->message);
			}
		}
	}

    // Set up pipes so child stdout can be read by the parent process
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
        // r = execlp(pkexec, pkexec, "/usr/bin/gparted", (const char *) 0);

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

		potential_pipe_pid = pid;

		if (polkit_handle) {
			// We'll need to handle this...
			ret = VANILLA_REQUIRES_PASSWORD_HANDLING;

			// Keep handling synchronous by waiting for polkit to send us a
			// "request" signal before returning to the password prompt
			while (!pk_session) {
				g_main_context_iteration(gmainctx, 1);
			}
		} else {
			// Polkit will be handled for us, wait for it to finish
			ret = vpi_start_epilog();
		}

        return ret;
    }
}

void vpi_update_pipe()
{
	if (gmainctx) {
		while (g_main_context_pending(gmainctx)) {
			g_main_context_iteration(gmainctx, 0);
		}
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
	vpi_close_polkit_session();
}

#else

// Empty function
void vpi_stop_pipe()
{
}

// Empty function
void vpi_update_pipe()
{
}

#endif
