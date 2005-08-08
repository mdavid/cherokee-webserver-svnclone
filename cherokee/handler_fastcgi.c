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
#include "handler_fastcgi.h"
#include "fastcgi.h"
#include "connection.h"
#include "connection-protected.h"
#include "thread.h"


cherokee_module_info_t cherokee_fastcgi_info = {
	cherokee_handler,               /* type     */
	cherokee_handler_fastcgi_new    /* new func */
};


/* Global managers
 */
static cherokee_table_t *__global_fastcgi_managers;
#ifdef HAVE_PTHREAD
static pthread_mutex_t   __global_fastcgi_managers_lock; 
#endif



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
	n->host_ref        = NULL;
	n->interpreter_ref = NULL;
	
	cherokee_buffer_init (&n->write_buffer);
	cherokee_buffer_init (&n->incoming_buffer);
	cherokee_buffer_init (&n->environment);

	if (properties) {
		cherokee_typed_table_get_str (properties, "server", &n->host_ref);
		cherokee_typed_table_get_str (properties, "interpreter", &n->interpreter_ref);
	}

	/* Return
	 */
	*hdl = HANDLER(n);
	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_free (cherokee_handler_fastcgi_t *hdl)
{
	cherokee_fcgi_manager_unregister_conn (hdl->manager_ref, HANDLER_CONN(hdl));

	cherokee_buffer_mrproper (&hdl->write_buffer);
	cherokee_buffer_mrproper (&hdl->incoming_buffer);
	cherokee_buffer_mrproper (&hdl->environment);

	return ret_ok;
}




static void
add_env_pair (cherokee_buffer_t *buf, 
	      char *key, int key_len,
	      char *val, int val_len)
{
	int len;

	len  = key_len + val_len;
        len += key_len > 127 ? 4 : 1;
        len += val_len > 127 ? 4 : 1;

	cherokee_buffer_ensure_size (buf, buf->len + key_len + val_len);

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
build_initial_packages (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t                    ret;
	cherokee_buffer_t        tmp = CHEROKEE_BUF_INIT;
	cherokee_connection_t   *conn;
	FCGI_BeginRequestRecord  request;

	conn = HANDLER_CONN(fcgi);


	/* FCGI_BEGIN_REQUEST
	 */
	fcgi_build_header (&request.header, FCGI_BEGIN_REQUEST, fcgi->id, sizeof(request.body), 0);
	fcgi_build_request_body (&request.body);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request, sizeof(FCGI_BeginRequestRecord));

	/* Add enviroment variables
	 */
	ret = cherokee_cgi_build_basic_env (conn, (cherokee_cgi_set_env_pair_t) add_env_pair, &tmp, &fcgi->write_buffer);
	if (unlikely (ret != ret_ok)) return ret;

	cherokee_buffer_mrproper (&tmp);

	fcgi_build_header (&request.header, FCGI_PARAMS, fcgi->id, tmp.size, 0);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));
	cherokee_buffer_add_buffer (&fcgi->write_buffer, &tmp);

	/* There aren't more parameters
	 */
	fcgi_build_header (&request.header, FCGI_PARAMS, fcgi->id, 0, 0);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));

	/* Stdin
	 */
	fcgi_build_header (&request.header, FCGI_STDIN, fcgi->id, 0, 0);
	cherokee_buffer_add (&fcgi->write_buffer, (void *)&request.header, sizeof(FCGI_Header));

	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_init (cherokee_handler_fastcgi_t *fcgi)
{
	ret_t                  ret;
	size_t                 sent   = 0;
	cherokee_connection_t *conn   = HANDLER_CONN(fcgi);

	if (fcgi->host_ref == NULL) {
		PRINT_ERROR_S ("ERROR: FastCGI without Host\n");
		return ret_error;
	}

	/* Look for the FCGI managers table
	 */
	CHEROKEE_MUTEX_LOCK (&__global_fastcgi_managers_lock);

	ret = cherokee_table_get (__global_fastcgi_managers, fcgi->host_ref, (void **)&fcgi->manager_ref);
	if (ret == ret_not_found) {
		cherokee_fcgi_manager_t *n;

		if (fcgi->interpreter_ref == NULL) {
			PRINT_ERROR_S ("Could connect to FastCGI server and hadn't interpreter to launch\n");
			return ret_error;
		}

		/* Create a new manager object
		 */
		ret = cherokee_fcgi_manager_new (&n);
		if (unlikely (ret != ret_ok)) return ret;
		
                /* Assign the object to that path
		 */
		ret = cherokee_table_add (__global_fastcgi_managers, fcgi->host_ref, n);
		if (unlikely (ret != ret_ok)) return ret;
		fcgi->manager_ref = n;

		/* Launch a new FastCGI server and connect to it
		 */
		ret = cherokee_fcgi_manager_spawn_srv (n, fcgi->interpreter_ref);
		if (unlikely (ret != ret_ok)) return ret;

		ret = cherokee_fcgi_manager_connect_to_srv (n, fcgi->host_ref);
		if (unlikely (ret != ret_ok)) return ret_error;
	}

	CHEROKEE_MUTEX_UNLOCK (&__global_fastcgi_managers_lock);


	/* Register this connection in the FastCGI manager
	 */
	ret = cherokee_fcgi_manager_register_conn (fcgi->manager_ref, conn, &fcgi->id);
	if (unlikely (ret != ret_ok)) return ret;

	/* Send the first packet
	 */
	ret = build_initial_packages (fcgi);
	if (unlikely (ret != ret_ok)) return ret;
	
	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_step (cherokee_handler_fastcgi_t *fcgi, cherokee_buffer_t *buffer)
{
	ret_t ret;
	size_t done;

	return_if_fail (fcgi->manager_ref != NULL, ret_error);


	printf ("cherokee_handler_fastcgi_step: begin\n");
	
	/* It has something to send
	 */
	if (! cherokee_buffer_is_empty (&fcgi->write_buffer)) {
		ret = cherokee_fcgi_manager_send (fcgi->manager_ref, &fcgi->write_buffer, &done);
		printf ("cherokee_handler_fastcgi_step: !empty, send: %d\n", ret);
		switch (ret) {
		case ret_ok:
			if (cherokee_buffer_is_empty (&fcgi->write_buffer))
				return ret_eagain;

			return ret_ok;

		case ret_eagain:
			return ret_eagain;

		case ret_eof:
		case ret_error:
			return ret_error;
		default:
			SHOULDNT_HAPPEN;
		}
	}

	/* Lets read from the FastCGI server
	 * As side effect it could update more connections in this call
	 */
	ret = cherokee_fcgi_manager_step (fcgi->manager_ref);
	printf ("cherokee_handler_fastcgi_step: manager_step: %d\n", ret);

	// To continue..

	return ret_ok;
}


ret_t 
cherokee_handler_fastcgi_add_headers (cherokee_handler_fastcgi_t *hdl, cherokee_buffer_t *buffer)
{
	return ret_ok;
}


/* Library init function
 */

void  
fastcgi_init (cherokee_module_loader_t *loader)
{
	PRINT_ERROR_S ("WARNING: The FastCGI is under development, it isn't ready to be used!\n");

	/* Init module related stuff
	 */
	cherokee_table_new (&__global_fastcgi_managers);
	CHEROKEE_MUTEX_INIT(&__global_fastcgi_managers_lock, NULL);
}
