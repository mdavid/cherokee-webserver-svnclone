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

#include "handler_dirlist.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <pwd.h>
#include <grp.h>
#include <sys/types.h>

#include "util.h"
#include "connection.h"
#include "connection-protected.h"
#include "server.h"
#include "server-protected.h"
#include "module_loader.h"
#include "icons.h"

cherokee_module_info_t cherokee_dirlist_info = {
	cherokee_handler,               /* type     */
	cherokee_handler_dirlist_new    /* new func */
};


ret_t
cherokee_handler_dirlist_new  (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{	
	CHEROKEE_NEW_STRUCT (n, handler_dirlist);
	
	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt);

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_dirlist_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_dirlist_free;
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_dirlist_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_dirlist_add_headers; 

	/* Supported features
	 */
	HANDLER(n)->support     = hsupport_nothing;

	/* Process the request_string, and build the arguments table..
	 * We'll need it later
	 */
	// cherokee_connection_parse_args (cnt);
	
	/* Init
	 */
	n->dir           = NULL;
	n->header        = NULL;
	n->page_begining = 0;

	n->show_size     = 1;
	n->show_date     = 0;
	n->show_owner    = 0;
	n->show_group    = 0;

	n->bgcolor       = "FFFFFF";
	n->text          = "000000";
	n->link          = "0000AA";
	n->vlink         = "0000CC";
	n->alink         = "0022EE";
	n->background    = NULL;
	n->header        = NULL;
	n->header_file   = NULL;

	/* Read the properties
	 */
	if (properties) {
		char *tmp;

		tmp = cherokee_table_get_val (properties, "bgcolor");
		if (tmp) n->bgcolor = tmp;

		tmp = cherokee_table_get_val (properties, "text");
		if (tmp) n->text = tmp;

		tmp = cherokee_table_get_val (properties, "link");
		if (tmp) n->link = tmp;

		tmp = cherokee_table_get_val (properties, "vlink");
		if (tmp) n->vlink = tmp;

		tmp = cherokee_table_get_val (properties, "alink");
		if (tmp) n->alink = tmp;

		tmp = cherokee_table_get_val (properties, "background");
		if (tmp) n->background = tmp;		

		n->show_size  = (cherokee_table_get_val (properties, "size") != NULL);
		n->show_date  = (cherokee_table_get_val (properties, "date") != NULL);
		n->show_owner = (cherokee_table_get_val (properties, "owner") != NULL);
		n->show_owner = (cherokee_table_get_val (properties, "group") != NULL);

		tmp = cherokee_table_get_val (properties, "headerfile");
		if (tmp != NULL) {
			cherokee_buffer_new (&n->header_file);
			cherokee_buffer_add (n->header_file, tmp, strlen(tmp));
		}
	}

	*hdl = HANDLER(n);
	return ret_ok;
}


ret_t
cherokee_handler_dirlist_free (cherokee_handler_dirlist_t *dhdl)
{
	if (dhdl->header != NULL) {
		cherokee_buffer_free (dhdl->header);
		dhdl->header = NULL;
	}

	if (dhdl->header_file != NULL) {
		cherokee_buffer_free (dhdl->header_file);
		dhdl->header_file = NULL;
	}

	if (dhdl->dir != NULL) {
		closedir (dhdl->dir);
		dhdl->dir = NULL;
	}
	 
	return ret_ok;
}


ret_t
cherokee_handler_dirlist_init (cherokee_handler_dirlist_t *dhdl)
{
	cherokee_connection_t *conn = HANDLER_CONN(dhdl);

	/* If it doesn't end with a slash
	 * we have to redirect the request
	 */
	if ((cherokee_buffer_is_empty(conn->request)) ||
	    (!cherokee_buffer_is_endding (conn->request, '/')))
	{
		cherokee_buffer_make_empty (conn->redirect);
		cherokee_buffer_ensure_size (conn->redirect, conn->request->len + conn->userdir->len + 4);

		if (! cherokee_buffer_is_empty (conn->userdir)) {
			cherokee_buffer_add (conn->redirect, "/~", 2);
			cherokee_buffer_add_buffer (conn->redirect, conn->userdir);
		}

		cherokee_buffer_add_buffer (conn->redirect, conn->request);
		cherokee_buffer_add (conn->redirect, "/", 1);
		
		conn->error_code = http_moved_permanently;
		return ret_error;		
	}

	/* Maybe read the header file
	 */
	if ((dhdl->header_file != NULL) && (! cherokee_buffer_is_empty(dhdl->header_file))) 
	{
		cherokee_buffer_add_buffer (conn->local_directory, conn->request);                                 /* (1)  */
		cherokee_buffer_add_buffer (conn->local_directory, dhdl->header_file);
		
		cherokee_buffer_new (&dhdl->header);
		cherokee_buffer_read_file (dhdl->header, conn->local_directory->buf);
		
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len + dhdl->header_file->len); /* (1') */	
	}

	/* Build de local request
	 */
	cherokee_buffer_add_buffer (conn->local_directory, conn->request);
	dhdl->dir = opendir (conn->local_directory->buf);
	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);

	if (dhdl->dir == NULL) {
		conn->error_code = http_not_found;
		return ret_error;
	}

 	return ret_ok;
}


static inline void
init_stat (struct stat *f,
	   char *dir,  int dir_len,
	   char *file, int file_len)
{
	   char *m = (char *) malloc(dir_len + file_len + 1);
	   memcpy (m, dir, dir_len);
	   memcpy (m+dir_len, file, file_len);
	   m[dir_len + file_len] = '\0';

	   stat (m, f);
	   free (m);
}


static ret_t
build_public_path (cherokee_handler_dirlist_t *dhdl, cherokee_buffer_t *buf)
{
	cherokee_connection_t *conn = HANDLER_CONN(dhdl);
	
	if (!cherokee_buffer_is_empty (conn->userdir)) {
		/* ~user local dir request
 		 */
		cherokee_buffer_add (buf, "/~", 2);
		cherokee_buffer_add_buffer (buf, conn->userdir);
	}
	
	cherokee_buffer_add_buffer (buf, conn->request);

	return ret_ok;
}


ret_t
cherokee_handler_dirlist_step (cherokee_handler_dirlist_t *dhdl, cherokee_buffer_t *buffer)
{
	char                  *icon;
	struct dirent         *entry;
	cherokee_connection_t *conn;
	cherokee_icons_t      *icons;

	conn  = HANDLER_CONN(dhdl);
	icons = HANDLER_SRV(dhdl)->icons;

	/* Page header
	 */
	if (dhdl->page_begining == 0) {
		CHEROKEE_NEW(path,buffer);

		cherokee_buffer_add (buffer, "<!DOCTYPE HTML PUBLIC \"-//W3C//DTD HTML 3.2 Final//EN\">" CRLF, 57);

		/* Step 1:
		 * Build the public path
		 */
		build_public_path (dhdl, path);

		/* Add some HTML: title and body begin
		 */
		cherokee_buffer_add (buffer, "<html><head><title>Index of ", 28);
		cherokee_buffer_add_buffer (buffer, path);
		cherokee_buffer_add (buffer, "</title></head><body ", 21);

		cherokee_buffer_add_va (buffer, "bgcolor=\"%s\" text=\"%s\" link=\"%s\" vlink=\"%s\" alink=\"%s\"", 
					dhdl->bgcolor, dhdl->text, dhdl->link, dhdl->vlink, dhdl->alink);

		if (dhdl->background) {
			cherokee_buffer_add_va (buffer, " background=\"%s\"", dhdl->background);
		}

		cherokee_buffer_add (buffer, "><h1>Index of ", 14);
		cherokee_buffer_add_buffer (buffer, path);
		cherokee_buffer_add (buffer, "</h1><hr><pre>", 14);
		
		/* Step 2:
		 * Free the path, we will not need it again
		 */
		cherokee_buffer_free (path);


		if (dhdl->header != NULL) {
			cherokee_buffer_add (buffer, dhdl->header->buf, dhdl->header->len);
		}

		if (icons && (icons->parentdir_icon != NULL)) {
			cherokee_buffer_add_va (buffer, "<a href=\"..\"><img border=\"0\" src=\"%s\" alt=\"[DIR]\"> Parent Directory</a>\n", 
						icons->parentdir_icon);
		} else {
			cherokee_buffer_add (buffer, "<a href=\"..\">Parent Directory</a>\n", 34);
		}

		dhdl->page_begining = 1;
	}
	

	/* Page entries
	 */
	dhdl->num = 0;

	while ((entry = readdir (dhdl->dir)) != NULL)
	{ 
		int is_dir;
 		int entry_len = strlen(entry->d_name);
		struct stat info;
		
		/* Ignore backup files
		 */
		if ((entry->d_name[0] == '.') ||
		    (entry->d_name[0] == '#') ||
		    (entry->d_name[entry_len-1] == '~') ||
		    ((dhdl->header_file) && (strncmp (entry->d_name, dhdl->header_file->buf, dhdl->header_file->len) == 0)))
		{
			continue;
		}

		
		/* Add an item
		 */
		cherokee_buffer_add (conn->local_directory, conn->request->buf, conn->request->len);
		init_stat (&info,  conn->local_directory->buf, conn->local_directory->len, entry->d_name, entry_len);
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);

		/* Add the icon
		 */
		icon   = "";
		is_dir = S_ISDIR(info.st_mode);

		if (icons == NULL) {
			cherokee_buffer_add (buffer, (is_dir) ? "[DIR] " : "[   ] ", 6);
		} else {
			if (is_dir && (icons->directory_icon != NULL)) {
				cherokee_buffer_add_va (buffer, "<img border=\"0\" src=\"%s\" alt=\"[DIR]\"> ", icons->directory_icon);
			} else {
				cherokee_icons_get_icon (icons, entry->d_name, &icon);
				cherokee_buffer_add_va (buffer, "<img border=\"0\" src=\"%s\" alt=\"[   ]\"> ", icon);
			}
		}


		/* Add the filename
		 */
		if (is_dir) {
			cherokee_buffer_add_va (buffer, "<a href=\"%s/\">%s/</a>", entry->d_name, entry->d_name); 
			entry_len++;
		} else {
			cherokee_buffer_add_va (buffer, "<a href=\"%s\">%s</a>", entry->d_name, entry->d_name); 
		}


		/* Maybe add more info
		 */
		if (dhdl->show_size  || dhdl->show_date  ||
		    dhdl->show_owner || dhdl->show_group) 
		{
			const int length = 40;

			/* Add the padding
			 */
			if (entry_len < length) {
				char blank[length];
				memset (blank, ' ', length);
				cherokee_buffer_add (buffer, blank, length-entry_len);
			}
			
			if (dhdl->show_date) {				
				int len;
				char tmp[32];
				len = strftime (tmp, 32, "%d-%b-%Y %H:%M   ", localtime(&info.st_mtime));
				cherokee_buffer_add (buffer, tmp, len);
			}

			if (dhdl->show_size) {
				char tmp[5];
				cherokee_strfsize (info.st_size, tmp);
				cherokee_buffer_add_va (buffer, "%s  ", tmp);
			}

			if (dhdl->show_owner) {
				struct passwd *user;
				char          *name;

				user = getpwuid (info.st_uid);
				name = (user->pw_name) ? user->pw_name : "unknown";

				cherokee_buffer_add_va (buffer, "%s", name);
			}

			if (dhdl->show_group) {
				struct group *user;
				char         *group;

				user = getgrgid (info.st_gid);
				group = (user->gr_name) ? user->gr_name : "unknown";

				cherokee_buffer_add_va (buffer, "%s", group);
			}
		}
		
		cherokee_buffer_add (buffer, "\n", 1);

		/* Add up to 15 each time
		 */
		dhdl->num++;
		if (dhdl->num >= 15) {
			return ret_ok;
		}
	}

	/* Page ending
	 */
	cherokee_buffer_add (buffer, "</pre><hr>", 10);

	if (CONN_SRV(conn)->server_token <= cherokee_version_product) {
		cherokee_buffer_add_version (buffer, HANDLER_SRV(dhdl)->port, ver_full_html);
	} else {
		cherokee_buffer_add_version (buffer, HANDLER_SRV(dhdl)->port, ver_port_html);
	}

	cherokee_buffer_add (buffer, "</body></html>", 14);

	return ret_eof_have_data;
}


ret_t
cherokee_handler_dirlist_add_headers (cherokee_handler_dirlist_t *dhdl, cherokee_buffer_t *buffer)
{
	cherokee_connection_t *conn = CONN(HANDLER(dhdl)->connection);
	
	if (! cherokee_buffer_is_empty (conn->redirect)) {
		cherokee_buffer_add (buffer, "Location: ", 10);
		cherokee_buffer_add_buffer (buffer, conn->redirect);
		cherokee_buffer_add (buffer, CRLF, 2);
	} else {
		cherokee_buffer_add (buffer, "Content-Type: text/html"CRLF, 25);

		/* This handler works on-the-fly sending some files each 
		 * iteration, so it doesn't know the "Content-length:" 
		 * and the server can't use Keep-alive.
		 */
		CONN(HANDLER(dhdl)->connection)->keepalive = 0;		
	}

	return ret_ok;
}



/* Library init function
 */
void
dirlist_init (cherokee_module_loader_t *loader)
{
}
