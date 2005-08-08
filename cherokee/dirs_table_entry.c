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

#include "common-internal.h"
#include "dirs_table_entry.h"

#include "access.h"

ret_t 
cherokee_dirs_table_entry_new (cherokee_dirs_table_entry_t **entry)
{
	CHEROKEE_NEW_STRUCT (n, dirs_table_entry);
		
	cherokee_dirs_table_entry_init (n);

	*entry = n;	
	return ret_ok;
}


ret_t 
cherokee_dirs_table_entry_init (cherokee_dirs_table_entry_t *entry)
{
	entry->parent             = NULL;
	entry->properties         = NULL;
	entry->handler_new_func   = NULL;
	entry->validator_new_func = NULL;
	entry->access             = NULL;
	entry->authentication     = http_auth_nothing;
	entry->only_secure        = false;

	entry->document_root      = NULL;
	entry->auth_realm         = NULL;
	entry->users              = NULL;

	return ret_ok;
}


ret_t 
cherokee_dirs_table_entry_free (cherokee_dirs_table_entry_t *entry) 
{
	if (entry->properties != NULL) {
		cherokee_typed_table_free (entry->properties);
		entry->properties = NULL;
	}
	
	if (entry->access != NULL) {
		cherokee_access_free (entry->access);
		entry->access = NULL;
	}

	if (entry->document_root != NULL) {
		cherokee_buffer_free (entry->document_root);
		entry->document_root = NULL;
	}

	if (entry->auth_realm != NULL) {
		cherokee_buffer_free (entry->auth_realm);
		entry->auth_realm = NULL;
	}

	if (entry->users != NULL) {
		cherokee_table_free (entry->users);
		entry->users = NULL;
	}

	free (entry);
	return ret_ok;
}


ret_t 
cherokee_dirs_table_entry_set_prop (cherokee_dirs_table_entry_t *entry, char *prop_name, cherokee_typed_table_types_t type, void *value, cherokee_table_free_item_t free_func)
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
	switch (type) {
	case typed_int:
		return cherokee_typed_table_add_int (entry->properties, prop_name, POINTER_TO_INT(value));
	case typed_str:
		return cherokee_typed_table_add_str (entry->properties, prop_name, value);
	case typed_data:
		return cherokee_typed_table_add_data (entry->properties, prop_name, value, free_func);
	case typed_list:
		return cherokee_typed_table_add_list (entry->properties, prop_name, value, free_func);
	default:
		SHOULDNT_HAPPEN;
	}
	
	return ret_error;
}


ret_t 
cherokee_dirs_table_entry_set_handler (cherokee_dirs_table_entry_t *entry, cherokee_module_info_t *modinfo)
{
	return_if_fail (modinfo != NULL, ret_error);

	if (modinfo->type != cherokee_handler) {
		PRINT_ERROR ("Directory '%s' has not a handler module!\n", entry->document_root->buf);
		return ret_error;
	}

	entry->handler_new_func = modinfo->new_func;

	return ret_ok;
}


ret_t 
cherokee_dirs_table_entry_complete (cherokee_dirs_table_entry_t *entry, cherokee_dirs_table_entry_t *main)
{
	/* If a temporary dirs_table entry inherits from valid entry, it will
	 * get references than mustn't be free'd, like 'user'. Take care.
	 */

	if (entry->parent == NULL) 
		entry->parent = main->parent;
	
	if (entry->properties == NULL)
		entry->properties = main->properties;

	if (entry->handler_new_func == NULL)
		entry->handler_new_func = main->handler_new_func;
	
	if (entry->authentication == 0)
		entry->authentication = main->authentication;
	
	if (entry->only_secure == false)
		entry->only_secure = main->only_secure;

	if (entry->access == NULL)
		entry->access = main->access;
	
	if (entry->validator_new_func == NULL)
		entry->validator_new_func = main->validator_new_func;

  	if (entry->document_root == NULL)  
 		entry->document_root = main->document_root;	 
	
 	if (entry->auth_realm == NULL) 
 		entry->auth_realm = main->auth_realm; 

	if (entry->users == NULL)
		entry->users = main->users;

	return ret_ok;
}


ret_t 
cherokee_dirs_table_entry_inherit (cherokee_dirs_table_entry_t *entry)
{
	cherokee_dirs_table_entry_t *parent = entry;

	while ((parent = parent->parent) != NULL) {
		cherokee_dirs_table_entry_complete (entry, parent);
	}

	return ret_ok;
}
