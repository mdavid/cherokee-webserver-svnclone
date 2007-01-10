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

#include "common-internal.h"
#include "fdpoll-protected.h"

#include <errno.h>

#ifdef HAVE_SYS_SELECT_H
# include <sys/select.h>
#endif

#ifndef INFTIM
# define INFTIM -1
#endif

typedef enum {
	fdp_read  = 0,
	fdp_write = 1
} cherokee_fdpoll_rw_t;


/***********************************************************************/
/* This fdpoll backend is based on the select() one..                  */
/*                                                                     */
/* Windows lacks of getrlimit() and setrlimit(), so it isn't possible  */
/* to know the length of the arrays inside the fdpoll object. This is  */
/* basically a broken implementation of fdpoll-select.. suitable for   */
/* for Windows boxes.                                                  */
/*                                                                     */
/***********************************************************************/

typedef struct {
	struct cherokee_fdpoll poll;

        fd_set  master_rfdset;
        fd_set  master_wfdset;
        fd_set  working_rfdset;
        fd_set  working_wfdset;
        int    *select_fds;
        int     maxfd;
        int     maxfd_changed;
} cherokee_fdpoll_select_t;


static ret_t 
_free (cherokee_fdpoll_select_t *fdp)
{
        free (fdp->select_fds);
        free (fdp);
        return ret_ok;
}


static ret_t 
_add (cherokee_fdpoll_select_t *fdp, int fd, int rw)
{
	cherokee_fdpoll_t *nfd = FDPOLL(fdp);

        /* Check the fd limit
         */
        if (cherokee_fdpoll_is_full(FDPOLL(fdp))) {
                return ret_error;
        }

        fdp->select_fds[nfd->npollfds] = fd;
        switch (rw) {
        case fdp_read: 
                FD_SET (fd, &fdp->master_rfdset); 
                break;
        case fdp_write: 
                FD_SET (fd, &fdp->master_wfdset);
                break;
        default: 
                break;
        }

        if (fd > fdp->maxfd) {
                fdp->maxfd = fd;
        }

        nfd->npollfds++;
        return ret_ok;
}


static ret_t 
_del (cherokee_fdpoll_select_t *fdp, int fd)
{
	cherokee_fdpoll_t *nfd = FDPOLL(fdp);
	int                i;

	for (i=0; i < nfd->npollfds; i++) {
		if (fdp->select_fds[i] != fd) 
			continue;
		
		nfd->npollfds--;
		fdp->select_fds[i]             = fdp->select_fds[nfd->npollfds];
		fdp->select_fds[nfd->npollfds] = -1;
		
		FD_CLR (fd, &fdp->master_rfdset);
		FD_CLR (fd, &fdp->master_wfdset);

		if (fd >= fdp->maxfd) {
			fdp->maxfd_changed = 1;
		}
		
		return ret_ok;
	}

	PRINT_ERROR ("Couldn't remove fd %d\n", fd);
	return ret_error;
}


static void  
_set_mode (cherokee_fdpoll_select_t *fdp, int fd, int rw)
{
        ret_t ret;

        ret = _del (fdp, fd);
        if (unlikely(ret < ret_ok)) return;

        _add (fdp, fd, rw);
}


static int   
_check (cherokee_fdpoll_select_t *fdp, int fd, int rw)
{
        switch (rw) {
        case fdp_read: 
                return FD_ISSET (fd, &fdp->working_rfdset);
        case fdp_write: 
                return FD_ISSET (fd, &fdp->working_wfdset);
        }

        return 0;
}


static int
select_get_maxfd (cherokee_fdpoll_select_t *fdp) 
{
	cherokee_fdpoll_t *nfd = FDPOLL(fdp);

        if (fdp->maxfd_changed) {
                int i;

                fdp->maxfd = -1;
                for (i = 0; i < nfd->npollfds; i++) {
                        if (fdp->select_fds[i] > fdp->maxfd ) {
                                fdp->maxfd = fdp->select_fds[i];
                        }
                }

                fdp->maxfd_changed = 0;
        }

        return fdp->maxfd;
}


static int   
_watch (cherokee_fdpoll_select_t *fdp, int timeout_msecs)
{
        int            mfd, r;
	struct timeval timeout;

        fdp->working_rfdset = fdp->master_rfdset;
        fdp->working_wfdset = fdp->master_wfdset;

        mfd = select_get_maxfd(fdp);

	if (mfd < 0) 
		sleep(1);
	
        if (timeout_msecs == INFTIM) {
                r = select (mfd + 1, &fdp->working_rfdset, &fdp->working_wfdset, NULL, NULL);
        } else {
                timeout.tv_sec = timeout_msecs / 1000L;
                timeout.tv_usec = ( timeout_msecs % 1000L ) * 1000L;

                r = select (mfd + 1, &fdp->working_rfdset, &fdp->working_wfdset, NULL, &timeout);
        }

        return r;
}


static ret_t 
_reset (cherokee_fdpoll_select_t *fdp, int fd)
{
	return ret_ok;
}



ret_t 
fdpoll_win32_new (cherokee_fdpoll_t **fdp, int system_fd_limit, int fd_limit)
{
        int                i;
	cherokee_fdpoll_t *nfd;
        CHEROKEE_NEW_STRUCT (n, fdpoll_select);

	nfd = FDPOLL(n);
        
	/* Init base class properties
	 */
	nfd->type          = cherokee_poll_win32;
        nfd->nfiles        = fd_limit;
        nfd->system_nfiles = system_fd_limit;
        nfd->npollfds      =  0;

	/* Init base class virtual methods
	 */
	nfd->free          = (fdpoll_func_free_t) _free;
	nfd->add           = (fdpoll_func_add_t) _add;
	nfd->del           = (fdpoll_func_del_t) _del;
	nfd->reset         = (fdpoll_func_reset_t) _reset;
	nfd->set_mode      = (fdpoll_func_set_mode_t) _set_mode;
	nfd->check         = (fdpoll_func_check_t) _check;
	nfd->watch         = (fdpoll_func_watch_t) _watch;	

	/* Init properties
	 */
        FD_ZERO (&n->master_rfdset);
        FD_ZERO (&n->master_wfdset);

        n->select_fds    = (int*) malloc(sizeof(int) * nfd->nfiles);
        n->maxfd         = -1;
        n->maxfd_changed =  0;

        for (i = 0; i < nfd->nfiles; ++i) {
                n->select_fds[i] = -1;
        }

        *fdp = nfd;
        return ret_ok;
}
