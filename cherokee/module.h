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

#if !defined (CHEROKEE_INSIDE_CHEROKEE_H) && !defined (CHEROKEE_COMPILATION)
# error "Only <cherokee/cherokee.h> can be included directly, this file may disappear or change contents."
#endif

#ifndef CHEROKEE_MODULE_H
#define CHEROKEE_MODULE_H

#include <cherokee/common.h>

CHEROKEE_BEGIN_DECLS


typedef enum {
	cherokee_logger    = 1,
	cherokee_handler   = 1<<1,
	cherokee_encoder   = 1<<2,
	cherokee_validator = 1<<3
} cherokee_module_type_t;


typedef ret_t (* module_func_new_t)  (void *);
typedef ret_t (* module_func_free_t) (void *);


typedef struct {
	cherokee_module_type_t  type;
	void                   *new_func;
} cherokee_module_info_t;

typedef struct {
	module_func_new_t   new;  /* constructor step begging */
	module_func_free_t  free; /* destructor               */
	void               *init; /* constructor step endding */
} cherokee_module_t;

#define MODULE(x) ((cherokee_module_t *) (x))


ret_t cherokee_module_init_base (cherokee_module_t *module);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_MODULE_H */
