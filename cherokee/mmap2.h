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

#ifndef CHEROKEE_MMAP2_H
#define CHEROKEE_MMAP2_H

#include <cherokee/common.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct cherokee_mmap2 cherokee_mmap2_t;

typedef struct {
	struct stat  state;
	void        *mmaped;
	size_t       mmaped_len;
} cherokee_mmap2_entry_t;

#define MMAP2(x)       ((cherokee_mmap2_t *)(x))
#define MMAP2_ENTRY(x) ((cherokee_mmap2_entry_t *)(x))


ret_t cherokee_mmap2_new         (cherokee_mmap2_t **mmap2);
ret_t cherokee_mmap2_free        (cherokee_mmap2_t  *mmap2);
ret_t cherokee_mmap2_clean       (cherokee_mmap2_t  *mmap2);

ret_t cherokee_mmap2_get         (cherokee_mmap2_t *mmap2, char *file, cherokee_mmap2_entry_t **entry);
ret_t cherokee_mmap2_unref_entry (cherokee_mmap2_t *mmap2, cherokee_mmap2_entry_t *entry);
ret_t cherokee_mmap2_clean_up    (cherokee_mmap2_t *mmap2);

#endif /* CHEROKEE_MMAP2_H */
