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

#ifndef CHEROKEE_FASTCGI_COMMON_H
#define CHEROKEE_FASTCGI_COMMON_H

CHEROKEE_BEGIN_DECLS

#include "list.h"
#include "buffer.h"
#include "typed_table.h"


typedef struct {
	struct list_head           entry;

	cherokee_buffer_t          interpreter;
	cherokee_buffer_t          host;

	cherokee_typed_free_func_t free_func;
} cherokee_fcgi_server_t;


typedef struct {
	cherokee_fcgi_server_t  entry;

	cherokee_fcgi_server_t *current_server;
#ifdef HAVE_PTHREAD
	pthread_mutex_t         current_server_lock;
#endif	
} cherokee_fcgi_server_first_t;

 
#define FCGI_SERVER(x)       ((cherokee_fcgi_server_t *)(x))
#define FCGI_FIRST_SERVER(x) ((cherokee_fcgi_server_first_t *)(x))


ret_t cherokee_fcgi_server_new       (cherokee_fcgi_server_t       **server);
ret_t cherokee_fcgi_server_first_new (cherokee_fcgi_server_first_t **server);

ret_t cherokee_fcgi_server_free      (cherokee_fcgi_server_t *server);


CHEROKEE_END_DECLS

#endif /* CHEROKEE_FASTCGI_COMMON_H */
