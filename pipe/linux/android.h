#ifndef VANILLA_PIPE_ANDROID_H
#define VANILLA_PIPE_ANDROID_H

struct nl_sock;
struct nl_addr;

int vanilla_android_acquire_wifi(const char *wireless_interface);
void vanilla_android_release_wifi(void);

int vanilla_android_install_console_routing(struct nl_sock *socket,
                                            int ifindex,
                                            struct nl_addr *source_address);
void vanilla_android_remove_console_routing(struct nl_sock *socket);

#endif // VANILLA_PIPE_ANDROID_H
