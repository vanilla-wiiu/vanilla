/* libbb_udhcp.h - busybox compatability wrapper */

/* bit of a hack, do this no matter what the order of the includes.
 * (for busybox) */

#ifdef CONFIG_INSTALL_NO_USR
#undef DEFAULT_SCRIPT
#define DEFAULT_SCRIPT  "/share/udhcpc/default.script"
#endif

#ifndef _LIBBB_UDHCP_H
#define _LIBBB_UDHCP_H

#ifdef IN_BUSYBOX
#include "busybox.h"

#ifdef CONFIG_FEATURE_UDHCP_SYSLOG
#define UDHCP_SYSLOG
#endif

#ifdef CONFIG_FEATURE_UDHCP_DEBUG
#define UDHCP_DEBUG
#endif

#define COMBINED_BINARY
#include "version.h"

#define xfopen bb_xfopen

#else /* ! BB_VER */

#include <stdlib.h>
#include <stdio.h>
#include <sys/sysinfo.h>

#define TRUE			1
#define FALSE			0

#define xmalloc malloc
#define xcalloc calloc

static inline FILE *xfopen(const char *file, const char *mode)
{
	FILE *fp;
	if (!(fp = fopen(file, mode))) {
		perror("could not open input file");
		exit(0);
	}
	return fp;
}

#endif /* BB_VER */

#endif /* _LIBBB_UDHCP_H */
