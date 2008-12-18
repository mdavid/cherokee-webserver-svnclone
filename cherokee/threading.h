/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2008 Alvaro Lopez Ortega
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

#ifndef CHEROKEE_THREADING_H
#define CHEROKEE_THREADING_H

#include "common.h"
#include <pthread.h>

#ifdef PTHREAD_ADAPTIVE_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t cherokee_mutexattr_fast;
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
extern pthread_mutexattr_t cherokee_mutexattr_errorcheck;
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
# define CHEROKEE_MUTEX_ERRORCHECK (&cherokee_mutexattr_errorcheck)
#else
# define CHEROKEE_MUTEX_ERRORCHECK NULL
#endif

#ifdef PTHREAD_ERRORCHECK_MUTEX_INITIALIZER_NP
# define CHEROKEE_MUTEX_FAST (&cherokee_mutexattr_fast)
#else
# define CHEROKEE_MUTEX_FAST NULL
#endif

ret_t cherokee_threading_init (void);
ret_t cherokee_threading_free (void);

#endif /* CHEROKEE_BOGOTIME_H */
