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
#include "logger_w3c.h"

#include <stdio.h>
#include <stdlib.h>
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
#include "module.h"
#include "server.h"
#include "server-protected.h"
#include "header.h"
#include "header-protected.h"

/* Plug-in initialization
 */
PLUGIN_INFO_LOGGER_EASIEST_INIT (w3c);


/* Documentation:
 * - http://www.w3.org/TR/WD-logfile
 */

#define IN_ADDR(c) ((struct in_addr) (c).sin_addr)

ret_t
cherokee_logger_w3c_new (cherokee_logger_t         **logger,
			 cherokee_virtual_server_t  *vsrv,
			 cherokee_config_node_t     *config)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, logger_w3c);
	
	/* Init the base class object
	 */
	cherokee_logger_init_base (LOGGER(n), PLUGIN_INFO_PTR(w3c), config);

	MODULE(n)->init           = (logger_func_init_t) cherokee_logger_w3c_init;
	MODULE(n)->free           = (logger_func_free_t) cherokee_logger_w3c_free;
	LOGGER(n)->flush          = (logger_func_flush_t) cherokee_logger_w3c_flush;
	LOGGER(n)->write_error    = (logger_func_write_error_t) cherokee_logger_w3c_write_error;
	LOGGER(n)->write_access   = (logger_func_write_access_t) cherokee_logger_w3c_write_access;
	LOGGER(n)->write_string   = (logger_func_write_string_t) cherokee_logger_w3c_write_string;
	LOGGER(n)->write_error_fd = (logger_func_write_error_fd_t) cherokee_logger_w3c_write_error_fd;

	ret = cherokee_logger_w3c_init_base (n, vsrv, config);
	if (unlikely(ret < ret_ok))
		return ret;

	/* Return the object
	 */
	*logger = LOGGER(n);
	return ret_ok;
}


ret_t 
cherokee_logger_w3c_init_base (cherokee_logger_w3c_t     *logger,
			       cherokee_virtual_server_t *vsrv,
			       cherokee_config_node_t    *config)
{
	ret_t                   ret;
	cherokee_config_node_t *subconf;

	/* Init Properties
	 */
	logger->header_added = false;
	logger->now_time = (time_t) -1;

	/* Init the local buffers
	 */
	cherokee_buffer_init (&logger->now_buf);
	cherokee_buffer_ensure_size (&logger->now_buf,   64);

	/* Init the logger writers
	 */
	ret = cherokee_config_node_get (config, "all", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_server_get_log_writer (VSERVER_SRV(vsrv), subconf, &logger->writer);
		if (ret != ret_ok) {
			return ret_error;
		}
	}

	return ret_ok;
}


ret_t 
cherokee_logger_w3c_init (cherokee_logger_w3c_t *logger)
{
	ret_t ret;

	/* Open the log writer.
	 */
	ret = cherokee_logger_writer_open (logger->writer);
	if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t
cherokee_logger_w3c_free (cherokee_logger_w3c_t *logger)
{
	cherokee_buffer_mrproper (&logger->now_buf);
	return ret_ok;
}


ret_t 
cherokee_logger_w3c_reopen (cherokee_logger_w3c_t *logger)
{
	return cherokee_logger_writer_reopen (logger->writer);
}


ret_t
cherokee_logger_w3c_flush (cherokee_logger_w3c_t *logger)
{
	return cherokee_logger_writer_flush (logger->writer, false);
}


ret_t 
cherokee_logger_w3c_write_error (cherokee_logger_w3c_t *logger, cherokee_connection_t *cnt)
{
	ret_t              ret;
	cuint_t            method_len = 0;
	const char        *method;
	cherokee_buffer_t *log;

	/* Get the logger writer buffer
	 */
	cherokee_logger_writer_get_buf (logger->writer, &log);

	/* Read the bogonow value from the server
	 */
	if (unlikely (logger->now_time != CONN_THREAD(cnt)->bogo_now)) {
		struct tm *pnow_tm;

		logger->now_time = CONN_THREAD(cnt)->bogo_now;
		pnow_tm = &CONN_THREAD(cnt)->bogo_now_tmloc;

		cherokee_buffer_clean (&logger->now_buf);
		cherokee_buffer_add_va (&logger->now_buf, 
				"%02d:%02d:%02d ",
				pnow_tm->tm_hour, 
				pnow_tm->tm_min, 
				pnow_tm->tm_sec);
	}

	if (unlikely (! logger->header_added)) {
		struct tm *pnow_tm = &CONN_THREAD(cnt)->bogo_now_tmloc;

		cherokee_buffer_add_va (log,
					"#Version 1.0\n"
					"#Date: %02d-%s-%4d %02d:%02d:%02d\n"
					"#Fields: time cs-method cs-uri\n",
					pnow_tm->tm_mday, 
					month[pnow_tm->tm_mon], 
					1900 + pnow_tm->tm_year,
					pnow_tm->tm_hour, 
					pnow_tm->tm_min, 
					pnow_tm->tm_sec);
		logger->header_added = true;
	}

	/* HTTP method
	 */
	cherokee_http_method_to_string (cnt->header.method, &method, &method_len);

	/* Build the string
	 */
	cherokee_buffer_add_buffer (log, &logger->now_buf);
	cherokee_buffer_add_str    (log, "[error] ");
	cherokee_buffer_add        (log, method, (size_t) method_len);
	cherokee_buffer_add_char   (log, ' ');

	if (! cherokee_buffer_is_empty (&cnt->request_original)) {
		cherokee_buffer_add_buffer (log, &cnt->request_original);
	} else {
		cherokee_buffer_add_buffer (log, &cnt->request);
		if (! cherokee_buffer_is_empty (&cnt->query_string)) {
			cherokee_buffer_add_char   (log, '?');
			cherokee_buffer_add_buffer (log, &cnt->query_string);
		}
	}

	cherokee_buffer_add_char   (log, '\n');

	/* Error are not buffered
	 */  	
	ret = cherokee_logger_writer_flush (logger->writer, true);
	if (unlikely (ret != ret_ok)) {
		goto error;
	}

ok:
	cherokee_logger_writer_release_buf (logger->writer);
	return ret_ok;

error:
	cherokee_logger_writer_release_buf (logger->writer);
	return ret_error;
}


ret_t
cherokee_logger_w3c_write_error_fd (cherokee_logger_w3c_t *logger, int fd)
{
	if ((logger->writer->fd != -1) &&
	    (logger->writer->fd != fd))
	{
		dup2 (logger->writer->fd, fd);
	}

	return ret_ok;
}


ret_t 
cherokee_logger_w3c_write_string (cherokee_logger_w3c_t *logger, const char *string)
{
	ret_t              ret;
	cherokee_buffer_t *log;

	/* Get the buffer
	 */
	cherokee_logger_writer_get_buf (logger->writer, &log);

	ret = cherokee_buffer_add (log, string, strlen(string));
 	if (unlikely (ret != ret_ok)) { 
		goto error;
	}
  
	/* Flush buffer if full
	 */  
  	if (log->len < logger->writer->max_bufsize) {
		goto ok;
	}

	ret = cherokee_logger_writer_flush (logger->writer, true);
	if (unlikely (ret != ret_ok)) {
		goto error;
	}

ok:
	cherokee_logger_writer_release_buf (logger->writer);
	return ret_ok;

error:
	cherokee_logger_writer_release_buf (logger->writer);
	return ret_error;
}


ret_t
cherokee_logger_w3c_write_access (cherokee_logger_w3c_t *logger, cherokee_connection_t *cnt)
{
	ret_t              ret;
	cuint_t            method_len = 0;
	const char        *method;
	cherokee_buffer_t *log;
	cherokee_buffer_t *request;

	/* Get the logger writer buffer
	 */
	cherokee_logger_writer_get_buf (logger->writer, &log);

	/* Read the bogonow value from the server
	 */
	if (unlikely (logger->now_time != CONN_THREAD(cnt)->bogo_now)) {
		struct tm *pnow_tm;

		logger->now_time = CONN_THREAD(cnt)->bogo_now;
		pnow_tm = &CONN_THREAD(cnt)->bogo_now_tmloc;

		cherokee_buffer_clean (&logger->now_buf);
		cherokee_buffer_add_va (&logger->now_buf, 
				"%02d:%02d:%02d ",
				pnow_tm->tm_hour, 
				pnow_tm->tm_min, 
				pnow_tm->tm_sec);
	}

	if (unlikely (! logger->header_added)) {
		struct tm *pnow_tm = &CONN_THREAD(cnt)->bogo_now_tmloc;

		cherokee_buffer_add_va (log,
					"#Version 1.0\n"
					"#Date: %02d-%s-%4d %02d:%02d:%02d\n"
					"#Fields: time cs-method cs-uri\n",
					pnow_tm->tm_mday, 
					month[pnow_tm->tm_mon], 
					1900 + pnow_tm->tm_year,
					pnow_tm->tm_hour, 
					pnow_tm->tm_min, 
					pnow_tm->tm_sec);
		logger->header_added = true;
	}

	/* HTTP Method
	 */
	cherokee_http_method_to_string (cnt->header.method, &method, &method_len);

	/* Build the string
	 */
	request = cherokee_buffer_is_empty(&cnt->request_original) ? 
		&cnt->request : &cnt->request_original;

	cherokee_buffer_add_buffer (log, &logger->now_buf);
	cherokee_buffer_add        (log, method, (size_t) method_len);
	cherokee_buffer_add_char   (log, ' ');
	cherokee_buffer_add_buffer (log, request);
	cherokee_buffer_add_char   (log, '\n');

	cherokee_logger_writer_release_buf (logger->writer);
	return ret_ok;
}

