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

#ifndef __CHEROKEE_VALIDATOR_H__
#define __CHEROKEE_VALIDATOR_H__

#include "common.h"
#include "buffer.h"
#include "module.h"
#include "table.h"


/* Callback function definitions
 */
typedef ret_t (* validator_func_new_t)   (void **validator, cherokee_table_t *properties); 
typedef ret_t (* validator_func_check_t) (void  *validator, void *conn);


typedef struct {
	/* Base
	 */
	cherokee_module_t module;
	
	/* Pure virtual methods	
	 */
	validator_func_check_t check;
	
} cherokee_validator_t;

#define VALIDATOR(x) ((cherokee_validator_t *)(x))


ret_t cherokee_validator_init_base    (cherokee_validator_t *validator);

ret_t cherokee_validator_free  (cherokee_validator_t *validator);
ret_t cherokee_validator_check (cherokee_validator_t *validaror, void *conn);

#endif /* __CHEROKEE_VALIDATOR_H__ */
