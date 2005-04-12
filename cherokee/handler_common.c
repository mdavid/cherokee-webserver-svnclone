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

#include "common-internal.h"
#include "handler_common.h"
#include "util.h"

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
#include "handler_cgi.h"
#include "list_ext.h"


cherokee_module_info_t cherokee_common_info = {
	cherokee_handler,              /* type     */
	cherokee_handler_common_new    /* new func */
};



static ret_t
instance_new_nondir_handler (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	int   len  = CONN(cnt)->request->len;
	char *buf  = CONN(cnt)->request->buf;

	/* It is to small to look for the extension
	 */
	if (len <= 3) {
		return cherokee_handler_file_new (hdl, cnt, properties);
	}

	/* Two charactes extension
	 */
	if (len <= 4) {
		if (!strncasecmp(".pl", buf+len-3, 3) ||
		    !strncasecmp(".py", buf+len-3, 3) ||
		    !strncasecmp(".sh", buf+len-3, 3)) {
			return cherokee_handler_cgi_new (hdl, cnt, properties);
		} else {
			return cherokee_handler_file_new (hdl, cnt, properties);
		}
	}
	
	/* Three or more characters
	 */
	if (!strncasecmp(".php", buf+len - 4, 4)) {
		return cherokee_handler_phpcgi_new (hdl, cnt, properties);	
	} else if (!strncasecmp(".cgi", buf+len - 4, 4)) {
		return cherokee_handler_cgi_new (hdl, cnt, properties);	
	}
	
	return cherokee_handler_file_new (hdl, cnt, properties);
}



ret_t 
cherokee_handler_common_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	int                    exists;
	struct stat            info;
	cherokee_connection_t *conn = CONN(cnt);
	cherokee_server_t     *srv  = CONN_SRV(cnt);

#if 0
	PRINT_DEBUG ("request: '%s'\n", conn->request->buf);
	PRINT_DEBUG ("local:   '%s'\n", conn->local_directory->buf);
#endif

	/* Check the request
	 */
	cherokee_buffer_add_buffer (conn->local_directory, conn->request);

	exists = (stat (conn->local_directory->buf, &info) == 0);
	if (!exists) {
		ret_t  ret;
		char  *pathinfo;
		int    pathinfo_len;
		int    begin;

		/* Maybe it could stat() the file because the request contains
		 * a PathInfo string at the end..
		 */
		begin = conn->local_directory->len - conn->request->len;
		
		ret = cherokee_split_pathinfo (conn->local_directory, begin, &pathinfo, &pathinfo_len);
		if (ret == ret_not_found) {
			conn->error_code = http_not_found;
			return ret_error;
		}
		
		/* Copy the PathInfo and clean the request - if needed
		 */
		if (pathinfo_len > 0) {
			cherokee_buffer_add (conn->pathinfo, pathinfo, pathinfo_len);
			cherokee_buffer_drop_endding (conn->request, pathinfo_len);
			cherokee_buffer_drop_endding (conn->local_directory, pathinfo_len);
		}

		/* Try to instance the handler with the clean request
		 */
		ret = instance_new_nondir_handler (hdl, cnt, properties);
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return ret;
	}	

	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);

	/* Is it a file?
	 */
	if (S_ISREG(info.st_mode)) {
		return instance_new_nondir_handler (hdl, cnt, properties);
	}

	/* Is it a directory
	 */
	if (S_ISDIR(info.st_mode)) {
		list_t *i;

		/* Maybe it has to be redirected
		 */
		if (conn->request->buf[conn->request->len-1] != '/') {
			return cherokee_handler_dirlist_new (hdl, cnt, properties);
		}

		/* Add the request
		 */
		cherokee_buffer_add_buffer (conn->local_directory, conn->request);

		/* Have an index file inside?
		 */
		list_for_each (i, &srv->index_list) {
			char *index     = LIST_ITEM_INFO(i);
			int   index_len = strlen (index);

			/* Check if the index is fullpath
			 */
			if (*index == '/') {
				cherokee_buffer_t *new_local_dir;

				/* This means there is a configuration entry like:
				 * 'DirectoryIndex index.php, /index_default.php'
				 * and it's proceesing '/index_default.php'.
				 */ 

				/* Build the secondary path
				 */
				cherokee_buffer_add_buffer (conn->effective_directory, conn->local_directory);

				/* Lets reconstruct the local directory
				 */
				cherokee_buffer_new (&new_local_dir);
				cherokee_buffer_add_buffer (new_local_dir, CONN_VSRV(conn)->root);
				cherokee_buffer_add (new_local_dir, index, index_len);
				
				exists = (stat (new_local_dir->buf, &info) == 0);
				if (!exists) {
					cherokee_buffer_free (new_local_dir);
					continue;
				}
				
				cherokee_buffer_drop_endding (new_local_dir, index_len);
				cherokee_buffer_free (conn->local_directory);
				conn->local_directory = new_local_dir;

				cherokee_buffer_make_empty (conn->request);
				cherokee_buffer_add (conn->request, index, index_len);

				return instance_new_nondir_handler (hdl, cnt, properties);
			}

			/* Stat() the possible new path
			 */
			cherokee_buffer_add (conn->local_directory, index, index_len);
			exists = (stat (conn->local_directory->buf, &info) == 0);
			cherokee_buffer_drop_endding (conn->local_directory, index_len);

			/* If the file doesn't exist or it is a directory, try with the next one
			 */
			if (!exists) continue;
			if (S_ISDIR(info.st_mode)) continue;
			
			/* Add the index file to the request and clean up
			 */
			cherokee_buffer_drop_endding (conn->local_directory, conn->request->len); 
			cherokee_buffer_add (conn->request, index, index_len);

			return instance_new_nondir_handler (hdl, cnt, properties);
		} 

		/* If t	he dir hasn't a index file, it uses dirlist
		 */
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return cherokee_handler_dirlist_new (hdl, cnt, properties);
	}

	/* Unknown request type
	 */
	conn->error_code = http_internal_error;
	SHOULDNT_HAPPEN;
	return ret_error;
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
