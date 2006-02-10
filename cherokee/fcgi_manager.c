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

#include <unistd.h>


#define ENTRIES "manager,fcgi"
#define CONN_STEP 20 

ret_t 
cherokee_fcgi_manager_new (cherokee_fcgi_manager_t **fcgim, cherokee_ext_source_t *src)
{
	ret_t   ret;
	cuint_t i;
        CHEROKEE_NEW_STRUCT (n,fcgi_manager);
	
	ret = cherokee_socket_new (&n->socket);
	if (unlikely (ret != ret_ok)) return ret;
	
	n->source             = src;
	n->conn.id2conn       = malloc (CONN_STEP * sizeof (cherokee_connection_t *));
	n->conn.size          = CONN_STEP;

	n->current.id         = 0;
	n->current.padding    = 0;
	n->current.remaining  = 0;
	n->current.return_val = 0;
	
	for (i=0; i<n->conn.size; i++)
		n->conn.id2conn[i] = NULL;

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


ret_t 
cherokee_fcgi_manager_reconnect (cherokee_fcgi_manager_t *fcgim)
{
	ret_t                  ret;
	cuint_t                try = 0;
	cherokee_ext_source_t *src = fcgim->source;
	
	/* Close
	 */
	cherokee_socket_close (fcgim->socket);
	
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


	TRACE (ENTRIES, "Connected sucessfully try=%d, fd=%d\n", try, fcgim->socket->socket);
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_ensure_is_connected (cherokee_fcgi_manager_t *fcgim)
{
	ret_t ret;
	
	if (fcgim->socket->status == socket_closed) {
		TRACE (ENTRIES, "It needs to reconnect %s", "\n");

		ret = cherokee_fcgi_manager_reconnect (fcgim);
		if (ret != ret_ok) return ret;
	}
	
	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_register_connection (cherokee_fcgi_manager_t *fcgim, cherokee_connection_t *conn, cuint_t *id)
{
	cuint_t            slot;
	cherokee_boolean_t found = false;
	
	/* Look for a available slot
	 */
	for (slot = 1; slot < fcgim->conn.size; slot++) {
		if (fcgim->conn.id2conn[slot] == NULL) {
			found = true;
			break;
		}
	}

	/* Do we need more memory for the index?
	 */
	if (found == false) {
		fcgim->conn.id2conn = (cherokee_connection_t **) realloc (
			fcgim->conn.id2conn, (fcgim->conn.size + CONN_STEP) * sizeof(cherokee_connection_t *));               

                if (unlikely (fcgim->conn.id2conn == NULL)) 
			return ret_nomem;

		memset (&fcgim->conn.id2conn[fcgim->conn.size], 0, CONN_STEP);
		
		slot = fcgim->conn.size + 1;
		fcgim->conn.size += CONN_STEP;
	}

	/* Register the connection
	 */
	*id = slot;
	fcgim->conn.id2conn[slot] = conn;
	
	TRACE (ENTRIES, "registered id=%d\n", slot);

	return ret_ok;
}


ret_t 
cherokee_fcgi_manager_unregister_id (cherokee_fcgi_manager_t *fcgim, cuint_t id)
{
	fcgim->conn.id2conn[id] = NULL;	
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
	case ret_error:
	case ret_eagain:
		return ret;
	default:
		RET_UNKNOWN(ret);
	}

	return ret_ok;
}


static void
process_buffer (cherokee_fcgi_manager_t *fcgim, void *data, cuint_t data_len)
{
        cherokee_connection_t *conn;
        cherokee_buffer_t      tmp = CHEROKEE_BUF_INIT;
	cuint_t                id  = fcgim->current.id;

	conn = fcgim->conn.id2conn[id];
	
        switch (fcgim->current.type) {
        case FCGI_STDERR: 
                cherokee_buffer_add (&tmp, data, data_len);
                cherokee_logger_write_string (CONN_VSRV(conn)->logger, "%s", tmp.buf);
                cherokee_buffer_mrproper (&tmp);
                break;

        case FCGI_STDOUT:
		cherokee_buffer_add (&conn->buffer, data, data_len);
		TRACE(ENTRIES, "Written %d to connection id=%d, %p\n", data_len, id, conn);
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

        offset = 0;
        ret    = ret_eagain;

        while (fcgim->read_buffer.len > 0) 
        { 
                if (fcgim->current.remaining == 0) { 
                        if (fcgim->read_buffer.len < sizeof(FCGI_Header))
                                return ret_eagain;

                        header = (FCGI_Header *) start;

                        if (!(header->type == FCGI_STDERR || 
                              header->type == FCGI_STDOUT || 
                              header->type == FCGI_END_REQUEST))
                        {
                                printf ("rb:%d x:%d rs:%d\n", fcgim->read_buffer.len, fcgim->current.padding, fcgim->current.remaining);
                                cherokee_buffer_print_debug (&fcgim->read_buffer, -1);
                                return ret_error;
                        }

                        fcgim->current.id         = (header->requestIdB0     | (header->requestIdB1 << 8));
                        fcgim->current.type       =  header->type;
                        len                       = (header->contentLengthB0 | (header->contentLengthB1 << 8));
                        fcgim->current.padding    =  header->paddingLength;
                        fcgim->current.return_val =  0;

                        offset = FCGI_HEADER_LEN;
                        if (len > (fcgim->read_buffer.len - FCGI_HEADER_LEN)) {
                                fcgim->current.remaining = len - fcgim->read_buffer.len - FCGI_HEADER_LEN + 16;
                                len = fcgim->read_buffer.len - FCGI_HEADER_LEN;
                                bytes_to_move = len;
                        } else {
                                fcgim->current.remaining = 0;
                                bytes_to_move = len;      
                                if ((fcgim->current.padding + len) > (fcgim->read_buffer.len - FCGI_HEADER_LEN)) {

                                        fcgim->current.padding = (fcgim->current.padding + len) - fcgim->read_buffer.len - FCGI_HEADER_LEN;
                                        bytes_to_move += fcgim->current.padding;
                                } else {
                                        bytes_to_move += fcgim->current.padding;
                                        fcgim->current.padding = 0;
                                }
                        }
                        bytes_to_move += FCGI_HEADER_LEN;
                } else {
                        if (fcgim->current.remaining > fcgim->read_buffer.len) {
                                fcgim->current.remaining = fcgim->current.remaining - fcgim->read_buffer.len;
                                len = fcgim->read_buffer.len;
                                bytes_to_move = len;
                        } else {
                                len = fcgim->current.remaining;
                                bytes_to_move = len;
                                if (fcgim->current.padding > 0) {
                                        if ((fcgim->current.remaining + fcgim->current.padding) > fcgim->read_buffer.len) {
                                                fcgim->current.padding = fcgim->current.remaining + fcgim->current.padding - fcgim->read_buffer.len;
                                        } else {
                                                bytes_to_move += fcgim->current.padding;
                                                fcgim->current.padding = 0;
                                        }
                                }
                                fcgim->current.remaining = 0;
                        } 
                }
   
                switch (fcgim->current.type) {
                case FCGI_STDERR:
                case FCGI_STDOUT:
                        if (len > 0) {
                                process_buffer (fcgim, (start + offset), len);
			}
                        break;

                case FCGI_END_REQUEST:
                        end_request = (FCGI_EndRequestBody *) (start + offset);

                        fcgim->current.return_val =  ((end_request->appStatusB0)       | 
						      (end_request->appStatusB0 << 8)  | 
						      (end_request->appStatusB0 << 16) | 
						      (end_request->appStatusB0 << 24));
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
        ret = process_read_buffer (fcgim);
	if (unlikely (ret != ret_ok)) return ret;

	return ret_ok;
}
