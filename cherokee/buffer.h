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

#ifndef CHEROKEE_BUFFER_H
#define CHEROKEE_BUFFER_H

#include <cherokee/common.h>
#include <stddef.h>


CHEROKEE_BEGIN_DECLS

typedef enum {
	ver_full_html,    /* Eg: <address>Cherokee web server 0.5.0 Port 80</address> */
	ver_port_html     /* Eg: <address>Cherokee web server Port 80</address> */
} cherokee_version_t;

typedef struct {
	   char *buf;
	   int   size;    /* total amount of memory */
	   int   len;     /* strlen (buf) */
} cherokee_buffer_t;

#define BUF(x) ((cherokee_buffer_t *)(x))


ret_t cherokee_buffer_new                (cherokee_buffer_t **buf);
ret_t cherokee_buffer_free               (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_clean              (cherokee_buffer_t  *buf);

ret_t cherokee_buffer_add                (cherokee_buffer_t  *buf, char *txt, size_t size);
ret_t cherokee_buffer_add_va             (cherokee_buffer_t  *buf, char *format, ...);
ret_t cherokee_buffer_prepend            (cherokee_buffer_t  *buf, char *txt, size_t size);
ret_t cherokee_buffer_add_buffer         (cherokee_buffer_t  *buf, cherokee_buffer_t *buf2);

ret_t cherokee_buffer_read_file          (cherokee_buffer_t  *buf, char *filename);
ret_t cherokee_buffer_read_from_fd       (cherokee_buffer_t  *buf, int fd, size_t size, size_t *ret_size);

ret_t cherokee_buffer_add_chunked        (cherokee_buffer_t  *buf, char *txt, size_t size);
ret_t cherokee_buffer_add_buffer_chunked (cherokee_buffer_t  *buf, cherokee_buffer_t *buf2);

ret_t cherokee_buffer_move_to_begin      (cherokee_buffer_t  *buf, int pos);
ret_t cherokee_buffer_drop_endding       (cherokee_buffer_t  *buf, int num_chars);
ret_t cherokee_buffer_multiply           (cherokee_buffer_t  *buf, int num);
ret_t cherokee_buffer_swap_chars         (cherokee_buffer_t  *buf, char a, char b);
ret_t cherokee_buffer_remove_dups        (cherokee_buffer_t  *buf, char c);
ret_t cherokee_buffer_remove_string      (cherokee_buffer_t  *buf, char *string, int string_len);
ret_t cherokee_buffer_replace_string     (cherokee_buffer_t  *buf, char *subs, int subs_len, char *repl, int repl_len);

ret_t cherokee_buffer_ensure_size        (cherokee_buffer_t  *buf, int size);
ret_t cherokee_buffer_make_empty         (cherokee_buffer_t  *buf);

int   cherokee_buffer_is_empty           (cherokee_buffer_t  *buf);
int   cherokee_buffer_is_endding         (cherokee_buffer_t  *buf, char c);
char  cherokee_buffer_end_char           (cherokee_buffer_t  *buf);

crc_t cherokee_buffer_crc32              (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_decode             (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_decode_base64      (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_encode_md5_digest  (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_encode_md5         (cherokee_buffer_t  *buf, cherokee_buffer_t *salt, cherokee_buffer_t *encoded);
ret_t cherokee_buffer_encode_hex         (cherokee_buffer_t  *buf);
ret_t cherokee_buffer_escape_html        (cherokee_buffer_t  *buf, cherokee_buffer_t **maybe_new);

ret_t cherokee_buffer_add_version        (cherokee_buffer_t  *buf, int port, cherokee_version_t ver);
ret_t cherokee_buffer_print_debug        (cherokee_buffer_t  *buf, int length);


CHEROKEE_END_DECLS

#endif /* CHEROKEE_BUFFER_H */
