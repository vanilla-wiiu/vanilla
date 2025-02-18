/* signalpipe.c
 *
 * Signal pipe infrastructure. A reliable way of delivering signals.
 *
 * Russ Dill <Russ.Dill@asu.edu> December 2003
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

#include <unistd.h>
#include <signal.h>
#include <sys/types.h>
#include <sys/socket.h>
#include <sys/select.h>


#include "signalpipe.h"
#include "common.h"

static int signal_pipe[2];

static void signal_handler(int sig)
{
	if (send(signal_pipe[1], &sig, sizeof(sig), MSG_DONTWAIT) < 0)
		DEBUG(LOG_ERR, "Could not send signal: %m");
}


/* Call this before doing anything else. Sets up the socket pair
 * and installs the signal handler */
void udhcp_sp_setup(void)
{
	socketpair(AF_UNIX, SOCK_STREAM, 0, signal_pipe);
	signal(SIGUSR1, signal_handler);
	signal(SIGUSR2, signal_handler);
	signal(SIGTERM, signal_handler);
}


/* Quick little function to setup the rfds. Will return the
 * max_fd for use with select. Limited in that you can only pass
 * one extra fd */
int udhcp_sp_fd_set(fd_set *rfds, int extra_fd)
{
	FD_ZERO(rfds);
	FD_SET(signal_pipe[0], rfds);
	if (extra_fd >= 0) FD_SET(extra_fd, rfds);
	return signal_pipe[0] > extra_fd ? signal_pipe[0] : extra_fd;
}


/* Read a signal from the signal pipe. Returns 0 if there is
 * no signal, -1 on error (and sets errno appropriately), and
 * your signal on success */
int udhcp_sp_read(fd_set *rfds)
{
	int sig;

	if (!FD_ISSET(signal_pipe[0], rfds))
		return 0;

	if (read(signal_pipe[0], &sig, sizeof(sig)) < 0)
		return -1;

	return sig;
}
