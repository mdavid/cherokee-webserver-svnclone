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

#include "unix4win32.h"
#include <conio.h>
#include <sys/timeb.h>
#include <tchar.h>
#include <assert.h>


static HANDLE hEventSource    = NULL;
static char   szServiceName[] = "Cherokee";


void 
openlog (const char *ident, int logopt, int facility)
{
        hEventSource = RegisterEventSource(NULL, TEXT(szServiceName));
}

void 
syslog (int priority, const char *message, ...)
{
        TCHAR    szMsg[1024];
	LPCSTR  *pmsg;
        va_list  args;
	
	/* Initialize variable arguments. 
	 */
        va_start (args, message);     
        vsprintf (szMsg, message, args);
        errno = GetLastError();
	
        *pmsg = (LPCSTR)szMsg;

        if (hEventSource != NULL) {
		ReportEvent(hEventSource,    /* handle of event source */
			    (WORD)priority,  /* event type */
			    0,               /* event category */
			    0,               /* event ID */
			    NULL,            /* current user's SID */
			    1,               /* strings in lpszStrings */
			    0,               /* no bytes of raw data */
			    pmsg,            /* array of error strings */
			    NULL);           /* no raw data */
        }
}

void 
closelog (void)
{
        DeregisterEventSource(hEventSource);
}




int 
getdtablesize (void)
{
        return OPEN_MAX;
}



static char          *login_strings[] = {"LOGIN", "USER", "MAILNAME", NULL};
static char          *anonymous       = "anonymous";
static char          *login_shell     = "not command.com!";
static char          *home_dir        = ".";
static char          *login           = NULL;
static char          *group           = NULL;
static struct passwd  __pw;
static struct group   __gr;


static char *
lookup_env (char *table[])
{
	char *ptr;
	char *entry;
	size_t len;
	
	/* scan table 
	 */
	while (*table && !(ptr = getenv (*table++))) ;        
	if (ptr == NULL) return NULL;

	len = strcspn (ptr, " \n\t\n\r");

	if (!(entry = malloc (len + 1))) {
		PRINT_ERROR ("Out of memory.\n");
	}
	
	strncpy (entry, ptr, len);
	entry[len] = '\0';

	return entry;
}


static char * 
win32getlogin ()
{
        static char name[256];
        DWORD dw = 256;
        GetUserName(name, &dw);
        return strdup(name);
}


/* return something like a username. 
*/
char *
getlogin (void)
{
	if (login == NULL) {
		login = win32getlogin();
	}

	if (login == NULL) {
		login = lookup_env (login_strings);
	}

	if (login == NULL) {
		login = anonymous;
	}

	return login;
}



/* return something like a username in a (butchered!) passwd structure. 
 */
struct passwd *
getpwuid (int uid)
{
	__pw.pw_name  = getlogin ();
	__pw.pw_dir   = home_dir;
	__pw.pw_shell = login_shell;
	__pw.pw_uid   = 0;
	
	return &__pw;
}

/* everyone is root on WIN32 
 */
struct passwd *
getpwnam (char *name)
{
        struct passwd* pw = (struct passwd*) malloc(sizeof(struct passwd));
        memset (pw, '\0', sizeof(struct passwd));
        return pw;
}
