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

#define set_env(cgi,key,val,len) \
	set_env_pair (cgi, key, sizeof(key)-1, val, len)


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
	cgi->init_phase          = hcgi_phase_build_headers;
	cgi->script_alias        = NULL;
	cgi->system_env          = NULL;
	cgi->content_length      = 0;
	cgi->got_eof             = false;
	cgi->is_error_handler    = 0;
	cgi->change_user         = 0;

	cherokee_buffer_init (&cgi->executable);
	cherokee_buffer_init (&cgi->param);
	cherokee_buffer_init (&cgi->param_extra);

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
	cherokee_buffer_mrproper (&cgi->executable);

	cherokee_buffer_mrproper (&cgi->param);
	cherokee_buffer_mrproper (&cgi->param_extra);

	return ret_ok;
}


void  
cherokee_handler_cgi_base_add_parameter (cherokee_handler_cgi_base_t *cgi, char *param, cuint_t param_len)
{
	cherokee_buffer_clean (&cgi->param_extra);
	cherokee_buffer_add (&cgi->param_extra, param, param_len);
}


ret_t 
cherokee_handler_cgi_base_build_basic_env (cherokee_handler_cgi_base_t              *cgi, 
					   cherokee_handler_cgi_base_add_env_pair_t  set_env_pair,
					   cherokee_connection_t                    *conn,
					   cherokee_buffer_t                        *tmp)
{
	int           r;
	ret_t         ret;
	char         *p;
	cuint_t       p_len;
	const char   *p_const;

	char remote_ip[CHE_INET_ADDRSTRLEN+1];
	CHEROKEE_TEMP(temp, 32);

	/* Set the basic variables
	 */
	set_env (cgi, "SERVER_SIGNATURE",  "<address>Cherokee web server</address>", 38);
	set_env (cgi, "SERVER_SOFTWARE",   "Cherokee " PACKAGE_VERSION, 9 + (sizeof(PACKAGE_VERSION) - 1));
	set_env (cgi, "GATEWAY_INTERFACE", "CGI/1.1", 7);
	set_env (cgi, "PATH",              "/bin:/usr/bin:/sbin:/usr/sbin", 29);

	/* Servers MUST supply this value to scripts. The QUERY_STRING
	 * value is case-sensitive. If the Script-URI does not include a
	 * query component, the QUERY_STRING metavariable MUST be defined
	 * as an empty string ("").
	 */
	set_env (cgi, "DOCUMENT_ROOT", conn->local_directory.buf, conn->local_directory.len);

	/* The IP address of the client sending the request to the
	 * server. This is not necessarily that of the user agent (such
	 * as if the request came through a proxy).
	 */
	memset (remote_ip, 0, sizeof(remote_ip));
	cherokee_socket_ntop (conn->socket, remote_ip, sizeof(remote_ip)-1);
	set_env (cgi, "REMOTE_ADDR", remote_ip, strlen(remote_ip));

	/* HTTP_HOST and SERVER_NAME. The difference between them is that
	 * HTTP_HOST can include the «:PORT» text, and SERVER_NAME only
	 * the name 
	 */
	cherokee_header_copy_known (conn->header, header_host, tmp);
	if (! cherokee_buffer_is_empty(tmp)) {
		set_env (cgi, "HTTP_HOST", tmp->buf, tmp->len);
		
		p = strchr (tmp->buf, ':');
		if (p != NULL) {
			set_env (cgi, "SERVER_NAME", tmp->buf, p - tmp->buf);
		} else {
			set_env (cgi, "SERVER_NAME", tmp->buf, tmp->len);
		}
	}

	/* Cookies :-)
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Cookie", 6, tmp);
	if (ret == ret_ok) {
		set_env (cgi, "HTTP_COOKIE", tmp->buf, tmp->len);
	}

	/* User Agent
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_user_agent, tmp);
	if (ret == ret_ok) {
		set_env (cgi, "HTTP_USER_AGENT", tmp->buf, tmp->len);
	}

	/* Set referer
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_referer, tmp);
	if (ret == ret_ok) {
		set_env (cgi, "HTTP_REFERER", tmp->buf, tmp->len);
	}

	/* Content-type and Content-lenght (if available) 
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Content-Type", 12, tmp);
	if (ret == ret_ok)
		set_env (cgi, "CONTENT_TYPE", tmp->buf, tmp->len);

	/* If there is a query_string, set the environment variable
	 */
	if (conn->query_string.len > 0) 
		set_env (cgi, "QUERY_STRING", conn->query_string.buf, conn->query_string.len);
	else
		set_env (cgi, "QUERY_STRING", "", 0);

	/* Set the server port
	 */
	r = snprintf (temp, temp_size, "%d", CONN_SRV(conn)->port);
	set_env (cgi, "SERVER_PORT", temp, r);

	/* Set the HTTP version
	 */
	ret = cherokee_http_version_to_string (conn->header->version, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env (cgi, "SERVER_PROTOCOL", (char *)p_const, p_len);

	/* Set the method
	 */
	ret = cherokee_http_method_to_string (conn->header->method, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env (cgi, "REQUEST_METHOD", (char *)p_const, p_len);

	/* Remote user
	 */
	if (conn->validator && !cherokee_buffer_is_empty (&conn->validator->user)) {
		set_env (cgi, "REMOTE_USER", conn->validator->user.buf, conn->validator->user.len);
	} else {
		set_env (cgi, "REMOTE_USER", "", 0);
	}

	/* Set PATH_INFO 
	 */
	if (! cherokee_buffer_is_empty (&conn->pathinfo)) {
		set_env (cgi, "PATH_INFO", conn->pathinfo.buf, conn->pathinfo.len);
	}

	/* Set REQUEST_URI 
	 */
	cherokee_buffer_clean (tmp);
	cherokee_header_copy_request_w_args (conn->header, tmp);
	set_env (cgi, "REQUEST_URI", tmp->buf, tmp->len);

	/* Set HTTPS
	 */
	if (conn->socket->is_tls) 
		set_env (cgi, "HTTPS", "on", 2);
	else 
		set_env (cgi, "HTTPS", "off", 3);

	/* If-None-Match header
	 */
	ret = cherokee_header_get_known (conn->header, header_if_none_match, &p, &p_len);
	if (ret == ret_ok) {
		set_env (cgi, "HTTP_IF_NONE_MATCH", p, p_len);		
	}

	return ret_ok;
}


ret_t 
cherokee_handler_cgi_base_build_envp (cherokee_handler_cgi_base_t *cgi, cherokee_connection_t *conn)
{
	ret_t              ret;
	list_t            *i;
	cuint_t            len = 0;
	char              *p   = NULL;
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
	ret = cherokee_handler_cgi_base_build_basic_env (cgi, cgi->add_env_pair, conn, &tmp);
	if (unlikely (ret != ret_ok)) return ret;

	/* SCRIPT_NAME:
	 * It is the request without the pathinfo if it exists
	 */	
	if (! cgi->script_alias) {
		if (cgi->param.len > 0) {
			/* phpcgi request	
			 */
			p = cgi->param.buf + conn->local_directory.len - 1;
			len = (cgi->param.buf + cgi->param.len) - p;
		} else {
			/* cgi, scgi or fastcgi	
			 */
			p = cgi->executable.buf + conn->local_directory.len - 1;
			len = (cgi->executable.buf + cgi->executable.len) - p;
		}
	}

	cherokee_buffer_clean (&tmp);
	
	if (conn->web_directory.len > 1) 
		cherokee_buffer_add_buffer (&tmp, &conn->web_directory);

	if (len > 0) 
		cherokee_buffer_add (&tmp, p, len);
	
	cgi->add_env_pair (cgi, "SCRIPT_NAME", 11, tmp.buf, tmp.len);


	/* SCRIPT_FILENAME
	 * It depends on the type of CGI (CGI, SCGI o FastCGI):
	 *    http://php.net/reserved.variables
	 */

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
	int                    req_len;
	int                    local_len;
	int                    pathinfo_len;

	/* ScriptAlias: If there is a ScriptAlias directive, it
	 * doesn't need to find the executable file..
	 */
	if (cgi->script_alias != NULL) {
		TRACE (ENTRIES, "Script alias '%s'\n", cgi->script_alias);

		if (stat(cgi->script_alias, &st) == -1) {
			conn->error_code = http_not_found;
			return ret_error;
		}

		cherokee_buffer_add (&cgi->executable, cgi->script_alias, strlen(cgi->script_alias));

		/* Check the path_info even if it uses a  scriptalias. The PATH_INFO 	
		 * is the rest of the substraction of request - configured directory.
		 */
		cherokee_buffer_add (&conn->pathinfo, 
				     conn->request.buf + conn->web_directory.len, 
				     conn->request.len - conn->web_directory.len);
		return ret_ok;
	}

	req_len      = conn->request.len;
	local_len    = conn->local_directory.len;
	pathinfo_len = conn->pathinfo.len;

	/* It is going to concatenate two paths like: 
	 * local_directory = "/usr/share/cgi-bin/", and 
	 * request = "/thing.cgi", so there would be two 
	 * slashes in the middle of the request.
	 */
	if (req_len > 0)
		cherokee_buffer_add (&conn->local_directory, 
				     conn->request.buf + 1, 
				     conn->request.len - 1); 

	if (pathinfo_len <= 0) {
		if (check_filename)
			ret = cherokee_handler_cgi_base_split_pathinfo (cgi, &conn->local_directory, local_len -1, false);
		else
			ret = cherokee_handler_cgi_base_split_pathinfo (cgi, &conn->local_directory, local_len -1,  true);
			
		if (unlikely(ret < ret_ok)) goto bye;
	}

	/* Is the filename set? 
	 */
	if (cgi->executable.len <= 0) {
		cherokee_buffer_add_buffer (&cgi->executable, &conn->local_directory);
		TRACE (ENTRIES, "Executable: '%s'\n", cgi->executable.buf);
	}

	if (check_filename) {
		/* We have to check if the file exists 
		 */
		if (stat (conn->local_directory.buf, &st) == -1) {
			conn->error_code = http_not_found;
			ret = ret_error;
			goto bye;
		}
	}
		
bye:
	if (pathinfo_len)
		cherokee_buffer_drop_endding (&conn->local_directory, req_len - 1);
	else
		cherokee_buffer_drop_endding (&conn->local_directory, req_len - (conn->pathinfo.len + 1));

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
cherokee_handler_cgi_base_add_headers (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *outbuf)
{
	ret_t                  ret;
	int                    len;
	char                  *content;
	int                    end_len;
	cherokee_buffer_t     *inbuf = &cgi->data; 

	/* Read some information
	 */
	ret = cgi->read_from_cgi (cgi, inbuf);
	switch (ret) {
	case ret_ok:
	case ret_eof_have_data:
		break;
		
	case ret_eof:
	case ret_error:
	case ret_eagain:
		return ret;
		
	default:
		RET_UNKNOWN(ret);
		return ret_error;
	}

	/* Look the end of headers
	 */
	content = strstr (inbuf->buf, CRLF CRLF);
	if (content != NULL) {
		end_len = 4;
	} else {
		content = strstr (inbuf->buf, "\n\n");
		end_len = 2;
	}
	
	if (content == NULL) {
		return (cgi->got_eof) ? ret_eof : ret_eagain;
	}

	/* Copy the header
	 */
	len = content - inbuf->buf;	

	cherokee_buffer_ensure_size (outbuf, len+6);
	cherokee_buffer_add (outbuf, inbuf->buf, len);
	cherokee_buffer_add (outbuf, CRLF CRLF, 4);

	/* Drop out the headers, we already have a copy
	 */
	cherokee_buffer_move_to_begin (inbuf, len + end_len);

	/* Parse the header.. it is likely we will have something to do with it.
	 */
	return parse_header (cgi, outbuf);	
}


ret_t 
cherokee_handler_cgi_base_step (cherokee_handler_cgi_base_t *cgi, cherokee_buffer_t *outbuf)
{
	ret_t              ret;
	cherokee_buffer_t *inbuf = &cgi->data; 

	/* If there is some data waiting to be sent in the CGI buffer, move it
	 * to the connection buffer and continue..
	 */
	if (! cherokee_buffer_is_empty (&cgi->data)) {
		TRACE (ENTRIES, "sending stored data: %d bytes\n", cgi->data.len);

		cherokee_buffer_add_buffer (outbuf, &cgi->data);
		cherokee_buffer_clean (&cgi->data);

		if (cgi->got_eof) {
			return ret_eof_have_data;
		}

		return ret_ok;
	}

	/* Read some information from the CGI
	 */
	ret = cgi->read_from_cgi (cgi, inbuf);

	if (inbuf->len > 0) {
		cherokee_buffer_add_buffer (outbuf, inbuf);
		cherokee_buffer_clean (inbuf);
	}

	return ret;
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

	/* Split the string
	 */
	if (pathinfo_len > 0) {
		cherokee_buffer_add (&conn->pathinfo, pathinfo, pathinfo_len);
		cherokee_buffer_drop_endding (buf, pathinfo_len);
	}

	TRACE (ENTRIES, "Pathinfo '%s'\n", conn->pathinfo.buf);

	return ret_ok;
}
