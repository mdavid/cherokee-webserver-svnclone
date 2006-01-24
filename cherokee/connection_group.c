/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2006 Alvaro Lopez Ortega
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
#include "connection_group.h"


ret_t 
cherokee_connection_group_new (cherokee_connection_group_t **group)
{
	   CHEROKEE_NEW_STRUCT(n, connection_group);

	   cherokee_connection_base_init (CONN_BASE(n));

	   INIT_LIST_HEAD(&n->base.list_entry);
	   INIT_LIST_HEAD(&n->group);	   

	   *group = n;
	   return ret_ok;
}


ret_t 
cherokee_connection_group_free (cherokee_connection_group_t *group)
{
	   free (group);
	   return ret_ok;
}

