/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2010 Alvaro Lopez Ortega
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#if !defined (CHEROKEE_INSIDE_CHEROKEE_H) && !defined (CHEROKEE_COMPILATION)
# error "Only <cherokee/cherokee.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef CHEROKEE_UTIL_H
#define CHEROKEE_UTIL_H

#include <cherokee/common.h>
#include <cherokee/avl.h>
#include <cherokee/trace.h>
#include <cherokee/plugin_loader.h>

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_PWD_H
# include <pwd.h>
#endif

#ifdef HAVE_GRP_H
# include <grp.h>
#endif

#ifdef HAVE_SYS_UIO_H
# include <sys/uio.h>
#endif

#include <time.h>
#include <dirent.h>
#include <errno.h>

#include <cherokee/buffer.h>
#include <cherokee/iocache.h>


CHEROKEE_BEGIN_DECLS

/* Error buffer size for cherokee_strerror_r().
 */
#define ERROR_MIN_BUFSIZE	64	/* min. buffer size */
#define ERROR_MAX_BUFSIZE	512	/* max. buffer size */

#ifdef _WIN32
# define cherokee_stat(path,buf)   cherokee_win32_stat(path,buf)
# define cherokee_lstat(path,buf)  cherokee_win32_stat(path,buf)
# define cherokee_error            GetLastError()
# define cherokee_mkdir(path,perm) mkdir(path)
#else
# define cherokee_stat(path,buf)   stat(path,buf)
# define cherokee_lstat(path,buf)  lstat(path,buf)
# define cherokee_error            errno
# define cherokee_mkdir(path,perm) mkdir(path,perm)
#endif

/* Missing functions
 */
#ifndef HAVE_STRNSTR
char *strnstr (const char *s, const char *find, size_t slen);
#endif
#ifndef HAVE_STRCASESTR
char *strcasestr (register char *s, register char *find);
#endif
#ifndef HAVE_STRLCAT
size_t strlcat (char *dst, const char *src, size_t siz);
#endif
#ifndef HAVE_MALLOC
void *rpl_malloc (size_t n);
#endif

/* Constants
 */
extern const char hex2dec_tab[256];
extern const char *month[13];

/* String management functions
 */
char   *cherokee_strerror_r         (int err, char *buf, size_t bufsize);
int     cherokee_isbigendian        (void);
char   *cherokee_min_str            (char *s1, char *s2);
char   *cherokee_max_str            (char *s1, char *s2);
int     cherokee_estimate_va_length (const char *format, va_list ap);
long    cherokee_eval_formated_time (cherokee_buffer_t *buf);
ret_t   cherokee_fix_dirpath        (cherokee_buffer_t *buf);

ret_t   cherokee_find_header_end    (cherokee_buffer_t  *buf,
				     char              **end,
				     cuint_t            *sep_len);

ret_t   cherokee_find_header_end_cstr (char      *c_str,
				       cint_t     c_len,
				       char     **end,
				       cuint_t   *sep_len);

ret_t   cherokee_parse_host         (cherokee_buffer_t *buf,
				     cherokee_buffer_t *host,
				     cuint_t           *port);

int     cherokee_string_is_ipv6     (cherokee_buffer_t *ip);
ret_t   cherokee_copy_local_address (void              *socket, cherokee_buffer_t *buf);

ret_t   cherokee_buf_add_bogonow    (cherokee_buffer_t *buf,
				     cherokee_boolean_t update);

ret_t   cherokee_buf_add_backtrace  (cherokee_buffer_t *buf, int n_skip, const char *new_line, const char *line_pre);

ret_t   cherokee_find_exec_in_path  (const char        *bin_name,
				     cherokee_buffer_t *fullpath);

ret_t   cherokee_atoi               (const char        *str,
				     int               *ret_value);

/* Time management functions
 */
struct tm *cherokee_gmtime           (const time_t *timep, struct tm *result);
struct tm *cherokee_localtime        (const time_t *timep, struct tm *result);
long      *cherokee_get_timezone_ref (void);

/* Thread safe functions
 */
int   cherokee_readdir       (DIR *dirstream, struct dirent *entry, struct dirent **result);
ret_t cherokee_gethostbyname (const char *hostname, void *addr);
ret_t cherokee_gethostname   (cherokee_buffer_t *buf);
ret_t cherokee_syslog        (int priority, cherokee_buffer_t *buf);
ret_t cherokee_getpwnam      (const char *name, struct passwd *pwbuf, char *buf, size_t buflen);
ret_t cherokee_getgrnam      (const char *name, struct group *pwbuf, char *buf, size_t buflen);
ret_t cherokee_mkstemp       (cherokee_buffer_t *buffer, int *fd);
ret_t cherokee_mkdir_p       (cherokee_buffer_t *path, int mode);
ret_t cherokee_ntop          (int family, struct sockaddr *addr, char *dst, size_t cnt);
ret_t cherokee_mkdir_p_perm  (cherokee_buffer_t *dir_path, int create_mode, int ensure_perm);

ret_t cherokee_io_stat       (cherokee_iocache_t        *iocache,
			      cherokee_buffer_t         *path,
			      cherokee_boolean_t         useit,
			      struct stat               *info_space,
			      cherokee_iocache_entry_t **io_entry,
			      struct stat              **info);

/* File descriptors
 */
ret_t cherokee_fd_set_nonblocking (int fd, cherokee_boolean_t enable);
ret_t cherokee_fd_set_nodelay     (int fd, cherokee_boolean_t enable);
ret_t cherokee_fd_set_closexec    (int fd);
ret_t cherokee_fd_close           (int fd);

/* Misc
 */
ret_t cherokee_sys_fdlimit_get (cuint_t *limit);
ret_t cherokee_sys_fdlimit_set (cuint_t  limit);
ret_t cherokee_get_shell       (const char **shell, const char **binary);
void  cherokee_print_wrapped   (cherokee_buffer_t *buffer);
ret_t cherokee_tmp_dir_copy    (cherokee_buffer_t *buffer);

/* IO vectors
 */
ret_t cherokee_iovec_skip_sent (struct iovec orig[], uint16_t  orig_len,
				struct iovec dest[], uint16_t *dest_len,
				size_t sent);
ret_t cherokee_iovec_was_sent  (struct iovec orig[], uint16_t orig_len, size_t sent);

/* Debug
 */
void  cherokee_trace           (const char *entry, const char *file, int line, const char *func, const char *fmt, ...);

/* Path management
 */
ret_t cherokee_path_short         (cherokee_buffer_t *path);
ret_t cherokee_path_arg_eval      (cherokee_buffer_t *path);

/* Path walking
 */
ret_t cherokee_split_pathinfo     (cherokee_buffer_t  *path,
				   cuint_t             init_pos,
				   int                 allow_dirs,
				   char              **pathinfo,
				   int                *pathinfo_len);

ret_t cherokee_split_arguments    (cherokee_buffer_t *request,
				   int                init_pos,
				   char             **arguments,
				   int               *arguments_len);

ret_t cherokee_parse_query_string (cherokee_buffer_t *qstring,
				   cherokee_avl_t  *arguments);

char *cherokee_header_get_next_line (char *line);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_UTIL_H */
