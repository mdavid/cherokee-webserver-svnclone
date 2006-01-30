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

#define DEFAULT_CHUNK_SIZE 10


ret_t 
cherokee_connection_group_new (cherokee_connection_group_t **group)
{
	CHEROKEE_NEW_STRUCT(n, connection_group);

	cherokee_connection_base_init (CONN_BASE(n), conn_base_group);
	
	n->group = (cherokee_connection_t **) malloc (sizeof (cherokee_connection_t *) * DEFAULT_CHUNK_SIZE);
	n->group_last = 0;
	n->group_size = DEFAULT_CHUNK_SIZE;
	
	n->reactive_cb    = NULL;
	n->reactive_param = NULL;

	*group = n;
	return ret_ok;
}


ret_t 
cherokee_connection_group_free (cherokee_connection_group_t *group)
{
	if (group->group != NULL) 
		free (group->group);
	
	free (group);
	return ret_ok;
}


ret_t 
cherokee_connection_group_add (cherokee_connection_group_t *group, cherokee_connection_t *conn)
{
	if (group->group_last >= group->group_size) {
		group->group = realloc (group->group, group->group_size + DEFAULT_CHUNK_SIZE);
		group->group_size += DEFAULT_CHUNK_SIZE;
	}

	group->group[group->group_last] = conn;
	group->group_last++;

	return ret_ok;
}


ret_t 
cherokee_connection_group_foreach (cherokee_connection_group_t *group, cherokee_connection_group_foreach_t func, void *param)
{
	cuint_t i;

	for (i=0; i < group->group_last; i++) {
		func (group->group[i], param);
	}
	
	return ret_ok;
}


ret_t 
cherokee_connection_group_reactive (cherokee_connection_group_t *group)
{
	if (group->reactive_cb == NULL)
		return ret_error;

	return group->reactive_cb (group, group->reactive_param);
}


ret_t 
cherokee_connection_group_set_reactive (cherokee_connection_group_t *group, cherokee_connection_group_reactive_t func, void *param)
{
	group->reactive_cb    = func;
	group->reactive_param = param;

	return ret_ok;
}

