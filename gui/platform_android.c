#include "platform_android.h"

#include <ctype.h>
#include <errno.h>
#include <stdio.h>
#include <sys/wait.h>

// Check for whether we have root access on Android (required to run vanilla-pipe)
int vpi_android_has_root_access(void)
{
    static int cached_result = -1;
    if (cached_result >= 0) {
        return cached_result;
    }

    // Attempt to run `id -u` as superuser - if it works, it'll return "0" meaning root
    FILE *command = popen("su -c 'id -u' 2>/dev/null", "r");
    if (!command) {
        cached_result = 0;
        return cached_result;
    }

    // See if command indeed returned "0"
    int reported_root = 0;
    char line[128];
    while (fgets(line, sizeof(line), command)) {
        char *end = NULL;
        errno = 0;
        long uid = strtol(line, &end, 10);
        while (end && isspace((unsigned char) *end)) {
            end++;
        }
        if (!errno && end != line && end && !*end && uid == 0) {
            reported_root = 1;
        }
    }

    // Clean up, cache and return result
    int status = pclose(command);
    cached_result = reported_root && status >= 0 && WIFEXITED(status) && WEXITSTATUS(status) == 0;
    return cached_result;
}
