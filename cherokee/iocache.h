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

#ifndef CHEROKEE_IOCACHE_H
#define CHEROKEE_IOCACHE_H

#include <cherokee/common.h>
#include <cherokee/server.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

typedef struct cherokee_iocache cherokee_iocache_t;

typedef struct {
	struct stat       state;
	void             *mmaped;
	size_t            mmaped_len;

	int cleaned;
} cherokee_iocache_entry_t;

#define IOCACHE(x)       ((cherokee_iocache_t *)(x))
#define IOCACHE_ENTRY(x) ((cherokee_iocache_entry_t *)(x))

ret_t cherokee_iocache_new_default   (cherokee_iocache_t **iocache, cherokee_server_t *srv);
ret_t cherokee_iocache_get_default   (cherokee_iocache_t **iocache);
ret_t cherokee_iocache_free_default  (cherokee_iocache_t  *iocache);

ret_t cherokee_iocache_clean         (cherokee_iocache_t *iocache);
ret_t cherokee_iocache_clean_up      (cherokee_iocache_t *iocache, cuint_t num);

ret_t cherokee_iocache_mmap_release  (cherokee_iocache_t *iocache, cherokee_iocache_entry_t *file);

ret_t cherokee_iocache_get_or_create_w_mmap (cherokee_iocache_t *iocache, cherokee_buffer_t *filename, cherokee_iocache_entry_t **ret_file, int *fd);
ret_t cherokee_iocache_get_or_create_w_stat (cherokee_iocache_t *iocache, cherokee_buffer_t *filename, cherokee_iocache_entry_t **ret_file);

#endif /* CHEROKEE_IOCACHE_H */
