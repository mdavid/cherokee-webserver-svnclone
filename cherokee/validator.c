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
#include "validator.h"

#include "connection.h"

ret_t 
cherokee_validator_init_base (cherokee_validator_t *validator)
{
	cherokee_module_init_base (MODULE(validator));
	
	validator->check = NULL;

	return ret_ok;
}


ret_t 
cherokee_validator_free_base (cherokee_validator_t *validator)
{
	free (validator);
	return ret_ok;
}


ret_t 
cherokee_validator_free (cherokee_validator_t *validator)
{
	module_func_free_t func;
	
	return_if_fail (validator!=NULL, ret_error);

	func = (module_func_free_t) MODULE(validator)->free;

	if (func == NULL) {
		return ret_error;
	}
	
	return func (validator);
}


ret_t 
cherokee_validator_check (cherokee_validator_t *validator, void *conn)
{
	if (validator->check == NULL) {
		return ret_error;
	}
	
	return validator->check (validator, CONN(conn));
}


ret_t 
cherokee_validator_add_headers (cherokee_validator_t *validator, void *conn, cherokee_buffer_t *buf)
{
	if (validator->add_headers == NULL) {
		return ret_error;
	}
	
	return validator->add_headers (validator, CONN(conn), buf);	
}
