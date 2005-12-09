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

#ifndef CHEROKEE_FCGI_MANAGER_H
#define CHEROKEE_FCGI_MANAGER_H

#include "common.h"
#include "connection.h"
#include "socket.h"
#include "fastcgi-common.h"


typedef struct {
	/* Connection to server		 
	 */
	cherokee_socket_t       *socket;
	int                      port;
	cherokee_buffer_t        hostname;	   
	cherokee_boolean_t       connected;

	cherokee_buffer_t        read_buffer;

	int                      request_type;
	cuint_t                  request_id;
	cherokee_buffer_t        request_buffer;
	int                      return_value;
	int                      status;

	cuint_t                  padding;
	cuint_t                  remaining_size;

	/* Connections
	 */
	cherokee_connection_t  **conn_poll;
	cuint_t                  conn_poll_size;

	cherokee_fcgi_server_t  *configuration_ref;
} cherokee_fcgi_manager_t;


ret_t cherokee_fcgi_manager_new             (cherokee_fcgi_manager_t **fcgim, cherokee_fcgi_server_t *);
ret_t cherokee_fcgi_manager_free            (cherokee_fcgi_manager_t  *fcgim);

ret_t cherokee_fcgi_manager_connect_to_srv  (cherokee_fcgi_manager_t *fcgim);
ret_t cherokee_fcgi_manager_spawn_srv       (cherokee_fcgi_manager_t *fcgim);

ret_t cherokee_fcgi_manager_register_conn   (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id);
ret_t cherokee_fcgi_manager_unregister_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn);

ret_t cherokee_fcgi_manager_step            (cherokee_fcgi_manager_t *fcgim, cuint_t);
ret_t cherokee_fcgi_manager_send            (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *info, size_t *sent);

ret_t cherokee_fcgi_manager_add_conn        (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn);
ret_t cherokee_fcgi_manager_del_conn        (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn);


#endif /* CHEROKEE_FCGI_MANAGER_H */
