#ifndef VANILLA_UTIL_H
#define VANILLA_UTIL_H

#include <stddef.h>
#include <stdio.h>

size_t read_line_from_fd(int fd, char *output, size_t max_output_size);
size_t read_line_from_file(FILE *file, char *output, size_t max_output_size);
size_t get_home_directory(char *buf, size_t buf_size);
size_t get_home_directory_file(const char *filename, char *buf, size_t buf_size);
size_t get_max_path_length();

#endif // VANILLA_UTIL_H