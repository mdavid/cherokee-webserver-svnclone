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

#ifndef CHEROKEE_H
#define CHEROKEE_H

#define CHEROKEE_INSIDE_CHEROKEE_H 1

/* Base library
 */
#include <cherokee/macros.h>
#include <cherokee/common.h>
#include <cherokee/util.h>
#include <cherokee/buffer.h>
#include <cherokee/fdpoll.h>
#include <cherokee/table.h>
#include <cherokee/http.h>
#include <cherokee/list.h>
#include <cherokee/mime.h>
#include <cherokee/list.h>
#include <cherokee/list_merge_sort.h>
#include <cherokee/url.h>
#include <cherokee/header.h>
#include <cherokee/resolv_cache.h>
#include <cherokee/typed_table.h>

/* Server library
 */
#include <cherokee/server.h>
#include <cherokee/module.h>
#include <cherokee/logger.h>
#include <cherokee/handler.h>
#include <cherokee/encoder.h>
#include <cherokee/connection.h>
#include <cherokee/module_loader.h>
#include <cherokee/nonce.h>

/* Client library
 */
#include <cherokee/downloader.h>

/* Config library
 */
#include <cherokee/admin_client.h>
#include <cherokee/connection_info.h>


#undef CHEROKEE_INSIDE_CHEROKEE_H

#endif /* CHEROKEE_H */
