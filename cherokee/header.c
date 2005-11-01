/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Christopher Pruden <pruden@dyndns.org>
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
#include "header.h"
#include "header-protected.h"

#include "util.h"

#ifdef HAVE_STRING_H
# include <string.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif


// #define HEADER_INTERNAL_DEBUG

#ifdef HEADER_INTERNAL_DEBUG
# define HEADER_INTERNAL_CHECK(h)       	                                                   \
	do { if (cherokee_buffer_crc32 (h->input_buffer) != h->input_buffer_crc)                   \
		fprintf (stderr, "Header sanity check failed: %s, line %d\n", __FILE__, __LINE__); \
        } while(0)
#else
# define HEADER_INTERNAL_CHECK(h)
#endif



static struct {
	char *name;
	int   len;
} 
known_header_names [] = {
	{"Connection",      10},
	{"Range",            5},
	{"Keep-Alive",      10},
	{"Accept",           6},
	{"Host",             4},
	{"Accept-Encoding", 15},
	{"User-Agent",      10},
	{"Referer",          7},
	{"Location",         8},
	{"Content-Length",  14},
	{"Upgrade",          7},
	{"If-Range",         8}
};


static void
clean_known_headers (cherokee_header_t *hdr)
{
	int i;
	
	for (i=0; i<HEADER_LENGTH; i++)
	{
		hdr->header[i].info_off =  0;
		hdr->header[i].info_len = -1;			 
	}
}

static void
clean_unknown_headers (cherokee_header_t *hdr)
{
	if (hdr->unknowns != NULL) {
		free (hdr->unknowns);
		hdr->unknowns     = NULL;
		hdr->unknowns_len = 0;
	}
}

static void
clean_headers (cherokee_header_t *hdr)
{
	clean_known_headers (hdr);
	clean_unknown_headers (hdr);
}


ret_t 
cherokee_header_new (cherokee_header_t **hdr)
{
	CHEROKEE_NEW_STRUCT(n,header);

	/* Known headers
	 */
	clean_known_headers (n);

	/* Unknown headers
	 */
	n->unknowns     = NULL;
	n->unknowns_len = 0;

	/* Properties
	 */
	n->method   = http_unknown;
	n->version  = http_version_unknown;
	n->response = http_unset; 

	/* Request
	 */
	n->request_off      = 0;
	n->request_len      = 0;
	n->request_args_len = 0;
	
	/* Query string
	 */
	n->query_string_off = 0;
	n->query_string_len = 0;
	
	/* Sanity
	 */
	n->input_buffer     = NULL;
	n->input_buffer_crc = 0;
	n->input_header_len = 0;

	*hdr = n;
	return ret_ok;
}

ret_t 
cherokee_header_free (cherokee_header_t *hdr)
{
	clean_unknown_headers (hdr);
	free (hdr);

	return ret_ok;
}


ret_t 
cherokee_header_clean (cherokee_header_t *hdr)
{
	clean_headers (hdr);	

	hdr->method   = http_unknown;
	hdr->version  = http_version_unknown;
	hdr->response = http_unset; 

	hdr->input_header_len = 0;

	hdr->request_off = 0;
	hdr->request_len = 0;

	hdr->query_string_off = 0;
	hdr->query_string_len = 0;	
	
	return ret_ok;
}


static ret_t 
add_known_header (cherokee_header_t *hdr, cherokee_common_header_t header, off_t info_off, int info_len)
{
	HEADER_INTERNAL_CHECK(hdr);

	hdr->header[header].info_off = info_off;
	hdr->header[header].info_len = info_len;

	return ret_ok;
}


static ret_t 
add_unknown_header (cherokee_header_t *hdr, off_t header_off, off_t info_off, int info_len)
{
	cherokee_header_unknown_entry_t *entry;

	hdr->unknowns_len++;

	hdr->unknowns = realloc (hdr->unknowns, 
				 sizeof(cherokee_header_unknown_entry_t) * hdr->unknowns_len);
				 
	if (hdr->unknowns == NULL) {
		return ret_nomem;
	}
			
	entry = &hdr->unknowns[hdr->unknowns_len-1];
	entry->header_off      = header_off;
	entry->header_info_off = info_off;
	entry->header_info_len = info_len;
	
	return ret_ok;
}


static ret_t
parse_response_first_line (cherokee_header_t *hdr, cherokee_buffer_t *buf, char **next_pos)
{
	char *line  = buf->buf;
	char *begin = line;
	char  tmp[4];
	char *end;

	end = strchr (line, '\r');
	if (end == NULL) {
		return ret_error;
	}

	/* Some security checks
	 */
	if (buf->len < 14) {
		return ret_error;
	}

	/* Return the line endding
	 */
	*next_pos = end + 2;

	/* Example:
	 * HTTP/1.0 403 Forbidden
	 */
	if (strncmp("HTTP/", begin, 5) != 0) {
		return ret_error;
	}


	/* Get the HTTP version
	 */
	switch (begin[7]) {
	case '1':
		hdr->version = http_version_11; 
		break;
	case '0':
		hdr->version = http_version_10; 
		break;
	case '9':
		hdr->version = http_version_09; 
		break;
	default:
		return ret_error;
	}


	/* Read the response code
	 */
	memcpy (tmp, begin+9, 3);
	tmp[3] = '\0';
	hdr->response = atoi (tmp);

	return ret_ok;
}

static ret_t
parse_request_first_line (cherokee_header_t *hdr, cherokee_buffer_t *buf, char **next_pos)
{
	char *line  = buf->buf;
	char *begin = line;
	char *end;
	char *ptr;

	/* Example:
	 * GET / HTTP/1.0
	 */
	end = strchr (line, '\r');
	if (end == NULL) {
		return ret_error;
	}

	/* Some security checks
	 */
	if (buf->len < 14) {
		return ret_error;
	}

	/* Return the line endding
	 */
	*next_pos = end + 2;

	/* Get the method
	 */
	if (strncmp (line, "GET ", 4) == 0) {
		hdr->method = http_get;
		begin += 4;

	} else if (strncmp (line, "POST ", 5) == 0) {
		hdr->method = http_post;
		begin += 5;

	} else if (strncmp (line, "HEAD ", 5) == 0) {
		hdr->method = http_head;
		begin += 5;
		
	} else if (strncmp (line, "OPTIONS ", 8) == 0) {
		hdr->method = http_options;
		begin += 8;
		
	} else {
		return ret_error;
	}


	/* Get the protocol version
	 */	
	switch (end[-1]) {
	case '1':
		if (unlikely(strncmp (end-8, "HTTP/1.1", 8) != 0))
			return ret_error;
		hdr->version = http_version_11; 
		break;
	case '0':
		if (unlikely(strncmp (end-8, "HTTP/1.0", 8) != 0))
			return ret_error;
		hdr->version = http_version_10; 
		break;
	case '9':
		if (unlikely(strncmp (end-8, "HTTP/0.9", 8) != 0))
			return ret_error;
		hdr->version = http_version_09; 
		break;
	default:
		return ret_error;
	}

	/* Skip the HTTP version string: "HTTP/x.y"
	 */
	end -= 9;

	/* Look for the QueryString
	 */
	hdr->request_args_len = end - begin;
	ptr = strchr (begin, '?');

	if ((ptr) && (ptr < end)) {
		end = ptr;
		hdr->query_string_off = ++ptr - buf->buf;
		hdr->query_string_len = (unsigned long) strchr(ptr, ' ') - (unsigned long) ptr;
	} else {
		hdr->query_string_len = 0;
	}

	/* Get the request
	 */
	hdr->request_off = begin - buf->buf;
	hdr->request_len = end - begin;

	/* Check if the request is a full URL
	 */
	begin = buf->buf + hdr->request_off;
	if (strncmp (begin, "http://", 7) == 0) {
		char *dir;
		char *host = begin + 7;

		dir = strchr (host, '/');
		if (dir == NULL) return ret_error;

		/* Add the host header
		 */
		add_known_header (hdr, header_host, begin - buf->buf, dir - host);
		
		/* Fix the URL
		 */
		hdr->request_len -= (dir - begin);
		hdr->request_off = dir - buf->buf;
	}

	return ret_ok;
}


static char *
get_new_line (char *string)
{
	char *end1;
	char *end2;
	char *farest;

	do {
		end1 = strchr (string, '\r');
		end2 = strchr (string, '\n');

		farest = cherokee_min_str (end1, end2);
		if (farest == NULL) return NULL;

		string = farest+1;
	} while (farest[1] == ' ');

	return farest;
}

ret_t 
cherokee_header_parse (cherokee_header_t *hdr, cherokee_buffer_t *buffer,  cherokee_type_header_t type)
{
	ret_t  ret;
	char  *begin = buffer->buf;
	char  *end   = NULL;
	char  *points;
	char  *header_end;

	/* Check the buffer content
	 */
	if ((buffer->buf == NULL) || (buffer->len < 5)) {
		PRINT_ERROR_S ("ERROR: Calling cherokee_header_parse() with a empty header\n");
		return ret_error;
	}

	/* Set the header buffer
	 */
	hdr->input_buffer = buffer;

#ifdef HEADER_INTERNAL_DEBUG
	hdr->input_buffer_crc = cherokee_buffer_crc32 (buffer);
#endif

	/* Look for the header endding 'CRLF CRLF'
	 */
	header_end = strstr (buffer->buf, CRLF CRLF);
	if (header_end == NULL) {
		PRINT_ERROR ("ERROR: Cannot find the end of the header:\n===\n%s===\n", buffer->buf);
		return ret_error;
	}
	
	header_end += 4; 
	hdr->input_header_len = (header_end - buffer->buf);

	/* Parse the speacial first line
	 */
	switch (type) {
	case header_type_request:
		/* Parse request. Something like this:
		 * GET /icons/compressed.png HTTP/1.1\r\n
		 */
		ret = parse_request_first_line (hdr, buffer, &begin);
		if (unlikely(ret < ret_ok)) {
			PRINT_DEBUG ("ERROR: Failed to parse header_type_request:\n===\n%s===\n", buffer->buf);
			return ret;
		}
		break;

	case header_type_response:
		ret = parse_response_first_line (hdr, buffer, &begin);
		if (unlikely(ret < ret_ok)) {
			PRINT_DEBUG ("ERROR: Failed to parse header_type_response:\n===\n%s===\n", buffer->buf);
			return ret;
		}
		break;

	case header_type_basic:
		/* Don't do anything
		 */
		break;

	default:
		SHOULDNT_HAPPEN;
	}


	/* Parse the rest of headers
	 */
	while ((end = get_new_line(begin)) && (end < header_end))
	{	       
		points = strchr (begin, ':');
		if (points == NULL) {
			goto next;
		}
		
		if (end < points +2) {
			goto next;
		}

		if ((hdr->header[header_accept_encoding].info_off == 0) && 
		    (strncasecmp(begin, "Accept-Encoding", 15) == 0))
		{
			ret = add_known_header (hdr, header_accept_encoding, (points+2)-buffer->buf, end-points-2);
		} 
		else if ((hdr->header[header_accept].info_off == 0) && 
		    (strncasecmp(begin, "Accept", 6) == 0)) 
		{
			ret = add_known_header (hdr, header_accept, (points+2)-buffer->buf, end-points-2);
		} 
		else if ((hdr->header[header_host].info_off == 0) && 
			 (strncasecmp(begin, "Host", 4) == 0))
		{
 			ret = add_known_header (hdr, header_host, (points+2)-buffer->buf, end-points-2);
		} 
		else if ((hdr->header[header_range].info_off == 0) && 
			 (strncasecmp(begin, "Range", 5) == 0))
		{
			ret = add_known_header (hdr, header_range, (points+2)-buffer->buf, end-points-2);
		} 
		else if ((hdr->header[header_connection].info_off == 0) && 
			 (strncasecmp(begin, "Connection", 10) == 0))
		{
			ret = add_known_header (hdr, header_connection, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_user_agent].info_off == 0) && 
			 (strncasecmp(begin, "User-Agent", 10) == 0))
		{
			ret = add_known_header (hdr, header_user_agent, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_keepalive].info_off == 0) && 
			 (strncasecmp(begin, "Keep-Alive", 10) == 0))
		{
			ret = add_known_header (hdr, header_keepalive, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_referer].info_off == 0) && 
			 (strncasecmp(begin, "Referer", 7) == 0))
		{
			ret = add_known_header (hdr, header_referer, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_location].info_off == 0) && 
			 (strncasecmp(begin, "Location", 8) == 0))
		{
			ret = add_known_header (hdr, header_location, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_content_length].info_off == 0) && 
			 (strncasecmp(begin, "Content-Length", 14) == 0))
		{
			ret = add_known_header (hdr, header_content_length, (points+2)-buffer->buf, end-points-2);
		}
		else if ((hdr->header[header_upgrade].info_off == 0) && 
			 (strncasecmp(begin, "Upgrade", 7) == 0))
		{
			ret = add_known_header (hdr, header_upgrade, (points+2)-buffer->buf, end-points-2);
		}
		else {
			/* Add a unknown header
			 */
			ret = add_unknown_header (hdr, begin-buffer->buf, (points+2)-buffer->buf, end-points-2);
		}

		if (ret < ret_ok) {
			PRINT_ERROR_S ("ERROR: Failed to add_(un)known_header()\n");
			return ret;
		}

	next:
		while ((*end == '\r') || (*end == '\n')) end++;
		begin = end;
	}
	
	return ret_ok;
}


ret_t 
cherokee_header_get_length (cherokee_header_t *hdr, cuint_t *len)
{
	*len = hdr->input_header_len;
	return ret_ok;
}


ret_t 
cherokee_header_get_unknown (cherokee_header_t *hdr, char *name, int name_len, char **header, int *header_len)
{
	int i;

	HEADER_INTERNAL_CHECK(hdr);

	for (i=0; i < hdr->unknowns_len; i++)
	{
		char *h = hdr->unknowns[i].header_off + hdr->input_buffer->buf;

		if (strncasecmp (h, name, name_len) == 0) {
			*header     = hdr->unknowns[i].header_info_off + hdr->input_buffer->buf;
			*header_len = hdr->unknowns[i].header_info_len;

			return ret_ok;
		}
	}

	return ret_not_found;
}


ret_t
cherokee_header_copy_unknown (cherokee_header_t *hdr, char *name, int name_len, cherokee_buffer_t *buf)
{
	ret_t  ret;
	char  *info;
	int    info_len;

	ret = cherokee_header_get_unknown (hdr, name, name_len, &info, &info_len);
	if (unlikely(ret != ret_ok)) return ret;

	return cherokee_buffer_add (buf, info, info_len);
}


ret_t 
cherokee_header_has_known (cherokee_header_t *hdr, cherokee_common_header_t header)
{
	if (hdr->header[header].info_off != 0) {
		return ret_ok;
	}

	return ret_not_found;
}


ret_t 
cherokee_header_get_known (cherokee_header_t *hdr, cherokee_common_header_t header, char **info, int *info_len)
{
	HEADER_INTERNAL_CHECK(hdr);

	if (hdr->header[header].info_off == 0) {
		return ret_not_found;
	}

	*info     = hdr->header[header].info_off + hdr->input_buffer->buf;
	*info_len = hdr->header[header].info_len;

	return ret_ok;
}


ret_t 
cherokee_header_copy_known (cherokee_header_t *hdr, cherokee_common_header_t header, cherokee_buffer_t *buf)
{
	ret_t ret;
	char *info     = NULL;
	int   info_len = 0;

	ret = cherokee_header_get_known (hdr, header, &info, &info_len);
	if (unlikely(ret != ret_ok)) return ret;

	return cherokee_buffer_add (buf, info, info_len); 
}


ret_t 
cherokee_header_copy_request (cherokee_header_t *hdr, cherokee_buffer_t *request)
{
	ret_t ret;

	if ((hdr->request_off == 0) ||
	    (hdr->request_len <= 0)) {
		return ret_error;
	}

	ret = cherokee_buffer_add (request, hdr->input_buffer->buf + hdr->request_off, hdr->request_len);
	if (unlikely(ret < ret_ok)) return ret;

	return cherokee_buffer_decode (request);
}


ret_t
cherokee_header_copy_request_w_args (cherokee_header_t *hdr, cherokee_buffer_t *request)
{
	ret_t ret;

	if ((hdr->request_off == 0) ||
	    (hdr->request_args_len <= 0)) {
		return ret_error;
	}

	ret = cherokee_buffer_add (request, hdr->input_buffer->buf + hdr->request_off, hdr->request_args_len);
	if (unlikely(ret < ret_ok)) return ret;

	return cherokee_buffer_decode (request);
}


ret_t 
cherokee_header_get_arguments (cherokee_header_t *hdr, cherokee_buffer_t *qstring, cherokee_table_t *arguments)
{
	ret_t ret;
 	char *string, *token; 


	if ((hdr->query_string_off == 0) ||
	    (hdr->query_string_len <= 0)) 
	{
		return ret_ok;
	}

	ret = cherokee_buffer_add (qstring, hdr->input_buffer->buf + hdr->query_string_off, hdr->query_string_len); 
	if (unlikely(ret < ret_ok)) return ret;

	string = qstring->buf;

	while ((token = (char *) strsep(&string, "&")) != NULL)
	{
		char *equ, *key, *val;

		if (token == NULL) continue;

		if ((equ = strchr(token, '=')))
		{
			*equ = '\0';

			key = token;
			val = equ+1;

			cherokee_table_add (arguments, key, strdup(val));

			*equ = '=';
		} else {
			cherokee_table_add (arguments, token, NULL);
		}

		/* UGLY hack, part 1:
		 * It restore the string modified by the strtok() function
		 */
		token[strlen(token)] = '&';
	}

	/* UGLY hack, part 2:
	 */
	qstring->buf[qstring->len] = '\0';
	return ret_ok;
}


ret_t 
cherokee_header_copy_method (cherokee_header_t *hdr, cherokee_buffer_t *buf)
{
	ret_t       ret;
	const char *tmp;
	int         len;

	ret = cherokee_http_method_to_string (HDR_METHOD(hdr), &tmp, &len);
	if (unlikely(ret != ret_ok)) return ret;

	return cherokee_buffer_add (buf, (char *)tmp, len);
}


ret_t 
cherokee_header_copy_version (cherokee_header_t *hdr, cherokee_buffer_t *buf)
{
	ret_t       ret;
	const char *tmp;
	int         len;

	ret = cherokee_http_version_to_string (HDR_METHOD(hdr), &tmp, &len);
	if (unlikely(ret != ret_ok)) return ret;

	return cherokee_buffer_add (buf, (char *)tmp, len);
}


ret_t 
cherokee_header_get_number (cherokee_header_t *hdr, int *ret_num)
{
	int   i;
	int num;

	/* Unknown headers
	 */
	num = hdr->unknowns_len;
	
	/* Known headers
	 */
	for (i=0; i<HEADER_LENGTH; i++)
	{
		if (hdr->header[i].info_off != 0) {
			num++;
		}
	}

	/* Retur the number
	 */
	*ret_num = num;
	return ret_ok;
}


ret_t 
cherokee_header_foreach (cherokee_header_t *hdr, cherokee_header_foreach_func_t func, 
			 cherokee_buffer_t *name, cherokee_buffer_t *cont, void *param)
{
	ret_t  ret;
	char  *header_name;
	int    header_len, i;

	/* Known headers
	 */
	for (i=0; i<HEADER_LENGTH; i++)
	{
		if (hdr->header[i].info_off != 0) {
			header_name = known_header_names[i].name;
			header_len  = known_header_names[i].len;

			cherokee_buffer_clean (name);
			cherokee_buffer_clean (cont);

			cherokee_buffer_add (name, header_name, header_len);
			cherokee_header_copy_known (hdr, i, cont);

			ret = func (name, cont, param);
			if (unlikely(ret < ret_ok)) return ret;
		}
	}	

	/* Unknown headers
	 */
	for (i=0; i < hdr->unknowns_len; i++)
	{
		header_name = hdr->unknowns[i].header_off + hdr->input_buffer->buf;
		header_len = strchr (header_name, ':') - header_name;

		cherokee_buffer_clean (name);
		cherokee_buffer_clean (cont);

		cherokee_buffer_add(name, header_name, header_len);
		cherokee_buffer_add(cont, 
				hdr->unknowns[i].header_info_off + hdr->input_buffer->buf,
				hdr->unknowns[i].header_info_len);

		ret = func(name, cont, param);
		if (unlikely(ret < ret_ok)) return ret;
	}

	return ret_ok;
}


ret_t 
cherokee_header_has_header (cherokee_header_t *hdr, cherokee_buffer_t *buffer, int tail_len)
{
	int   tail;
	char *start;
	
	/* Too few information?
	 * len("GET / HTTP/1.0" CRLF CRLF) = 18
	 */
	if (buffer->len < 18) {
		return ret_deny;
	}

	/* Look for the starting point
	 */
	tail  = (tail_len < buffer->len) ? tail_len : buffer->len;
	start = buffer->buf + (buffer->len - tail);		

	/* It could be a partial header, or maybe a POST request
	 */
	return (strstr(start, CRLF CRLF) != NULL) ? ret_ok : ret_error;
}
