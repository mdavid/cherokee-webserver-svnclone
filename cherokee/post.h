/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2009 Alvaro Lopez Ortega
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#ifndef CHEROKEE_POST_H
#define CHEROKEE_POST_H

typedef enum {
	cherokee_post_read_header_init,
	cherokee_post_read_header_100cont
} cherokee_port_rh_phase_t;

typedef struct {
	off_t                    len;
	cherokee_port_rh_phase_t read_header_phase;
	cherokee_buffer_t        read_header_100cont;
	cherokee_buffer_t        header_surplus;
} cherokee_post_t;



#define POST(x) ((cherokee_post_t *)(x))


CHEROKEE_BEGIN_DECLS

ret_t cherokee_post_init           (cherokee_post_t *post);
ret_t cherokee_post_clean          (cherokee_post_t *post);
ret_t cherokee_post_mrproper       (cherokee_post_t *post);

ret_t cherokee_post_read_header    (cherokee_post_t *post, void *conn);

CHEROKEE_END_DECLS

#endif /* CHEROKEE_POST_H */
