/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2009 Alvaro Lopez Ortega
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */ 

#include "common-internal.h"
#include "logger_ncsa.h"

#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else 
# include <time.h>
#endif

#include "util.h"
#include "connection.h"
#include "connection-protected.h"
#include "header.h"
#include "header-protected.h"
#include "server.h"
#include "server-protected.h"
#include "connection.h"
#include "module.h"


/* Plug-in initialization
 */
PLUGIN_INFO_LOGGER_EASIEST_INIT (ncsa);


ret_t
cherokee_logger_ncsa_new (cherokee_logger_t         **logger,
			  cherokee_virtual_server_t  *vsrv,
			  cherokee_config_node_t     *config)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, logger_ncsa);

	/* Init the base class object
	 */
	cherokee_logger_init_base (LOGGER(n), PLUGIN_INFO_PTR(ncsa), config);

	MODULE(n)->init             = (logger_func_init_t) cherokee_logger_ncsa_init;
	MODULE(n)->free             = (logger_func_free_t) cherokee_logger_ncsa_free;

	LOGGER(n)->flush            = (logger_func_flush_t) cherokee_logger_ncsa_flush;
	LOGGER(n)->reopen           = (logger_func_reopen_t) cherokee_logger_ncsa_reopen;
	LOGGER(n)->get_error_writer = (logger_func_get_error_writer_t)  cherokee_logger_ncsa_get_error_writer;
	LOGGER(n)->write_error      = (logger_func_write_error_t)  cherokee_logger_ncsa_write_error;
	LOGGER(n)->write_access     = (logger_func_write_access_t) cherokee_logger_ncsa_write_access;

	n->writer_access = NULL;
	n->writer_error  = NULL;

	ret = cherokee_logger_ncsa_init_base (n, VSERVER(vsrv), config);
	if (unlikely(ret < ret_ok)) return ret;

	/* Return the object
	 */
	*logger = LOGGER(n);
	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_init_base (cherokee_logger_ncsa_t    *logger,
				cherokee_virtual_server_t *vsrv,
				cherokee_config_node_t    *config)
{
	ret_t                   ret;
	long                   *this_timezone = NULL;
	cherokee_config_node_t *subconf;

	/* Get the timezone reference and compute tz offset
	 */
	this_timezone = cherokee_get_timezone_ref();
	logger->tz = - (*this_timezone / 60);
	logger->now_time = (time_t) -1;

	/* Init the local buffers
	 */
	cherokee_buffer_init (&logger->now_dtm);
	cherokee_buffer_init (&logger->referer);
	cherokee_buffer_init (&logger->useragent);
	cherokee_buffer_ensure_size (&logger->now_dtm,   64);
	cherokee_buffer_ensure_size (&logger->referer, 1024);
	cherokee_buffer_ensure_size (&logger->useragent, 512);

	/* Init the logger writers
	 */
	ret = cherokee_config_node_get (config, "access", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_server_get_log_writer (VSERVER_SRV(vsrv), subconf, &logger->writer_access);
		if (ret != ret_ok) {
			return ret_error;
		}
	}

	ret = cherokee_config_node_get (config, "error", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_server_get_log_writer (VSERVER_SRV(vsrv), subconf, &logger->writer_error);
		if (ret != ret_ok) {
			return ret_error;
		}
	}

	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_init (cherokee_logger_ncsa_t *logger)
{
	ret_t ret;

	ret = cherokee_logger_writer_open (logger->writer_access);
	if (ret != ret_ok) return ret;

	ret = cherokee_logger_writer_open (logger->writer_error);
	if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t
cherokee_logger_ncsa_free (cherokee_logger_ncsa_t *logger)
{
	cherokee_buffer_mrproper (&logger->now_dtm);
	cherokee_buffer_mrproper (&logger->referer);
	cherokee_buffer_mrproper (&logger->useragent);

	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_flush (cherokee_logger_ncsa_t *logger)
{
	return cherokee_logger_writer_flush (logger->writer_access, false);
}


static ret_t
build_log_string (cherokee_logger_ncsa_t *logger, cherokee_connection_t *cnt, cherokee_buffer_t *buf)
{
	ret_t              ret;
	size_t             username_len = 0;
	cuint_t            method_len = 0;
	cuint_t            version_len = 0;
	char              *username;
	const char        *method;
	const char        *version;
	char               ipaddr[CHE_INET_ADDRSTRLEN];

	/* Read the bogonow value from the thread and
	 * if time has changed then recreate the date-time string.
	 */
	if (unlikely (logger->now_time != CONN_THREAD(cnt)->bogo_now)) {
		struct tm *pnow_tm;

		logger->now_time = CONN_THREAD(cnt)->bogo_now;
		pnow_tm = &CONN_THREAD(cnt)->bogo_now_tmloc;
		cherokee_buffer_clean (&logger->now_dtm);
		cherokee_buffer_add_va (&logger->now_dtm, 
				" [%02d/%s/%d:%02d:%02d:%02d %c%02d%02d] \"",
				pnow_tm->tm_mday, 
				month[pnow_tm->tm_mon], 
				1900 + pnow_tm->tm_year,
				pnow_tm->tm_hour, 
				pnow_tm->tm_min, 
				pnow_tm->tm_sec,
				(logger->tz < 0) ? '-' : '+', 
				(int) (abs(logger->tz) / 60), 
				(int) (abs(logger->tz) % 60));
	}

	/* Look for the user
	 */
	if (cnt->validator && !cherokee_buffer_is_empty (&cnt->validator->user)) {
		username_len = cnt->validator->user.len;
		username = cnt->validator->user.buf;
	} else {
		username_len = 1;
		username = "-";
	}

	/* Get the method and version strings
	 */
	ret = cherokee_http_method_to_string (cnt->header.method, &method, &method_len);
	if (unlikely(ret < ret_ok)) return ret;

	ret = cherokee_http_version_to_string (cnt->header.version, &version, &version_len);
	if (unlikely(ret < ret_ok)) return ret;

	/* Build the log string
	 *
	 * "%s - %s [%02d/%s/%d:%02d:%02d:%02d %c%02d%02d] \"%s %s %s\" %d "
	 * FMT_OFFSET
	 */
	if (cherokee_buffer_is_empty (&cnt->logger_real_ip)) {
		memset (ipaddr, 0, sizeof(ipaddr));
		cherokee_socket_ntop (&cnt->socket, ipaddr, sizeof(ipaddr)-1);
		cherokee_buffer_add (buf, ipaddr, strlen(ipaddr));
	} else {
		cherokee_buffer_add_buffer (buf, &cnt->logger_real_ip);
	}

	cherokee_buffer_add_str (buf, " - ");
	cherokee_buffer_add     (buf, username, username_len);

	/* " [date time] "
	 */
	cherokee_buffer_add_buffer (buf, &logger->now_dtm);
	cherokee_buffer_add        (buf, method, method_len);
	cherokee_buffer_add_char   (buf, ' ');

	if (! cherokee_buffer_is_empty (&cnt->request_original)) {
		cherokee_buffer_add_buffer (buf, &cnt->request_original);
	} else {
		cherokee_buffer_add_buffer (buf, &cnt->request);
		if (! cherokee_buffer_is_empty (&cnt->query_string)) {
			cherokee_buffer_add_char   (buf, '?');
			cherokee_buffer_add_buffer (buf, &cnt->query_string);
		}
	}

	cherokee_buffer_add_char   (buf, ' ');
	cherokee_buffer_add        (buf, version, version_len);
	cherokee_buffer_add_str    (buf, "\" ");
	cherokee_buffer_add_long10 (buf, cnt->error_code);
	cherokee_buffer_add_char   (buf, ' ');
	cherokee_buffer_add_ullong10 (buf, (cullong_t) (cnt->range_end - cnt->range_start));

	/* Look for the "combined" information
	 */
	if (!logger->combined) {
		cherokee_buffer_add_char (buf, '\n');
		return ret_ok;
	}

	/* "combined" information
	 */
	{
		cherokee_buffer_t *referer   = &logger->referer;
		cherokee_buffer_t *useragent = &logger->useragent;

		cherokee_buffer_clean (referer);
		cherokee_buffer_clean (useragent);

		cherokee_header_copy_known (&cnt->header, header_referer, referer);
		cherokee_header_copy_known (&cnt->header, header_user_agent, useragent);
		cherokee_buffer_ensure_addlen (buf, 8 + referer->len + referer->len);

		if (referer->len > 0) {
			cherokee_buffer_add_str    (buf, " \"");
			cherokee_buffer_add_buffer (buf, referer);
			cherokee_buffer_add_str    (buf, "\" \"");
		} else {
			cherokee_buffer_add_str (buf, " \"-\" \"");
		}
		
		if (useragent->len > 0) {
			cherokee_buffer_add_buffer (buf, useragent);
		} 
		cherokee_buffer_add_str (buf, "\"\n");
	}

	return ret_ok;
}


ret_t
cherokee_logger_ncsa_write_access (cherokee_logger_ncsa_t *logger, cherokee_connection_t *cnt)
{
	ret_t              ret;
	cherokee_buffer_t *log;
	
	/* Get the buffer
	 */
	cherokee_logger_writer_get_buf (logger->writer_access, &log);

	/* Add the new string
	 */
	ret = build_log_string (logger, cnt, log);
	if (unlikely (ret != ret_ok)) 
		goto error;

	/* Flush buffer if full
	 */  
	if (log->len < logger->writer_access->max_bufsize)
		goto ok;

	ret = cherokee_logger_writer_flush (logger->writer_access, true);
	if (unlikely (ret != ret_ok))
		goto error;

ok:
	cherokee_logger_writer_release_buf (logger->writer_access);
	return ret_ok;

error:
	cherokee_logger_writer_release_buf (logger->writer_access);
	return ret_error;
}


ret_t 
cherokee_logger_ncsa_write_error (cherokee_logger_ncsa_t *logger,
				  cherokee_buffer_t      *error)
{
	ret_t              ret;
	cherokee_buffer_t *log;

	/* Get the buffer
	 */
	cherokee_logger_writer_get_buf (logger->writer_error, &log);

	/* Add the new string
	 */
	ret = cherokee_buffer_add_buffer (log, error);
	if (unlikely (ret != ret_ok)) {
		ret = ret_error;
		goto out;
	}

	/* It's an error, flush it right away.
	 */
	ret = cherokee_logger_writer_flush (logger->writer_error, true);
	if (unlikely (ret != ret_ok)) {
		ret = ret_error;
		goto out;
	}

	ret = ret_ok;

out:
	cherokee_logger_writer_release_buf (logger->writer_error);
	return ret;
}


ret_t 
cherokee_logger_ncsa_get_error_writer (cherokee_logger_ncsa_t    *logger,
				       cherokee_logger_writer_t **writer)
{
	*writer = logger->writer_error;
	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_reopen (cherokee_logger_ncsa_t *logger)
{
	ret_t ret1;
	ret_t ret2;

	ret1 = cherokee_logger_writer_reopen (logger->writer_access);
	ret2 = cherokee_logger_writer_reopen (logger->writer_error);

	if (ret1 != ret_ok)
		return ret1;

	return ret2;
}

