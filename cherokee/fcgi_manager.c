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

#include "common-internal.h"

#include "connection.h"
#include "connection-protected.h"
#include "common-internal.h"
#include "handler_fastcgi.h"
#include "fcgi_manager.h"
#include "fastcgi.h"
#include "util.h"

#include <sys/types.h>
#include <unistd.h>

#define ENTRIES "fastcgi,fastcgi-manager,manager"

#define DEFAULT_PORT        8002
#define CONN_POLL_INCREMENT 16


/* The FastCGI manager has to behave like a monitor
 */
#define LOCK(fcgim)   CHEROKEE_MUTEX_LOCK(&fcgim->sem)
#define UNLOCK(fcgim) CHEROKEE_MUTEX_UNLOCK(&fcgim->sem)


ret_t 
cherokee_fcgi_manager_new (cherokee_fcgi_manager_t **fcgim, cherokee_fcgi_server_t *config)
{
	int   i;
	ret_t ret;
	char *port;
	CHEROKEE_NEW_STRUCT (n,fcgi_manager);

	/* Init
	 */
	ret = cherokee_socket_new (&n->socket);
	if (unlikely(ret != ret_ok)) return ret;

	cherokee_buffer_init (&n->hostname);
	cherokee_buffer_init (&n->read_buffer);
	cherokee_buffer_ensure_size (&n->read_buffer, DEFAULT_READ_SIZE);

	n->port              = DEFAULT_PORT;
	n->request_id        = 0;
	n->connected         = false;
	n->configuration_ref = config;

	n->conn_poll_size    = 0;
	n->conn_poll         = (cherokee_connection_t **) malloc (
		CONN_POLL_INCREMENT * sizeof(cherokee_connection_t *));

	n->remaining_size = 0;
	for (i=0; i<CONN_POLL_INCREMENT; i++) {
		n->conn_poll[i] = NULL;
	}

	/* Parse the host
	 */
	if (cherokee_buffer_is_empty (&config->host)) {
		return ret_error;
	}

	if (config->host.buf[0] == '/') {
		cherokee_buffer_add_buffer (&n->hostname, &config->host);
	} else {
		/* Parse host name
		 */
		port = strchr(config->host.buf, ':');
		if (port == NULL) {
			cherokee_buffer_add_buffer (&n->hostname, &config->host);
		} else {
			*port = '\0';
			n->port = atoi(port+1);
			cherokee_buffer_add (&n->hostname, config->host.buf, port - config->host.buf);
			*port = ':';
		}
	}

	CHEROKEE_MUTEX_INIT (&n->sem, NULL);
	TRACE (ENTRIES, "New manager \"%s\"\n", config->host.buf);

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

	CHEROKEE_MUTEX_DESTROY (&fcgim->sem);

	free (fcgim);
	return ret_ok;
}


static ret_t
unregister_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	cherokee_handler_fastcgi_t *fcgi;
	cuint_t                     id;
	cuint_t                     slot;

	fcgi = FCGI(conn->handler);
	id = fcgi->id;

	if (id <= 0) {
		return ret_error;
	}

	TRACE (ENTRIES, "Manager(%p)::unregistered id=%d\n", fcgim, id);

	slot = id - 1;
	fcgim->conn_poll[slot] = NULL;
	return ret_ok;
}


static ret_t
connect_to_srv (cherokee_fcgi_manager_t *fcgim)
{
	ret_t ret;

	if (fcgim->hostname.buf[0] == '/') {
		ret = cherokee_socket_set_client (fcgim->socket, AF_UNIX);
		if (ret != ret_ok) return ret;

		ret = cherokee_socket_gethostbyname (fcgim->socket, &fcgim->hostname);
		if (ret != ret_ok) return ret;
	} else {
		ret = cherokee_socket_set_client (fcgim->socket, AF_INET);
		if (ret != ret_ok) return ret;

		ret = cherokee_socket_gethostbyname (fcgim->socket, &fcgim->hostname);
		if (ret != ret_ok) return ret;

		SOCKET_SIN_PORT(fcgim->socket) = htons(fcgim->port);
	}

	ret = cherokee_socket_connect (fcgim->socket);
	fcgim->connected = (ret == ret_ok);

#if 0	
	cherokee_fd_set_nonblocking (fcgim->socket->socket);
#endif
	return ret;
}


static void
connection_poll_clean (cherokee_fcgi_manager_t *fcgim)
{
	cherokee_connection_t *conn;
	int                    i;

	for (i=0; i<fcgim->conn_poll_size; i++) {
		conn = fcgim->conn_poll[i];		
		if (conn != NULL)
			unregister_conn (fcgim, conn);
	}
}


static ret_t
reset_connections (cherokee_fcgi_manager_t *fcgim)
{
	/* The LOCK must be set when this function is called.  It uses
	 * internal functions rather than public functions.
	 */
	TRACE (ENTRIES, "Manager(%p)::reset connections (poll size: %d)\n", fcgim, fcgim->conn_poll_size);

	connection_poll_clean (fcgim);
	return connect_to_srv (fcgim);
}


static ret_t 
spawn_new_srv (cherokee_fcgi_manager_t *fcgim)
{
	int                re;
	int                child;
	char             **envp;
	char              *argv[]       = {"sh", "-c", NULL, NULL};
	char              *empty_envp[] = {NULL};
	cherokee_buffer_t  tmp          = CHEROKEE_BUF_INIT;

	/* Set a custom enviroment variable set if it was defined
	 * on the configuration file
	 */
	if (fcgim->configuration_ref && fcgim->configuration_ref->custom_env) 
		envp = fcgim->configuration_ref->custom_env;
	else 
		envp = empty_envp;

	/* Execute the FastCGI server
	 */
	cherokee_buffer_add_va (&tmp, "exec %s", fcgim->configuration_ref->interpreter.buf);

	TRACE (ENTRIES, "Manager(%p)::spawn \"/bin/sh %s\"\n", fcgim, fcgim->configuration_ref->interpreter.buf);

	child = fork();
	switch (child) {
	case 0:
		argv[2] = (char *)tmp.buf;

		re = execve ("/bin/sh", argv, envp);
		if (re < 0) {
			PRINT_ERROR ("ERROR: Could spawn %s\n", tmp.buf);
			exit (1);
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


static cherokee_handler_fastcgi_t *
get_handler_ref (cherokee_fcgi_manager_t *fcgim)
{
	cherokee_handler_fastcgi_t *fcgi = NULL;
	cherokee_connection_t      *conn;

	TRACE (ENTRIES, "Accessing handler for ID=%d\n", fcgim->request_id);

	conn = fcgim->conn_poll [fcgim->request_id - 1];
	if (conn == NULL) {
		SHOULDNT_HAPPEN;
		return NULL;
	}
	
	fcgi = FCGI(conn->handler);
	if (fcgi == NULL) {
		SHOULDNT_HAPPEN;
		return NULL;
	}

	return fcgi;
}


static void
process_buffer (cherokee_fcgi_manager_t *fcgim, cherokee_handler_fastcgi_t *fcgi, void *data, cuint_t data_len)
{
	cherokee_connection_t *conn;
	cherokee_buffer_t      tmp = CHEROKEE_BUF_INIT;
	  
	conn = fcgim->conn_poll [fcgim->request_id - 1];

	switch (fcgim->request_type) {
	case FCGI_STDERR: 
		cherokee_buffer_add (&tmp, data, data_len);
		cherokee_logger_write_string (CONN_VSRV(conn)->logger, "%s", tmp.buf);
		cherokee_buffer_mrproper (&tmp);
		break;

	case FCGI_STDOUT:
		cherokee_buffer_add (&fcgi->incoming_buffer, data, data_len);
		break;
	}
}


static ret_t
process_read_buffer (cherokee_fcgi_manager_t *fcgim)
{
	ret_t    ret;
	cuint_t  len;
	cuint_t  offset;
	cuint_t  bytes_to_move;

	FCGI_Header                *header;
	FCGI_EndRequestBody        *end_request;
	void                       *start = fcgim->read_buffer.buf;
	cherokee_handler_fastcgi_t *fcgi  = NULL; 

	offset = 0;
	ret    = ret_eagain;

	while (fcgim->read_buffer.len > 0)
	{ 
		if (fcgim->remaining_size == 0) { 
			if (fcgim->read_buffer.len < sizeof(FCGI_Header))
				return ret_eagain;

			header = (FCGI_Header *) start;

			if (!(header->type == FCGI_STDERR || 
			      header->type == FCGI_STDOUT || 
			      header->type == FCGI_END_REQUEST))
			{
				printf ("rb:%d x:%d rs:%d\n", fcgim->read_buffer.len, fcgim->padding, fcgim->remaining_size);
				cherokee_buffer_print_debug (&fcgim->read_buffer, -1);
				return ret_error;
			}

			fcgim->request_id   = (header->requestIdB0     | (header->requestIdB1 << 8));
			fcgim->request_type =  header->type;
			len                 = (header->contentLengthB0 | (header->contentLengthB1 << 8));
			fcgim->padding      =  header->paddingLength;
			fcgim->return_value =  0;

			offset = FCGI_HEADER_LEN;
			if (len > (fcgim->read_buffer.len - FCGI_HEADER_LEN)) {
				fcgim->remaining_size = len - fcgim->read_buffer.len - FCGI_HEADER_LEN + 16;
				len = fcgim->read_buffer.len - FCGI_HEADER_LEN;
				bytes_to_move = len;
			} else {
				fcgim->remaining_size = 0;
				bytes_to_move = len;      
				if ((fcgim->padding + len) > (fcgim->read_buffer.len - FCGI_HEADER_LEN)) {

					fcgim->padding = (fcgim->padding + len) - fcgim->read_buffer.len - FCGI_HEADER_LEN;
					bytes_to_move += fcgim->padding;
				} else {
					bytes_to_move += fcgim->padding;
					fcgim->padding = 0;
				}
			}
			bytes_to_move += FCGI_HEADER_LEN;
		} else {
			if (fcgim->remaining_size > fcgim->read_buffer.len) {
				fcgim->remaining_size = fcgim->remaining_size - fcgim->read_buffer.len;
				len = fcgim->read_buffer.len;
				bytes_to_move = len;
			} else {
				len = fcgim->remaining_size;
				bytes_to_move = len;
				if (fcgim->padding > 0) {
					if ((fcgim->remaining_size + fcgim->padding) > fcgim->read_buffer.len) {
						fcgim->padding = fcgim->remaining_size + fcgim->padding - fcgim->read_buffer.len;
					} else {
						bytes_to_move += fcgim->padding;
						fcgim->padding = 0;
					}
				}
				fcgim->remaining_size = 0;
			} 
		}
   

		fcgi = get_handler_ref (fcgim);
		if (fcgi == NULL) 
			return ret_error;

		switch (fcgim->request_type) {
		case FCGI_STDERR:
		case FCGI_STDOUT:
			if (len > 0)
				process_buffer (fcgim, fcgi, (start + offset), len);
			break;

		case FCGI_END_REQUEST:
			end_request = (FCGI_EndRequestBody *) (start + offset);

			fcgim->return_value =  ((end_request->appStatusB0)       | 
						(end_request->appStatusB0 << 8)  | 
						(end_request->appStatusB0 << 16) | 
						(end_request->appStatusB0 << 24));

			fcgi->phase = fcgi_phase_finished;
			ret = ret_eof_have_data;
			break;

		default:
			PRINT_ERROR ("ERROR: Unknown FCGI header type: %X\n", header->type);
			cherokee_buffer_print_debug (&fcgim->read_buffer, -1);
			ret = ret_error; 
		}

		cherokee_buffer_move_to_begin (&fcgim->read_buffer, bytes_to_move);

		if (ret == ret_error)
			break;
	}

	if (fcgim->read_buffer.len == 0)
		cherokee_buffer_mrproper (&fcgim->read_buffer);

	return ret;
}


ret_t 
cherokee_fcgi_manager_spawn_connect (cherokee_fcgi_manager_t *fcgim)
{
	ret_t ret;

	LOCK (fcgim);

	/* Try to connect..
	 */
	ret = connect_to_srv (fcgim);
	if (ret == ret_ok) {
		UNLOCK (fcgim);
		return ret_ok;
	}

	/* Clean the socket
	 */
	cherokee_socket_close (fcgim->socket);

	/* If it couldn't connect, spawn a new server
	 */
	ret = spawn_new_srv (fcgim);
	if (unlikely (ret != ret_ok)) 
		goto error;

	/* Try to connect again
	 */
	ret = connect_to_srv (fcgim);
	if (unlikely (ret != ret_ok))
		goto error;


	UNLOCK (fcgim);
	return ret_ok;

error:
	UNLOCK (fcgim);
	return ret_error;
}



ret_t 
cherokee_fcgi_manager_unregister_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	ret_t ret;

	LOCK (fcgim);
	ret = unregister_conn (fcgim, conn);
	UNLOCK (fcgim);

	return ret;
}


ret_t 
cherokee_fcgi_manager_register_conn (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id)
{
	cuint_t i;
	cint_t  slot = -1;

	LOCK (fcgim);
	
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
		/* Get more memory
		 */
		fcgim->conn_poll = (cherokee_connection_t **) realloc (
			fcgim->conn_poll, 
			(fcgim->conn_poll_size + CONN_POLL_INCREMENT) * sizeof(cherokee_connection_t *));		
		
		if (unlikely (fcgim->conn_poll == NULL)) {
			UNLOCK (fcgim);
			return ret_nomem;
		}

		/* Init the memory
		 */
		memset (fcgim->conn_poll + fcgim->conn_poll_size, 0, 
			CONN_POLL_INCREMENT * sizeof(cherokee_connection_t *));

		slot = fcgim->conn_poll_size;
		fcgim->conn_poll_size += CONN_POLL_INCREMENT;
	}

	/* Set the connection reference
	 */
	fcgim->conn_poll[slot] = conn;
	*id = slot + 1;

	TRACE (ENTRIES, "Manager(%p)::registered ID=%d\n", fcgim, slot+1);

	UNLOCK (fcgim);
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_send (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *info, size_t *sent)
{
	ret_t ret;

	LOCK (fcgim);
	
	ret = cherokee_socket_write (fcgim->socket, info, sent);
#if 0
	cherokee_buffer_print_debug (info, -1);
#endif

	if (ret != ret_ok) {
		reset_connections (fcgim);

		UNLOCK (fcgim);
		return ret;
	}
	UNLOCK (fcgim);

	cherokee_buffer_move_to_begin (info, *sent);
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_step (cherokee_fcgi_manager_t *fcgim)
{
	ret_t   ret;
	size_t  size = 0; 

	LOCK (fcgim);
  
	TRACE (ENTRIES, "Manager(%p)::step in_buf.len=%d\n", fcgim, fcgim->read_buffer.len);

	/* Read from the FastCGI application	
	 */
	if (fcgim->read_buffer.len < sizeof(FCGI_Header)) {
		ret = cherokee_socket_read (fcgim->socket, &fcgim->read_buffer, DEFAULT_READ_SIZE, &size);
		switch (ret) {
		case ret_ok:
			break;
		case ret_eagain:
			UNLOCK (fcgim);
			return ret;
		case ret_eof:
		case ret_error:
			reset_connections (fcgim);
			UNLOCK (fcgim);
			return ret;
		default:
			RET_UNKNOWN(ret);
			UNLOCK (fcgim);
			return ret_error;
		}
		
		TRACE (ENTRIES, "Manager(%p)::read ret=%d size=%d\n", fcgim, ret, size);
	}
	
	/* Process the new chunk
	 */
	ret = process_read_buffer (fcgim);
	switch (ret) {
	case ret_ok:
	case ret_eagain:
		break;
	case ret_error:
	case ret_eof_have_data:
		UNLOCK (fcgim);
		return ret;
	default:
		RET_UNKNOWN(ret);
	}

	UNLOCK (fcgim);
	return ret;
}
