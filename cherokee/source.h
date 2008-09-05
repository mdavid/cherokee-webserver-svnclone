/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2008 Alvaro Lopez Ortega
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

#ifndef CHEROKEE_SOURCE_H
#define CHEROKEE_SOURCE_H

#include <cherokee/list.h>
#include <cherokee/buffer.h>
#include <cherokee/socket.h>
#include <cherokee/config_node.h>
#include <cherokee/connection.h>

CHEROKEE_BEGIN_DECLS

typedef enum {
	source_host,
	source_interpreter
} cherokee_source_type_t;

typedef struct {
	cherokee_source_type_t type;
	cherokee_buffer_t      original;
	cherokee_buffer_t      unix_socket;
	cherokee_buffer_t      host;
	cint_t                 port;

	cherokee_func_free_t   free;
} cherokee_source_t;

#define SOURCE(s)  ((cherokee_source_t *)(s))

ret_t cherokee_source_new       (cherokee_source_t **src);
ret_t cherokee_source_free      (cherokee_source_t  *src);
ret_t cherokee_source_init      (cherokee_source_t  *src);
ret_t cherokee_source_mrproper  (cherokee_source_t  *src);

ret_t cherokee_source_configure (cherokee_source_t *src, cherokee_config_node_t *conf);
ret_t cherokee_source_connect   (cherokee_source_t *src, cherokee_socket_t *socket);

ret_t cherokee_source_connect_polling (cherokee_source_t     *src, 
				       cherokee_socket_t     *socket,
				       cherokee_connection_t *conn);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_SOURCE_H */
