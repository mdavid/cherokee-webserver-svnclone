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

#ifndef CHEROKEE_MIME_H
#define CHEROKEE_MIME_H

#include <cherokee/common.h>
#include <cherokee/table.h>
#include <cherokee/buffer.h>
#include <cherokee/list.h>

CHEROKEE_BEGIN_DECLS

typedef struct cherokee_mime cherokee_mime_t;

typedef struct {
	struct list_head   base;
	int                max_age;
	cherokee_buffer_t *mime_name;
} cherokee_mime_entry_t;


#define MIME(m)            ((cherokee_mime_t *)(m))
#define MIME_ENTRY(m)      ((cherokee_mime_entry_t *)(m))
#define MIME_ENTRY_AGE(m)  (MIME_ENTRY(m)->max_age)
#define MIME_ENTRY_NAME(m) (MIME_ENTRY(m)->mime_name)

/* Mime methods
 */
ret_t cherokee_mime_init        (cherokee_mime_t **mime);
ret_t cherokee_mime_free        (cherokee_mime_t  *mime);
ret_t cherokee_mime_get_default (cherokee_mime_t **mime);

ret_t cherokee_mime_load (cherokee_mime_t  *mime, char *filename);
ret_t cherokee_mime_get  (cherokee_mime_t  *mime, char *suffix, cherokee_mime_entry_t **entry);
ret_t cherokee_mime_add  (cherokee_mime_t  *mime, cherokee_mime_entry_t *entry);

/* Mime entry methods
 */
ret_t cherokee_mime_entry_new  (cherokee_mime_entry_t **mentry);
ret_t cherokee_mime_entry_free (cherokee_mime_entry_t  *mentry);


CHEROKEE_END_DECLS

#endif /* CHEROKEE_MIME_H */
