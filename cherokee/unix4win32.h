/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001, 2002, 2003, 2004, 2005 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

/* Some fragments of code from win32equ.[h,c]
 * Copyright (C)1999 by James Ewing <jim@ewingdata.com>
 */

#ifndef __CHEROKEE_UNIX_4_WIN32__
#define __CHEROKEE_UNIX_4_WIN32__

#include <windows.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>
#include <malloc.h>
#include <winsock2.h>
#include <process.h>
#include <io.h>
#include <direct.h>

#define _POSIX_
#include <errno.h>
#include <limits.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <time.h>

#include "unix4win32_errno.h"


#ifndef OPEN_MAX
# define OPEN_MAX 32
#endif


/* syslog.h
 */
#define LOG_NOTICE      EVENTLOG_INFORMATION_TYPE
#define LOG_NDELAY      EVENTLOG_WARNING_TYPE
#define LOG_PID         EVENTLOG_WARNING_TYPE
#define LOG_EMERG       EVENTLOG_ERROR_TYPE    
#define LOG_ALERT       EVENTLOG_WARNING_TYPE
#define LOG_CRIT        EVENTLOG_ERROR_TYPE
#define LOG_ERR         EVENTLOG_ERROR_TYPE
#define LOG_WARNING     EVENTLOG_WARNING_TYPE
#define LOG_INFO        EVENTLOG_INFORMATION_TYPE
#define LOG_DEBUG       EVENTLOG_INFORMATION_TYPE
#define LOG_KERN        EVENTLOG_WARNING_TYPE
#define LOG_USER        EVENTLOG_INFORMATION_TYPE
#define LOG_MAIL        EVENTLOG_INFORMATION_TYPE
#define LOG_DAEMON      EVENTLOG_INFORMATION_TYPE
#define LOG_AUTH        EVENTLOG_INFORMATION_TYPE
#define LOG_LPR         EVENTLOG_INFORMATION_TYPE
#define LOG_NEWS        EVENTLOG_INFORMATION_TYPE
#define LOG_UUCP        EVENTLOG_INFORMATION_TYPE
#define LOG_CRON        EVENTLOG_INFORMATION_TYPE
#define LOG_LOCAL0      EVENTLOG_INFORMATION_TYPE
#define LOG_LOCAL1      EVENTLOG_INFORMATION_TYPE
#define LOG_LOCAL2      EVENTLOG_INFORMATION_TYPE
#define LOG_LOCAL3      EVENTLOG_INFORMATION_TYPE
#define LOG_LOCAL4      EVENTLOG_INFORMATION_TYPE

void openlog  (const char *ident, int logopt, int facility);
void syslog   (int priority, const char *message, ...);
void closelog (void);


/* Supplement to <sys/types.h>.
 */
#define uid_t   int
#define gid_t   int
#define pid_t   unsigned long
#define ssize_t int
#define mode_t  int
#define key_t   long
#define ushort  unsigned short

struct passwd {
	char *pw_name;   /* login user id  */
	char *pw_dir;    /* home directory */
	char *pw_shell;  /* login shell    */
	int   pw_gid;
	int   pw_uid;
};

struct group {
	char *gr_name;   /* login user id  */
	int  gr_gid;
};

struct passwd *getpwuid (int uid);
struct passwd *getpwnam (char *name);


/* Structure for scatter/gather I/O.  
 */
struct iovec {
	void *iov_base;     /* Pointer to data.  */
	size_t iov_len;     /* Length of data.  */
};


typedef unsigned long int in_addr_t;
int  getdtablesize (void);

#endif /* __CHEROKEE_UNIX_4_WIN32__ */
