#ifndef VANILLA_PI_PIPE_MGMT_H
#define VANILLA_PI_PIPE_MGMT_H

#define VANILLA_REQUIRES_PASSWORD_HANDLING -1000
#define VANILLA_PASSWORD_FAILED -1001

#ifdef VANILLA_PIPE_AVAILABLE
int vpi_start_pipe();
int vpi_start_epilog();
void vpi_cancel_pw();

#define VPI_POLKIT_RESULT_SUCCESS 0
#define VPI_POLKIT_RESULT_RETRY 1
#define VPI_POLKIT_RESULT_FAIL 2

typedef void (*vpi_pw_callback)(int result, void *userdata);
void vpi_submit_pw(const char *s, vpi_pw_callback callback, void *userdata);
#endif
void vpi_stop_pipe();
void vpi_update_pipe();

#endif // VANILLA_PI_PIPE_MGMT_H
