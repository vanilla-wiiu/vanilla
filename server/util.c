#include "util.h"

#include <stdlib.h>
#include <sys/stat.h>
#include <unistd.h>

#include "status.h"

static const char *home_directory = NULL;

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
        mkdir(buf, 0600);
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