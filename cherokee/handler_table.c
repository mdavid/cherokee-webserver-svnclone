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

#include "handler_table.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#ifdef HAVE_DLFCN_H
# include <dlfcn.h>
#endif

#include "handler.h"


ret_t
cherokee_handler_table_new  (cherokee_handler_table_t **pt)
{
	CHEROKEE_NEW_STRUCT(n, handler_table);

	/* Init tables 
	 */
	cherokee_table_init (&n->table);
	
	/* Return the object
	 */
	*pt = n;
	return ret_ok;
}


ret_t
cherokee_handler_table_free (cherokee_handler_table_t *pt)
{
	cherokee_table_clean (&pt->table);
	
	free (pt);
	return ret_ok;
}


ret_t 
cherokee_handler_table_get (cherokee_handler_table_t *pt, cherokee_buffer_t *requested_url, 
			    cherokee_handler_table_entry_t **plugin_entry, cherokee_buffer_t *web_directory)
{
	ret_t  ret;
	char  *slash;
	cherokee_handler_table_entry_t *entry = NULL;

	cherokee_buffer_add_buffer (web_directory, requested_url);

	do {
		ret = cherokee_table_get (&pt->table, web_directory->buf, (void **)&entry);
		
		/* Found
		 */
		if ((ret == ret_ok) && (entry != NULL))
			goto go_out;
 
		if (web_directory->len <= 0)
			goto go_out;


		/* Modify url for next loop:
		 * Find the last / and finish the string there.
		 */
		if (cherokee_buffer_is_endding (web_directory, '/')) {
			cherokee_buffer_drop_endding (web_directory, 1);

		} else {
			slash = strrchr (web_directory->buf, '/');
			if (slash == NULL) goto go_out;

			slash[1] = '\0';
			web_directory->len -= ((web_directory->buf + web_directory->len) - slash -1);
		}

#if 0
		printf ("cherokee_handler_table_get(): requested_url: %s\n", requested_url->buf);
		printf ("cherokee_handler_table_get(): web_directory: %s\n", web_directory->buf);
		printf ("\n");
#endif

	} while (entry == NULL);


go_out:	
	/* Set the in-out values
	 */
	*plugin_entry = entry;

#if 0
	printf ("ptable::GET - entry %p\n", entry);
	if (entry) printf ("ptable::GET - entry->properties: %p\n", entry->properties);
#endif

	return (entry == NULL) ? ret_error : ret_ok;
}


ret_t inline
cherokee_handler_table_add (cherokee_handler_table_t *pt, char *dir, cherokee_handler_table_entry_t *plugin_entry)
{
#if 0
	printf ("cherokee_handler_table_add(table=%p, dir=\"%s\", entry=%p)\n", pt, dir, plugin_entry);
#endif

	/* Add to "dir <-> plugin_entry" table
	 */
	return cherokee_table_add (TABLE(&pt->table), dir, (void*)plugin_entry);
}


