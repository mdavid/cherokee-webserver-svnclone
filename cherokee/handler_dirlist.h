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

#ifndef CHEROKEE_DIRLIST_HANDLER_H
#define CHEROKEE_DIRLIST_HANDLER_H

#include "common-internal.h"

#include <dirent.h>

#include "buffer.h"
#include "handler.h"
#include "module_loader.h"


typedef struct {
	cherokee_handler_t handler;

	DIR  *dir;	
	int page_begining;
	int num;

	/* Properties
	 */
	char *bgcolor;     /* background color for the document */
	char *text;        /* color for the text of the document */
	char *link;        /* color for unvisited hypertext links */
	char *vlink;       /* color for visited hypertext links */
	char *alink;       /* color for active hypertext links */
	char *background;  /* URL for an image to be used to tile the background */

	int show_size;
	int show_date;
	int show_owner;
	int show_group;
	
	cherokee_buffer_t *header;       /* Header content */
	cherokee_buffer_t *header_file;  /* Header filename */

} cherokee_handler_dirlist_t;

#define DHANDLER(x)  ((cherokee_handler_dirlist_t *)(x))


/* Library init function
 */
void dirlist_init                  (cherokee_module_loader_t *loader);
ret_t cherokee_handler_dirlist_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties);

/* virtual methods implementation
 */
ret_t cherokee_handler_dirlist_init        (cherokee_handler_dirlist_t *dhdl);
ret_t cherokee_handler_dirlist_free        (cherokee_handler_dirlist_t *dhdl);
ret_t cherokee_handler_dirlist_step        (cherokee_handler_dirlist_t *dhdl, cherokee_buffer_t *buffer);
ret_t cherokee_handler_dirlist_add_headers (cherokee_handler_dirlist_t *dhdl, cherokee_buffer_t *buffer);


#endif /* CHEROKEE_DIRLIST_HANDLER_H */
