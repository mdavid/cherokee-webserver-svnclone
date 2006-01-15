/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2006 Alvaro Lopez Ortega
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

#ifndef CHEROKEE_HANDLER_FASTCGI_H
#define CHEROKEE_HANDLER_FASTCGI_H

#include "common-internal.h"

#include "buffer.h"
#include "handler.h"
#include "module_loader.h"
#include "connection.h"
#include "socket.h"
#include "fastcgi.h"
#include "fastcgi-common.h"
#include "cgi.h"
#include "fcgi_manager.h"

#include <unistd.h>
#include <stdio.h>
#include <stdlib.h>	
#include <errno.h>	
#include <string.h>	
#include <sys/types.h>

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>	
#endif

#ifdef HAVE_SYS_UN_H
# include <sys/un.h>	
#endif

typedef union {
	cherokee_sockaddr_t tcp;
} cherokee_fcgi_sockaddr_t;

typedef enum {
	fcgi_post_init,
	fcgi_post_read,
	fcgi_post_send,
	fcgi_post_finished
} cherokee_fcgi_post_phase_t;

typedef enum {
	fcgi_phase_init,
	fcgi_phase_send_post,
	fcgi_phase_send_header,
	fcgi_phase_read_fcgi,
	fcgi_phase_finished
} cherokee_fcgi_phase_t;


typedef struct {
	cherokee_handler_t  handler;
	
	/* FastCGI manager
	 */
	cherokee_fcgi_manager_t      *manager_ref;
	cuint_t                       id;

	/* FastCGI protocol stuff
	 */
	cherokee_buffer_t             environment;
	cherokee_buffer_t             write_buffer;
	cherokee_buffer_t             incoming_buffer;
	cherokee_buffer_t             data;
	cherokee_fcgi_phase_t         phase;

	cherokee_boolean_t            post_sent;
	cherokee_fcgi_post_phase_t    post_phase;

	cherokee_fcgi_server_t       *configuration;
	list_t                       *server_list;
	int                           max_manager;

	list_t                       *system_env;
} cherokee_handler_fastcgi_t;


#define FCGI(x)                ((cherokee_handler_fastcgi_t *)(x))
#define FCGI_SOCKADD_LOCAL(x)  (FCGI(x)->addr.local)
#define FCGI_SOCKADD_TCP(x)    (FCGI(x)->addr.tcp)


/* Library init function
 */
void  fastcgi_init (cherokee_module_loader_t *loader);
ret_t cherokee_handler_fastcgi_new (cherokee_handler_t **hdl, cherokee_connection_t *cnt, cherokee_table_t *properties);

/* Virtual methods
 */
ret_t cherokee_handler_fastcgi_init        (cherokee_handler_fastcgi_t *hdl);
ret_t cherokee_handler_fastcgi_free        (cherokee_handler_fastcgi_t *hdl);
ret_t cherokee_handler_fastcgi_step        (cherokee_handler_fastcgi_t *hdl, cherokee_buffer_t *buffer);
ret_t cherokee_handler_fastcgi_add_headers (cherokee_handler_fastcgi_t *hdl, cherokee_buffer_t *buffer);

#endif /* CHEROKEE_HANDLER_FASTCGI_H */
