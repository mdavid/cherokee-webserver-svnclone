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

#include "handler_common.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STRINGS_H
# include <strings.h>
#endif

#include "connection.h"
#include "connection-protected.h"
#include "server.h"
#include "server-protected.h"
#include "module.h"
#include "connection.h"
#include "handler_file.h"
#include "handler_dirlist.h"
#include "handler_phpcgi.h"
#include "list_ext.h"


cherokee_module_info_t cherokee_common_info = {
	cherokee_handler,              /* type     */
	cherokee_handler_common_new    /* new func */
};



static ret_t
manage_file (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	cherokee_connection_t *conn = CONN(cnt);

	if ((conn->local_directory->len > 4) &&
	    (strncasecmp(".php", conn->local_directory->buf + conn->local_directory->len-4, 4) == 0)) 
	{
		/* PHP Handler
		 */
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return cherokee_handler_phpcgi_new (hdl, cnt, properties);		

	}

	/* File handler
	 */
	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
	return cherokee_handler_file_new (hdl, cnt, properties);
}


ret_t 
cherokee_handler_common_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	struct stat            info;
	cherokee_connection_t *conn = CONN(cnt);
	cherokee_server_t     *srv  = CONN_SRV(cnt);

#if 0
	PRINT_DEBUG ("request: '%s'\n", conn->request->buf);
	PRINT_DEBUG ("local:   '%s'\n", conn->local_directory->buf);
#endif

	cherokee_buffer_add_buffer (conn->local_directory, conn->request);

	/* Request is a directory
	 */
	if ((stat (conn->local_directory->buf, &info) == 0) && S_ISDIR(info.st_mode)) {
		list_t *i;

		/* Maybe it has to be redirected
		 */
		if (conn->local_directory->buf[conn->local_directory->len-1] != '/') {
			cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
			return cherokee_handler_dirlist_new (hdl, cnt, properties);
		}

		/* Have an index file inside?
		 */
		list_for_each (i, &srv->index_list) {
			int   is_file, exists;
			char *index     = LIST_ITEM_INFO(i);
			int   index_len = strlen (index);
			
			/* 1.- Add the index filename
			 */
			cherokee_buffer_add (conn->local_directory, index, index_len);

			/* stat() it
			 */
			exists = (stat (conn->local_directory->buf, &info) == 0);
	
			/* 2.- Drop the index filename
			 */
			cherokee_buffer_drop_endding (conn->local_directory, index_len);

			/* If the file doesn't exist, go and try with the next one
			 */
			if (! exists) {
				continue;
			}
			
			/* Check if it is a directory
			 */
			is_file = S_ISREG(info.st_mode);
			if (! is_file) {
				cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
				return cherokee_handler_dirlist_new (hdl, cnt, properties);
			}

			/* Add the index file to the request
			 */
			cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
			cherokee_buffer_add (conn->request, index, index_len);
			
			if ((index_len > 4) && (strncasecmp(".php", index+index_len-4, 4) == 0)) {
				return cherokee_handler_phpcgi_new (hdl, cnt, properties);		
			} else {
				return cherokee_handler_file_new (hdl, cnt, properties);
			}
		} 

		/* If t	he dir hasn't a index file, it uses dirlist
		 */
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return cherokee_handler_dirlist_new (hdl, cnt, properties);
	}


	/* Request is a file
	 */
	return manage_file (hdl, cnt, properties);
}



/* Library init function
 */
static cherokee_boolean_t _common_is_init = false;

void
common_init (cherokee_module_loader_t *loader)
{
	if (_common_is_init) {
		return;
	}

	/* Load the dependences
	 */
	cherokee_module_loader_load (loader, "file");
	cherokee_module_loader_load (loader, "phpcgi");
	cherokee_module_loader_load (loader, "dirlist");

	_common_is_init = true;
}
