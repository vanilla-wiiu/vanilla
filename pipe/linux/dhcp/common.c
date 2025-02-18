/* common.c
 *
 * Functions for debugging and logging as well as some other
 * simple helper functions.
 *
 * Russ Dill <Russ.Dill@asu.edu> 2001-2003
 * Rewritten by Vladimir Oleynik <dzo@simtreas.ru> (C) 2003
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>
#include <stdlib.h>
#include <signal.h>
#include <paths.h>
#include <sys/socket.h>
#include <stdarg.h>

#include "common.h"
#include "pidfile.h"


static int daemonized;

long uptime(void)
{
	struct sysinfo info;
	sysinfo(&info);
	return info.uptime;
}


/*
 * This function makes sure our first socket calls
 * aren't going to fd 1 (printf badness...) and are
 * not later closed by daemon()
 */
static inline void sanitize_fds(void)
{
	int zero;
	if ((zero = open(_PATH_DEVNULL, O_RDWR, 0)) < 0) return;
	while (zero < 3) zero = dup(zero);
	close(zero);
}


void background(const char *pidfile)
{
#ifdef __uClinux__
	LOG(LOG_ERR, "Cannot background in uclinux (yet)");
#else /* __uClinux__ */
	int pid_fd;

	/* hold lock during fork. */
	pid_fd = pidfile_acquire(pidfile);
	if (daemon(0, 0) == -1) {
		perror("fork");
		exit(1);
	}
	daemonized++;
	pidfile_write_release(pid_fd);
#endif /* __uClinux__ */
}


#ifdef UDHCP_SYSLOG
void udhcp_logging(int level, const char *fmt, ...)
{
	va_list p;
	va_list p2;

	va_start(p, fmt);
	__va_copy(p2, p);
	if(!daemonized) {
		vprintf(fmt, p);
		putchar('\n');
	}
	vsyslog(level, fmt, p2);
	va_end(p);
}


void start_log_and_pid(const char *client_server, const char *pidfile)
{
	int pid_fd;

	/* Make sure our syslog fd isn't overwritten */
	sanitize_fds();

	/* do some other misc startup stuff while we are here to save bytes */
	pid_fd = pidfile_acquire(pidfile);
	pidfile_write_release(pid_fd);

	/* equivelent of doing a fflush after every \n */
	setlinebuf(stdout);

	openlog(client_server, LOG_PID | LOG_CONS, LOG_LOCAL0);
	udhcp_logging(LOG_INFO, "%s (v%s) started", client_server, VERSION);
}


#else


static char *syslog_level_msg[] = {
	[LOG_EMERG]   = "EMERGENCY!",
	[LOG_ALERT]   = "ALERT!",
	[LOG_CRIT]    = "critical!",
	[LOG_WARNING] = "warning",
	[LOG_ERR]     = "error",
	[LOG_INFO]    = "info",
	[LOG_DEBUG]   = "debug"
};


void udhcp_logging(int level, const char *fmt, ...)
{
	va_list p;

	va_start(p, fmt);
	if(!daemonized) {
		printf("%s, ", syslog_level_msg[level]);
		vprintf(fmt, p);
		putchar('\n');
	}
	va_end(p);
}


void start_log_and_pid(const char *client_server, const char *pidfile)
{
	int pid_fd;

	/* Make sure our syslog fd isn't overwritten */
	sanitize_fds();

	/* do some other misc startup stuff while we are here to save bytes */
	pid_fd = pidfile_acquire(pidfile);
	pidfile_write_release(pid_fd);

	/* equivelent of doing a fflush after every \n */
	setlinebuf(stdout);

	udhcp_logging(LOG_INFO, "%s (v%s) started", client_server, VERSION);
}
#endif

