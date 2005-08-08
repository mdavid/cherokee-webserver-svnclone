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

#ifndef CHEROKEE_CGI_H
#define CHEROKEE_CGI_H

#include "common-internal.h"
#include "connection.h"


typedef void (* cherokee_cgi_set_env_pair_t) (void *param, char *key, int key_len, char *val, int val_len);

ret_t cherokee_cgi_build_basic_env (cherokee_connection_t       *conn,
				    cherokee_cgi_set_env_pair_t  set_env_pair, 
				    cherokee_buffer_t           *tmp,
				    void                        *param);

#endif /* CHEROKEE_CGI_H */
