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

#ifndef __CHEROKEE_VALIDATOR_PLAIN_H__
#define __CHEROKEE_VALIDATOR_PLAIN_H__

#include "validator.h"
#include "connection.h"

typedef struct {
	   cherokee_validator_t validator;
	   const char *plain_file_ref;
} cherokee_validator_plain_t;

#define PLAIN(x) ((cherokee_validator_plain_t *)(x))


ret_t cherokee_validator_plain_new   (cherokee_validator_plain_t **plain, cherokee_table_t *properties);
ret_t cherokee_validator_plain_free  (cherokee_validator_plain_t  *plain);
ret_t cherokee_validator_plain_check (cherokee_validator_plain_t  *plain, cherokee_connection_t *conn);

#endif /* __CHEROKEE_VALIDATOR_PLAIN_H__ */
