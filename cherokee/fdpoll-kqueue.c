/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Hiroshi Yamashita <piro7@SoftHome.net>
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

#include <errno.h>
#include <unistd.h>
#include <sys/types.h>
#include <sys/event.h>
#include <sys/time.h>


/***********************************************************************/
/* kqueue()                                                            */
/*                                                                     */
/* #include <sys/event.h>                                              */
/*                                                                     */
/* int kevent();                                                       */
/*                                                                     */
/***********************************************************************/

typedef struct {
	struct cherokee_fdpoll poll;

        int  kq;
        int  nevents;
        int  nchanges;
        int *clidx;
        int *elidx;
        struct kevent *eventlist;
        struct kevent *changelist;
} cherokee_fdpoll_kqueue_t;


static ret_t
_free (cherokee_fdpoll_kqueue_t *fdp)
{
	free (fdp->eventlist);
	free (fdp->changelist);
	free (fdp->clidx);
	free (fdp->elidx);
       
	free (fdp);
	return ret_ok;
}

static ret_t
_add (cherokee_fdpoll_kqueue_t *fdp, int fd, int rw)
{
//	LOCK_WRITER(fdp);

	fdp->changelist[fdp->nchanges].ident = fd;
	fdp->changelist[fdp->nchanges].flags = EV_ADD;
	switch (rw)
        {
        case 0:  
		fdp->changelist[fdp->nchanges].filter = EVFILT_READ; 
		break;
        case 1: 
		fdp->changelist[fdp->nchanges].filter = EVFILT_WRITE; 
		break;
        }
	fdp->clidx[fd] = fdp->nchanges;
	fdp->nchanges++;

//	UNLOCK(fdp);
	return ret_ok;
}

static void
_set_mode (cherokee_fdpoll_kqueue_t *fdp, int fd, int rw)
{
	   _add(fdp,fd,rw);
}


static ret_t
_del (cherokee_fdpoll_kqueue_t *fdp, int fd)
{
/*
	   fdp->changelist[fdp->nchanges].ident = fd;
	   fdp->changelist[fdp->nchanges].flags = EV_DELETE;
	   fdp->nchanges++;
*/
	   return ret_ok;
}


static int
_watch (cherokee_fdpoll_kqueue_t *fdp, int timeout_msecs)
{
	int ret,i;
	struct timespec ts;

	ts.tv_sec  = timeout_msecs / 1000L;
	ts.tv_nsec = (timeout_msecs %1000L) * 1000000L;

	ret = kevent (fdp->kq, fdp->changelist, fdp->nchanges, fdp->eventlist, FDPOLL(fdp)->nfiles, &ts);

	fdp->nchanges = 0;
	if (ret == -1) {
		return -1;
	}

	for(i=0;i<ret;i++) {
		fdp->elidx[fdp->eventlist[i].ident] = i;
	}

	return ret;
}


static int
_check (cherokee_fdpoll_kqueue_t *fdp, int fd, int rw)
{
	int ret,evidx;

	evidx = fdp->elidx[fd];

	if (fdp->eventlist[evidx].ident != fd) {
		return 0;
	}

	if (fdp->eventlist[evidx].flags & EV_ERROR) {
		return 0;
	}

	ret = 0;
	switch (rw) {
        case 0: 
		ret = fdp->eventlist[evidx].filter == EVFILT_READ;
		break;
        case 1: 
		ret = fdp->eventlist[evidx].filter == EVFILT_WRITE;
		break;
	}

	if (ret) {
		_add (fdp, fd, rw);
	}

	return ret;
}

static ret_t
_reset (cherokee_fdpoll_kqueue_t *fdp, int fd)
{
}



ret_t
fdpoll_kqueue_new (cherokee_fdpoll_t **fdp, int system_fd_limit, int fd_limit)
{
	cherokee_fdpoll_t *nfd;
	CHEROKEE_NEW_STRUCT (n, fdpoll_kqueue);

	nfd = FDPOLL(&n->poll);

	/* Look for max fd limit
	 */
	nfd->type          = cherokee_poll_kqueue;
	nfd->nfiles        = fd_limit;
	nfd->system_nfiles = system_fd_limit;

	/* Init base class virtual methods
	 */
	nfd->free          = (fdpoll_func_free_t) _free;
	nfd->add           = (fdpoll_func_add_t) _add;
	nfd->del           = (fdpoll_func_del_t) _del;
	nfd->reset         = (fdpoll_func_reset_t) _reset;
	nfd->set_mode      = (fdpoll_func_set_mode_t) _set_mode;
	nfd->check         = (fdpoll_func_check_t) _check;
	nfd->watch         = (fdpoll_func_watch_t) _watch;	
       
	/* Get memory
	 */
	n->nevents       = 0;
	n->nchanges      = 0;

	n->kq = kqueue();

	n->eventlist = (struct kevent *) malloc (sizeof(struct kevent) * nfd->nfiles);
	return_if_fail (n->eventlist, ret_nomem);

	n->changelist = (struct kevent *) malloc (sizeof(struct kevent) * nfd->nfiles);
	return_if_fail (n->changelist, ret_nomem);
          
	n->clidx = (int*) malloc (sizeof(int) * nfd->system_nfiles);
	return_if_fail (n->clidx, ret_nomem);

	n->elidx = (int*) malloc (sizeof(int) * nfd->system_nfiles);
	return_if_fail (n->elidx, ret_nomem);

	memset(n->changelist,0,sizeof(struct kevent) * nfd->nfiles);
	memset(n->eventlist,0,sizeof(struct kevent) * nfd->nfiles);
	memset(n->clidx,0,sizeof(int) * nfd->system_nfiles);
	memset(n->elidx,0,sizeof(int) * nfd->system_nfiles);

	/* Return it
	 */
	*fdp = FDPOLL(n);
	return ret_ok;
}

