/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
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

#include "handler_table_entry.h"
#include "access.h"


ret_t 
cherokee_handler_table_entry_new (cherokee_handler_table_entry_t **entry)
{
	CHEROKEE_NEW_STRUCT (n, handler_table_entry);
	
	n->properties         = NULL;
	n->handler_new_func   = NULL;
	n->validator_new_func = NULL;
	n->access             = NULL;
	n->authentication     = http_auth_nothing;
	n->only_secure        = false;
	
	cherokee_buffer_new (&n->document_root);
	cherokee_buffer_new (&n->auth_realm);
	
	*entry = n;	
	return ret_ok;
}


static void
free_item (void *item)
{
	if (item != NULL) {
		free (item);
	}
}

ret_t 
cherokee_handler_table_entry_free (cherokee_handler_table_entry_t *entry) 
{
	if (entry->properties != NULL) {
		cherokee_table_free2 (entry->properties, free_item);
		entry->properties = NULL;
	}
	
	if (entry->access != NULL) {
		cherokee_access_free (entry->access);
		entry->access = NULL;
	}

	cherokee_buffer_free (entry->document_root);
	cherokee_buffer_free (entry->auth_realm);

	free (entry);
	
	return ret_ok;
}


ret_t 
cherokee_handler_table_entry_set (cherokee_handler_table_entry_t *entry, char *prop_name, char *prop_value)
{
	ret_t ret;
	
	/* Create the table on demand to save memory
	 */
	if (entry->properties == NULL) {
		ret = cherokee_table_new (&entry->properties);
		if (unlikely(ret != ret_ok)) return ret;
	}
	
	/* Add the property
	 */
	return cherokee_table_add (entry->properties, prop_name, prop_value);
}


ret_t 
cherokee_handler_table_enty_get_info (cherokee_handler_table_entry_t *entry, cherokee_module_info_t *modinfo)
{
	return_if_fail (modinfo != NULL, ret_error);

	if (modinfo->type != cherokee_handler) {
		PRINT_ERROR ("Directory '%s' has not a handler module!\n", entry->document_root->buf);
		return ret_error;
	}

	entry->handler_new_func = modinfo->new_func;

	return ret_ok;
}
