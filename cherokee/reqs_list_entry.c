/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2007 Alvaro Lopez Ortega
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
#include "reqs_list_entry.h"

ret_t 
cherokee_reqs_list_entry_new  (cherokee_reqs_list_entry_t **entry)
{
	CHEROKEE_NEW_STRUCT (n, reqs_list_entry);

	/* Init properties
	 */
	memset (n->ovector, 0, sizeof(int)*OVECTOR_LEN);
	n->ovecsize = 0;

	cherokee_buffer_init (&n->request);
	INIT_LIST_HEAD (&n->list_node);

	/* Init base class
	 */
	cherokee_config_entry_init (CONF_ENTRY(n));

	*entry = n;	
	return ret_ok;
}


ret_t 
cherokee_reqs_list_entry_free (cherokee_reqs_list_entry_t *entry)
{
	if (entry == NULL)
		return ret_ok;

	cherokee_buffer_mrproper (&entry->request);
	cherokee_config_entry_mrproper (CONF_ENTRY(entry));
	
	free (entry);
	return ret_ok;
}

