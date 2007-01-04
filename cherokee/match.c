/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Juan Cespedes <cespedes@debian.org>
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
#include "match.h"


ret_t
match (const char *pattern, const char *filename) 
{
	if (!pattern[0] && !filename[0]) {
		return ret_ok;
	} else if (!pattern[0]) {
		return ret_not_found;
	} else if (pattern[0]=='?' && filename[0]) {
		return match(&pattern[1], &filename[1]);
	} else if (pattern[0]!='*') {
		if (pattern[0]==filename[0]) {
			return match(&pattern[1], &filename[1]);
		} else {
			return ret_not_found;
		}
	}   /* Hay un '*' */

	pattern++;
	
	do {
		if (match(pattern, filename)) {
			return ret_ok;
		}
	} while (*filename++);
	
	return ret_not_found;
}
