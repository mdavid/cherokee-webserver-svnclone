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

#if !defined (CHEROKEE_INSIDE_CHEROKEE_H) && !defined (CHEROKEE_COMPILATION)
# error "Only <cherokee/cherokee.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef CHEROKEE_UTIL_H
#define CHEROKEE_UTIL_H

#include <cherokee/common.h>
#include <cherokee/buffer.h>

#include <time.h>
#include <dirent.h>

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif


CHEROKEE_BEGIN_DECLS

/* Some global information
 */
const extern char *cherokee_months[]; 
const extern char *cherokee_weekdays[]; 

/* System
 */
ret_t cherokee_tls_init (void);

/* String management functions
 */
int   cherokee_hexit              (char c);
char *cherokee_min_str            (char *s1, char *s2);
char *cherokee_strfsize           (unsigned long long size, char *buf);
int   cherokee_estimate_va_length (char *format, va_list ap);

/* Thread safe functions
 */
struct tm *cherokee_gmtime        (const time_t *timep, struct tm *result);
int        cherokee_readdir       (DIR *dirstream, struct dirent *entry, struct dirent **result);
ret_t      cherokee_gethostbyname (const char *hostname, struct in_addr *addr);

/* Misc
 */
ret_t cherokee_sys_fdlimit_get (cuint_t *limit);
ret_t cherokee_sys_fdlimit_set (cuint_t  limit);


/* Path walking
 */
ret_t cherokee_split_pathinfo (cherokee_buffer_t  *path, 
			       int                 init_pos,
			       char              **pathinfo,
			       int                *pathinfo_len);

ret_t cherokee_split_arguments (cherokee_buffer_t *request,
				int                init_pos,
				char             **arguments,
				int               *arguments_len);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_UTIL_H */
