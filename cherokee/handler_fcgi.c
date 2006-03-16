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
#include "handler_fcgi.h"
#include "header.h"
#include "connection-protected.h"
#include "util.h"
#include "thread.h"

#include "fastcgi.h"

#define POST_PACKAGE_LEN 32600
#define ENTRIES "fcgi,handler"


#define set_env(cgi,key,val,len) \
	set_env_pair (cgi, key, sizeof(key)-1, val, len)

cherokee_module_info_handler_t MODULE_INFO(fcgi) = {
	.module.type     = cherokee_handler,                /* type         */
	.module.new_func = cherokee_handler_fcgi_new,       /* new func     */
	.valid_methods   = http_get | http_post | http_head /* http methods */
};


static void set_env_pair (cherokee_handler_cgi_base_t *cgi_base, 
			  char *key, int key_len, 
			  char *val, int val_len);
static ret_t
process_package (cherokee_handler_fcgi_t *hdl, cherokee_buffer_t *inbuf)
{
	FCGI_Header           *header;
	FCGI_EndRequestBody   *ending;
	cherokee_buffer_t     *outbuf;
	cherokee_connection_t *conn = HANDLER_CONN(hdl);

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
//		printf ("READ:STDOUT id=%d gen=%d eof=%d (%s): %d", id, hdl->generation, CGI_BASE(hdl)->got_eof, conn->query_string.buf, len);
		cherokee_buffer_add (outbuf, data, len);
		break;

	case FCGI_END_REQUEST: 
		ending = (FCGI_EndRequestBody *)data;

		return_val = ((ending->appStatusB0)       | 
			      (ending->appStatusB0 << 8)  | 
			      (ending->appStatusB0 << 16) | 
			      (ending->appStatusB0 << 24));

		CGI_BASE(hdl)->got_eof    = true;
//		printf ("READ:END id=%d gen=%d", id, hdl->generation);
		break;

	default:
		SHOULDNT_HAPPEN;
	}

	cherokee_buffer_move_to_begin (inbuf, len + FCGI_HEADER_LEN + padding);
//	printf ("- FCGI quedan %d\n", inbuf->len);
	return ret_eagain;
}


static ret_t
process_buffer (cherokee_handler_fcgi_t *hdl, cherokee_buffer_t *buffer)
{
	ret_t ret;
	
	do {
		ret = process_package (hdl, buffer);
	} while (ret == ret_eagain);

	/* ok, error
	 */
	return ret;
}

static ret_t 
read_from_fcgi (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *buffer)
{
	ret_t                    ret;
	size_t                   read = 0;
	cherokee_handler_fcgi_t *fcgi = HANDLER_FCGI(cgi);
	
	ret = cherokee_socket_read (&fcgi->socket, buffer, 4096, &read);

	switch (ret) {
	case ret_eagain:
		cherokee_thread_deactive_to_polling (HANDLER_THREAD(cgi), HANDLER_CONN(cgi), 
						     fcgi->socket.socket, 0, false);
		return ret_eagain;

	case ret_ok:
		TRACE (ENTRIES, "%d bytes readed\n", read);
		return process_buffer (fcgi, buffer);		

	case ret_eof:
	case ret_error:
		cgi->got_eof = true;
		return ret;

	default:
		RET_UNKNOWN(ret);
	}

	SHOULDNT_HAPPEN;
	return ret_error;	
}


ret_t 
cherokee_handler_fcgi_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	CHEROKEE_NEW_STRUCT (n, handler_fcgi);

	/* Init the base class
	 */
	cherokee_handler_cgi_base_init (CGI_BASE(n), cnt, properties, 
					set_env_pair, read_from_fcgi);
	
	/* Virtual methods
	 */
	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_fcgi_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_fcgi_free;

	/* Virtual methods: implemented by handler_cgi_base
	 */
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_cgi_base_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_cgi_base_add_headers;

	/* Properties
	 */
	n->src         = NULL;
	n->post_phase  = fcgi_post_init;
	n->server_list = NULL;
	n->post_len    = 0;
	
	cherokee_socket_init (&n->socket);
	cherokee_buffer_init (&n->write_buffer);
	cherokee_buffer_ensure_size (&n->write_buffer, 512);

	if (properties) {
		cherokee_typed_table_get_list (properties, "servers", &n->server_list);
	}

	/* Return the object
	 */
	*hdl = HANDLER(n);
	return ret_ok;	   
}


ret_t 
cherokee_handler_fcgi_free (cherokee_handler_fcgi_t *hdl)
{
	cherokee_socket_close (&hdl->socket);
	cherokee_socket_mrproper (&hdl->socket);

	cherokee_buffer_mrproper (&hdl->write_buffer);
	return ret_ok;
}


static void
fcgi_build_header (FCGI_Header *hdr, cuchar_t type, cushort_t request_id, cuint_t content_length, cuchar_t padding)
{
        hdr->version         = FCGI_VERSION_1;
        hdr->type            = type;
        hdr->requestIdB0     = (cuchar_t) request_id;
        hdr->requestIdB1     = (cuchar_t) (request_id >> 8) & 0xff;
        hdr->contentLengthB0 = (cuchar_t) (content_length % 256);
        hdr->contentLengthB1 = (cuchar_t) (content_length / 256);
        hdr->paddingLength   = padding;
        hdr->reserved        = 0;
}

static void
fcgi_build_request_body (FCGI_BeginRequestRecord *request)
{
        request->body.roleB0      = FCGI_RESPONDER;
        request->body.roleB1      = 0;
        request->body.flags       = 0; // FCGI_KEEP_CONN
        request->body.reserved[0] = 0;
        request->body.reserved[1] = 0;
        request->body.reserved[2] = 0;
        request->body.reserved[3] = 0;
        request->body.reserved[4] = 0;
}

static void 
set_env_pair (cherokee_handler_cgi_base_t *cgi_base, 
	      char *key, int key_len, 
	      char *val, int val_len)
{
        int                       len;
        FCGI_BeginRequestRecord   request;
	cherokee_handler_fcgi_t  *hdl = HANDLER_FCGI(cgi_base);	
	cherokee_buffer_t        *buf = &hdl->write_buffer;

        len  = key_len + val_len;
        len += key_len > 127 ? 4 : 1;
        len += val_len > 127 ? 4 : 1;

        fcgi_build_header (&request.header, FCGI_PARAMS, 1, len, 0);

        cherokee_buffer_ensure_size (buf, buf->len + sizeof(FCGI_Header) + key_len + val_len);
        cherokee_buffer_add (buf, (void *)&request.header, sizeof(FCGI_Header));

        if (key_len <= 127) {
                buf->buf[buf->len++] = key_len;
        } else {
                buf->buf[buf->len++] = ((key_len >> 24) & 0xff) | 0x80;
                buf->buf[buf->len++] =  (key_len >> 16) & 0xff;
                buf->buf[buf->len++] =  (key_len >> 8)  & 0xff;
                buf->buf[buf->len++] =  (key_len >> 0)  & 0xff;
        }

        if (val_len <= 127) {
                buf->buf[buf->len++] = val_len;
        } else {
                buf->buf[buf->len++] = ((val_len >> 24) & 0xff) | 0x80;
                buf->buf[buf->len++] =  (val_len >> 16) & 0xff;
                buf->buf[buf->len++] =  (val_len >> 8)  & 0xff;
                buf->buf[buf->len++] =  (val_len >> 0)  & 0xff;
        }

        cherokee_buffer_add (buf, key, key_len);
        cherokee_buffer_add (buf, val, val_len);
}


static ret_t
add_extra_fcgi_env (cherokee_handler_fcgi_t *hdl, cuint_t *last_header_offset)
{
	ret_t                        ret;
	cherokee_handler_cgi_base_t *cgi_base = CGI_BASE(hdl);
        cherokee_buffer_t            buffer   = CHEROKEE_BUF_INIT;
	cherokee_connection_t       *conn     = HANDLER_CONN(hdl);

	/* CONTENT_LENGTH
	 */
	ret = cherokee_header_copy_known (conn->header, header_content_length, &buffer);
	if (ret == ret_ok)
		set_env (cgi_base, "CONTENT_LENGTH", buffer.buf, buffer.len);

	/* PATH_INFO
	 */
	cherokee_buffer_clean (&buffer); 
	cherokee_buffer_add_buffer (&buffer, &conn->local_directory);
	if (conn->request.len > 0) {
		cherokee_buffer_add (&buffer, conn->request.buf + 1, conn->request.len - 1);
        }

	if (! cherokee_buffer_is_empty (&conn->pathinfo)) {
		set_env (cgi_base, "PATH_INFO", conn->pathinfo.buf, conn->pathinfo.len);
	}

	/* A few more..
	 */
	set_env (cgi_base, "PATH_TRANSLATED", buffer.buf, buffer.len);

	/* The last one
	 */
	*last_header_offset = hdl->write_buffer.len;
	set_env (cgi_base, "SCRIPT_FILENAME", buffer.buf, buffer.len);

	cherokee_buffer_mrproper (&buffer);
	return ret_ok;
}


static void
fixup_padding (cherokee_buffer_t *buf, cuint_t last_header_offset)
{
        cuint_t      rest;
        cuint_t      pad;
        static char  padding[8] = {0, 0, 0, 0, 0, 0, 0, 0};
        FCGI_Header *last_header;

        if (buf->len <= 0)
                return;
        last_header = (FCGI_Header *) (buf->buf + last_header_offset);
        rest        = buf->len % 8;
        pad         = 8 - rest;

        if (rest == 0) 
                return;

        last_header->paddingLength = pad;

        cherokee_buffer_ensure_size (buf, buf->len + pad);
        cherokee_buffer_add (buf, padding, pad);
}

static void
add_empty_packet (cherokee_handler_fcgi_t *hdl, cuint_t type)
{
        FCGI_BeginRequestRecord  request;
  
        fcgi_build_header (&request.header, type, 1, 0, 0);
        cherokee_buffer_add (&hdl->write_buffer, (void *)&request.header, sizeof(FCGI_Header));

        TRACE (ENTRIES, "empty packet type=%d, len=%d\n", type, hdl->write_buffer.len);
}


static ret_t
build_header (cherokee_handler_fcgi_t *hdl, cherokee_buffer_t *buffer)
{
	FCGI_BeginRequestRecord  request;
	cherokee_connection_t   *conn    = HANDLER_CONN(hdl);
        cuint_t                  last_header_offset;

        /* Take care here, if the connection is reinjected, it
	 * shouldn't parse the arguments again.
	 */
        if (conn->arguments == NULL)
                cherokee_connection_parse_args (conn);
	
	cherokee_buffer_clean (buffer);

	/* FCGI_BEGIN_REQUEST
	 */	
        fcgi_build_header (&request.header, FCGI_BEGIN_REQUEST, 1, sizeof(request.body), 0);
        fcgi_build_request_body (&request);
	
	cherokee_buffer_add (buffer, (void *)&request, sizeof(FCGI_BeginRequestRecord));
        TRACE (ENTRIES, "Added FCGI_BEGIN_REQUEST, len=%d\n", buffer->len);

	/* Add enviroment variables
	 */
	cherokee_handler_cgi_base_build_envp (CGI_BASE(hdl), conn);

	add_extra_fcgi_env (hdl, &last_header_offset);
        fixup_padding (buffer, last_header_offset);

	/* There are not more parameters
         */
	add_empty_packet (hdl, FCGI_PARAMS);

        TRACE (ENTRIES, "Added FCGI_PARAMS, len=%d\n", buffer->len);
	return ret_ok;
}


static ret_t 
connect_to_server (cherokee_handler_fcgi_t *hdl)
{
	ret_t ret;

	if (hdl->src == NULL) {
		ret = cherokee_ext_source_get_next (EXT_SOURCE_HEAD(hdl->server_list->next), hdl->server_list, &hdl->src);
		if (unlikely (ret != ret_ok)) return ret;
	}
	
	ret = cherokee_ext_source_connect (hdl->src, &hdl->socket);
	if (unlikely (ret != ret_ok)) return ret;
	
	return ret_ok;
}


static ret_t 
do_send (cherokee_handler_fcgi_t *hdl, cherokee_buffer_t *buffer)
{
	ret_t  ret;
	size_t written = 0;
	
	ret = cherokee_socket_write (&hdl->socket, buffer, &written);
	if (ret != ret_ok) return ret;
	
	cherokee_buffer_move_to_begin (buffer, written);

	TRACE (ENTRIES, "sent remaining=%d\n", buffer->len);

	if (! cherokee_buffer_is_empty (buffer))
		return ret_eagain;
	
	return ret_ok;
}


static ret_t
send_post (cherokee_handler_fcgi_t *hdl, cherokee_buffer_t *buf)
{
	ret_t                    ret;
	cherokee_connection_t   *conn         = HANDLER_CONN(hdl);
	static FCGI_Header       empty_header = {0,0,0,0,0,0,0,0};
			
	switch (hdl->post_phase) {
	case fcgi_post_init:
		TRACE (ENTRIES, "Post phase = %s\n", "init");

		/* Init the POST storing object
		 */
		cherokee_post_walk_reset (&conn->post);
		cherokee_post_get_len (&conn->post, &hdl->post_len);

		if (hdl->post_len <= 0)
			return ret_ok;
		
		hdl->post_phase = fcgi_post_read;

	case fcgi_post_read:
		TRACE (ENTRIES, "Post phase = %s\n", "read");

		if (cherokee_buffer_is_empty (buf)) {
			cherokee_buffer_add (buf, (char *)&empty_header, sizeof (FCGI_Header));
		}

		ret = cherokee_post_walk_read (&conn->post, buf, POST_PACKAGE_LEN);
		switch (ret) {
		case ret_ok:
                case ret_eagain:
                        break;
		case ret_error:
                        return ret;
		default:
                        RET_UNKNOWN(ret);
                        return ret_error;
		}

		if (buf->len > sizeof(FCGI_Header)) {
			fcgi_build_header ((FCGI_Header *)buf->buf, FCGI_STDIN, 1,
					   buf->len - sizeof(FCGI_Header), 0);
		}

		ret = cherokee_post_walk_finished (&conn->post);
		if (ret == ret_ok) {
			add_empty_packet (hdl, FCGI_STDIN);
		}

		hdl->post_phase = fcgi_post_write;

	case fcgi_post_write:
		TRACE (ENTRIES, "Post phase = write, buf.len=%d\n", buf->len);

		if (! cherokee_buffer_is_empty (buf)) {
			ret = do_send (hdl, buf);
			switch (ret) {
                        case ret_ok:
                                break;
                        case ret_eagain:
                                return ret_eagain;
                        case ret_eof:
                        case ret_error:
                                return ret_error;
                        default:
				RET_UNKNOWN(ret);
				return ret_error;
                        }
		}

		if (! cherokee_buffer_is_empty (buf))
			return ret_eagain;

		ret = cherokee_post_walk_finished (&conn->post);
		switch (ret) {
		case ret_ok:
			break;
		case ret_error:
			return ret_error;
		case ret_eagain:
			hdl->post_phase = fcgi_post_read;
			return ret_eagain;
		default:
			RET_UNKNOWN(ret);
			return ret_error;
		}

		TRACE (ENTRIES, "Post phase = %s\n", "finished");
		return ret_ok;

	default:
		SHOULDNT_HAPPEN;
	}

	return ret_error;
}


ret_t 
cherokee_handler_fcgi_init (cherokee_handler_fcgi_t *hdl)
{
	ret_t                  ret;
	cherokee_connection_t *conn = HANDLER_CONN(hdl);

	switch (CGI_BASE(hdl)->init_phase) {
	case hcgi_phase_build_headers:
		/* Prepare Post
		 */
		if (! cherokee_post_is_empty (&conn->post)) {
			cherokee_post_walk_reset (&conn->post);
			cherokee_post_get_len (&conn->post, &hdl->post_len);
		}

		/* Build the headers
		 */
		ret = build_header (hdl, &hdl->write_buffer);
		if (unlikely (ret != ret_ok)) return ret;

		/* Connect	
		 */
		ret = connect_to_server (hdl);
		if (unlikely (ret != ret_ok)) return ret;
		
		CGI_BASE(hdl)->init_phase = hcgi_phase_send_headers;

	case hcgi_phase_send_headers:
		/* Send the header
		 */
		ret = do_send (hdl, &hdl->write_buffer);
		if (ret != ret_ok) return ret;

		CGI_BASE(hdl)->init_phase = hcgi_phase_send_post;

	case hcgi_phase_send_post:
		/* Send the Post
		 */
		if (hdl->post_len > 0) {
			return send_post (hdl, &hdl->write_buffer);
		}
		break;
	}

	return ret_ok;
}


/* Module init
 */
void  
MODULE_INIT(fcgi) (cherokee_module_loader_t *loader)
{
}

