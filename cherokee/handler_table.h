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

#ifndef CHEROKEE_HANDLER_TABLE_H
#define CHEROKEE_HANDLER_TABLE_H

#include "common.h"
#include "table.h"
#include "table-protected.h"
#include "handler_table_entry.h"

typedef struct {
	cherokee_table_t table;      /* Web_directory -> Handler_table_entry */
} cherokee_handler_table_t;

#define PTABLE(x) ((cherokee_handler_table_t *)(x))


ret_t cherokee_handler_table_new  (cherokee_handler_table_t **pt);
ret_t cherokee_handler_table_free (cherokee_handler_table_t  *pt);

ret_t cherokee_handler_table_get        (cherokee_handler_table_t *pt, cherokee_buffer_t *requested_url, cherokee_handler_table_entry_t **plugin_entry, cherokee_buffer_t *web_directory);
ret_t cherokee_handler_table_add        (cherokee_handler_table_t *pt, char *dir,           cherokee_handler_table_entry_t  *plugin_entry);
ret_t cherokee_handler_table_fill_entry (cherokee_handler_table_t *pt, char *handler_name,  cherokee_handler_table_entry_t  *plugin_entry);


#endif /* CHEROKEE_HANDLER_TABLE_H */
