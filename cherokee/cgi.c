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


#include "cgi.h"
#include "socket.h"
#include "connection.h"
#include "connection-protected.h"
#include "server.h"
#include "server-protected.h"
#include "header.h"
#include "header-protected.h"


ret_t 
cherokee_cgi_build_basic_env (cherokee_connection_t       *conn,
			      cherokee_cgi_set_env_pair_t  set_env_pair, 
			      cherokee_buffer_t           *tmp,
			      void                        *param)
{
	int                r;
	ret_t              ret;
	char              *p;
	const char        *p_const;
	int                p_len;

	char remote_ip[CHE_INET_ADDRSTRLEN+1];
	CHEROKEE_TEMP(temp, 32);

/*                           0         1         2         3         4         5         6         7
		             01234567890123456789012345678901234567890123456789012345678901234567890 */

	/* Set the basic variables
	 */
	set_env_pair (param, "SERVER_SIGNATURE",  16, "<address>Cherokee web server</address>", 38);
	set_env_pair (param, "SERVER_SOFTWARE",   15, "Cherokee " PACKAGE_VERSION, 9 + sizeof(PACKAGE_VERSION));
	set_env_pair (param, "GATEWAY_INTERFACE", 17, "CGI/1.1", 7);
	set_env_pair (param, "PATH",               4, "/bin:/usr/bin:/sbin:/usr/sbin", 29);

	/* Servers MUST supply this value to scripts. The QUERY_STRING
	 * value is case-sensitive. If the Script-URI does not include a
	 * query component, the QUERY_STRING metavariable MUST be defined
	 * as an empty string ("").
	 */
	set_env_pair (param, "DOCUMENT_ROOT", 13, conn->local_directory.buf, conn->local_directory.len);

	/* The IP address of the client sending the request to the
	 * server. This is not necessarily that of the user agent (such
	 * as if the request came through a proxy).
	 */
	memset (remote_ip, 0, sizeof(remote_ip));
	cherokee_socket_ntop (conn->socket, remote_ip, sizeof(remote_ip)-1);
	set_env_pair (param, "REMOTE_ADDR", 11, remote_ip, strlen(remote_ip));

	/* HTTP_HOST and SERVER_NAME. The difference between them is that
	 * HTTP_HOST can include the «:PORT» text, and SERVER_NAME only
	 * the name 
	 */
	cherokee_header_copy_known (conn->header, header_host, tmp);
	if (! cherokee_buffer_is_empty(tmp)) {
		set_env_pair (param, "HTTP_HOST", 9, tmp->buf, tmp->len);

		p = strchr (tmp->buf, ':');
		if (p != NULL) *p = '\0';

		set_env_pair (param, "SERVER_NAME", 11, tmp->buf, tmp->len);
	}

	/* Cookies :-)
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Cookie", 6, tmp);
	if (ret == ret_ok) {
		set_env_pair (param, "HTTP_COOKIE", 11, tmp->buf, tmp->len);
	}

	/* User Agent
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_user_agent, tmp);
	if (ret == ret_ok) {
		set_env_pair (param, "HTTP_USER_AGENT", 15, tmp->buf, tmp->len);
	}

	/* Set referer
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_referer, tmp);
	if (ret == ret_ok) {
		set_env_pair (param, "HTTP_REFERER", 12, tmp->buf, tmp->len);
	}

	/* Content-type and Content-lenght (if available) 
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Content-Type", 12, tmp);
	if (ret == ret_ok)
		set_env_pair (param, "CONTENT_TYPE", 12, tmp->buf, tmp->len);

	cherokee_buffer_clean (tmp); 
	ret = cherokee_header_copy_known (conn->header, header_content_length, tmp);
	if (ret == ret_ok)
		set_env_pair (param, "CONTENT_LENGTH", 14, tmp->buf, tmp->len);

	/* If there is a query_string, set the environment variable
	 */
	if (conn->query_string.len > 0) 
		set_env_pair (param, "QUERY_STRING", 12, conn->query_string.buf, conn->query_string.len);
	else
		set_env_pair (param, "QUERY_STRING", 12, "", 0);

	/* Set the server port
	 */
	r = snprintf (temp, temp_size, "%d", CONN_SRV(conn)->port);
	set_env_pair (param, "SERVER_PORT", 11, temp, r);

	/* Set the HTTP version
	 */
	ret = cherokee_http_version_to_string (conn->header->version, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (param, "SERVER_PROTOCOL", 15, (char *)p_const, p_len);

	/* Set the method
	 */
	ret = cherokee_http_method_to_string (conn->header->method, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (param, "REQUEST_METHOD", 14, (char *)p_const, p_len);

	/* Remote user
	 */
	if (conn->validator && !cherokee_buffer_is_empty (&conn->validator->user)) {
		set_env_pair (param, "REMOTE_USER", 11, conn->validator->user.buf, conn->validator->user.len);
	} else {
		set_env_pair (param, "REMOTE_USER", 11, "", 0);
	}

	/* Set the host name
	 */
	if (!cherokee_buffer_is_empty (&conn->host)) {
		p = strchr (conn->host.buf, ':');
		if (p != NULL) *p = '\0';
		
		set_env_pair (param, "SERVER_NAME", 11, conn->host.buf, conn->host.len);

		if (p != NULL) *p = ':';
	}

	/* Set PATH_INFO 
	 */
	if (! cherokee_buffer_is_empty (&conn->pathinfo)) {
		set_env_pair (param, "PATH_INFO", 9, conn->pathinfo.buf, conn->pathinfo.len);
	}

	/* Set REQUEST_URI 
	 */
	cherokee_buffer_clean (tmp);
	cherokee_header_copy_request_w_args (conn->header, tmp);
	set_env_pair (param, "REQUEST_URI", 11, tmp->buf, tmp->len);

	/* Set HTTPS
	 */
	if (conn->socket->is_tls) 
		set_env_pair (param, "HTTPS", 5, "on", 2);
	else 
		set_env_pair (param, "HTTPS", 5, "off", 3);

	return ret_ok;
}
