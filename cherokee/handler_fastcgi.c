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

#include "handler_fastcgi.h"
#include "buffer.h"
#include "module.h"
#include "fastcgi.h"
#include "connection.h"
#include "connection-protected.h"
#include "thread.h"
#include "list_ext.h"
#include "util.h"


cherokee_module_info_handler_t MODULE_INFO(fastcgi) = {
	.module.type     = cherokee_handler,                /* type         */
	.module.new_func = cherokee_handler_fastcgi_new,    /* new func     */
	.valid_methods   = http_get | http_post | http_head /* http methods */
};


#define ENTRIES "handler,fastcgi"

#define FCGI_SCRIPT_NAME_VAR      "SCRIPT_NAME"
#define FCGI_SCRIPT_FILENAME_VAR  "SCRIPT_FILENAME"
#define FCGI_QUERY_STRING_VAR     "QUERY_STRING"
#define FCGI_PATH_INFO_VAR        "PATH_INFO"
#define FCGI_PATH_TRANSLATED_VAR  "PATH_TRANSLATED"


static cherokee_table_t __fcgi_managers;


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
	request->body.flags       = FCGI_KEEP_CONN;
	request->body.reserved[0] = 0;
	request->body.reserved[1] = 0;
	request->body.reserved[2] = 0;
	request->body.reserved[3] = 0;
	request->body.reserved[4] = 0;
}


ret_t 
cherokee_handler_fastcgi_new (cherokee_handler_t **hdl, cherokee_connection_t *cnt, cherokee_table_t *properties)
{
	CHEROKEE_NEW_STRUCT (n, handler_fastcgi);

	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt);
	   
	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_fastcgi_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_fastcgi_free;
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_fastcgi_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_fastcgi_add_headers; 

	/* Supported features
	 */
	HANDLER(n)->support     = hsupport_nothing;

	/* Init
	 */	
	n->id              = 0;
	n->manager_ref     = NULL;
	n->server_list     = NULL;
	n->configuration   = NULL;
	n->phase           = fcgi_phase_init;
	n->system_env      = NULL;

	n->post_sent       = false;
	n->post_phase      = fcgi_post_init;

	cherokee_buffer_init (&n->data);
	cherokee_buffer_init (&n->environment);
	cherokee_buffer_init (&n->write_buffer);
	cherokee_buffer_init (&n->incoming_buffer);

	/* Check the parameters
	 */
	if (properties != NULL) {
		cherokee_typed_table_get_list (properties, "servers", &n->server_list);
		cherokee_typed_table_get_list (properties, "env", &n->system_env);
	}

	if ((n->server_list == NULL) || (list_empty (n->server_list))) {
		PRINT_ERROR_S ("FastCGI misconfigured\n");
		return ret_error;
	}

	/* Return
	 */
	*hdl = HANDLER(n);
	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_free (cherokee_handler_fastcgi_t *hdl)
{
	cherokee_connection_t *conn = HANDLER_CONN(hdl);

	if (hdl->manager_ref != NULL) {
		cherokee_fcgi_manager_unregister_conn (hdl->manager_ref, conn);
	}

	cherokee_buffer_mrproper (&hdl->data);
	cherokee_buffer_mrproper (&hdl->write_buffer);
	cherokee_buffer_mrproper (&hdl->incoming_buffer);
	cherokee_buffer_mrproper (&hdl->environment);

	return ret_ok;
}


static void
fixup_padding (cherokee_buffer_t *buf, cuint_t id, cuint_t last_header_offset)
{
	cuint_t      rest;
	cuint_t      pad;
	char         padding[8] = {0, 0, 0, 0, 0, 0, 0, 0};
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
add_env_pair_with_id (cherokee_buffer_t *buf, cuint_t id,
		      char *key, int key_len,
		      char *val, int val_len)
{
	FCGI_BeginRequestRecord  request;
	int                      len;
 
	len  = key_len + val_len;
	len += key_len > 127 ? 4 : 1;
	len += val_len > 127 ? 4 : 1;

	fcgi_build_header (&request.header, FCGI_PARAMS, id, len, 0);

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


static void
add_env_pair_2_params (void *params[2], 
		       char *key, int key_len,
		       char *val, int val_len)
{
	cherokee_buffer_t *buf;
	cuint_t            id;

	buf = (cherokee_buffer_t *)params[0];
	id  = POINTER_TO_INT(params[1]);

	add_env_pair_with_id (buf, id, key, key_len, val, val_len);
}


static void
add_more_env (cherokee_handler_fastcgi_t *fcgi, cherokee_buffer_t *buf, cuint_t *last_header_offset)
{
	cherokee_connection_t   *conn;
	cherokee_buffer_t        buffer = CHEROKEE_BUF_INIT;

	conn = HANDLER_CONN(fcgi);
	cherokee_buffer_add_buffer (&buffer, &conn->local_directory);

	if (conn->request.len > 0) {
		if (conn->request.buf [0] == '/') {
			cherokee_buffer_add (&buffer, conn->request.buf + 1, conn->request.len - 1);
		} else {
			cherokee_buffer_add (&buffer, conn->request.buf, conn->request.len);
		}
	}
  
	add_env_pair_with_id (buf, fcgi->id,
			      FCGI_SCRIPT_FILENAME_VAR, sizeof (FCGI_SCRIPT_FILENAME_VAR) - 1,
			      buffer.buf, buffer.len);
	
	if (!cherokee_buffer_is_empty (&conn->query_string)) {
		add_env_pair_with_id (buf, fcgi->id,
				      FCGI_QUERY_STRING_VAR, sizeof (FCGI_QUERY_STRING_VAR) - 1,
				      conn->query_string.buf, conn->query_string.len);
	}
 
	if (!cherokee_buffer_is_empty (&conn->pathinfo)) {
		add_env_pair_with_id (buf, fcgi->id,
				      FCGI_PATH_INFO_VAR, sizeof (FCGI_PATH_INFO_VAR) - 1,
				      conn->pathinfo.buf, conn->pathinfo.len);

		cherokee_buffer_mrproper (&buffer);
		cherokee_buffer_add_buffer (&buffer, &conn->local_directory);
		if (conn->local_directory.buf [conn->local_directory.len - 1] == '/')
			cherokee_buffer_add (&buffer, conn->pathinfo.buf + 1, conn->pathinfo.len - 1);
		else
			cherokee_buffer_add_buffer (&buffer, &conn->pathinfo);
	} else {
		char              *pathinfo;
		int                pathinfo_len, ret;
		cherokee_buffer_t  tmp = CHEROKEE_BUF_INIT;

		cherokee_buffer_add_buffer (&tmp, &conn->local_directory);
		cherokee_buffer_add (&tmp, conn->request.buf + 1, conn->request.len - 1);

		ret = cherokee_split_pathinfo (&tmp, conn->local_directory.len, &pathinfo, &pathinfo_len); 
		if (ret == ret_ok) {
			add_env_pair_with_id (buf,  fcgi->id,
					      FCGI_PATH_INFO_VAR, sizeof (FCGI_PATH_INFO_VAR) - 1,
					      pathinfo, pathinfo_len);
		}
		cherokee_buffer_mrproper (&tmp);
	}

	add_env_pair_with_id (buf, fcgi->id,
			      FCGI_PATH_TRANSLATED_VAR, sizeof (FCGI_PATH_TRANSLATED_VAR) - 1,
			      buffer.buf, buffer.len);


	/* Add the custom define environment variables
	 */
	if (fcgi->system_env != NULL) {
		list_t *i;

		list_for_each (i, fcgi->system_env) {
			char    *name;
			cuint_t  name_len;
			char    *value;
			
			name     = LIST_ITEM_INFO(i);
			name_len = strlen(name);
			value    = name + name_len + 1;
			
			add_env_pair_with_id (buf, fcgi->id, name, name_len, value, strlen(value));
		}
	}	

	/* The last package has a special treatment, we need
	 * to now where the header begins to set the right padding
	 */
	*last_header_offset = buf->len;

	add_env_pair_with_id (buf, fcgi->id,
			      FCGI_SCRIPT_NAME_VAR, sizeof (FCGI_SCRIPT_NAME_VAR) - 1,
			      conn->request.buf, conn->request.len);

	cherokee_buffer_mrproper (&buffer);
}


static ret_t
build_initial_packages (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t                    ret;
	FCGI_BeginRequestRecord  request;
	void                    *params[2];
	cuint_t                  last_header_offset;
	cherokee_buffer_t        tmp         = CHEROKEE_BUF_INIT;
	cherokee_buffer_t        write_tmp   = CHEROKEE_BUF_INIT;
	cherokee_connection_t   *conn        = HANDLER_CONN (fcgi);;

	cherokee_connection_parse_args (conn);

	/* FCGI_BEGIN_REQUEST
	 */
	fcgi_build_header (&request.header, FCGI_BEGIN_REQUEST, fcgi->id, sizeof(request.body), 0);
	fcgi_build_request_body (&request);

	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request, sizeof(FCGI_BeginRequestRecord));

	TRACE (ENTRIES, "Added FCGI_BEGIN_REQUEST, len=%d\n", fcgi->write_buffer.len);
  
	/* Add enviroment variables
	 */ 
	params[0] = (void *) &write_tmp;
	params[1] = (void *) INT_TO_POINTER(fcgi->id);

	ret = cherokee_cgi_build_basic_env (conn, (cherokee_cgi_set_env_pair_t) add_env_pair_2_params, &tmp, params);
	if (unlikely (ret != ret_ok)) return ret; 

	add_more_env (fcgi, &write_tmp, &last_header_offset);
	fixup_padding (&write_tmp, fcgi->id, last_header_offset);
	
	cherokee_buffer_add_buffer (&fcgi->write_buffer, &write_tmp);

	/* Clean it up
	 */
	cherokee_buffer_mrproper (&tmp);
	cherokee_buffer_mrproper (&write_tmp);

	/* There aren't more parameters
	 */
	fcgi_build_header (&request.header, FCGI_PARAMS, fcgi->id, 0, 0);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));

	TRACE (ENTRIES, "Added FCGI_PARAMS, len=%d\n", fcgi->write_buffer.len);

	return ret_ok;
}


static cherokee_fcgi_server_t *
next_server (cherokee_handler_fastcgi_t *fcgi)
{
	cherokee_fcgi_server_t       *current_config;
	cherokee_fcgi_server_first_t *first_config = FCGI_FIRST_SERVER(fcgi->server_list->next);

	CHEROKEE_MUTEX_LOCK (&first_config->current_server_lock);
	
	/* Set the next server
	 */
	current_config               = first_config->current_server;
	first_config->current_server = FCGI_SERVER(((list_t *)current_config)->next);

	/* This is a special case: if the next is the base of the list, we have to
	 * skip the entry and point to the next one
	 */
	if ((list_t*)first_config->current_server == fcgi->server_list) {
		current_config = first_config->current_server;
		first_config->current_server = FCGI_SERVER(((list_t *)current_config)->next);
	}		

	CHEROKEE_MUTEX_UNLOCK (&first_config->current_server_lock);
	return first_config->current_server;
}


ret_t 
cherokee_handler_fastcgi_init (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t                    ret;
	cherokee_connection_t   *conn      = HANDLER_CONN(fcgi);
	cherokee_table_t        *managers  = &__fcgi_managers;
	
	/* Read the current server and set the next one
	 */
	fcgi->configuration = next_server (fcgi);

	TRACE (ENTRIES, "Using: host \"%s\" and interpreter \"%s\"\n", 
	       fcgi->configuration->host.buf, fcgi->configuration->interpreter.buf);

	/* FastCGI manager
	 */
	ret = cherokee_table_get (managers, fcgi->configuration->host.buf, (void **)&fcgi->manager_ref);
	if (ret == ret_not_found) {
		cherokee_fcgi_manager_t *n;
 
		/* Create a new manager object
		 */
		ret = cherokee_fcgi_manager_new (&n, fcgi->configuration);
		if (unlikely (ret != ret_ok)) return ret;

		TRACE (ENTRIES, "Creating new manager \"%s\"\n", fcgi->configuration->host.buf);
		
		/* Assign the object to that path
		 */
		ret = cherokee_table_add (managers, fcgi->configuration->host.buf, n);
		if (unlikely (ret != ret_ok)) return ret;
		fcgi->manager_ref = n;

		/* Connect to a running serber
		 */
		ret = cherokee_fcgi_manager_connect_to_srv (n);
		if (unlikely (ret != ret_ok)) {

			TRACE (ENTRIES, "Could not connect to \"%s\"\n", fcgi->configuration->host.buf);

			if (! cherokee_buffer_is_empty(&fcgi->configuration->interpreter)) {

				/* Spawn a new FastCGI server
				 */
				ret = cherokee_fcgi_manager_spawn_srv (n);
				if (unlikely (ret != ret_ok)) return ret_error;

				TRACE (ENTRIES, "Spawning \"%s\"\n", fcgi->configuration->interpreter.buf);

				/* Try to connect again
				 */
				ret = cherokee_fcgi_manager_connect_to_srv (n);
				if (unlikely (ret != ret_ok)) {
					PRINT_ERROR ("Couldn't connect to FCGI server.\n");
					return ret_error;
				}
			} else return ret_error;
		}
	}


	TRACE (ENTRIES, "Manager \"%s\" ready, registering conn\n", fcgi->configuration->host.buf);

	/* Register this connection in the FastCGI manager
	 */
	ret = cherokee_fcgi_manager_register_conn (fcgi->manager_ref, conn, &fcgi->id);
  	if (unlikely (ret != ret_ok)) return ret;

	/* Build the first packets
	 */
	ret = build_initial_packages (fcgi);
	if (unlikely (ret != ret_ok)) return ret;
	
	return ret_ok;
}


static void
send_empty_stdin (cherokee_handler_fastcgi_t *fcgi)
{
	FCGI_BeginRequestRecord  request;
  
	fcgi_build_header (&request.header, FCGI_STDIN, fcgi->id, 0, 0);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));

	TRACE (ENTRIES, "Adding empty FCGI_STDIN, len=%d\n", fcgi->write_buffer.len);
}


static ret_t 
read_fcgi (cherokee_handler_fastcgi_t *fcgi)
{
	cherokee_fcgi_manager_t *fcgim = fcgi->manager_ref;

	/* Read info from the FastCGI
	 */
	return cherokee_fcgi_manager_step (fcgim);
}


static ret_t
send_write_buffer (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t  ret;
	size_t size = 0;

	TRACE (ENTRIES, "Sending: there are %d bytes to be sent\n", fcgi->write_buffer.len);
	
	ret = cherokee_fcgi_manager_send (fcgi->manager_ref, &fcgi->write_buffer, &size);

	TRACE (ENTRIES, "Sending: now, there are %d bytes, had ret=%d\n", fcgi->write_buffer.len, ret);

	switch (ret) {
	case ret_ok:
		if (cherokee_buffer_is_empty (&fcgi->write_buffer))
			return ret_ok;
		return ret_eagain;
	case ret_eagain:
		return ret_eagain;
	case ret_eof:
		return ret_error;
	case ret_error:
		return ret_error;
	default:
		SHOULDNT_HAPPEN;
	}	

	return ret_eagain;
}


static ret_t
send_post (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t                    ret;
	size_t                   size;
	cherokee_connection_t   *conn        = HANDLER_CONN(fcgi);
	cherokee_buffer_t        post_buffer = CHEROKEE_BUF_INIT;
	FCGI_BeginRequestRecord  request;


	TRACE (ENTRIES, "Sending POST %s\n", "");

	/* Send Post
	 */
	switch (fcgi->post_phase) {
	case fcgi_post_init:
		TRACE (ENTRIES, "Read from FCGI, phase %s\n", "init");

		/* Does it have post?
		 */
		if (cherokee_post_is_empty (&conn->post)) {
			send_empty_stdin(fcgi);

			fcgi->post_phase = fcgi_post_finished;
			return ret_ok;
		}

		/* Prepare the stdin header
		 */
		cherokee_post_walk_reset (&conn->post);
		cherokee_post_get_len (&conn->post, &size);
		if (size > 0) {
			fcgi_build_header (&request.header, FCGI_STDIN, fcgi->id, size, 0);
			cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));
			cherokee_buffer_add_buffer (&fcgi->write_buffer, &post_buffer);
			cherokee_buffer_mrproper (&post_buffer);
		}

		fcgi->post_phase = fcgi_post_read;

	case fcgi_post_read:
		TRACE (ENTRIES, "Read from FCGI, phase %s\n", "post read");

		ret = cherokee_post_walk_read (&conn->post, &post_buffer, DEFAULT_READ_SIZE);
		switch (ret) {
		case ret_ok:
			break;
		case ret_error:
		case ret_eagain:
			return ret;
		default:
			RET_UNKNOWN(ret);
			return ret_error;
		}
			
		fcgi->post_phase = fcgi_post_send;

	case fcgi_post_send:
		TRACE (ENTRIES, "Read from FCGI, phase %s\n", "post send");

		if (! cherokee_buffer_is_empty (&fcgi->write_buffer)) { 
			ret = cherokee_fcgi_manager_send (fcgi->manager_ref, &fcgi->write_buffer, &size); 
			
			switch (ret) {
			case ret_ok:
				break;
			case ret_eagain:
				return ret_eagain;
			case ret_eof:
				return ret_error;
			case ret_error:
				return ret_error;
			default:
				SHOULDNT_HAPPEN;
			}

			if (cherokee_buffer_is_empty (&fcgi->write_buffer)) {
				fcgi->post_phase = fcgi_post_read;
				return ret_eagain;
			}
		}
		
	case fcgi_post_finished:
		TRACE (ENTRIES, "Read from FCGI, phase %s\n", "finished");
		break;
	}

	return ret_ok;
}


static ret_t
process_header (cherokee_handler_fastcgi_t *fcgi, cherokee_buffer_t *buf)
{
	cherokee_connection_t *conn   = HANDLER_CONN(fcgi);
	char                  *tmp, *end;

	tmp = buf->buf;
	while (1) {
		if (tmp == NULL || *tmp == 0)
			break;

		end = strstr (tmp, CRLF);
		if (end == NULL)
			end = tmp + strlen (tmp);

		if (strncmp (tmp, "Status: ", 8) == 0) {
			int  real_status;
			char original_byte;

			original_byte = *end;      
			*end = 0;
			tmp += 8;
			real_status = atoi (tmp);
			*end = original_byte;
      
			if (real_status <= 0) {
				conn->error_code = http_internal_error;
				return ret_error;
			}
			conn->error_code = real_status;
		}
		else if (strncmp (tmp, "Location: ", 10) == 0) {
			tmp += 10;
			cherokee_buffer_add (&conn->redirect, tmp, end - tmp);      
		}

		tmp = end + (*end == 0 ? 0 : 2);
	}

	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_add_headers (cherokee_handler_fastcgi_t *fcgi, cherokee_buffer_t *buffer)
{
	ret_t  ret;
	int    len;
	int    end_len;
	char  *content;

	switch (fcgi->phase) {
	case fcgi_phase_init:
		fcgi->phase = fcgi_phase_send_header;

	case fcgi_phase_send_post:
		TRACE (ENTRIES, "Adding headers, phase %s\n", "send POST");

		if (! fcgi->post_sent) {
			ret = send_post(fcgi);
			switch (ret) {
			case ret_ok:
				fcgi->post_phase = fcgi_post_finished;
				fcgi->post_sent  = true;
				break;
			default:
				return ret;
			}
		}

		fcgi->phase = fcgi_phase_send_header;

	case fcgi_phase_send_header:
		TRACE (ENTRIES, "Adding headers, phase %s\n", "send header");

		if (!cherokee_buffer_is_empty (&fcgi->write_buffer)) {
			ret = send_write_buffer (fcgi);
			switch (ret) {
			case ret_ok:
				break;
			case ret_error:
			case ret_eagain:
				return ret;
			default:
				RET_UNKNOWN(ret);
			}
		}

		fcgi->phase = fcgi_phase_read_fcgi;

	case fcgi_phase_read_fcgi:
		TRACE (ENTRIES, "Adding headers, phase %s\n", "read fcgi");

		ret = read_fcgi (fcgi);
		switch (ret) {
		case ret_ok:
		case ret_eof:
		case ret_eof_have_data:
			break;
		case ret_error:
		case ret_eagain:
			return ret;			
		default:
			RET_UNKNOWN(ret);
		}

		if (cherokee_buffer_is_empty (&fcgi->incoming_buffer)) {
			if (fcgi->phase != fcgi_phase_finished)
				return ret_eagain;
			return ret_error;
		}
			
		/* Look the end of headers
		 */ 
		content = strstr (fcgi->incoming_buffer.buf, CRLF CRLF);
		if (content != NULL) {
			end_len = 4;
		} else {
			content = strstr (fcgi->incoming_buffer.buf, "\n\n");
			end_len = 2;
		}
		
		if (content == NULL) {
			return (ret == ret_eof) ? ret_eof : ret_eagain;
		}
		
		/* Copy the header
		 */
		len = content - fcgi->incoming_buffer.buf;	
		
		cherokee_buffer_ensure_size (buffer, len+6);
		cherokee_buffer_add (buffer, fcgi->incoming_buffer.buf, len);
		cherokee_buffer_add (buffer, CRLF, 2);
	
		/* Drop out the headers, we already have a copy
		 */
		cherokee_buffer_move_to_begin (&fcgi->incoming_buffer, len + end_len);

		ret = process_header (fcgi, buffer);
		switch (ret) {
		case ret_ok:
		case ret_eof:
		case ret_eof_have_data:
			break;
		default:
			return ret;
		}

		fcgi->phase = fcgi_phase_finished;

	case fcgi_phase_finished:
		return ret_ok;
	}

	SHOULDNT_HAPPEN;
	return ret_error;
}


ret_t 
cherokee_handler_fastcgi_step (cherokee_handler_fastcgi_t *fcgi, cherokee_buffer_t *buffer)
{
	ret_t ret = ret_ok;

	/* Send remaining information
	 */
	if (!cherokee_buffer_is_empty (&fcgi->incoming_buffer)) {
		cherokee_buffer_add_buffer (buffer, &fcgi->incoming_buffer);
		cherokee_buffer_mrproper (&fcgi->incoming_buffer);

		if (fcgi->phase == fcgi_phase_finished)
			return ret_eof_have_data;

		return ret_ok;
	}

	/* Lets read from the FastCGI server
	 */
	ret = read_fcgi (fcgi); 
	switch (ret) {
	case ret_ok:
	case ret_eof_have_data:
		TRACE (ENTRIES, "Last package, len=%d\n", fcgi->incoming_buffer.len);

		cherokee_buffer_add_buffer (buffer, &fcgi->incoming_buffer);
		cherokee_buffer_mrproper (&fcgi->incoming_buffer);
		return ret;

	case ret_error:
	case ret_eagain:
		return ret;
	default:
		RET_UNKNOWN(ret);
	}
	
	return ret_error;
}


/* Library init function
 */
static cherokee_boolean_t is_init = false;

void  
MODULE_INIT(fastcgi) (cherokee_module_loader_t *loader)
{
#ifdef HAVE_PTHREAD
	extern pthread_mutex_t __fcgi_managers_sem;
#endif

	if (is_init) 
		return;

	PRINT_ERROR_S ("WARNING: The FastCGI is under development, it isn't ready to be used!\n");

	cherokee_table_init(&__fcgi_managers);
	CHEROKEE_MUTEX_INIT (&__fcgi_managers_sem, NULL);

	is_init = true;
}
