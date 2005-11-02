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
#include "reqs_list.h"
#include "regex.h"
#include "pcre/pcre.h"


ret_t 
cherokee_reqs_list_init (cherokee_reqs_list_t *rl)
{
	INIT_LIST_HEAD (rl);
	return ret_ok;
}


ret_t 
cherokee_reqs_list_mrproper (cherokee_reqs_list_t *rl)
{
	// TODO!
	return ret_ok;
}


ret_t 
cherokee_reqs_list_get (cherokee_reqs_list_t     *rl, 
			cherokee_buffer_t        *requested_url, 
			cherokee_config_entry_t  *plugin_entry,
			cherokee_regex_table_t   *regexs)
{
	ret_t   ret;
	list_t *i;
	list_t *reqs = (list_t *)rl;
	
	if (regexs == NULL) 
		return ret_ok;
	
	list_for_each (i, reqs) {
		int   rei;
		pcre *re                            = NULL;
		char *request_pattern               = RQ_ENTRY(i)->request.buf;
		cherokee_reqs_list_entry_t  *lentry = list_entry (i, cherokee_reqs_list_entry_t, list_entry);
		cherokee_config_entry_t     *entry  = &lentry->base_entry;

		printf ("i %p: %s\n", i, request_pattern);
		
		if (request_pattern == NULL)
			continue;
		
		ret = cherokee_regex_table_get (regexs, request_pattern, (void **)&re);
		if (ret != ret_ok) continue;
		
		rei = pcre_exec (re, NULL, requested_url->buf, requested_url->len, 0, 0, NULL, 0);
		if (rei <= 0) continue;
		
		cherokee_config_entry_complete (plugin_entry, entry, false);
		return ret_ok;
	}
	
	return ret_not_found;
}


ret_t 
cherokee_reqs_list_add  (cherokee_reqs_list_t       *rl, 
			 cherokee_reqs_list_entry_t *plugin_entry,
			 cherokee_regex_table_t     *regexs)
{
	/* Add the new connection
	 */
	list_add (&plugin_entry->list_entry, (list_t *)rl);
	
	/* Compile the expression
	 */
	if (regexs != NULL) {
		if (! cherokee_buffer_is_empty (&plugin_entry->request)) {
			cherokee_regex_table_add (regexs, plugin_entry->request.buf);
		}
	}
	
	return ret_ok;
}
