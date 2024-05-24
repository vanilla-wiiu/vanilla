#ifndef VANILLA_UTIL_H
#define VANILLA_UTIL_H

#include <stddef.h>
#include <stdio.h>
#include <sys/types.h>

size_t read_line_from_fd(int fd, char *output, size_t max_output_size);
size_t read_line_from_file(FILE *file, char *output, size_t max_output_size);
size_t get_home_directory(char *buf, size_t buf_size);
size_t get_home_directory_file(const char *filename, char *buf, size_t buf_size);
size_t get_max_path_length();

int is_interrupted();
void force_interrupt();
void install_interrupt_handler();
void uninstall_interrupt_handler();

int start_process(const char **argv, pid_t *pid_out, int *stdout_pipe);

const char *get_wireless_authenticate_config_filename();
const char *get_wireless_connect_config_filename();

#endif // VANILLA_UTIL_H