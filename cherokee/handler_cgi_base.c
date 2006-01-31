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
#include "handler_cgi_base.h"

#include "typed_table.h"
#include "socket.h"
#include "list_ext.h"
#include "util.h"

#include "connection-protected.h"
#include "server-protected.h"
#include "header-protected.h"


#define ENTRIES "cgibase"


ret_t 
cherokee_handler_cgi_base_init (cherokee_handler_cgi_base_t              *cgi, 
				cherokee_connection_t                    *conn,
				cherokee_table_t                         *properties,
				cherokee_handler_cgi_base_add_env_pair_t  add_env_pair,
				cherokee_handler_cgi_base_read_from_cgi_t read_from_cgi)
{
	ret_t ret;

	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(cgi), conn);

	/* Supported features
	 */
	HANDLER(cgi)->support = hsupport_maybe_length;

	/* Process the request_string, and build the arguments table..
	 * We'll need it later
	 */
	ret = cherokee_connection_parse_args (conn);
	if (unlikely(ret < ret_ok)) return ret;

	/* Init to default values
	 */
	cgi->init_phase        = hcgi_phase_build_headers;
	cgi->filename          = NULL;
	cgi->parameter         = NULL;
	cgi->script_alias      = NULL;
	cgi->extra_param       = NULL;
	cgi->system_env        = NULL;
	cgi->content_length    = 0;
	cgi->is_error_handler  = 0;
	cgi->change_user       = 0;

	cherokee_buffer_init (&cgi->data);
	cherokee_buffer_ensure_size (&cgi->data, 2*1024);

	/* Virtual methods
	 */
	cgi->add_env_pair  = add_env_pair;
	cgi->read_from_cgi = read_from_cgi;

	/* Read the properties
	 */
	if (properties) {
		cherokee_typed_table_get_str  (properties, "scriptalias",   &cgi->script_alias);
		cherokee_typed_table_get_list (properties, "env",           &cgi->system_env);
		cherokee_typed_table_get_int  (properties, "error_handler", &cgi->is_error_handler);
		cherokee_typed_table_get_int  (properties, "changeuser",    &cgi->change_user);		
	}

	if (cgi->is_error_handler) {
		HANDLER(cgi)->support |= hsupport_error;		
	}
	
	return ret_ok;
}


ret_t 
cherokee_handler_cgi_base_free (cherokee_handler_cgi_base_t *cgi)
{
	cherokee_buffer_mrproper (&cgi->data);
	
	if (cgi->filename != NULL) {
		cherokee_buffer_free (cgi->filename);
		cgi->filename = NULL;
	}
	
	if (cgi->parameter != NULL) {
		cherokee_buffer_free (cgi->parameter);
		cgi->parameter = NULL;
	}

	return ret_ok;
}


void  
cherokee_handler_cgi_base_add_parameter (cherokee_handler_cgi_base_t *cgi, char *param)
{
	cgi->extra_param = param;
}


static ret_t 
build_basic_env (cherokee_handler_cgi_base_t *cgi, 
		 cherokee_connection_t       *conn,
		 cherokee_buffer_t           *tmp)
{
	int                r;
	ret_t              ret;
	char              *p;
	const char        *p_const;
	int                p_len;
	cherokee_handler_cgi_base_add_env_pair_t set_env_pair = cgi->add_env_pair;

	char remote_ip[CHE_INET_ADDRSTRLEN+1];
	CHEROKEE_TEMP(temp, 32);

/*                           0         1         2         3         4         5         6         7
		             01234567890123456789012345678901234567890123456789012345678901234567890 */

	/* Set the basic variables
	 */
	set_env_pair (cgi, "SERVER_SIGNATURE",  16, "<address>Cherokee web server</address>", 38);
	set_env_pair (cgi, "SERVER_SOFTWARE",   15, "Cherokee " PACKAGE_VERSION, 9 + sizeof(PACKAGE_VERSION)-1);
	set_env_pair (cgi, "GATEWAY_INTERFACE", 17, "CGI/1.1", 7);
	set_env_pair (cgi, "PATH",               4, "/bin:/usr/bin:/sbin:/usr/sbin", 29);

	/* Servers MUST supply this value to scripts. The QUERY_STRING
	 * value is case-sensitive. If the Script-URI does not include a
	 * query component, the QUERY_STRING metavariable MUST be defined
	 * as an empty string ("").
	 */
	set_env_pair (cgi, "DOCUMENT_ROOT", 13, conn->local_directory.buf, conn->local_directory.len);

	/* The IP address of the client sending the request to the
	 * server. This is not necessarily that of the user agent (such
	 * as if the request came through a proxy).
	 */
	memset (remote_ip, 0, sizeof(remote_ip));
	cherokee_socket_ntop (conn->socket, remote_ip, sizeof(remote_ip)-1);
	set_env_pair (cgi, "REMOTE_ADDR", 11, remote_ip, strlen(remote_ip));

	/* HTTP_HOST and SERVER_NAME. The difference between them is that
	 * HTTP_HOST can include the «:PORT» text, and SERVER_NAME only
	 * the name 
	 */
	cherokee_header_copy_known (conn->header, header_host, tmp);
	if (! cherokee_buffer_is_empty(tmp)) {
		set_env_pair (cgi, "HTTP_HOST", 9, tmp->buf, tmp->len);

		p = strchr (tmp->buf, ':');
		if (p != NULL) *p = '\0';

		set_env_pair (cgi, "SERVER_NAME", 11, tmp->buf, tmp->len);
	}

	/* Cookies :-)
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Cookie", 6, tmp);
	if (ret == ret_ok) {
		set_env_pair (cgi, "HTTP_COOKIE", 11, tmp->buf, tmp->len);
	}

	/* User Agent
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_user_agent, tmp);
	if (ret == ret_ok) {
		set_env_pair (cgi, "HTTP_USER_AGENT", 15, tmp->buf, tmp->len);
	}

	/* Set referer
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_referer, tmp);
	if (ret == ret_ok) {
		set_env_pair (cgi, "HTTP_REFERER", 12, tmp->buf, tmp->len);
	}

	/* Content-type and Content-lenght (if available) 
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Content-Type", 12, tmp);
	if (ret == ret_ok)
		set_env_pair (cgi, "CONTENT_TYPE", 12, tmp->buf, tmp->len);

	cherokee_buffer_clean (tmp); 
	ret = cherokee_header_copy_known (conn->header, header_content_length, tmp);
	if (ret == ret_ok)
		set_env_pair (cgi, "CONTENT_LENGTH", 14, tmp->buf, tmp->len);

	/* If there is a query_string, set the environment variable
	 */
	if (conn->query_string.len > 0) 
		set_env_pair (cgi, "QUERY_STRING", 12, conn->query_string.buf, conn->query_string.len);
	else
		set_env_pair (cgi, "QUERY_STRING", 12, "", 0);

	/* Set the server port
	 */
	r = snprintf (temp, temp_size, "%d", CONN_SRV(conn)->port);
	set_env_pair (cgi, "SERVER_PORT", 11, temp, r);

	/* Set the HTTP version
	 */
	ret = cherokee_http_version_to_string (conn->header->version, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (cgi, "SERVER_PROTOCOL", 15, (char *)p_const, p_len);

	/* Set the method
	 */
	ret = cherokee_http_method_to_string (conn->header->method, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (cgi, "REQUEST_METHOD", 14, (char *)p_const, p_len);

	/* Remote user
	 */
	if (conn->validator && !cherokee_buffer_is_empty (&conn->validator->user)) {
		set_env_pair (cgi, "REMOTE_USER", 11, conn->validator->user.buf, conn->validator->user.len);
	} else {
		set_env_pair (cgi, "REMOTE_USER", 11, "", 0);
	}

	/* Set the host name
	 */
	if (!cherokee_buffer_is_empty (&conn->host)) {
		p = strchr (conn->host.buf, ':');
		if (p != NULL) *p = '\0';
		
		set_env_pair (cgi, "SERVER_NAME", 11, conn->host.buf, conn->host.len);

		if (p != NULL) *p = ':';
	}

	/* Set PATH_INFO 
	 */
	if (! cherokee_buffer_is_empty (&conn->pathinfo)) {
		set_env_pair (cgi, "PATH_INFO", 9, conn->pathinfo.buf, conn->pathinfo.len);
	}

	/* Set REQUEST_URI 
	 */
	cherokee_buffer_clean (tmp);
	cherokee_header_copy_request_w_args (conn->header, tmp);
	set_env_pair (cgi, "REQUEST_URI", 11, tmp->buf, tmp->len);

	/* Set HTTPS
	 */
	if (conn->socket->is_tls) 
		set_env_pair (cgi, "HTTPS", 5, "on", 2);
	else 
		set_env_pair (cgi, "HTTPS", 5, "off", 3);

	return ret_ok;
}


ret_t 
cherokee_handler_cgi_base_build_envp (cherokee_handler_cgi_base_t *cgi, cherokee_connection_t *conn)
{
	ret_t              ret;
	list_t            *i;
	char              *p;
	cherokee_buffer_t  tmp = CHEROKEE_BUF_INIT;

	/* Add user defined variables at the beginning,
	 * these have precedence..
	 */
	if (cgi->system_env != NULL) {
		list_for_each (i, cgi->system_env) {
			char    *name;
			cuint_t  name_len;
			char    *value;
			
			name     = LIST_ITEM_INFO(i);
			name_len = strlen(name);
			value    = name + name_len + 1;
			
			cgi->add_env_pair (cgi, name, name_len, value, strlen(value));
		}
	}

	/* Add the basic enviroment variables
	 */
	ret = build_basic_env (cgi, conn, &tmp);
	if (unlikely (ret != ret_ok)) return ret;

	/* SCRIPT_NAME:
	 * It is the request without the pathinfo if it exists
	 */
	if (cgi->parameter == NULL) {
		cherokee_buffer_clean (&tmp);
		cherokee_header_copy_request (conn->header, &tmp);

		if (conn->pathinfo.len > 0) {
			cgi->add_env_pair (cgi, "SCRIPT_NAME", 11, tmp.buf, tmp.len - conn->pathinfo.len);
		} else {
			cgi->add_env_pair (cgi, "SCRIPT_NAME", 11, tmp.buf, tmp.len);
		}
	} else {
		p = cgi->parameter->buf + conn->local_directory.len -1;
		cgi->add_env_pair (cgi, "SCRIPT_NAME", 11, p, (cgi->parameter->buf + cgi->parameter->len) - p);
	}

	/* SCRIPT_FILENAME
	 */
	if (cgi->filename) {
		cgi->add_env_pair (cgi, "SCRIPT_FILENAME", 16, cgi->filename->buf, cgi->filename->len);
	}

	/* TODO: Fill the others CGI environment variables
	 * 
	 * http://hoohoo.ncsa.uiuc.edu/cgi/env.html
	 * http://cgi-spec.golux.com/cgi-120-00a.html
	 */
	cherokee_buffer_mrproper (&tmp);
	return ret_ok;
}


ret_t 
cherokee_handler_cgi_base_extract_path (cherokee_handler_cgi_base_t *cgi, cherokee_boolean_t check_filename)
{
	struct stat            st;
	ret_t                  ret  = ret_ok;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	/* ScriptAlias: If there is a ScriptAlias directive, it
	 * doesn't need to find the executable file..
	 */
	if (cgi->script_alias != NULL) {
		TRACE (ENTRIES, "Script alias '%s'\n", cgi->script_alias);

		if (stat(cgi->script_alias, &st) == -1) {
			conn->error_code = http_not_found;
			return ret_error;
		}

		cherokee_buffer_new (&cgi->filename);
		cherokee_buffer_add (cgi->filename, cgi->script_alias, strlen(cgi->script_alias));

		/* Check the path_info even if it uses a
		 * scriptalias. the PATH_INFO is the rest of the
		 * substraction * of request - configured directory.
		 */
		if (cgi->script_alias != NULL) {
			cherokee_buffer_add (&conn->pathinfo, 
					     conn->request.buf + conn->web_directory.len, 
					     conn->request.len - conn->web_directory.len);
		}

		return ret_ok;
	}

	/* Maybe the request contains pathinfo
	 */
	if ((cgi->parameter == NULL) &&
	    cherokee_buffer_is_empty (&conn->pathinfo)) 
	{
		int req_len;
		int local_len;

		/* It is going to concatenate two paths like:
		 * local_directory = "/usr/share/cgi-bin/", and
		 * request = "/thing.cgi", so there will be two
		 * slashes in the middle of the request.
		 */
		req_len   = conn->request.len;
		local_len = conn->local_directory.len;

		if (conn->request.len > 0)
			cherokee_buffer_add (&conn->local_directory, 
					     conn->request.buf + 1, 
					     conn->request.len - 1); 

		if (check_filename)
			ret = cherokee_handler_cgi_base_split_pathinfo (cgi, &conn->local_directory, local_len -1, false);
		else
			ret = cherokee_handler_cgi_base_split_pathinfo (cgi, &conn->local_directory, local_len -1,  true);

		if (unlikely(ret < ret_ok)) goto bye;
		
		/* Is the filename set? 
		 */
		if ((cgi->filename == NULL) && (check_filename))
		{ 
			/* We have to check if the file exists 
			 */
			if (stat (conn->local_directory.buf, &st) == -1) {
				conn->error_code = http_not_found;
				return ret_error;
			}
			
			cherokee_buffer_new (&cgi->filename);
			cherokee_buffer_add_buffer (cgi->filename, &conn->local_directory);
			
			TRACE (ENTRIES, "Filename: '%s'\n", cgi->filename->buf);
		}
		
	bye:
		cherokee_buffer_drop_endding (&conn->local_directory, req_len - 1);
	}

	return ret;
}


static ret_t
parse_header (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *buffer)
{
	char                  *end;
	char                  *end1;
	char                  *end2;
	char                  *begin;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	
	if (cherokee_buffer_is_empty (buffer) || buffer->len <= 5)
		return ret_ok;

	/* It is possible that the header ends with CRLF CRLF
	 * In this case, we have to remove the last two characters
	 */
	if ((buffer->len > 4) &&
	    (strncmp (CRLF CRLF, buffer->buf + buffer->len - 4, 4) == 0))
	{
		cherokee_buffer_drop_endding (buffer, 2);
	}
	
	/* Process the header line by line
	 */
	begin = buffer->buf;
	while (begin != NULL) {
		end1 = strchr (begin, '\r');
		end2 = strchr (begin, '\n');

		end = cherokee_min_str (end1, end2);
		if (end == NULL) break;

		end2 = end;
		while (((*end2 == '\r') || (*end2 == '\n')) && (*end2 != '\0')) end2++;

		if (strncasecmp ("Status: ", begin, 8) == 0) {
			int  code;
			char status[4];

			memcpy (status, begin+8, 3);
			status[3] = '\0';
		
			code = atoi (status);
			if (code <= 0) {
				conn->error_code = http_internal_error;
				return ret_error;
			}

			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);

			conn->error_code = code;			
			continue;
		}

		else if (strncasecmp ("Content-length: ", begin, 16) == 0) {
			cherokee_buffer_t tmp = CHEROKEE_BUF_INIT;

			cherokee_buffer_add (&tmp, begin+16, end - (begin+16));
			cgi->content_length = atoll (tmp.buf);
			cherokee_buffer_mrproper (&tmp);

			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);
		}

		else if (strncasecmp ("Location: ", begin, 10) == 0) {
			cherokee_buffer_add (&conn->redirect, begin+10, end - (begin+10));
			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);
		}

		begin = end2;
	}
	
	return ret_ok;
}


ret_t 
cherokee_handler_cgi_base_add_headers (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *buffer)
{
	ret_t  ret;
	int    len;
	char  *content;
	int    end_len;

	/* Sanity check
	 */
	return_if_fail (buffer != NULL, ret_error);

	/* Read information from the CGI
	 */
	ret = cgi->read_from_cgi (cgi, &cgi->data);

	switch (ret) {
	case ret_ok:
	case ret_eof:
		break;

	case ret_error:
	case ret_eagain:
		return ret;

	default:
		RET_UNKNOWN(ret);
		return ret_error;
	}

	/* Sanity check
	 */
	if (cherokee_buffer_is_empty (&cgi->data)) {
		return ret_error;
	}

	/* Look the end of headers
	 */
	content = strstr (cgi->data.buf, CRLF CRLF);
	if (content != NULL) {
		end_len = 4;
	} else {
		content = strstr (cgi->data.buf, "\n\n");
		end_len = 2;
	}

	if (content == NULL) {
		return (ret == ret_eof) ? ret_eof : ret_eagain;
	}

	/* Copy the header
	 */
	len = content - cgi->data.buf;	

	cherokee_buffer_ensure_size (buffer, len+6);
	cherokee_buffer_add (buffer, cgi->data.buf, len);
	cherokee_buffer_add (buffer, CRLF CRLF, 4);
	
	/* Drop out the headers, we already have a copy
	 */
	cherokee_buffer_move_to_begin (&cgi->data, len + end_len);

	/* Parse the header.. it is likely we will have something to do with it.
	 */
	return parse_header (cgi, buffer);	
}


ret_t 
cherokee_handler_cgi_base_step (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *buffer)
{
	/* Maybe it has some stored data to be send
	 */
	if (!cherokee_buffer_is_empty (&cgi->data)) {
		TRACE (ENTRIES, "sending stored data: %d bytes\n", cgi->data.len);

		cherokee_buffer_add_buffer (buffer, &cgi->data);
		cherokee_buffer_mrproper (&cgi->data);
		return ret_ok;
	}
	
	/* Read some information from the CGI
	 */
	return cgi->read_from_cgi (cgi, buffer);
}


ret_t
cherokee_handler_cgi_base_split_pathinfo (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *buf, int init_pos, int allow_dirs) 
{
	ret_t                  ret;
	char                  *pathinfo;
	int                    pathinfo_len;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	/* Look for the pathinfo
	 */
	ret = cherokee_split_pathinfo (buf, init_pos, allow_dirs, &pathinfo, &pathinfo_len);
	if (ret == ret_not_found) {
		conn->error_code = http_not_found;
		return ret_error;
	}

	/* Build the PathInfo string 
	 */
	cherokee_buffer_add (&conn->pathinfo, pathinfo, pathinfo_len);
	
	/* Drop it out from the original string
	 */
	cherokee_buffer_drop_endding (buf, pathinfo_len);

	TRACE (ENTRIES, "Pathinfo '%s'\n", conn->pathinfo.buf);

	return ret_ok;
}
