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

#ifndef CHEROKEE_CONNECTION_GROUP_H
#define CHEROKEE_CONNECTION_GROUP_H

#include "common-internal.h"

#include "list.h"
#include "connection_base.h"
#include "connection.h"


typedef void  (* cherokee_connection_group_foreach_t)  (cherokee_connection_t *, void *param);
typedef ret_t (* cherokee_connection_group_reactive_t) (void *group, void *param);


typedef struct {
	cherokee_connection_base_t  base;

	cherokee_connection_t     **group;
	cuint_t                     group_last;
	cuint_t                     group_size;

	cherokee_connection_group_reactive_t  reactive_cb;
	void                                 *reactive_param;
} cherokee_connection_group_t;

#define CONN_GROUP(g) ((cherokee_connection_group_t *)(g))


ret_t cherokee_connection_group_new  (cherokee_connection_group_t **group);
ret_t cherokee_connection_group_free (cherokee_connection_group_t  *group);

ret_t cherokee_connection_group_add      (cherokee_connection_group_t *group, cherokee_connection_t *conn);
ret_t cherokee_connection_group_foreach  (cherokee_connection_group_t *group, cherokee_connection_group_foreach_t func, void *param);

ret_t cherokee_connection_group_set_reactive (cherokee_connection_group_t *group, cherokee_connection_group_reactive_t, void *param);
ret_t cherokee_connection_group_reactive     (cherokee_connection_group_t *group);


#endif /* CHEROKEE_CONNECTION_GROUP_H */
