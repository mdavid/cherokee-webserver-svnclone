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
#include "handler_nn.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>

#include "connection.h"
#include "connection-protected.h"
#include "module.h"
#include "module_loader.h"
#include "connection.h"
#include "handler_common.h"
#include "handler_redir.h"
#include "levenshtein_distance.h"


cherokee_module_info_t cherokee_nn_info = {
	cherokee_handler,          /* type     */
	cherokee_handler_nn_new    /* new func */
};


static char *
get_nearest_from_directory (char *directory, char *request)
{
	char          *nearest  = NULL;
	int            min_diff = 9999;
	DIR           *dir;
	struct dirent *entry;


	dir = opendir(directory);
	if (dir == NULL) goto go_out;

	while ((entry = readdir (dir)) != NULL)
	{ 
		int dis;
			 
		if (!strncmp (entry->d_name, ".", 1)) continue;
		if (!strncmp (entry->d_name, "..",2)) continue;

		dis = distance ((char *)request, entry->d_name);
		if (dis < min_diff) {
			min_diff = dis;

			if (nearest != NULL) {
				free (nearest);
			}

			nearest = strdup(entry->d_name);
		}
			 
	}
	closedir (dir);

go_out:
	return nearest;
}


static char *
get_nearest (cherokee_buffer_t *local,
	     cherokee_buffer_t *request)
{
	char              *tmp, *rest;
	char              *ret = NULL;
	cherokee_buffer_t *req;       /* Request w/o last word */
	cherokee_buffer_t *dir_srch;  /* Directory to search in */

	cherokee_buffer_new (&req);
	cherokee_buffer_new (&dir_srch);

	/* Build the local request path
	 */
	rest = strrchr (request->buf, '/');
	if (rest == NULL) goto go_out;
	rest++;

	cherokee_buffer_add (req, local->buf, local->len);
	cherokee_buffer_add (req, request->buf, rest - request->buf);

	tmp = get_nearest_from_directory (req->buf, rest);
	if (tmp == NULL) goto go_out;


	/* Build new web request
	 */
	cherokee_buffer_make_empty (req);
	cherokee_buffer_add (req, request->buf, rest - request->buf);
	cherokee_buffer_add (req, tmp, strlen(tmp));

	ret = strdup (req->buf);
	free (tmp);

go_out:
	cherokee_buffer_free (req);
	cherokee_buffer_free (dir_srch);

	return ret;
}


ret_t 
cherokee_handler_nn_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	ret_t                  ret;
	struct stat            info;
	int                    stat_ret;
	cherokee_connection_t *conn;

	conn = CONN(cnt);

	cherokee_buffer_add (conn->local_directory, conn->request->buf, conn->request->len);
	stat_ret = stat (conn->local_directory->buf, &info);
	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
	
	/* Maybe the file/dir exists
	 */
	if (stat_ret == 0) {
		return cherokee_handler_common_new (hdl, cnt, properties);
	} 
	
	/* Create the redir handler
	 */
	ret = cherokee_handler_redir_new (hdl, cnt, properties);
	if (unlikely(ret < ret_ok)) return ret;

	MODULE(*hdl)->init = (handler_func_init_t) cherokee_handler_nn_init;
	
	return ret;
}


ret_t 
cherokee_handler_nn_init (cherokee_handler_t *hdl)
{
	char                  *new_request;
	cherokee_connection_t *conn;
	conn = CONN(HANDLER(hdl)->connection);
	
	/* Look for the `nearest neighbor'
	 */
	new_request = get_nearest (conn->local_directory, conn->request);
	if (new_request == NULL) {
		CONN(hdl->connection)->error_code = http_not_found;
		return ret_error;
	}

	cherokee_buffer_add (conn->redirect, new_request, strlen(new_request));
	CONN(hdl->connection)->error_code = http_moved_permanently;
	
	return ret_ok;
}



/*   Library init function
 */
static int _nn_is_init = 0;

void
nn_init (cherokee_module_loader_t *loader)
{
	/* Is init?
	 */
	if (_nn_is_init)
		return;
	   
	/* Load the dependences
	 */
	cherokee_module_loader_load (loader, "common");
	cherokee_module_loader_load (loader, "redir");

	_nn_is_init = 1;
}


