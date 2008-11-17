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

#include "common-internal.h"
#include "cryptor.h"


ret_t
cherokee_cryptor_init_base (cherokee_cryptor_t      *cryp,
			    cherokee_plugin_info_t  *info)
{
	ret_t ret;

	ret = cherokee_module_init_base (MODULE(cryp), NULL, info);
	if (ret != ret_ok)
		return ret;

	cryp->vserver_new = NULL;
	cryp->socket_new  = NULL;
	cryp->configure   = NULL;
	
	return ret_ok;
}


ret_t
cherokee_cryptor_free (cherokee_cryptor_t *cryp)
{
	ret_t ret;

	if (MODULE(cryp)->free == NULL) 
		return ret_error;

	ret = MODULE(cryp)->free (cryp);
	return ret;
}


ret_t
cherokee_cryptor_configure (cherokee_cryptor_t     *cryp,
			    cherokee_config_node_t *conf,
			    cherokee_server_t      *srv)
{
	ret_t ret;

	if (cryp->configure == NULL)
		return ret_error;

	ret = cryp->configure (cryp, conf, srv);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}


ret_t
cherokee_cryptor_vserver_new (cherokee_cryptor_t          *cryp,
			      void                        *vsrv,
			      cherokee_cryptor_vserver_t **cryp_vsrv)
{
	if (unlikely (cryp->vserver_new == NULL))
		return ret_error;
	
	return cryp->vserver_new (cryp, vsrv, (void **)cryp_vsrv);
}


ret_t
cherokee_cryptor_socket_new (cherokee_cryptor_t         *cryp,
			     cherokee_cryptor_socket_t **cryp_sock)
{
	if (unlikely (cryp->socket_new == NULL))
		return ret_error;

	return cryp->socket_new (cryp, (void **)cryp_sock);
}


ret_t
cherokee_cryptor_client_new (cherokee_cryptor_t         *cryp,
			     cherokee_cryptor_client_t **cryp_client)
{
	if (unlikely (cryp->socket_new == NULL))
		return ret_error;

	return cryp->client_new (cryp, (void **)cryp_client);
}


/* Virtual Server
 */

ret_t
cherokee_cryptor_vserver_init_base (cherokee_cryptor_vserver_t *crypt)
{
	crypt->free = NULL;

	return ret_ok;
}

ret_t
cherokee_cryptor_vserver_free (cherokee_cryptor_vserver_t *cryp)
{
	ret_t ret;

	if (cryp->free == NULL)
		return ret_error;

	ret = cryp->free (cryp);
	free (cryp);
	return ret;
}


/* Socket
 */

ret_t
cherokee_cryptor_socket_init_base (cherokee_cryptor_socket_t *cryp)
{
	cryp->initialized = false;
	cryp->vserver_ref = NULL;
	cryp->free        = NULL;
	cryp->clean       = NULL;
	cryp->init_tls    = NULL;
	cryp->close       = NULL;
	cryp->read        = NULL;
	cryp->write       = NULL;
	cryp->pending     = NULL;

	return ret_ok;
}

ret_t
cherokee_cryptor_socket_clean_base (cherokee_cryptor_socket_t *cryp)
{
	cryp->initialized = false;
	cryp->vserver_ref = NULL;

	return ret_ok;
}


ret_t
cherokee_cryptor_socket_free (cherokee_cryptor_socket_t *cryp)
{
	ret_t ret;

	if (unlikely (cryp->free == NULL))
		return ret_error;

	ret = cryp->free (cryp);
	free (cryp);
	return ret;
}

ret_t
cherokee_cryptor_socket_clean (cherokee_cryptor_socket_t *cryp)
{
	if (unlikely (cryp->clean == NULL))
		return ret_error;

	return cryp->clean (cryp);
}

ret_t
cherokee_cryptor_socket_init_tls (cherokee_cryptor_socket_t *cryp,
				  void                      *sock,
				  void                      *vsrv)
{
	if (unlikely (cryp->init_tls == NULL))
		return ret_error;

	return cryp->init_tls (cryp, sock, vsrv);
}

ret_t
cherokee_cryptor_socket_close (cherokee_cryptor_socket_t *cryp)
{
	if (unlikely (cryp->close == NULL))
		return ret_error;

	return cryp->close (cryp);
}

ret_t
cherokee_cryptor_socket_read (cherokee_cryptor_socket_t *cryp,
			      char *buf, int len, size_t *re_len)
{
	if (unlikely (cryp->read == NULL))
		return ret_error;

	return cryp->read (cryp, buf, len, re_len);
}

ret_t
cherokee_cryptor_socket_write (cherokee_cryptor_socket_t *cryp,
			       char *buf, int len, size_t *re_len)
{
	if (unlikely (cryp->write == NULL))
		return ret_error;

	return cryp->write (cryp, buf, len, re_len);
}

ret_t
cherokee_cryptor_socket_pending (cherokee_cryptor_socket_t *cryp)
{
	if (unlikely (cryp->pending == NULL))
		return ret_error;

	return cryp->pending (cryp);
}


/* Client Socket
 */

ret_t
cherokee_cryptor_client_init (cherokee_cryptor_client_t *cryp,
			      cherokee_buffer_t         *host,
			      void                      *socket)
{
	if (unlikely (CRYPTOR_SOCKET(cryp)->init_tls == NULL))
		return ret_error;

	return CRYPTOR_SOCKET(cryp)->init_tls (cryp, host, socket);	
}
