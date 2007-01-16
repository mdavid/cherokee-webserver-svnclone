/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 * 
 * Some pieces of code by:
 *      Miguel Angel Ajo Pelayo <ajo@godsmaze.org> 
 *      Pablo Neira Ayuso <pneira@optimat.com>
 *
 * Copyright (C) 2001-2007 Alvaro Lopez Ortega
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
#include "logger_ncsa.h"

#include <stdarg.h>
#include <sys/types.h>
#include <unistd.h>
#include <fcntl.h>

#ifdef HAVE_SYSLOG_H
# include <syslog.h>
#endif

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


/* Some constants
 */
static char *month[] = {
	"Jan", "Feb", "Mar", "Apr", "May", "Jun",
	"Jul", "Aug", "Sep", "Oct", "Nov", "Dec", 
	NULL
};


ret_t
cherokee_logger_ncsa_new (cherokee_logger_t **logger, cherokee_config_node_t *config)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, logger_ncsa);
	
	/* Init the base class object
	 */
	cherokee_logger_init_base (LOGGER(n), PLUGIN_INFO_PTR(ncsa));

	MODULE(n)->init         = (logger_func_init_t) cherokee_logger_ncsa_init;
	MODULE(n)->free         = (logger_func_free_t) cherokee_logger_ncsa_free;
	
	LOGGER(n)->flush        = (logger_func_flush_t) cherokee_logger_ncsa_flush;
	LOGGER(n)->reopen       = (logger_func_reopen_t) cherokee_logger_ncsa_reopen;
	LOGGER(n)->write_error  = (logger_func_write_error_t)  cherokee_logger_ncsa_write_error;
	LOGGER(n)->write_access = (logger_func_write_access_t) cherokee_logger_ncsa_write_access;
	LOGGER(n)->write_string = (logger_func_write_string_t) cherokee_logger_ncsa_write_string;

	ret = cherokee_logger_ncsa_init_base (n, config);
	if (unlikely(ret < ret_ok)) return ret;

	/* Return the object
	 */
	*logger = LOGGER(n);
	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_init_base (cherokee_logger_ncsa_t *logger, cherokee_config_node_t *config)
{
	ret_t                   ret;
	cherokee_config_node_t *subconf;

	/* Init the logger writers
	 */
	ret = cherokee_logger_writer_init (&logger->writer_access);
	if (ret != ret_ok) return ret;

	ret = cherokee_logger_writer_init (&logger->writer_error);
	if (ret != ret_ok) return ret;

	/* Configure them
	 */
	ret = cherokee_config_node_get (config, "access", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_logger_writer_configure (&logger->writer_access, subconf);
		if (ret != ret_ok) return ret;
	}

	ret = cherokee_config_node_get (config, "error", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_logger_writer_configure (&logger->writer_error, subconf);
		if (ret != ret_ok) return ret;
	}
	
	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_init (cherokee_logger_ncsa_t *logger)
{
	ret_t ret;

	ret = cherokee_logger_writer_open (&logger->writer_access);
	if (ret != ret_ok) return ret;

	ret = cherokee_logger_writer_open (&logger->writer_error);
	if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t
cherokee_logger_ncsa_free (cherokee_logger_ncsa_t *logger)
{
	ret_t ret;

	ret = cherokee_logger_writer_mrproper (&logger->writer_access);
	if (ret != ret_ok) return ret;

	ret = cherokee_logger_writer_mrproper (&logger->writer_error);
	if (ret != ret_ok) return ret;

	return ret_ok;
}


ret_t 
cherokee_logger_ncsa_flush (cherokee_logger_ncsa_t *logger)
{
	ret_t ret;

	cherokee_logger_writer_lock (&logger->writer_access);
	ret = cherokee_logger_writer_flush (&logger->writer_access);
	cherokee_logger_writer_unlock (&logger->writer_access);

	return ret;
}


static ret_t
build_log_string (cherokee_logger_ncsa_t *logger, cherokee_connection_t *cnt, cherokee_buffer_t *buf)
{
  	long int           z;
	ret_t              ret;
	char              *username;
	const char        *method;
        const char        *version;
	struct tm         *conn_time;
	char               ipaddr[CHE_INET_ADDRSTRLEN];
	static long       *this_timezone = NULL;
	cherokee_buffer_t  combined_info = CHEROKEE_BUF_INIT;
	cherokee_buffer_t *request;

	/* Read the bogonow value from the server
	 */
	conn_time = &CONN_THREAD(cnt)->bogo_now_tm;

	/* Get the timezone reference
	 */
	if (this_timezone == NULL) {
		this_timezone = cherokee_get_timezone_ref();
	}
	
	z = - (*this_timezone / 60);

	memset (ipaddr, 0, sizeof(ipaddr));
	cherokee_socket_ntop (&cnt->socket, ipaddr, sizeof(ipaddr)-1);

	/* Look for the user
	 */
	if (cnt->validator && !cherokee_buffer_is_empty (&cnt->validator->user)) {
		username = cnt->validator->user.buf;
	} else {
		username = "-";
	}

	/* Get the method and version strings
	 */
	ret = cherokee_http_method_to_string (cnt->header.method, &method, NULL);
	if (unlikely(ret < ret_ok)) return ret;

	ret = cherokee_http_version_to_string (cnt->header.version, &version, NULL);
	if (unlikely(ret < ret_ok)) return ret;

	/* Look for the "combined" information
	 */
	if (logger->combined) {
		cherokee_buffer_t referer   = CHEROKEE_BUF_INIT;
		cherokee_buffer_t useragent = CHEROKEE_BUF_INIT;

		cherokee_header_copy_known (&cnt->header, header_referer, &referer);
		cherokee_header_copy_known (&cnt->header, header_user_agent, &useragent);

		cherokee_buffer_ensure_size (&combined_info, 8 + referer.len + referer.len);

		if (referer.len > 0) {
			cherokee_buffer_add_str (&combined_info, " \"");
			cherokee_buffer_add_buffer (&combined_info, &referer);
			cherokee_buffer_add_str (&combined_info, "\" \"");
		} else {
			cherokee_buffer_add_str (&combined_info, "\"-\" \"");
		}
		
		if (useragent.len > 0) {
			cherokee_buffer_add_buffer (&combined_info, &useragent);
		} 
		cherokee_buffer_add_str (&combined_info, "\"");

		cherokee_buffer_mrproper (&referer);
		cherokee_buffer_mrproper (&useragent);
	}

	request = cherokee_buffer_is_empty(&cnt->request_original) ? 
		&cnt->request : &cnt->request_original;

	/* Build the log string
	 */
	cherokee_buffer_add_va (buf, 
				"%s - %s [%02d/%s/%d:%02d:%02d:%02d %c%02d%02d] \"%s %s %s\" %d " FMT_OFFSET,
				ipaddr,
				username, 
				conn_time->tm_mday, 
				month[conn_time->tm_mon], 
				1900 + conn_time->tm_year,
				conn_time->tm_hour, 
				conn_time->tm_min, 
				conn_time->tm_sec,
				(z < 0) ? '-' : '+', 
				(int) z/60, 
				(int) z%60, 
				method,
				request->buf, 
				version, 
				cnt->error_code,
				(CST_OFFSET) (cnt->range_end - cnt->range_start));

	if (logger->combined) {
		cherokee_buffer_add_buffer (buf, &combined_info);
	}

	cherokee_buffer_add_str (buf, "\n");
	
	/* Maybe free some memory..
	 */
	cherokee_buffer_mrproper (&combined_info);
	return ret_ok;

}


ret_t 
cherokee_logger_ncsa_write_string (cherokee_logger_ncsa_t *logger, const char *string)
{
	ret_t              ret;
	cherokee_buffer_t *log;

	cherokee_logger_writer_lock (&logger->writer_access);

	ret = cherokee_logger_writer_get_buf (&logger->writer_access, &log);
	if (unlikely (ret != ret_ok)) goto error;

	cherokee_buffer_add (log, (char *)string, strlen(string));

	cherokee_logger_writer_unlock (&logger->writer_access);
	return ret_ok;

error:
	cherokee_logger_writer_unlock (&logger->writer_access);
	return ret_error;
}


ret_t
cherokee_logger_ncsa_write_access (cherokee_logger_ncsa_t *logger, cherokee_connection_t *cnt)
{
	ret_t              ret;
	cherokee_buffer_t *log;

	cherokee_logger_writer_lock (&logger->writer_access);

	/* Get the buffer
	 */
	ret = cherokee_logger_writer_get_buf (&logger->writer_access, &log);
	if (unlikely (ret != ret_ok)) goto error;

	/* Add the new string
	 */
	ret = build_log_string (logger, cnt, log);
	if (unlikely (ret != ret_ok)) goto error;
	
	cherokee_logger_writer_unlock (&logger->writer_access);
	return ret_ok;

error:
	cherokee_logger_writer_unlock (&logger->writer_access);
	return ret_error;
}


ret_t 
cherokee_logger_ncsa_write_error (cherokee_logger_ncsa_t *logger, cherokee_connection_t *cnt)
{
	ret_t              ret;
	cherokee_buffer_t *log;

	cherokee_logger_writer_lock (&logger->writer_error);

	/* Get the buffer
	 */
	ret = cherokee_logger_writer_get_buf (&logger->writer_error, &log);
	if (unlikely (ret != ret_ok)) goto error;
	
	/* Add the new string
	 */
	ret = build_log_string (logger, cnt, log);
	if (unlikely (ret != ret_ok)) goto error;

	/* It's an error. Flush it!
	 */
	ret = cherokee_logger_writer_flush (&logger->writer_error);
	if (unlikely (ret != ret_ok)) goto error;
	
	cherokee_logger_writer_unlock (&logger->writer_error);
	return ret_ok;

error:
	cherokee_logger_writer_unlock (&logger->writer_error);
	return ret_error;
}


ret_t 
cherokee_logger_ncsa_reopen (cherokee_logger_ncsa_t *logger)
{
	ret_t ret;

	ret  = cherokee_logger_writer_reopen (&logger->writer_access);
	ret |= cherokee_logger_writer_reopen (&logger->writer_error);

	return (ret == ret_ok) ? ret_ok : ret_error;
}

