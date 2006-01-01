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
#include "fastcgi-common.h"


static void
init_server (cherokee_fcgi_server_t *n)
{
	INIT_LIST_HEAD((list_t *)n);
	
	cherokee_buffer_init (&n->interpreter);
	cherokee_buffer_init (&n->host);
}

static void
mrproper_server (cherokee_fcgi_server_t *n)
{
	cherokee_buffer_mrproper (&n->interpreter);
	cherokee_buffer_mrproper (&n->host);
}


static void
server_free (cherokee_fcgi_server_t *n)
{
	mrproper_server (n);
	free (n);
}


static void
server_first_free (cherokee_fcgi_server_first_t *n)
{
	mrproper_server (FCGI_SERVER(n));
	CHEROKEE_MUTEX_DESTROY (&n->current_server_lock);

	free (n);
}


ret_t 
cherokee_fcgi_server_new (cherokee_fcgi_server_t **server)
{
	CHEROKEE_NEW_STRUCT (n, fcgi_server);
	
	init_server (n);
	n->free_func = (cherokee_typed_free_func_t) server_free;
	
	*server = n;
	return ret_ok;
}


ret_t 
cherokee_fcgi_server_first_new  (cherokee_fcgi_server_first_t **server)
{
	CHEROKEE_NEW_STRUCT (n, fcgi_server_first);
	
	init_server (FCGI_SERVER(n));
	FCGI_SERVER(n)->free_func = (cherokee_typed_free_func_t) server_first_free;
	
	n->current_server = FCGI_SERVER(n);
	CHEROKEE_MUTEX_INIT (&n->current_server_lock, NULL);
	
	*server = n;
	return ret_ok;
}


ret_t 
cherokee_fcgi_server_free (cherokee_fcgi_server_t *server)
{
	if (server->free_func == NULL)
		return ret_error;

	server->free_func(server);
	return ret_ok;
}
