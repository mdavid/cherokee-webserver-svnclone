/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2007 Alvaro Lopez Ortega
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

#ifndef CHEROKEE_REGEX_TABLE_H
#define CHEROKEE_REGEX_TABLE_H

#include <cherokee/common.h>

CHEROKEE_BEGIN_DECLS

typedef struct cherokee_regex_table cherokee_regex_table_t;
#define REGEX(x) ((cherokee_regex_table_t *)(x))

ret_t cherokee_regex_table_new   (cherokee_regex_table_t **table);
ret_t cherokee_regex_table_free  (cherokee_regex_table_t  *table);
ret_t cherokee_regex_table_clean (cherokee_regex_table_t  *table);

ret_t cherokee_regex_table_get  (cherokee_regex_table_t *table, char *pattern, void **pcre);
ret_t cherokee_regex_table_add  (cherokee_regex_table_t *table, char *pattern);


CHEROKEE_END_DECLS

#endif /* CHEROKEE_REGEX_TABLE_H */
