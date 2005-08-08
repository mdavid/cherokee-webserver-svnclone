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
#include <sys/types.h>
#include <dirent.h>

#include "module_read_config.h"
#include "module_loader.h"
#include "server.h"
#include "server-protected.h"
#include "list_ext.h"
#include "mime.h"


cherokee_module_info_t MODULE_INFO(read_config) = {
	cherokee_generic,                  /* type     */
	cherokee_module_read_config_new    /* new func */
};



ret_t 
cherokee_module_read_config_new  (cherokee_module_read_config_t **config)
{
	CHEROKEE_NEW_STRUCT(n,module_read_config);

	cherokee_module_init_base (MODULE(n));

	MODULE(n)->new  = (module_func_new_t)  cherokee_module_read_config_new;
	MODULE(n)->free = (module_func_free_t) cherokee_module_read_config_free;

	*config = n;
	return ret_ok;
}


ret_t 
cherokee_module_read_config_free (cherokee_module_read_config_t *config)
{
	free (config);
	return ret_ok;
}



/* Read config stuff
 */

static ret_t
read_config_single_file (cherokee_server_t *srv, char *filename)
{
	int   error;
	void *bufstate;
	extern FILE *yyin;
	extern char *current_yacc_file;

	extern int   yyparse             (void *);
	extern void  yyrestart           (FILE *);
	extern void *yy_create_buffer    (FILE *file, int size);
	extern void  yy_switch_to_buffer (void *);
	extern void  yy_delete_buffer    (void *);

	/* Maybe set a default configuration filename
	 */
	if (filename == NULL) {
		filename = (srv->config_file)? srv->config_file : CHEROKEE_CONFDIR"/cherokee.conf";

	} else {
		/* Maybe free an old filename
		 */
		if (srv->config_file != NULL) {
			free (srv->config_file);
		}
		
		/* Store last configuration filename
		 */
		srv->config_file = strdup(filename);
	}

	/* Set the file to read
	 */
	current_yacc_file = filename;

	yyin = fopen (filename, "r");
	if (yyin == NULL) {
		PRINT_ERROR("Can't read the configuration file: '%s'\n", filename);
		return ret_error;
	}

	/* Cooooome on :-)
	 */
	yyrestart(yyin);

	bufstate = (void *) yy_create_buffer (yyin, 65535);
	yy_switch_to_buffer (bufstate);
	error = yyparse ((void *)srv);
	yy_delete_buffer (bufstate);

	fclose (yyin);

	return (error == 0) ? ret_ok : ret_error;
}


static ret_t
read_config_path (cherokee_server_t *srv, char *path)
{
	int         re;
	ret_t       ret;
	struct stat info;
	
	re = stat (path, &info);
	if (re < 0) return ret_not_found;
	
	if (S_ISREG(info.st_mode)) {
		ret = read_config_single_file (srv, path);
		if (unlikely(ret < ret_ok)) return ret;
		
	} else if (S_ISDIR(info.st_mode)) {
		DIR *dir;
		int entry_len;
		struct dirent *entry;
		
		dir = opendir(path);
		if (dir == NULL) return ret_error;
		
		while ((entry = readdir(dir)) != NULL) {
			char *full_new;
			int   full_new_len;

			/* Ignore backup files
			 */
			entry_len = strlen(entry->d_name);

			if ((entry->d_name[0] == '.') ||
			    (entry->d_name[0] == '#') ||
			    (entry->d_name[entry_len-1] == '~')) 
			{
				continue;
			}
			
			full_new_len = strlen(path) + strlen(entry->d_name) + 2;
			full_new = (char *)malloc(full_new_len);
			if (full_new == NULL) return ret_error;

			snprintf (full_new, full_new_len, "%s/%s", path, entry->d_name);
			cherokee_list_add_tail (&srv->include_list, full_new);
		}
		
		closedir (dir);
	} else {
		SHOULDNT_HAPPEN;
		return ret_error;
	}
	
	return ret_ok;
}


ret_t
read_config_file (cherokee_server_t *srv, char *filename)
{
	ret_t   ret;
	list_t *i;
	
	/* Support null filename as default config file
	 */
	if (filename == NULL) {
		filename = CHEROKEE_CONFDIR"/cherokee.conf";
	}

	/* Read the main config file
	 */
	ret = read_config_path (srv, filename);
	if (unlikely(ret < ret_ok)) return ret;

	/* Process the includes form the config file
	 */
	list_for_each (i, &srv->include_list) {
		char *path = LIST_ITEM_INFO(i);

		ret = read_config_path (srv, path);
		if (unlikely(ret < ret_ok)) return ret;
	}


#ifndef CHEROKEE_EMBEDDED
	/* Maybe read the Icons file
	 */
	if (srv->icons_file != NULL) {
		ret = cherokee_icons_read_config_file (srv->icons, srv->icons_file);
		if (ret < ret_ok) {
			PRINT_ERROR_S ("Cannot read the icons file\n");
		}
	}

	/* Maybe read the MIME file
	 */
	if (srv->mime_file != NULL) {
		do {
			cherokee_mime_t *mime = NULL;

			ret = cherokee_mime_get_default (&mime);

			if (ret == ret_not_found) {
				ret = cherokee_mime_init (&mime);
			}

			if (ret < ret_ok) {
				PRINT_ERROR_S ("Can not get default MIME configuration file\n");
				break;
			}
			
			ret = cherokee_mime_load (mime, srv->mime_file);
			if (ret < ret_ok) {
				PRINT_ERROR ("Can not load MIME configuration file %s\n", srv->mime_file);
			}
		} while (0);
	}
#endif

	return ret;
}


ret_t 
read_config_string (cherokee_server_t *srv, char *config_string)
{
	ret_t   ret;
	int     error;
	void   *bufstate;

	extern int  yyparse             (void *);
	extern int  yy_scan_string      (const char *);
	extern void yy_switch_to_buffer (void *);
	extern void yy_delete_buffer    (void *);

	bufstate = (void *) yy_scan_string (config_string);
	yy_switch_to_buffer(bufstate);

	error = yyparse((void *)srv);

	yy_delete_buffer (bufstate);

	ret = (error == 0) ? ret_ok : ret_error;
	if (ret < ret_ok) {
		return ret;
	}
	
#ifndef CHEROKEE_EMBEDDED
	/* Maybe read the Icons file
	 */
	if (srv->icons_file != NULL) {
		ret = cherokee_icons_read_config_file (srv->icons, srv->icons_file);
		if (ret < ret_ok) {
			PRINT_ERROR_S ("Cannot read the icons file\n");
		}
	}

	/* Maybe read the MIME file
	 */
	if (srv->mime_file != NULL) {
		do {
			cherokee_mime_t *mime = NULL;

			ret = cherokee_mime_get_default (&mime);

			if (ret == ret_not_found) {
				ret = cherokee_mime_init (&mime);
			}

			if (ret < ret_ok) {
				PRINT_ERROR_S ("Can not get default MIME configuration file\n");
				break;
			}
			
			ret = cherokee_mime_load (mime, srv->mime_file);
			if (ret < ret_ok) {
				PRINT_ERROR ("Can not load MIME configuration file %s\n", srv->mime_file);
			}
		} while (0);
	}	
#endif

	return ret;
}




/* Library init function
 */
void
MODULE_INIT(read_config) (cherokee_module_loader_t *loader)
{
}
