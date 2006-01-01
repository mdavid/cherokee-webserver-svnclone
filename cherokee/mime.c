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

#include "mime.h"
#include "mime-protected.h"


extern int  yy_mime_restart           (FILE *);
extern int  yy_mime__create_buffer    (FILE *, int size);
extern void yy_mime__switch_to_buffer (void *);
extern void yy_mime__delete_buffer    (void *);


static ret_t 
cherokee_mime_new (cherokee_mime_t **mime)
{
	CHEROKEE_NEW_STRUCT(n, mime);

	cherokee_table_init (&n->mime_table);

	INIT_LIST_HEAD(&n->mime_list);
	INIT_LIST_HEAD(&n->name_list);
	   
	/* Return the object
	 */
	*mime = n;
	return ret_ok;
}

ret_t
cherokee_mime_free (cherokee_mime_t *mime)
{
	list_t *i, *tmp;

	cherokee_table_clean (&mime->mime_table);

	list_for_each_safe (i, tmp, &mime->mime_list) {
		list_del (i);
		cherokee_mime_entry_free (MIME_ENTRY(i));
	}

	free (mime);
	return ret_ok;
}


ret_t
cherokee_mime_load (cherokee_mime_t *mime, char *filename)
{
	int   error;
	char *file;
	void *bufstate;

	extern FILE *yy_mime_in;
	extern int yy_mime_parse (void *);

	file = (filename) ? filename : CHEROKEE_CONFDIR"/mime.conf";

	yy_mime_in = fopen (file, "r");
	if (yy_mime_in == NULL) {
		PRINT_ERROR("Can't read the mime spec file: '%s'\n", file);
		return ret_error;
	}

	yy_mime_restart(yy_mime_in);

	bufstate = (void *) yy_mime__create_buffer (yy_mime_in, 65535);
	yy_mime__switch_to_buffer (bufstate);
	error = yy_mime_parse (mime);
	yy_mime__delete_buffer (bufstate);

	fclose (yy_mime_in);

	return (error)? ret_error : ret_ok;
}



ret_t 
cherokee_mime_get (cherokee_mime_t *mime, char *suffix, cherokee_mime_entry_t **entry)
{
	return cherokee_table_get (MIME_TABLE(mime), suffix, (void **)entry);
}


ret_t 
cherokee_mime_add (cherokee_mime_t *mime, cherokee_mime_entry_t *entry)
{
	list_add ((list_t *)entry, MIME_LIST(mime));
	return ret_ok;
}



/* Mime global
 */

static cherokee_mime_t *_mime_global  = NULL;

ret_t 
cherokee_mime_init (cherokee_mime_t **mime)
{
	ret_t ret;

	if (_mime_global == NULL) {
		ret = cherokee_mime_new (&_mime_global);
		if (unlikely(ret < ret_ok)) return ret;
	}

	*mime = _mime_global;
	return ret_ok;
}

ret_t 
cherokee_mime_get_default (cherokee_mime_t **mime)
{
	if (_mime_global == NULL) {
		return ret_not_found;
	}
	
	*mime = _mime_global;
	return ret_ok;
}



/* Mime entry methods
 */

ret_t 
cherokee_mime_entry_new  (cherokee_mime_entry_t **mentry)
{
	CHEROKEE_NEW_STRUCT(n, mime_entry);

	INIT_LIST_HEAD(&n->base);
	n->max_age   = -1;
	cherokee_buffer_new (&n->mime_name);

	*mentry = n;
	return ret_ok;
}


ret_t 
cherokee_mime_entry_free (cherokee_mime_entry_t *mentry)
{
	if (mentry->mime_name != NULL) {
		cherokee_buffer_free (mentry->mime_name);
		mentry->mime_name = NULL;
	}

	free (mentry);
	return ret_ok;
}
