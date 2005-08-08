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

#ifndef CHEROKEE_MODULE_LOADER_H
#define CHEROKEE_MODULE_LOADER_H

#include <cherokee/common.h>
#include <cherokee/module.h>
#include <cherokee/table.h>

CHEROKEE_BEGIN_DECLS


//typedef struct cherokee_module_loader cherokee_module_loader_t;
typedef struct cherokee_table cherokee_module_loader_t;

#define MODINFO(x)   ((cherokee_module_info_t *) (x))
#define MODLOADER(x) ((cherokee_module_loader_t *) (x))


ret_t cherokee_module_loader_init     (cherokee_module_loader_t *loader);
ret_t cherokee_module_loader_mrproper (cherokee_module_loader_t *loader);

ret_t cherokee_module_loader_load           (cherokee_module_loader_t *loader, char *modname);
ret_t cherokee_module_loader_load_no_global (cherokee_module_loader_t *loader, char *modname);
ret_t cherokee_module_loader_unload         (cherokee_module_loader_t *loader, char *modname);

ret_t cherokee_module_loader_get_info (cherokee_module_loader_t *loader, char *modname, cherokee_module_info_t **info);
ret_t cherokee_module_loader_get_sym  (cherokee_module_loader_t *loader, char *modname, char *name, void **sym);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_MODULE_LOADER_H */
