/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee Benchmarker
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
#include "url.h"

#include <strings.h>
#include <netinet/in.h>


ret_t 
cherokee_url_new  (cherokee_url_t **url)
{
	   ret_t ret;
	   CHEROKEE_NEW_STRUCT(n, url);

	   /* New buffer objects
	    */
	   ret = cherokee_buffer_new (&n->host);
	   if (unlikely(ret < ret_ok)) return ret;

	   ret = cherokee_buffer_new (&n->request);
	   if (unlikely(ret < ret_ok)) return ret;

	   /* Set default values
	    */
	   n->port = 80;

	   /* Return the object
	    */
	   *url = n;
	   return ret_ok;
}


ret_t 
cherokee_url_free (cherokee_url_t *url)
{
	if (url->host != NULL) {
		cherokee_buffer_free (url->host);
		url->host = NULL;
	}
	
	if (url->request != NULL) {
		cherokee_buffer_free (url->request);
		url->request = NULL;
	}
	
	free (url);
	return ret_ok;
}


static ret_t
parse_protocol (cherokee_url_t *url, char *string, int *len)
{
	/* Drop "http://"
	 */
	if (strncasecmp("http://", string, 7) == 0) {
		url->protocol = http;
		*len          = 7;

		return ret_ok;
	}

	/* Drop "https://"
	 */
	if (strncasecmp("https://", string, 8) == 0) {
		url->protocol = https;
		*len          = 8;

		return ret_ok;
	}

	return ret_error;
}


static ret_t 
cherokee_url_parse_ptr (cherokee_url_t *url, char *url_string)
{
	int    len;
	ret_t  ret;
	char  *port;
	char  *slash;
	char  *server;
	
	/* Drop protocol, if exists..
	 */
	ret = parse_protocol (url, url_string, &len);
	if (unlikely(ret != ret_ok)) return ret;

	/* Split the host/request
	 */
	server = url_string + len;
	len    = strlen (server);
	slash  = strpbrk (server, "/\\");
	
	if (slash == NULL) {
		cherokee_buffer_add (url->request, "/", 1);
		cherokee_buffer_add (url->host, server, len);
	} else {
		cherokee_buffer_add (url->request, slash, len-(slash-server));
		cherokee_buffer_add (url->host, server, slash-server);
	}

	/* Drop up the port, if exists..
	 */
	port = strchr (url->host->buf, ':');
	if (port != NULL) {

		/* Read port number
		 */
		if (slash != NULL) *slash = '\0';
		URL_PORT(url) = atoi (port+1);
		if (slash != NULL) *slash =  '/';

		/* .. and remove it
		 */
		ret = cherokee_buffer_drop_endding (url->host, strlen(port));
		if (unlikely(ret < ret_ok)) return ret;
	}
	
#if 0
	cherokee_url_print (url);
#endif
	   
	return ret_ok;
}


ret_t 
cherokee_url_parse (cherokee_url_t *url, cherokee_buffer_t *string)
{
	if (cherokee_buffer_is_empty(string)) {
		return ret_error;
	}

	return cherokee_url_parse_ptr (url, string->buf);
}


ret_t 
cherokee_url_build_string (cherokee_url_t *url, cherokee_buffer_t *buf)
{
	cherokee_buffer_add_buffer (buf, url->host);

	if (url->port != 80) {
		cherokee_buffer_add_va (buf, ":%d", url->port);
	}

	cherokee_buffer_add_buffer (buf, url->request);

	return ret_ok;
}


ret_t 
cherokee_url_print (cherokee_url_t *url)
{
	printf ("Host:    %s\n", url->host->buf);
	printf ("Request: %s\n", url->request->buf);
	printf ("Port:    %d\n", url->port);

	return ret_ok;
}
