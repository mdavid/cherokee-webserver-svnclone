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

#ifndef CHEROKEE_HANDLER_TABLE_ENTRY_H
#define CHEROKEE_HANDLER_TABLE_ENTRY_H

#include "common-internal.h"

#include "table.h"
#include "handler.h"
#include "encoder.h"
#include "module.h"
#include "validator.h"
#include "http.h"


typedef struct {
	/* Properties table
	 */
	cherokee_table_t     *properties; 

	/* Document root
	 */
	cherokee_buffer_t    *document_root;

	/* Function "New"
	 */
	handler_func_new_t    handler_new_func;

	/* Authentication
	 */
	cherokee_buffer_t    *auth_realm;
	validator_func_new_t  validator_new_func;
	cherokee_http_auth_t  authentication;

	/* Security
	 */
	cherokee_boolean_t    only_secure;

	/* Direction checks: cherokee_access_t
	 */
	void                 *access;

} cherokee_handler_table_entry_t; 

#define HT_ENTRY(x) ((cherokee_handler_table_t *)(x))


ret_t cherokee_handler_table_entry_new  (cherokee_handler_table_entry_t **entry);
ret_t cherokee_handler_table_entry_free (cherokee_handler_table_entry_t  *entry);

ret_t cherokee_handler_table_entry_set     (cherokee_handler_table_entry_t *entry, char *prop_name, char *prop_value);
ret_t cherokee_handler_table_enty_get_info (cherokee_handler_table_entry_t *entry, cherokee_module_info_t *modinfo); 

#endif /* CHEROKEE_HANDLER_TABLE_ENTRY_H */
