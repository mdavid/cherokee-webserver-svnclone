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

#include "virtual_server.h"
#include "handler_error.h"

#include <errno.h>


ret_t 
cherokee_virtual_server_new (cherokee_virtual_server_t **vserver)
{
	ret_t ret;

       	CHEROKEE_NEW_STRUCT (vsrv, virtual_server);

	INIT_LIST_HEAD(&vsrv->list_entry);

	vsrv->error_handler = NULL;

	vsrv->logger        = NULL;
	vsrv->logger_props  = NULL;

	vsrv->data.rx       = 0;
	vsrv->data.tx       = 0;
	
	vsrv->server_cert  = NULL;
	vsrv->server_key   = NULL;
	vsrv->ca_cert      = NULL;

#ifdef HAVE_TLS
	ret = cherokee_session_cache_new (&vsrv->session_cache);
	if (unlikely(ret < ret_ok)) return ret;

# ifdef HAVE_GNUTLS
	vsrv->credentials = NULL;
# endif
# ifdef HAVE_OPENSSL
	vsrv->context     = NULL;
# endif 
#endif

	ret = cherokee_buffer_new (&vsrv->root);
	if (unlikely(ret < ret_ok)) return ret;	
	
	ret = cherokee_buffer_new (&vsrv->name);
	if (unlikely(ret < ret_ok)) return ret;

	ret = cherokee_handler_table_new (&vsrv->plugins);
	if (unlikely(ret < ret_ok)) return ret;

        /* User dir
         */
	ret = cherokee_buffer_new (&vsrv->userdir);
	if (unlikely(ret < ret_ok)) return ret;

	ret = cherokee_handler_table_new (&vsrv->userdir_plugins);
	if (unlikely(ret < ret_ok)) return ret;

	/* Return the object
	 */
	*vserver = vsrv;
	return ret_ok;
}


ret_t 
cherokee_virtual_server_free (cherokee_virtual_server_t *vserver)
{
	if (vserver->server_cert != NULL) {
		free (vserver->server_cert);
		vserver->server_cert = NULL;
	}
	
	if (vserver->server_key != NULL) {
		free (vserver->server_key);
		vserver->server_key = NULL;
	}

	if (vserver->ca_cert != NULL) {
		free (vserver->ca_cert);
		vserver->ca_cert = NULL;
	}

	if (vserver->error_handler != NULL) {
		cherokee_handler_table_entry_free (vserver->error_handler);
		vserver->error_handler = NULL;
	}

#ifdef HAVE_TLS
	cherokee_session_cache_free (vserver->session_cache);


# ifdef HAVE_GNUTLS
	if (vserver->credentials != NULL) {
		gnutls_certificate_free_credentials (vserver->credentials);
		vserver->credentials = NULL;
	}
# endif
# ifdef HAVE_OPENSSL
	if (vserver->context != NULL) {
		SSL_CTX_free (vserver->context);
		vserver->context = NULL;
	}
# endif
#endif

	cherokee_buffer_free (vserver->name);
	cherokee_buffer_free (vserver->root);

	if (vserver->logger != NULL) {
		cherokee_logger_free (vserver->logger);
		vserver->logger = NULL;
	}
	if (vserver->logger_props != NULL) {
		cherokee_table_free (vserver->logger_props);
		vserver->logger_props = NULL;
	}
	
	cherokee_handler_table_free (vserver->plugins);
	vserver->plugins = NULL;

	/* User dir
	 */
	cherokee_handler_table_free (vserver->userdir_plugins);
	vserver->userdir_plugins = NULL;

	cherokee_buffer_free (vserver->userdir);
	vserver->userdir = NULL;

	free (vserver);	
	return ret_ok;
}


ret_t 
cherokee_virtual_server_clean (cherokee_virtual_server_t *vserver)
{
	if (vserver->server_cert != NULL) {
		free (vserver->server_cert);
		vserver->server_cert = NULL;
	}
	
	if (vserver->server_key != NULL) {
		free (vserver->server_key);
		vserver->server_key = NULL;
	}

	if (vserver->ca_cert != NULL) {
		free (vserver->ca_cert);
		vserver->ca_cert = NULL;
	}

	if (vserver->error_handler != NULL) {
		cherokee_handler_table_entry_free (vserver->error_handler);
		vserver->error_handler = NULL;
	}

#ifdef HAVE_TLS
	cherokee_session_cache_free (vserver->session_cache);
	cherokee_session_cache_new (&vserver->session_cache);

# ifdef HAVE_GNUTLS
	if (vserver->credentials != NULL) {
		gnutls_certificate_free_credentials (vserver->credentials);
		vserver->credentials = NULL;
	}
# endif
# ifdef HAVE_OPENSSL
	if (vserver->context != NULL) {
		SSL_CTX_free (vserver->context);
		vserver->context = NULL;
	}
# endif
#endif

	cherokee_buffer_free (vserver->name);
	vserver->name = NULL;

	cherokee_buffer_free (vserver->root);
	vserver->root = NULL;
	
	if (vserver->logger != NULL) {
		cherokee_logger_free (vserver->logger);
		vserver->logger = NULL;
	}

	if (vserver->logger_props != NULL) {
		cherokee_table_free (vserver->logger_props);
		vserver->logger_props = NULL;
	}

	cherokee_buffer_clean (vserver->userdir);

	/* TODO:
	 * It has to clean the plugin tables:
	 * ->plugins and ->userdir_plugins
	 */

	return ret_ok;
}


#ifdef HAVE_GNUTLS
static ret_t
generate_dh_params (gnutls_dh_params *dh_params)
{
	gnutls_dh_params_init (dh_params);
	gnutls_dh_params_generate2 (*dh_params, 1024);

	return ret_ok;
}

static ret_t
generate_rsa_params (gnutls_rsa_params *rsa_params)
{
	/* Generate RSA parameters - for use with RSA-export cipher
	 * suites. These should be discarded and regenerated once a
	 * day, once every 500 transactions etc. Depends on the
	 * security requirements.
	 */

	gnutls_rsa_params_init (rsa_params);
	gnutls_rsa_params_generate2 (*rsa_params, 512);

	return ret_ok;
}
#endif /* HAVE_GNUTLS */



ret_t 
cherokee_virtual_server_have_tls (cherokee_virtual_server_t *vserver)
{
#ifndef HAVE_TLS
	return ret_not_found;
#endif

	return ((vserver->server_cert != NULL) ||
		(vserver->server_key  != NULL) ||
		(vserver->ca_cert     != NULL)) ? ret_ok : ret_not_found;
}


ret_t 
cherokee_virtual_server_init_tls (cherokee_virtual_server_t *vsrv)
{
	int rc;

#ifndef HAVE_TLS
	return ret_ok;
#endif

	if ((vsrv->ca_cert     == NULL) &&
	    (vsrv->server_cert == NULL) &&
	    (vsrv->server_key  == NULL)) 
	{
		return ret_not_found;
	}

	if ((vsrv->ca_cert     == NULL) ||
	    (vsrv->server_cert == NULL) ||
	    (vsrv->server_key  == NULL)) 
	{
		return ret_error;
	}

#ifdef HAVE_GNUTLS
        rc = gnutls_certificate_allocate_credentials (&vsrv->credentials);
	if (rc < 0) {
		PRINT_ERROR ("ERROR: Couldn't allocate credentials.\n");
		return ret_error;
	}
	
	/* CA file
	 */
	rc = gnutls_certificate_set_x509_trust_file (vsrv->credentials,
						     vsrv->ca_cert,
						     GNUTLS_X509_FMT_PEM);
	if (rc < 0) {
		PRINT_ERROR ("ERROR: reading X.509 CA Certificate: '%s'\n", vsrv->ca_cert);
		return ret_error;
	}

	/* Key file
	 */
	rc = gnutls_certificate_set_x509_key_file (vsrv->credentials,
						   vsrv->server_cert,
						   vsrv->server_key,
						   GNUTLS_X509_FMT_PEM);	
	if (rc < 0) {
		PRINT_ERROR ("ERROR: reading X.509 key '%s' or certificate '%s' file\n", 
			     vsrv->server_key, vsrv->server_cert);
		return ret_error;
	}

	generate_dh_params (&vsrv->dh_params);
	generate_rsa_params (&vsrv->rsa_params);

	gnutls_certificate_set_dh_params (vsrv->credentials, vsrv->dh_params);
	gnutls_certificate_set_rsa_params (vsrv->credentials, vsrv->rsa_params);
#endif

#ifdef HAVE_OPENSSL
	/* Init the OpenSSL context
	 */
	vsrv->context = SSL_CTX_new (SSLv23_server_method());
	if (vsrv->context == NULL) {
		PRINT_ERROR_S("ERROR: OpenSSL: Couldn't allocate OpenSSL context\n");
		return ret_error;
	}

	/* Certificate
	 */
	rc = SSL_CTX_use_certificate_file (vsrv->context, vsrv->server_cert, SSL_FILETYPE_PEM);
	if (rc < 0) {
		PRINT_ERROR("ERROR: OpenSSL: Can not use certificate file '%s':  %s\n", 
			    vsrv->server_cert, ERR_error_string(ERR_get_error(), NULL));
		return ret_error;
	}

	/* Private key
	 */
	rc = SSL_CTX_use_PrivateKey_file (vsrv->context, vsrv->server_key, SSL_FILETYPE_PEM);
	if (rc < 0) {
		PRINT_ERROR("ERROR: OpenSSL: Can not use private key file '%s': %s\n", 
			    vsrv->server_key, ERR_error_string(ERR_get_error(), NULL));
		return ret_error;
	}

	/* Check private key
	 */
	rc = SSL_CTX_check_private_key (vsrv->context);
	if (rc != 1) {
		PRINT_ERROR_S("ERROR: OpenSSL: Private key does not match the certificate public key\n");
		return ret_error;
	}
#endif

	return ret_ok;
}


void  
cherokee_virtual_server_add_rx (cherokee_virtual_server_t *vserver, size_t rx)
{
	CHEROKEE_MUTEX_LOCK(&vserver->data.rx_mutex);
	vserver->data.rx += rx;
	CHEROKEE_MUTEX_UNLOCK(&vserver->data.rx_mutex);
}


void
cherokee_virtual_server_add_tx (cherokee_virtual_server_t *vserver, size_t tx)
{
	CHEROKEE_MUTEX_LOCK(&vserver->data.tx_mutex);
	vserver->data.tx += tx;
	CHEROKEE_MUTEX_UNLOCK(&vserver->data.tx_mutex);
}

