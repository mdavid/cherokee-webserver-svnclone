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

#ifndef CHEROKEE_CONNECTION_BASE_H
#define CHEROKEE_CONNECTION_BASE_H

#include "list.h"


typedef enum {
	conn_base_connection,
	conn_base_group
} cherokee_connectio_types_t;


typedef struct {
	struct list_head           list_entry;
	cherokee_connectio_types_t type;

	time_t                     timeout;
	int                        extra_polling_fd;
} cherokee_connection_base_t;

#define CONN_BASE(c) ((cherokee_connection_base_t *)(c))


ret_t cherokee_connection_base_init  (cherokee_connection_base_t *conn, cherokee_connectio_types_t type);
ret_t cherokee_connection_base_clean (cherokee_connection_base_t *conn);


#endif /* CHEROKEE_CONNECTION_BASE_H */
