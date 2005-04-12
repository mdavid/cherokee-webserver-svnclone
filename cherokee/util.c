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
#include "util.h"

#include <stdlib.h>
#include <stdarg.h>
#include <string.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#endif

#if defined (HAVE_SYS_RESOURCE_H)
# include <sys/resource.h>
#elif defined (HAVE_RESOURCE_H)
# include <resource.h>
#endif


char *cherokee_version = PACKAGE_VERSION;

int
cherokee_hexit (char c)
{
	if ( c >= '0' && c <= '9' )
		return c - '0';
	if ( c >= 'a' && c <= 'f' )
		return c - 'a' + 10;
	if ( c >= 'A' && c <= 'F' )
		return c - 'A' + 10;

	/* shouldn't happen, we're guarded by isxdigit() */
	return 0;           
}


/* This function is licenced under:
 * The Apache Software License, Version 1.1
 *
 * Original name: apr_strfsize()
 *
 * Copyright (c) 2000-2003 The Apache Software Foundation.  
 * All rights reserved.
 */
char *
cherokee_strfsize (unsigned long long size, char *buf)
{
	const char ord[] = "KMGTPE";
	const char *o = ord;
	int remain;

	if (size < 0) {
		return strcpy(buf, "  - ");
	}
	if (size < 973) {
		sprintf(buf, "%3d ", (int) size);
		return buf;
	}
	do {
		remain = (int)(size & 1023);
		size >>= 10;
		if (size >= 973) {
			++o;
			continue;
		}
		if (size < 9 || (size == 9 && remain < 973)) {
			if ((remain = ((remain * 5) + 256) / 512) >= 10)
				++size, remain = 0;
			sprintf(buf, "%d.%d%c", (int) size, remain, *o);
			return buf;
		}
		if (remain >= 512)
			++size;
		sprintf(buf, "%3d%c", (int) size, *o);
		return buf;
	} while (1);
}



char *
cherokee_min_str (char *s1, char *s2)
{
	if ((s1 == NULL) && 
	    (s2 == NULL)) return NULL;

	if ((s1 != NULL) && 
	    (s2 == NULL)) return s1;

	if ((s2 != NULL) && 
	    (s1 == NULL)) return s2;
	
	return (s1<s2) ? s1 : s2;
}


ret_t 
cherokee_sys_fdlimit_get (uint32_t *limit)
{
#ifdef HAVE_GETRLIMIT
        struct rlimit rlp;

        rlp.rlim_cur = rlp.rlim_max = RLIM_INFINITY;
        if (getrlimit(RLIMIT_NOFILE, &rlp))
            return ret_error;

        *limit = rlp.rlim_cur;
	return ret_ok;
#else
#ifdef HAVE_GETDTABLESIZE
	int nfiles;

	nfiles = getdtablesize();
        if (nfiles <= 0) return ret_error;

	*limit = nfiles;
	return ret_ok;
#else
#ifdef OPEN_MAX
        *limit = OPEN_MAX;         /* need to include limits.h somehow */
	return ret_ok;
#else
        *limit = FD_SETSIZE;  
	return ret_ok;
#endif
#endif
#endif
}


ret_t
cherokee_sys_fdlimit_set (uint32_t limit)
{
	int           re;
	struct rlimit rl;

	rl.rlim_cur = limit;
	rl.rlim_max = limit;

	re = setrlimit (RLIMIT_NOFILE, &rl);
	if (re != 0) {
		return ret_error;
	}

	return ret_ok;
}


#ifndef HAVE_STRSEP
char *
strsep (char** str, const char* delims)
{
	char* token;

	if (*str == NULL) {
		/* No more tokens 
		 */
		return NULL;
	}
	
	token = *str;
	while (**str!='\0') {
		if (strchr(delims,**str)!=NULL) {
			**str='\0';
			(*str)++;
			return token;
		}
		(*str)++;
	}
	
	/* There is no other token 	
	 */
	*str=NULL;
	return token;
}
#endif


#ifndef HAVE_STRCASESTR
char *
strcasestr (register char *s, register char *find)
{
	register char c, sc;
	register size_t len;

	if ((c = *find++) != 0) {
		len = strlen(find);
		do {
			do {
				if ((sc = *s++) == 0)
					return (NULL);
			} while (sc != c);
		} while (strncasecmp(s, find, len) != 0);
		s--;
	}
	return ((char *) s);
}
#endif




#ifdef HAVE_PTHREAD
static pthread_mutex_t env_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

int   
cherokee_setenv (const char *name, const char *value, int overwrite)
{
#if !defined (HAVE_PTHREAD) || defined (SETENV_IS_THREADSAFE)
	return setenv(name, value, overwrite);
#else
	int r;

	CHEROKEE_MUTEX_LOCK (&env_mutex);
	r = setenv (name, value, overwrite);
	CHEROKEE_MUTEX_UNLOCK (&env_mutex);

	return r;
# endif
}



#if defined(HAVE_PTHREAD) && !defined(HAVE_GMTIME_R)
static pthread_mutex_t gmtime_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

struct tm *
cherokee_gmtime (const time_t *timep, struct tm *result)
{
#ifndef HAVE_PTHREAD
	struct tm *tmp;

	tmp = gmtime (timep);
	memcpy (result, tmp, sizeof(struct tm));
	return result;	
#else 
# ifdef HAVE_GMTIME_R
	return gmtime_r (timep, result);
# else
	struct tm *tmp;

	CHEROKEE_MUTEX_LOCK (&gmtime_mutex);
	tmp = gmtime (timep);
	memcpy (result, tmp, sizeof(struct tm));
	CHEROKEE_MUTEX_UNLOCK (&gmtime_mutex);

	return result;
# endif
#endif
}


ret_t 
cherokee_split_pathinfo (cherokee_buffer_t  *path, 
			 int                 init_pos,
			 char              **pathinfo,
			 int                *pathinfo_len)
{
	char        *cur;
	struct stat  st;
	
	for (cur = path->buf + init_pos; *cur; ++cur) {
		if (*cur != '/') continue;		
		*cur = '\0';

		/* Handle not found case
		 */
		if (stat (path->buf, &st) == -1) {
			*cur = '/';
			return ret_not_found;
		}

		/* Handle directory case
		 */
		if (S_ISDIR(st.st_mode)) {
			*cur = '/';
			continue;
		}
		
		/* Build the PathInfo string 
		 */
		*cur = '/';
		*pathinfo = cur;
		*pathinfo_len = (path->buf + path->len) - cur;
		return ret_ok;
	}

	*pathinfo_len = 0;
	return ret_ok;
}

