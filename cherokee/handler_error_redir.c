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

#include "handler_redir.h"
#include "handler_error_redir.h"

#include "connection.h"
#include "module_loader.h"
#include "connection-protected.h"


typedef struct {
	list_t            entry;
	cuint_t           error;
	cherokee_buffer_t url;
} error_entry_t;


static ret_t 
props_free (cherokee_handler_error_redir_props_t *props)
{
	list_t *i, *j;

	list_for_each_safe (i, j, &props->errors) {
		error_entry_t *entry = (error_entry_t *)i;

		cherokee_buffer_mrproper (&entry->url);
		free (entry);
	}

	return cherokee_handler_props_free_base (HANDLER_PROPS(props));
}

ret_t 
cherokee_handler_error_redir_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_handler_props_t **_props)
{
	list_t                               *i;
	cherokee_handler_error_redir_props_t *props;

	if (*_props == NULL) {
		CHEROKEE_NEW_STRUCT (n, handler_error_redir_props);

		cherokee_handler_props_init_base (HANDLER_PROPS(n), 
						  HANDLER_PROPS_FREE(props_free));
		INIT_LIST_HEAD (&n->errors);
		*_props = HANDLER_PROPS(n);
	}
	
	props = PROP_ERREDIR(*_props);

	cherokee_config_node_foreach (i, conf) {
		cuint_t                 error;
		error_entry_t          *entry;
		cherokee_config_node_t *subconf = CONFIG_NODE(i);

		error = atoi (subconf->key.buf);
		if (!http_type_300 (error) &&
		    !http_type_400 (error) &&
		    !http_type_500 (error)) 
		{
			PRINT_ERROR ("ERROR: error_redir: Wrong error code: '%s'\n", subconf->key.buf);
			continue;
		}

		entry = (error_entry_t *) malloc (sizeof(error_entry_t));
		if (entry == NULL) return ret_nomem;

		INIT_LIST_HEAD (&entry->entry);
		entry->error = error;
		cherokee_buffer_init (&entry->url);
		cherokee_buffer_add_buffer (&entry->url, &subconf->val);

		list_add (&entry->entry, &props->errors);
	}

	return ret_ok;
}


ret_t 
cherokee_handler_error_redir_new (cherokee_handler_t **hdl, cherokee_connection_t *conn, cherokee_handler_props_t *props)
{
	list_t *i;

	list_for_each (i, &PROP_ERREDIR(props)->errors) {
		error_entry_t *entry = (error_entry_t *)i;
		
		if (entry->error != conn->error_code)
			continue;

		cherokee_buffer_clean (&conn->redirect);
		cherokee_buffer_add_buffer (&conn->redirect, &entry->url); 
		
		conn->error_code = http_moved_permanently; 
		return cherokee_handler_redir_new (hdl, conn, props);
	}
	
	return ret_error;
}


/* Library init function
 */
static cherokee_boolean_t _error_redir_is_init = false;

void
MODULE_INIT(error_redir) (cherokee_module_loader_t *loader)
{
	/* Is init?
	 */
	if (_error_redir_is_init) return;
	_error_redir_is_init = true;
	   
	/* Load the dependences
	 */
	cherokee_module_loader_load (loader, "redir");
}

HANDLER_MODULE_INFO_INIT_EASY (error_redir, http_all_methods);
