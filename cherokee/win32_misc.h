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

#ifndef CHEROKEE_WIN32_MISC_H
#define CHEROKEE_WIN32_MISC_H

#include "common-internal.h"


#undef  localtime_r       /* in <pthread.h> */
#define SHUT_WR           SD_SEND
#define strerror(e)       win_strerror(e)
#define pipe(h)           _pipe(h,0,0)


char      *win_strerror (int err);
struct tm *localtime_r (const time_t *time, struct tm *tm);

#endif /* CHEROKEE_WIN32_MISC_H */
