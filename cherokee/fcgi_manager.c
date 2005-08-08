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

#include "common-internal.h"
#include "fcgi_manager.h"
#include "fastcgi.h"

#include <sys/types.h>
#include <unistd.h>

#define DEFAULT_PORT        8002
#define CONN_POLL_INCREMENT 16


ret_t 
cherokee_fcgi_manager_new  (cherokee_fcgi_manager_t **fcgim)
{
	int   i;
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n,fcgi_manager);

	/* Init
	 */
	ret = cherokee_socket_new (&n->socket);
	if (unlikely(ret != ret_ok)) return ret;

	n->port = DEFAULT_PORT;
	cherokee_buffer_init (&n->hostname);
	cherokee_buffer_init (&n->read_buffer);

	cherokee_buffer_ensure_size (&n->read_buffer, DEFAULT_READ_SIZE);

	n->conn_poll_size = 0;
	n->conn_poll      = (cherokee_connection_t **) malloc (
		CONN_POLL_INCREMENT * sizeof(cherokee_connection_t *));

	for (i=0; i<CONN_POLL_INCREMENT; i++) {
		n->conn_poll[i] = NULL;
	}

	/* Return
	 */
	*fcgim = n;
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_free (cherokee_fcgi_manager_t *fcgim)
{
	cherokee_buffer_mrproper (&fcgim->hostname);
	cherokee_buffer_mrproper (&fcgim->read_buffer);

	if (fcgim->socket != NULL) {
		cherokee_socket_free (fcgim->socket);
		fcgim->socket = NULL;
	}

	if (fcgim->conn_poll != NULL) {
		free (fcgim->conn_poll);
		fcgim->conn_poll = NULL;
	}

	free (fcgim);
	return ret_ok;
}


static ret_t
connect_to_fastcgi_server (cherokee_fcgi_manager_t *fcgim)
{
	ret_t ret;

	ret = cherokee_socket_set_client (fcgim->socket, AF_INET);
	if (ret != ret_ok) return ret;

	ret = cherokee_socket_gethostbyname (fcgim->socket, &fcgim->hostname);
	if (ret != ret_ok) return ret;

	SOCKET_SIN_PORT(fcgim->socket) = htons(fcgim->port);

	return cherokee_socket_connect (fcgim->socket);
}


ret_t 
cherokee_fcgi_manager_connect_to_srv (cherokee_fcgi_manager_t *fcgim, char *host)
{
	char *port;
	ret_t ret;
	
	/* Parse host name
	 */
	port = strchr(host, ':');
	if (port == NULL) {
		cherokee_buffer_add (&fcgim->hostname, host, strlen(host));
	} else {
		*port = '\0';
		fcgim->port = atoi(port+1);
		cherokee_buffer_add (&fcgim->hostname, host, port - host);
		*port = ':';
	}
	
	/* Connect to the server
	 */
	ret = connect_to_fastcgi_server (fcgim);
	if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_spawn_srv (cherokee_fcgi_manager_t *fcgim, char *command)
{
	int                re;
	int                child;
	char              *argv[] = {"sh", "-c", NULL, NULL};
	char              *envp[] = {NULL};
	cherokee_buffer_t  tmp;

	cherokee_buffer_init (&tmp);
	cherokee_buffer_add_va (&tmp, "exec %s", command);

	printf ("FastCGI server not running, launching: %s\n", command);

	child = fork();
	switch (child) {
	case 0:
		argv[2] = (char *)tmp.buf;

		re = execve ("/bin/sh", argv, envp);
		if (re < 0) {
			PRINT_ERROR ("ERROR: Could spawn %s\n", tmp.buf);
		}

	case -1:
		goto error;
		
	default:
		sleep (1);
		break;
		
	}

	cherokee_buffer_mrproper (&tmp);
	return ret_ok;

error:
	cherokee_buffer_mrproper (&tmp);
	return ret_error;
}


ret_t 
cherokee_fcgi_manager_register_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id)
{
	cuint_t i;
	cint_t  slot = -1;

	/* Look for the first free slot
	 */
	for (i=0; i<fcgim->conn_poll_size; i++) {
		if (fcgim->conn_poll[i] == NULL) {
			slot = i;
			break;
		}
	}
	
	/* If there isn't a free slot, get more memory
	 */
	if (slot == -1) {
		fcgim->conn_poll = (cherokee_connection_t **) realloc (fcgim->conn_poll, 
			(fcgim->conn_poll_size + CONN_POLL_INCREMENT) * sizeof(cherokee_connection_t *));		
		if (unlikely (fcgim->conn_poll == NULL)) return ret_nomem;

		for (i=0; i<CONN_POLL_INCREMENT; i++) {
			fcgim->conn_poll[fcgim->conn_poll_size+i] = NULL;
		}
		
		slot = fcgim->conn_poll_size;
		fcgim->conn_poll_size += CONN_POLL_INCREMENT;
	}

	/* Set the connection reference
	 */
	fcgim->conn_poll[slot] = conn;

	printf ("registered id=%d\n", slot);
	*id = slot;
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_unregister_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	int i;

	/* Look for the connection reference
	 */
	for (i=0; i<fcgim->conn_poll_size; i++) {
		if (fcgim->conn_poll[i] == conn) {
			fcgim->conn_poll[i] = NULL;
			return ret_ok;
		}
	}

	return ret_error;
}


ret_t 
cherokee_fcgi_manager_send (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *info, size_t *sent)
{
	ret_t ret;

	ret = cherokee_socket_write (fcgim->socket, info, sent);
	if (ret != ret_ok) return ret;

	cherokee_buffer_move_to_begin (info, *sent);
	return ret_ok;
}


static ret_t
process_read_buffer (cherokee_fcgi_manager_t *fcgim)
{
	cuint_t      offset;
	FCGI_Header *header;
	
	offset = 0;
	while (fcgim->read_buffer.len - offset >= sizeof(FCGI_EndRequestRecord))
	{
		cuint_t id;
		cuint_t len;

		header = (FCGI_Header *)((&fcgim->read_buffer.buf) + offset);
		id     = (header->requestIdB0 | (header->requestIdB1 << 8));
		len    = (header->contentLengthB0 | (header->contentLengthB1 << 8)) + header->paddingLength;

		switch (header->type) {
		case FCGI_STDERR:
			printf ("strerr\n");
			break;
		case FCGI_STDOUT:
			printf ("stdout\n");
			break;
		case FCGI_END_REQUEST:
			printf ("end request\n");
			break;
		default:
			PRINT_ERROR ("ERROR: Unknown FCGI header type: %d\n", header->type);
		}

		offset += sizeof(FCGI_EndRequestRecord);
	}

	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_step (cherokee_fcgi_manager_t *fcgim)
{
	ret_t  ret;
	size_t size;

	/* Read from the FastCGI application	
	 */
	if (fcgim->read_buffer.len < sizeof(FCGI_Header)) 
	{
		ret = cherokee_socket_read (fcgim->socket, &fcgim->read_buffer, DEFAULT_READ_SIZE, &size);
		printf ("cherokee_fcgi_manager_step: _read %d\n", ret);
		if (ret != ret_ok) return ret;
	}

	/* Process the information
	 */
	if (fcgim->read_buffer.len >= sizeof(FCGI_Header))
	{
		ret = process_read_buffer (fcgim);
		printf ("cherokee_fcgi_manager_step: process %d\n", ret);
		return ret_ok;
	}

	/* Read
	 */
	return ret_eagain;
}


ret_t 
cherokee_fcgi_manager_add_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_del_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	return ret_ok;
}
