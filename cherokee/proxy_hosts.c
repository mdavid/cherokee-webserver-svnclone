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

#include "common-internal.h"
#include "proxy_hosts.h"
#include "util.h"


static void
poll_free (void *p)
{
	cherokee_handler_proxy_poll_t *poll = p;
	poll = p;
}

ret_t
cherokee_handler_proxy_hosts_init (cherokee_handler_proxy_hosts_t *hosts)
{
 	CHEROKEE_MUTEX_INIT (&hosts->hosts_mutex, CHEROKEE_MUTEX_FAST);
	cherokee_avl_init (&hosts->hosts);
	cherokee_buffer_init (&hosts->tmp);

	return ret_ok;
}

ret_t
cherokee_handler_proxy_hosts_mrproper (cherokee_handler_proxy_hosts_t *hosts)
{
	CHEROKEE_MUTEX_DESTROY (&hosts->hosts_mutex);
	cherokee_avl_mrproper (&hosts->hosts, poll_free);
	cherokee_buffer_mrproper (&hosts->tmp);

	return ret_ok;
}

ret_t
cherokee_handler_proxy_hosts_get (cherokee_handler_proxy_hosts_t  *hosts,
				  cherokee_source_t               *src,
				  cherokee_handler_proxy_poll_t  **poll,
				  cuint_t                          reuse_max)
{
	ret_t ret;

	CHEROKEE_MUTEX_LOCK (&hosts->hosts_mutex);

	/* Build the index name */
	cherokee_buffer_clean       (&hosts->tmp);
	cherokee_buffer_add_buffer  (&hosts->tmp, &src->host);
	cherokee_buffer_add_char    (&hosts->tmp, ':');
	cherokee_buffer_add_ulong10 (&hosts->tmp, src->port);

	/* Check the hosts tree */
	ret = cherokee_avl_get (&hosts->hosts, &hosts->tmp, (void **)poll);
	switch (ret) {
	case ret_ok:
		break;
	case ret_not_found: {
		cherokee_handler_proxy_poll_t *n;

		ret = cherokee_handler_proxy_poll_new (&n, reuse_max);
		if (ret != ret_ok)
			return ret;

		cherokee_avl_add (&hosts->hosts, &hosts->tmp, n);
		*poll = n;
		break;
	}
	default:
		goto error;
	}

	/* Got it */
	CHEROKEE_MUTEX_UNLOCK (&hosts->hosts_mutex);
	return ret_ok;

error:
	CHEROKEE_MUTEX_LOCK (&hosts->hosts_mutex);
	return ret_error;
}


/* Polls
 */

ret_t
cherokee_handler_proxy_poll_new (cherokee_handler_proxy_poll_t **poll,
				 cuint_t                         reuse_max)
{
	CHEROKEE_NEW_STRUCT (n, handler_proxy_poll);

	n->reuse_len = 0;
	n->reuse_max = reuse_max;

	INIT_LIST_HEAD (&n->active);
	INIT_LIST_HEAD (&n->reuse);
	CHEROKEE_MUTEX_INIT (&n->mutex, CHEROKEE_MUTEX_FAST);

	*poll = n;
	return ret_ok;
}


ret_t
cherokee_handler_proxy_poll_free (cherokee_handler_proxy_poll_t *poll)
{
	cherokee_list_t *i, *j;

	list_for_each_safe (i, j, &poll->active) {
	}

	list_for_each_safe (i, j, &poll->reuse) {
	}

	CHEROKEE_MUTEX_DESTROY (&poll->mutex);
	return ret_ok;
}


ret_t
cherokee_handler_proxy_poll_get (cherokee_handler_proxy_poll_t  *poll,
				 cherokee_handler_proxy_conn_t **pconn,
				 cherokee_source_t              *src)
{
	ret_t            ret;
	cherokee_list_t *i;

	CHEROKEE_MUTEX_LOCK (&poll->mutex);

	if (poll->reuse_len > 0) {
		/* Reuse a prev connection */
		poll->reuse_len -= 1;

		i = poll->reuse.prev;
		cherokee_list_del (i);
		cherokee_list_add (i, &poll->active);

		*pconn = PROXY_CONN(i);
	} else {
		cherokee_handler_proxy_conn_t *n;

		/* Create a new connection */
		ret = cherokee_handler_proxy_conn_new (&n);
		if (ret != ret_ok)
			goto error;

		ret = cherokee_proxy_util_init_socket (&n->socket, src);
		if (ret != ret_ok) {
			cherokee_handler_proxy_conn_free (n);
			goto error;
		}

		cherokee_list_add (&n->listed, &poll->active);
		n->poll_ref = poll;
		*pconn = n;
	}	

	CHEROKEE_MUTEX_UNLOCK (&poll->mutex);
	return ret_ok;
error:
	CHEROKEE_MUTEX_UNLOCK (&poll->mutex);
	return ret_error;
}

static ret_t
poll_release (cherokee_handler_proxy_poll_t *poll,
	      cherokee_handler_proxy_conn_t *pconn)
{
	/* Not longer an active connection */
	cherokee_list_del (&pconn->listed);

	/* Don't reuse connection w/o keep-alive
	 */
	if (! pconn->keepalive_in) {
		cherokee_handler_proxy_conn_free (pconn);
		return ret_ok;
	}

	/* If the reuse-list is full, dispose the oldest obj
	 */
	if (poll->reuse_len > poll->reuse_max) {
		cherokee_handler_proxy_conn_t *oldest;		

		oldest = PROXY_CONN(poll->reuse.prev);
		cherokee_list_del (&oldest->listed);
		poll->reuse_len -= 1;

		cherokee_handler_proxy_conn_free (oldest);
	}

	/* Clean up
	 */
	pconn->keepalive_in = false;
	pconn->size_in      = 0;
	pconn->sent_out     = 0;
	pconn->enc          = pconn_enc_none;

	cherokee_buffer_clean (&pconn->header_in_raw);

	/* Store it to be reused
	 */
	poll->reuse_len += 1;
	cherokee_list_add (&pconn->listed, &poll->reuse);

	return ret_ok;
}
	

/* Conn's
 */

ret_t
cherokee_handler_proxy_conn_new (cherokee_handler_proxy_conn_t **pconn)
{
	CHEROKEE_NEW_STRUCT (n, handler_proxy_conn);

	/* Socket stuff
	 */
	cherokee_socket_init (&n->socket);

	cherokee_buffer_init (&n->header_in_raw);
	cherokee_buffer_ensure_size (&n->header_in_raw, 512);

	n->poll_ref      = NULL;
	n->keepalive_in  = false;
	n->size_in       = 0;
	n->sent_out      = 0;
	n->enc           = pconn_enc_none;

	*pconn = n;
	return ret_ok;
}


ret_t
cherokee_handler_proxy_conn_free (cherokee_handler_proxy_conn_t *pconn)
{
	cherokee_socket_close    (&pconn->socket);
	cherokee_socket_mrproper (&pconn->socket);

	cherokee_buffer_mrproper (&pconn->header_in_raw);

	return ret_ok;
}


ret_t
cherokee_handler_proxy_conn_release (cherokee_handler_proxy_conn_t *pconn)
{
	ret_t                          ret;
	cherokee_handler_proxy_poll_t *poll = pconn->poll_ref;

	CHEROKEE_MUTEX_LOCK (&poll->mutex);
	ret = poll_release (poll, pconn);
	CHEROKEE_MUTEX_UNLOCK (&poll->mutex);

	return ret;
}


ret_t
cherokee_handler_proxy_conn_send (cherokee_handler_proxy_conn_t *pconn,
				  cherokee_buffer_t             *buf)
{
	ret_t  ret;
	size_t sent = 0;

	ret = cherokee_socket_bufwrite (&pconn->socket, buf, &sent);
	if (unlikely(ret != ret_ok)) {
		return ret;
	}

	if (sent >= buf->len) {
		cherokee_buffer_clean (buf);
		return ret_ok;
	}

	cherokee_buffer_move_to_begin (buf, sent);
	return ret_eagain;
}

ret_t
cherokee_handler_proxy_conn_recv_headers (cherokee_handler_proxy_conn_t *pconn,
					  cherokee_buffer_t             *body)
{
	ret_t    ret;
	char    *end;
	cuint_t  sep_len;
	size_t   size     = 0;

	/* Read 
	 */
	ret = cherokee_socket_bufread (&pconn->socket,
				       &pconn->header_in_raw,
				       512, &size);
	switch (ret) {
	case ret_ok:
		break;
	case ret_eof:
	case ret_error:
	case ret_eagain:
		return ret;
	default:
		RET_UNKNOWN(ret);
	}

	/* Look for the end of header
	 */
	ret = cherokee_find_header_end (&pconn->header_in_raw, &end, &sep_len);
	switch (ret) {
	case ret_ok:
		break;
	case ret_not_found:
		return ret_eagain;
	default:
		return ret_error;
	}

	/* Copy the body if there is any
	 */
	size = (pconn->header_in_raw.buf + pconn->header_in_raw.len) - (end + sep_len);

	cherokee_buffer_add (body, end+sep_len, size);
	cherokee_buffer_drop_ending (&pconn->header_in_raw, size);

	return ret_ok;
}



/* Utils
 */

ret_t
cherokee_proxy_util_init_socket (cherokee_socket_t *socket, 
				 cherokee_source_t *src)
{
	ret_t ret;
	
	/* Family */
	if (cherokee_string_is_ipv6 (&src->host)) {
		ret = cherokee_socket_set_client (socket, AF_INET6);
	} else {
		ret = cherokee_socket_set_client (socket, AF_INET);
	}

        if (unlikely(ret != ret_ok))
		return ret_error;

	/* TCP port */
        SOCKET_SIN_PORT(socket) = htons (src->port);

        /* IP host */
        ret = cherokee_socket_pton (socket, &src->host);
        if (ret != ret_ok) {
                ret = cherokee_socket_gethostbyname (socket, &src->host);
                if (unlikely(ret != ret_ok)) {
			return ret_error;
		}
        }

	/* Set a few properties */
	cherokee_fd_set_closexec    (socket->socket);
	cherokee_fd_set_nonblocking (socket->socket, true);
	cherokee_fd_set_nodelay     (socket->socket, true);

	return ret_ok;
}
