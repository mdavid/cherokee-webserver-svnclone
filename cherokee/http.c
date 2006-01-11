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
#include "http.h"

#include <stdio.h>

#define entry(name,name_str)                       \
	    case name:	                           \
	         if (len) *len = sizeof(name_str); \
	         *str = name_str;                  \
 		 return ret_ok;

ret_t 
cherokee_http_method_to_string (cherokee_http_method_t method, const char **str, int *len)
{
	switch (method) {
		/* HTTP 1.1 methods
		 */
		entry (http_get, "GET");
		entry (http_post, "POST");
		entry (http_head, "HEAD");
		entry (http_put, "PUT");
		entry (http_options, "PUT");
		entry (http_delete, "DELETE");
		entry (http_trace, "TRACE");
		entry (http_connect, "CONNECT");

		/* WebDAV methods
		 */
		entry (http_copy, "COPY");
		entry (http_lock, "LOCK");
		entry (http_mkcol, "MKCOL");
		entry (http_move, "MOVE");
		entry (http_notify, "NOTIFY");
		entry (http_poll, "POLL");
		entry (http_propfind, "PROPFIND");
		entry (http_proppatch, "PROPPATCH");
		entry (http_search, "SEARCH");
		entry (http_subscribe, "SUBSCRIBE");
		entry (http_unlock, "UNLOCK");
		entry (http_unsubscribe, "UNSUBSCRIBE");

	default:
		break;
	};

	if (len) *len = 7;
	*str = "UNKNOWN";
	return ret_error;
}


ret_t 
cherokee_http_version_to_string (cherokee_http_version_t version, const char **str, int *len)
{
	switch (version) {
		entry (http_version_11, "HTTP/1.1");
		entry (http_version_10, "HTTP/1.0");
		entry (http_version_09, "HTTP/0.9");
	default:
		break;
	};

	if (len) *len = 12;
	*str = "HTTP/Unknown";
	return ret_error;
}


ret_t 
cherokee_http_code_to_string (cherokee_http_t code, const char **str)
{
	switch (code) {
	case http_ok:  	                 *str = http_ok_string; break;
	case http_accepted:              *str = http_accepted_string; break;
	case http_partial_content:       *str = http_partial_content_string; break;
	case http_bad_request:           *str = http_bad_request_string; break;
	case http_access_denied:         *str = http_access_denied_string; break;
	case http_not_found:             *str = http_not_found_string; break;
	case http_internal_error:        *str = http_internal_error_string; break;
	case http_moved_permanently:     *str = http_moved_permanently_string; break;
	case http_moved_temporarily:     *str = http_moved_temporarily_string; break;
	case http_unauthorized:          *str = http_unauthorized_string; break;
	case http_not_modified:          *str = http_not_modified_string; break;
	case http_length_required:       *str = http_length_required_string; break;
	case http_request_uri_too_long:  *str = http_request_uri_too_long_string; break;
	case http_range_not_satisfiable: *str = http_range_not_satisfiable_string; break;
	case http_upgrade_required:      *str = http_upgrade_required_string; break;
	case http_continue:              *str = http_continue_string; break;
	case http_switching_protocols:   *str = http_switching_protocols_string; break;
	default:
		*str = "Unknown error";
		return ret_error;
	}

	return ret_ok;
}

ret_t 
cherokee_http_code_copy (cherokee_http_t code, cherokee_buffer_t *buf)
{
	switch (code) {
	case http_ok:
		return cherokee_buffer_add (buf, http_ok_string, 6);
	case http_accepted:
		return cherokee_buffer_add (buf, http_accepted_string, 12);
	case http_partial_content:
		return cherokee_buffer_add (buf, http_partial_content_string, 19);		
	case http_bad_request:
		return cherokee_buffer_add (buf, http_bad_request_string, 15);
	case http_access_denied:
		return cherokee_buffer_add (buf, http_access_denied_string, 13);
	case http_not_found:
		return cherokee_buffer_add (buf, http_not_found_string, 13);
	case http_internal_error:
		return cherokee_buffer_add (buf, http_internal_error_string, 25);
	case http_moved_permanently: 
		return cherokee_buffer_add (buf, http_moved_permanently_string, 21);
	case http_moved_temporarily:
		return cherokee_buffer_add (buf, http_moved_temporarily_string, 21);	
	case http_unauthorized:
		return cherokee_buffer_add (buf, http_unauthorized_string, 26);
	case http_not_modified:
		return cherokee_buffer_add (buf, http_not_modified_string, 16);	
	case http_length_required:
		return cherokee_buffer_add (buf, http_length_required_string, 19);	
	case http_request_uri_too_long:
		return cherokee_buffer_add (buf, http_request_uri_too_long_string, 24);	
	case http_range_not_satisfiable:
		return cherokee_buffer_add (buf, http_range_not_satisfiable_string, 37);
	case http_upgrade_required:
		return cherokee_buffer_add (buf, http_upgrade_required_string, 20);
	case http_continue:
		return cherokee_buffer_add (buf, http_continue_string, 12);
	case http_switching_protocols:
		return cherokee_buffer_add (buf, http_switching_protocols_string, 23);
	default:
		break;
	}

	SHOULDNT_HAPPEN;
	return ret_error;
}

