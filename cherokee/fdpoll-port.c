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
#include "fdpoll-protected.h"

#include <stdio.h>
#include <poll.h>
#include <port.h>
#include <unistd.h>

/***********************************************************************/
/* Solaris 10: Event ports                                             */
/*                                                                     */
/* #include <port.h>                                                   */
/*                                                                     */
/* int port_get (int port, port_event_t  *pe,                          */
/*               const timespec_t        *timeout);                    */
/*                                                                     */
/* Info:                                                               */
/* http://iforce.sun.com/protected/solaris10/adoptionkit/tech/tecf.html*/
/*                                                                     */
/* Event ports is an event completion framework that provides a        */
/* scalable way for multiple threads or processes to wait for multiple */
/* pending asynchronous events from multiple objects.                  */
/*                                                                     */
/***********************************************************************/


typedef struct {
	struct cherokee_fdpoll poll;

	int port;
} cherokee_fdpoll_port_t;



static ret_t 
_free (cherokee_fdpoll_port_t *fdp)
{
	port_close (fdp->port);

        free (fdp);        
        return ret_ok;	   
}


static ret_t
_add (cherokee_fdpoll_port_t *fdp, int fd, int rw)
{
/*	int port_associate (int port, 
			    int source,  
			    uintptr_t object,
			    int events, 
			    void *user);*/
	     

	port_associate (fdp->port,         /* port */
			PORT_SOURCE_FD,    /* source */
			fd,                /* object */
			rw?POLLOUT:POLLIN, /* events */
			NULL);             /* user */
}


static ret_t
_del (cherokee_fdpoll_port_t *fdp, int fd)
{
}


static int
_watch (cherokee_fdpoll_port_t *fdp, int timeout_msecs)
{
	int          re;
	int          i, fd;
	uint_t       nget;
	timespec_t   to;
	port_event_t pe[5];

	to.tv_sec  = timeout_msecs / 1000L;
	to.tv_nsec = ( timeout_msecs % 1000L ) * 1000000L;
	
	re = port_getn (fdp->port, &pe, 5, &nget, &to);
	if ((re) || (nget == -1)) {
		/* port_close, close ? */
		return ret_error;
	}

	fd = pe.portev_object;
	printf ("fd = %d\n", fd);
	
/*
     int port_get(int port, port_event_t  *pe,  const  timespec_t
     *timeout);

     int port_getn(int port,  port_event_t  list[],  uint_t  max,
     uint_t *nget, const timespec_t *timeout);
*/
}


static int
_check (cherokee_fdpoll_port_t *fdp, int fd, int rw)
{
}


static ret_t
_reset (cherokee_fdpoll_port_t *fdp, int fd)
{
}

static void
_set_mode (cherokee_fdpoll_port_t *fdp, int fd, int rw)
{
}


ret_t 
fdpoll_port_new (cherokee_fdpoll_t **fdp, int sys_limit, int limit)
{
	cherokee_fdpoll_t *nfd;
	CHEROKEE_NEW_STRUCT (n, fdpoll_port);

	nfd = FDPOLL(n);

	/* Init base class properties
	 */
	nfd->type          = cherokee_poll_port;
	nfd->nfiles        = limit;
	nfd->system_nfiles = sys_limit;
	nfd->npollfds      = 0;

	/* Init base class virtual methods
	 */
	nfd->free          = (fdpoll_func_free_t) _free;
	nfd->add           = (fdpoll_func_add_t) _add;
	nfd->del           = (fdpoll_func_del_t) _del;
	nfd->reset         = (fdpoll_func_reset_t) _reset;
	nfd->set_mode      = (fdpoll_func_set_mode_t) _set_mode;
	nfd->check         = (fdpoll_func_check_t) _check;
	nfd->watch         = (fdpoll_func_watch_t) _watch;	

	/* Look for max fd limit	
	 */
	n->port = port_create();

	/* Return the object
	 */
	*fdp = nfd;
	return ret_ok;
}
