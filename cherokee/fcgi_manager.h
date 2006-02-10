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

#ifndef CHEROKEE_FCGI_MANAGER_H
#define CHEROKEE_FCGI_MANAGER_H

#include "common.h"
#include "connection.h"
#include "socket.h"
#include "ext_source.h"
#include "thread.h"


typedef struct {
	cherokee_ext_source_t  *source;
	cherokee_socket_t      *socket;
	cherokee_buffer_t       read_buffer;
	cherokee_buffer_t       incomm_buffer;

	struct {
		cherokee_connection_t **id2conn;
		cuint_t                 size;
	} conn;

	struct {
		cuint_t                 id;
		cint_t                  type;
		cint_t                  return_val;
		cuint_t                 remaining;
		cuint_t                 padding;
	} current;

} cherokee_fcgi_manager_t;

#define FCGI_MANAGER(f) ((cherokee_fcgi_manager_t *)(f))


ret_t cherokee_fcgi_manager_new             (cherokee_fcgi_manager_t **fcgim, cherokee_ext_source_t *fcgi);
ret_t cherokee_fcgi_manager_free            (cherokee_fcgi_manager_t  *fcgim);

ret_t cherokee_fcgi_manager_reconnect             (cherokee_fcgi_manager_t *fcgim);
ret_t cherokee_fcgi_manager_ensure_is_connected   (cherokee_fcgi_manager_t *fcgim);

ret_t cherokee_fcgi_manager_register_connection   (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id);
ret_t cherokee_fcgi_manager_unregister_id         (cherokee_fcgi_manager_t *fcgim, cuint_t id);

ret_t cherokee_fcgi_manager_send_and_remove       (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *buf);
ret_t cherokee_fcgi_manager_step                  (cherokee_fcgi_manager_t *fcgim);

#endif /* CHEROKEE_FCGI_MANAGER_H */
