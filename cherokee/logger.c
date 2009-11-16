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
#include "logger.h"
#include "access.h"
#include "header.h"
#include "connection-protected.h"

#include <stdlib.h>
#include <string.h>
#include <stdarg.h>


struct cherokee_logger_private {
	CHEROKEE_MUTEX_T   (mutex);
	cherokee_boolean_t  backup_mode;

	cherokee_boolean_t  x_real_ip_enabled;
	cherokee_access_t  *x_real_ip_access;
	cherokee_boolean_t  x_real_ip_access_all;
};

#define PRIV(x)  (LOGGER(x)->priv)

static ret_t
add_access (char *address, void *data)
{
	ret_t              ret;
	cherokee_logger_t *logger = LOGGER(data);

	if (logger->priv->x_real_ip_access == NULL) {
		ret = cherokee_access_new (&logger->priv->x_real_ip_access);
		if (ret != ret_ok) {
			return ret;
		}
	}

	ret = cherokee_access_add (logger->priv->x_real_ip_access, address);
	if (ret != ret_ok) {
		return ret;
	}

	return ret_ok;
	
}

ret_t
cherokee_logger_init_base (cherokee_logger_t      *logger,
			   cherokee_plugin_info_t *info,
			   cherokee_config_node_t *config)
{
	ret_t                   ret;
	cherokee_config_node_t *subconf;
	CHEROKEE_NEW_TYPE(priv, struct cherokee_logger_private);

	/* Init the base class
	 */
	cherokee_module_init_base (MODULE(logger), NULL, info);

	/* Pure virtual methods
	 */
	logger->priv             = priv;
	logger->write_access     = NULL;
	logger->write_error      = NULL;
	logger->get_error_writer = NULL;

	/* Private
	 */	
	logger->priv->backup_mode          = false;
	logger->priv->x_real_ip_enabled    = false;
	logger->priv->x_real_ip_access     = NULL;
	logger->priv->x_real_ip_access_all = false;

	CHEROKEE_MUTEX_INIT (&PRIV(logger)->mutex, NULL);

	/* Read the configuration
	 */
	cherokee_config_node_read_bool (config, "x_real_ip_enabled",
					&logger->priv->x_real_ip_enabled);
	
	cherokee_config_node_read_bool (config, "x_real_ip_access_all",
					&logger->priv->x_real_ip_access_all);
	
	ret = cherokee_config_node_get (config, "x_real_ip_access", &subconf);
	if (ret == ret_ok) {
		ret = cherokee_config_node_read_list (subconf, NULL, add_access, logger);
		if (ret != ret_ok) {
			LOG_ERROR_S (CHEROKEE_ERROR_LOGGER_X_REAL_IP_PARSE);
			return ret_error;
		}
	}

	return ret_ok;
}



/* Virtual method hiding layer
 */
ret_t
cherokee_logger_free (cherokee_logger_t *logger)
{
	CHEROKEE_MUTEX_DESTROY (&PRIV(logger)->mutex);

	if (MODULE(logger)->free) {
		MODULE(logger)->free (logger);
	}

	if (logger->priv) {
		if (logger->priv->x_real_ip_access) {
			cherokee_access_free (logger->priv->x_real_ip_access);
		}
		free (logger->priv);
	}

	free (logger);
	return ret_ok;
}


ret_t
cherokee_logger_flush (cherokee_logger_t *logger)
{
	ret_t ret = ret_error;

	/* If the logger is on backup mode, it shouldn't write anything
	 * to the disk.  Maintenance tasks have been taking place.
	 */
	if (logger->priv->backup_mode) {
		return ret_ok;
	}

	if (logger->flush) {
		CHEROKEE_MUTEX_LOCK (&PRIV(logger)->mutex);
		ret = logger->flush(logger);
		CHEROKEE_MUTEX_UNLOCK (&PRIV(logger)->mutex);
	}

	return ret;
}


ret_t
cherokee_logger_init (cherokee_logger_t *logger)
{
	logger_func_init_t init_func;

	init_func = (logger_func_init_t) MODULE(logger)->init;

	if (init_func) {
		return init_func (logger);
	}

	return ret_error;
}

static ret_t
parse_x_real_ip (cherokee_logger_t *logger, cherokee_connection_t *conn)
{
	ret_t    ret;
	cuint_t  len  = 0;
	char    *val  = NULL;
	
	/* Look for the X-Real-IP header
	 */
	ret = cherokee_header_get_known (&conn->header, header_x_real_ip, &val, &len);
	if (ret != ret_ok) {
		char *p;

		/* Look for the X-Forwarded-For header
		 */
		ret = cherokee_header_get_known (&conn->header, header_x_forwarded_for, &val, &len);
		if (ret != ret_ok) {
			return ret_not_found;
		}

		p = val;
		while (*p) {
			if ((*p == ' ') || (*p == ',')) {
				len = p - val;
				break;
			}
			p++;
		}
	}
	
	/* Is the client allowed to use X-Real-IP?
	 */
	if (! logger->priv->x_real_ip_access_all) {
		if (logger->priv->x_real_ip_access == NULL) {
			return ret_deny;
		}

		ret = cherokee_access_ip_match (logger->priv->x_real_ip_access, &conn->socket);
		if (ret != ret_ok) {
			return ret_deny;
		}
	}
		
	/* Store the X-Real-IP value
	 */
	ret = cherokee_buffer_add (&conn->logger_real_ip, val, len);
	if (unlikely (ret != ret_ok)) {
		return ret_error;
	}

	return ret_ok;
}

ret_t
cherokee_logger_write_access (cherokee_logger_t *logger, void *conn)
{
	ret_t ret;

	/* Sanity check
	 */
	if (unlikely (logger->write_access == NULL)) {
		return ret_error;
	}
	
	/* Deal with X-Real-IP
	 */
	if (logger->priv->x_real_ip_enabled) {
		ret = parse_x_real_ip (logger, CONN(conn));
		if (unlikely (ret == ret_error))
			return ret_error;
	}

	/* Call the virtual method
	 */
	CHEROKEE_MUTEX_LOCK (&PRIV(logger)->mutex);
	ret = logger->write_access (logger, conn);
	CHEROKEE_MUTEX_UNLOCK (&PRIV(logger)->mutex);

	return ret;
}


ret_t
cherokee_logger_get_error_writer (cherokee_logger_t         *logger,
				  cherokee_logger_writer_t **writer)
{
	ret_t ret = ret_error;

	if (logger->get_error_writer) {
		CHEROKEE_MUTEX_LOCK (&PRIV(logger)->mutex);
		ret = logger->get_error_writer (logger, writer);
		CHEROKEE_MUTEX_UNLOCK (&PRIV(logger)->mutex);
	}

	return ret;
}


ret_t 
cherokee_logger_write_error (cherokee_logger_t *logger,
			     cherokee_buffer_t *error)
{
	ret_t ret;

	if (logger == NULL) 
		return ret_ok;

	if (logger->write_error) {
		CHEROKEE_MUTEX_LOCK(&PRIV(logger)->mutex);
		ret = logger->write_error (logger, error);
		CHEROKEE_MUTEX_UNLOCK(&PRIV(logger)->mutex);

		return ret;
	}

	return ret_error;
}


ret_t 
cherokee_logger_reopen (cherokee_logger_t *logger)
{
	ret_t ret = ret_error;

	if (logger->reopen != NULL) {
		CHEROKEE_MUTEX_LOCK (&PRIV(logger)->mutex);
		ret = logger->reopen (logger);
		CHEROKEE_MUTEX_UNLOCK (&PRIV(logger)->mutex);
	}

	return ret;
}


ret_t 
cherokee_logger_set_backup_mode (cherokee_logger_t *logger, cherokee_boolean_t active)
{
	ret_t ret;

	/* Set backup mode: ON
	 */
	if (active == true) {
		logger->priv->backup_mode = true;
		return ret_ok;
	}

	/* Set backup mode: OFF
	 */
	logger->priv->backup_mode = false;

	ret = cherokee_logger_reopen (logger);
	if (unlikely(ret != ret_ok)) return ret;

	ret = cherokee_logger_flush (logger);
	if (unlikely(ret != ret_ok)) return ret;

	return ret_ok;
}


ret_t 
cherokee_logger_get_backup_mode (cherokee_logger_t *logger, cherokee_boolean_t *active)
{
	*active = logger->priv->backup_mode;
	return ret_ok;
}

