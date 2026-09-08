#include "ui_sdl_linux.h"

#include <dirent.h>
#include <errno.h>
#include <math.h>
#include <stdio.h>
#include <unistd.h>

#include "ui_util.h"

#define VUI_LINUX_BACKLIGHT_ROOT "/sys/class/backlight"
#define VUI_LINUX_BACKLIGHT_DEFAULT "backlight"
#define VUI_LINUX_BACKLIGHT_PATH_SIZE 4096

static int vui_sdl_linux_set_backlight_device(const char *device, float brightness)
{
    char path[VUI_LINUX_BACKLIGHT_PATH_SIZE];
    unsigned int max_brightness;
    unsigned int value;
    int path_length;
    int write_failed;
    int close_failed;
    FILE *file;

    if (!isfinite(brightness)) {
        errno = EINVAL;
        return -1;
    }

    brightness = CLAMP(brightness, 0.0f, 1.0f);

    // Attempt to read max_brightness information
    path_length = snprintf(path, sizeof(path), "%s/%s/max_brightness", VUI_LINUX_BACKLIGHT_ROOT, device);
    if (path_length < 0 || path_length >= (int) sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    file = fopen(path, "r");
    if (!file) {
        return -1;
    }

    if (fscanf(file, "%u", &max_brightness) != 1 || !max_brightness) {
        fclose(file);
        errno = EINVAL;
        return -1;
    }
    fclose(file);

    // Now that we have max_brightness, attempt to set actual brightness to a percentage of max brightness
    value = lroundf(brightness * max_brightness);

    path_length = snprintf(path, sizeof(path), "%s/%s/brightness", VUI_LINUX_BACKLIGHT_ROOT, device);
    if (path_length < 0 || path_length >= (int) sizeof(path)) {
        errno = ENAMETOOLONG;
        return -1;
    }

    file = fopen(path, "w");
    if (!file) {
        return -1;
    }

    write_failed = fprintf(file, "%u\n", value) < 0;
    close_failed = fclose(file) != 0;
    if (write_failed || close_failed) {
        return -1;
    }

    return 0;
}

int vui_sdl_linux_set_brightness(float brightness)
{
    DIR *directory;
    struct dirent *entry;

    // See if "backlight" device exists (should exist on Nintendo Switch)
    if (access(VUI_LINUX_BACKLIGHT_ROOT "/" VUI_LINUX_BACKLIGHT_DEFAULT, F_OK) == 0) {
        return vui_sdl_linux_set_backlight_device(VUI_LINUX_BACKLIGHT_DEFAULT, brightness);
    }

    // Otherwise, iterate over directory to find next best device to set brightness on
    directory = opendir(VUI_LINUX_BACKLIGHT_ROOT);
    if (!directory) {
        return -1;
    }

    while ((entry = readdir(directory))) {
        if (entry->d_name[0] == '.') {
            continue;
        }

        if (vui_sdl_linux_set_backlight_device(entry->d_name, brightness) == 0) {
            closedir(directory);
            return 0;
        }
    }

    closedir(directory);
    return -1;
}
