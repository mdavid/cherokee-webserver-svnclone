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
#include "fcgi_manager.h"
#include "util.h"
#include "fastcgi.h"
#include "connection-protected.h"
#include "handler_fastcgi.h"

#include <unistd.h>


#define ENTRIES "manager,fcgi"
#define CONN_STEP 20 


static void
reset_connections (cherokee_fcgi_manager_t *fcgim)
{
	cuint_t i;

//	printf ("\n++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n");
//	printf ("+ El reconnect de la muerte ataca de nuevo!! Nooooooooooooo... +\n");
//	printf ("++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++++\n\n");

	for (i=1; i<fcgim->conn.size; i++) {
		cherokee_handler_cgi_base_t *cgi;
		
		if (fcgim->conn.id2conn[i].conn == NULL)
			continue;
			
		cgi = CGI_BASE(fcgim->conn.id2conn[i].conn->handler);

		if (fcgim->generation != HANDLER_FASTCGI(cgi)->generation) {
//			printf ("CLEAN: diff gen %i\n", i);
			continue;
		}
		
//		printf ("reconnect CLEAN: reset_connections id=%d\n", i);
		
		cgi->got_eof = true;
		fcgim->conn.id2conn[i].conn = NULL;
		fcgim->conn.id2conn[i].eof  = true;
	}
}

ret_t 
cherokee_fcgi_manager_new (cherokee_fcgi_manager_t **fcgim, cherokee_ext_source_t *src)
{
	ret_t   ret;
	cuint_t i;
        CHEROKEE_NEW_STRUCT (n,fcgi_manager);
	
	ret = cherokee_socket_new (&n->socket);
	if (unlikely (ret != ret_ok)) return ret;
	
	n->source         = src;
	n->generation     = 0;
	n->first_connect  = true;

	n->conn.size      = CONN_STEP;
	n->conn.id2conn   = (conn_entry_t *) malloc (CONN_STEP * sizeof (conn_entry_t));

	for (i=0; i<CONN_STEP; i++) {
		n->conn.id2conn[i].conn = NULL;
		n->conn.id2conn[i].eof  = true;		
	}

	cherokee_buffer_init (&n->read_buffer);
	
	*fcgim = n;
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_free (cherokee_fcgi_manager_t *fcgim)
{
	cherokee_socket_close (fcgim->socket);
	cherokee_socket_free (fcgim->socket);

	cherokee_buffer_mrproper (&fcgim->read_buffer);
	
	free (fcgim);
	return ret_ok;
}


static ret_t 
reconnect (cherokee_fcgi_manager_t *fcgim, cherokee_thread_t *thd, cherokee_boolean_t clean_up)
{
	ret_t                  ret;
	cuint_t                next;
	cuint_t                try    = 0;
	cherokee_ext_source_t *src    = fcgim->source;

	/* Do some clean up
	 */
	if (clean_up) {
		TRACE (ENTRIES, "Cleaning up before reconecting %s", "\n");

		cherokee_thread_close_polling_connections (thd, SOCKET_FD(fcgim->socket), NULL);

		reset_connections (fcgim);
		cherokee_buffer_clean (&fcgim->read_buffer);
	
		/* Set the new generation number
		 */
		next = fcgim->generation;
		fcgim->generation = ++next % 255;

		/* Close	
		 */
		cherokee_socket_close (fcgim->socket);
	}
	
	/* If it connects we're done here..
	 */
	ret = cherokee_ext_source_connect (src, fcgim->socket);
	if (ret != ret_ok) {
		/* It didn't sucess to connect, so lets spawn a new server
		 */
		ret = cherokee_ext_source_spawn_srv (src);
		if (ret != ret_ok) {
			TRACE (ENTRIES, "Couldn't spawn: %s\n", src->host.buf ? src->host.buf : src->unix_socket.buf);
			return ret;
		}
		
		for (; try < 4; try++) {
			/* Try to connect again	
			 */
			ret = cherokee_ext_source_connect (src, fcgim->socket);
			if (ret == ret_ok) break;

			TRACE (ENTRIES, "Couldn't connect: %s, try %d\n", src->host.buf ? src->host.buf : src->unix_socket.buf, try);
			sleep (1);
		}
	}

	cherokee_fd_set_nonblocking (fcgim->socket->socket);

	TRACE (ENTRIES, "Connected sucessfully try=%d, fd=%d\n", try, fcgim->socket->socket);
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_ensure_is_connected (cherokee_fcgi_manager_t *fcgim, cherokee_thread_t *thd)
{
	ret_t ret;
	
	if (fcgim->socket->status == socket_closed) {
		printf ("RECONNECT %p\n", fcgim->socket);
		TRACE (ENTRIES, "It needs to reconnect %s", "\n");

		ret = reconnect (fcgim, thd, !fcgim->first_connect);
		if (ret != ret_ok) return ret;
		
		if (fcgim->first_connect)
			fcgim->first_connect = false;
	}
	
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_register_connection (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id, cuchar_t *gen)
{
	cuint_t            slot;
	cherokee_boolean_t found = false;

	/* Look for a available slot
	 */
	for (slot = 1; slot < fcgim->conn.size; slot++) {
		if ((fcgim->conn.id2conn[slot].eof) &&
		    (fcgim->conn.id2conn[slot].conn == NULL)) 
		{
			found = true;
			break;
		}
	}

	/* Do we need more memory for the index?
	 */
	if (found == false) {
		cuint_t i;

		fcgim->conn.id2conn = (conn_entry_t *) realloc (
			fcgim->conn.id2conn, (fcgim->conn.size + CONN_STEP) * sizeof(conn_entry_t));

                if (unlikely (fcgim->conn.id2conn == NULL)) 
			return ret_nomem;

		for (i = 0; i < CONN_STEP; i++) {
			fcgim->conn.id2conn[i + fcgim->conn.size].eof  = true;
			fcgim->conn.id2conn[i + fcgim->conn.size].conn = NULL;			
		}
		
		slot = fcgim->conn.size;
		fcgim->conn.size += CONN_STEP;
	}

	/* Register the connection
	 */
	fcgim->conn.id2conn[slot].conn = conn;
	fcgim->conn.id2conn[slot].eof  = false;

	*gen = fcgim->generation;
	*id  = slot;
	
	TRACE (ENTRIES, "registered id=%d (gen=%d)\n", slot, fcgim->generation);
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_unregister_id (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn)
{
	cherokee_handler_fastcgi_t *hdl = HANDLER_FASTCGI(conn->handler);

	if (hdl->generation != fcgim->generation) {
		TRACE (ENTRIES, "Unregister: Different generation id=%d gen=%d, mgr=%d\n", 
		       hdl->id, fcgim->generation, fcgim->generation);
		return ret_ok;
	}

	if (fcgim->conn.id2conn[hdl->id].conn != conn) {
		SHOULDNT_HAPPEN;
		exit(1);
		return ret_error;
	}

	TRACE (ENTRIES, "UNregistered id=%d (gen=%d)\n", hdl->id, fcgim->generation);
	fcgim->conn.id2conn[hdl->id].conn = NULL;	
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_send_and_remove (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *buf)
{
	ret_t  ret;
	size_t written = 0;

	ret = cherokee_socket_write (fcgim->socket, buf, &written);
	switch (ret) {
	case ret_ok:
		TRACE (ENTRIES, "Sent %db\n", written);
		cherokee_buffer_move_to_begin (buf, written);
		return ret_ok;
	case ret_eof:
	case ret_error:
	case ret_eagain:
		return ret;
	default:
		RET_UNKNOWN(ret);
	}

	return ret_ok;
}


static ret_t
process_package (cherokee_fcgi_manager_t *fcgim, cherokee_buffer_t *inbuf)
{
	FCGI_Header                *header;
	FCGI_EndRequestBody        *ending;
	cherokee_buffer_t          *outbuf;
	cherokee_connection_t      *conn;
	cherokee_handler_fastcgi_t *hdl;

	cuint_t  len;
	char    *data;
	cint_t   return_val;
	cuint_t  type;
	cuint_t  id;
	cuint_t  padding;
		
	/* Is there enough information?
	 */
	if (inbuf->len < sizeof(FCGI_Header))
		return ret_ok;

	/* At least there is a header
	 */
	header = (FCGI_Header *)inbuf->buf;

	if (header->version != 1) {
		cherokee_buffer_print_debug (inbuf, -1);
		PRINT_ERROR ("Parsing error: unknown version\n");
		return ret_error;
	}
	
	if (header->type != FCGI_STDERR &&
	    header->type != FCGI_STDOUT && 
	    header->type != FCGI_END_REQUEST)
	{
		cherokee_buffer_print_debug (inbuf, -1);
		PRINT_ERROR ("Parsing error: unknown type\n");
		return ret_error;
	}
	
	/* Read the header
	 */
	type    =  header->type;
	padding =  header->paddingLength;
	id      = (header->requestIdB0     | (header->requestIdB1 << 8));
	len     = (header->contentLengthB0 | (header->contentLengthB1 << 8));

	/* Is the package complete?
	 */
	if (len > inbuf->len - (FCGI_HEADER_LEN + padding))
		return ret_ok;

	/* Locate the connection
	 */
	conn = fcgim->conn.id2conn[id].conn;
	if (conn == NULL) {
		if (fcgim->conn.id2conn[id].eof)
			goto error;

		goto go_out;
	}

	hdl      = HANDLER_FASTCGI(conn->handler);
	outbuf   = &CGI_BASE(hdl)->data;
	data     = inbuf->buf +  FCGI_HEADER_LEN;

	/* It has received the full package content
	 */
	switch (type) {
	case FCGI_STDERR: 
		if (CONN_VSRV(conn)->logger != NULL) {
			cherokee_buffer_t tmp = CHEROKEE_BUF_INIT;
			
			cherokee_buffer_add (&tmp, data, len);
			cherokee_logger_write_string (CONN_VSRV(conn)->logger, "%s", tmp.buf);
			cherokee_buffer_mrproper (&tmp);		
		}
		exit(1);
		break;

	case FCGI_STDOUT:
		printf ("READ:STDOUT id=%d gen=%d eof=%d (%s): %d", id, hdl->generation, CGI_BASE(hdl)->got_eof, conn->query_string.buf, len);
		cherokee_buffer_add (outbuf, data, len);
		break;

	case FCGI_END_REQUEST: 
		ending = (FCGI_EndRequestBody *)data;

		return_val = ((ending->appStatusB0)       | 
			      (ending->appStatusB0 << 8)  | 
			      (ending->appStatusB0 << 16) | 
			      (ending->appStatusB0 << 24));

		CGI_BASE(hdl)->got_eof      = true;
		fcgim->conn.id2conn[id].eof = true;

		printf ("READ:END id=%d gen=%d", id, hdl->generation);
		break;

	default:
		SHOULDNT_HAPPEN;
		exit(1);
	}

go_out:
	cherokee_buffer_move_to_begin (inbuf, len + FCGI_HEADER_LEN + padding);
	printf ("- FCGI quedan %d\n", inbuf->len);
	return ret_eagain;

error:
	cherokee_buffer_move_to_begin (inbuf, len + FCGI_HEADER_LEN + padding);
	return ret_error;
}


static ret_t
process_buffer (cherokee_fcgi_manager_t *fcgim)
{
	ret_t ret;
	
	do {
		ret = process_package (fcgim, &fcgim->read_buffer);
	} while (ret == ret_eagain);

	/* ok, error
	 */
	return ret;
}


ret_t 
cherokee_fcgi_manager_step (cherokee_fcgi_manager_t *fcgim)
{
	ret_t  ret;
	size_t size = 0; 
		
	if (fcgim->read_buffer.len < sizeof(FCGI_Header)) {
		ret = cherokee_socket_read (fcgim->socket, &fcgim->read_buffer, DEFAULT_READ_SIZE, &size);
		switch (ret) {
		case ret_ok:
			TRACE (ENTRIES, "%d bytes readed\n", size);
			break;

		case ret_eof:
			TRACE (ENTRIES, "%s\n", "EOF");
			return ret_eof;

		case ret_error:
		case ret_eagain:
			return ret;

		default:
			RET_UNKNOWN(ret);
			return ret_error;	
		}
	}

	/* Process the new chunk
	 */
	ret = process_buffer (fcgim);
	if (unlikely (ret != ret_ok)) return ret;

	return ret_ok;
}

