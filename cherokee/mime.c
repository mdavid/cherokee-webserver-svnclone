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
#include "util.h"


ret_t 
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
	cherokee_list_t *i, *tmp;

	cherokee_table_mrproper (&mime->mime_table);

	list_for_each_safe (i, tmp, &mime->mime_list) {
		list_del (i);
		cherokee_mime_entry_free (MIME_ENTRY(i));
	}

	free (mime);
	return ret_ok;
}


ret_t 
cherokee_mime_set_by_suffix (cherokee_mime_t *mime, char *suffix, cherokee_mime_entry_t *entry)
{
	return cherokee_table_add (&mime->mime_table, suffix, (void *)entry);
}

ret_t 
cherokee_mime_get_by_suffix (cherokee_mime_t *mime, char *suffix, cherokee_mime_entry_t **entry)
{
	return cherokee_table_get (&mime->mime_table, suffix, (void **)entry);
}


ret_t 
cherokee_mime_get_by_type (cherokee_mime_t *mime, char *type, cherokee_mime_entry_t **entry)
{
	ret_t            ret;
	cherokee_list_t *i;

	list_for_each (i, &mime->mime_list) {
		cherokee_buffer_t *itype;

		ret = cherokee_mime_entry_get_type (MIME_ENTRY(i), &itype);
		if (ret != ret_ok) continue;

		if (strcmp (type, itype->buf) == 0) {
			*entry = MIME_ENTRY(i);
			return ret_ok;
		}
	}

	return ret_not_found;
}


ret_t 
cherokee_mime_add_entry (cherokee_mime_t *mime, cherokee_mime_entry_t *entry)
{
	list_add (LIST(entry), &mime->mime_list);
	return ret_ok;
}


ret_t 
cherokee_mime_load_mime_types (cherokee_mime_t *mime, char *filename)
{
	ret_t              ret;
	char              *p;
	char              *end;
	cherokee_buffer_t  file = CHEROKEE_BUF_INIT;
	cherokee_buffer_t  ext  = CHEROKEE_BUF_INIT;
	cherokee_buffer_t  type = CHEROKEE_BUF_INIT;


	ret = cherokee_buffer_read_file (&file, filename);
	if (ret != ret_ok) goto error;
	
	p   = file.buf;
	end = file.buf + file.len;

	while (p < end) {
		char   *nl, *c1, *c2, *tmp;
		cuint_t len;
		cherokee_mime_entry_t *entry;

		cherokee_buffer_clean (&ext);
		cherokee_buffer_clean (&type);

		/* Look for the EOL
		 */
		c1 = strchr (p, '\r');
		c2 = strchr (p, '\n');

		nl  = cherokee_min_str (c1, c2);
		if (nl == NULL) break;

		/* Skip comments and empty lines
		 */
		*nl = '\0';
		if (*p == '#') 
			goto next_line;
		
		len = strlen (p);
		if (len < 3)
			goto next_line;

		/* Parse the mime type
		 */
		c1 = strchr (p, ' ');
		c2 = strchr (p, '\t');

		tmp  = cherokee_min_str (c1, c2);
		if (tmp == NULL) goto next_line;

		/* Create the new MIME entry obj
		 */
		cherokee_buffer_add (&type, p, tmp-p);

		ret = cherokee_mime_get_by_type (mime, type.buf, &entry);
		if (ret != ret_ok) {
			cherokee_mime_entry_new (&entry);
			cherokee_mime_add_entry (MIME(mime), entry);
		}

		cherokee_mime_entry_set_type (entry, type.buf);
			
		/* Adds its extensions
		 */
		p = tmp;
		while (p < nl) {
			cherokee_buffer_clean (&ext);
			
			/* Look for the next extension
			 */
			while ((*p == ' ') || (*p == '\t')) p++;
			if (p >= nl) break;

			c1 = strchr (p, ' ');
			c2 = strchr (p, '\t');

			tmp  = cherokee_min_str (c1, c2);
			if (tmp == NULL) tmp = nl;

			/* Add it to the table
			 */
			cherokee_buffer_add (&ext, p, tmp-p);
			cherokee_table_add (&mime->mime_table, ext.buf, entry);

			p = tmp;
		}

	next_line:
		p = nl + 1;
		while ((*p == '\r') || (*p == '\n')) p++;
	}

	cherokee_buffer_mrproper (&ext);
	cherokee_buffer_mrproper (&type);
	cherokee_buffer_mrproper (&file);
	return ret_ok;

error:
	cherokee_buffer_mrproper (&ext);
	cherokee_buffer_mrproper (&type);
	cherokee_buffer_mrproper (&file);
	return ret_error;
}

