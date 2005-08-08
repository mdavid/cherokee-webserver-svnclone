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

#include "admin_server.h"
#include "util.h"
#include "server-protected.h"
#include "connection-protected.h"
#include "connection_info.h"

/* Server Port
 */
ret_t 
cherokee_admin_server_reply_get_port (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	cherokee_server_t *srv = HANDLER_SRV(ahdl);
	cherokee_buffer_add_va (reply, "server.port is %d\n", srv->port);
	return ret_ok;
}


ret_t 
cherokee_admin_server_reply_set_port (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	cherokee_server_t *srv = HANDLER_SRV(ahdl);
	srv = srv;
	cherokee_buffer_add (reply, "ok\n", 3);
	return ret_ok;
}


/* Server Port TLS
 */
ret_t 
cherokee_admin_server_reply_get_port_tls (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	cherokee_server_t *srv = HANDLER_SRV(ahdl);
	cherokee_buffer_add_va (reply, "server.port_tls is %d\n", srv->port_tls);
	return ret_ok;
}

ret_t 
cherokee_admin_server_reply_set_port_tls (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	cherokee_server_t *srv = HANDLER_SRV(ahdl);
	srv = srv;
	cherokee_buffer_add (reply, "ok\n", 3);
	return ret_ok;
}


/* Get RX/TX
 */
ret_t 
cherokee_admin_server_reply_get_tx (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	size_t             rx, tx;
	char               tmp[5];
	cherokee_server_t *srv = HANDLER_SRV(ahdl);

	cherokee_server_get_total_traffic (srv, &rx, &tx);

	cherokee_strfsize (tx, tmp);
	cherokee_buffer_add_va (reply, "server.tx is %s", tmp);
	return ret_ok;
}


ret_t 
cherokee_admin_server_reply_get_rx (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	size_t             rx, tx;
	char               tmp[5];
	cherokee_server_t *srv = HANDLER_SRV(ahdl);

	cherokee_server_get_total_traffic (srv, &rx, &tx);

	cherokee_strfsize (rx, tmp);
	cherokee_buffer_add_va (reply, "server.rx is %s", tmp);
	return ret_ok;
}


/* Get connections
 */
static void
serialize_connection (cherokee_connection_info_t *info, cherokee_buffer_t *buf)
{
	cherokee_buffer_add_va (buf, "[id=%s,ip=%s,phase=%s", 
				info->id.buf,
				info->ip.buf,
				info->phase.buf);

	if (!cherokee_buffer_is_empty(&info->rx)) {
		cherokee_buffer_add_va (buf, ",rx=%s", info->rx.buf);
	}

	if (!cherokee_buffer_is_empty(&info->tx)) {
		cherokee_buffer_add_va (buf, ",tx=%s", info->tx.buf);
	}

	if (!cherokee_buffer_is_empty(&info->request)) {
		cherokee_buffer_add_va (buf, ",request=%s", info->request.buf);
	}

	if (!cherokee_buffer_is_empty(&info->handler)) {
		cherokee_buffer_add_va (buf, ",handler=%s", info->handler.buf);
	}

	if (!cherokee_buffer_is_empty(&info->total_size)) {
		cherokee_buffer_add_va (buf, ",total_size=%s", info->total_size.buf);
	}

	if (!cherokee_buffer_is_empty(&info->percent)) {
		cherokee_buffer_add_va (buf, ",percent=%s", info->percent.buf);
	}
	
	if (!cherokee_buffer_is_empty(&info->icon)) {
		cherokee_buffer_add_va (buf, ",icon=%s", info->icon.buf);
	}

	cherokee_buffer_add_str (buf, "]");
}


ret_t
cherokee_admin_server_reply_get_connections (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	ret_t                       ret;
	list_t                     *i, *tmp;
	cherokee_server_t          *server = HANDLER_SRV(ahdl);
	LIST_HEAD(connections);

	/* Get the connection info list
	 */
	ret = cherokee_connection_info_list_server (&connections, server, HANDLER(ahdl));
	switch (ret) {
 	case ret_ok:
		break;
	case ret_not_found:
		cherokee_buffer_add_str (reply, "server.connections are \n");		
		return ret_ok;
	case ret_error:
		return ret_error;
	default:
		RET_UNKNOWN(ret);
		return ret_error;
	}

	/* Build the string
	 */
	cherokee_buffer_add_str (reply, "server.connections are ");
	list_for_each (i, &connections) {
		cherokee_connection_info_t *conn = CONN_INFO(i);
		
		/* It won't include details about the admin requests
		 */
		if (!cherokee_buffer_is_empty (&conn->handler) &&  
		    (!strcmp(conn->handler.buf, "admin"))) {
			continue;
		}

		serialize_connection (conn, reply);
	}

	cherokee_buffer_add_str (reply, "\n");

	/* Free the connection info objects
	 */
	list_for_each_safe (i, tmp, &connections) {
		cherokee_connection_info_free (CONN_INFO(i));
	}

	return ret_ok;
}


ret_t 
cherokee_admin_server_reply_del_connection (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	ret_t              ret;
	char              *begin;
	cherokee_server_t *server = HANDLER_SRV(ahdl);
                            
	if (strncmp (question->buf, "del server.connection ", 22)) {
		return ret_error;
	}

	begin = question->buf + 22;
	ret = cherokee_server_del_connection (server, begin);

	cherokee_buffer_add_va (reply, "server.connection %s has been deleted\n", begin);
	return ret_ok;
}


ret_t 
cherokee_admin_server_reply_get_thread_num (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	
	cherokee_server_t *srv = HANDLER_SRV(ahdl);

	cherokee_buffer_add_va (reply, "server.thread_num is %d\n", srv->thread_num);
	return ret_ok;
}


ret_t 
cherokee_admin_server_reply_set_backup_mode (cherokee_handler_admin_t *ahdl, cherokee_buffer_t *question, cherokee_buffer_t *reply)
{
	ret_t              ret;
	cherokee_server_t *srv = HANDLER_SRV(ahdl);
	cherokee_boolean_t active;
	cherokee_boolean_t mode;

	/* Read if the resquest if for turning it on or off
	 */
	if (!strncmp (question->buf, "set server.backup_mode on", 25)) {
		mode = true;
	} else if (!strncmp (question->buf, "set server.backup_mode off", 26)) {
		mode = false;
	} else {
		return ret_error;
	}
	
	/* Do it
	 */
	ret = cherokee_server_set_backup_mode (srv, mode);
	if (ret != ret_ok) return ret;

	/* Build the reply
	 */
	cherokee_server_get_backup_mode (srv, &active);

	if (active) 
		cherokee_buffer_add_str (reply, "server.backup_mode is on\n");
	else
		cherokee_buffer_add_str (reply, "server.backup_mode is off\n");

	return ret_ok;
}

