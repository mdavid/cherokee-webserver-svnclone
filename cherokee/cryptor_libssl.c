/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2010 Alvaro Lopez Ortega
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

/* IN CASE THIS PLUG-IN IS COMPILED WITH OPENSSL:
 *
 * In addition, as a special exception, the copyright holders give
 * permission to link the code of portions of this program with the
 * OpenSSL library under certain conditions as described in each
 * individual source file, and distribute linked combinations
 * including the two.
 *
 * You must obey the GNU General Public License in all respects for
 * all of the code used other than OpenSSL.  If you modify file(s)
 * with this exception, you may extend this exception to your version
 * of the file(s), but you are not obligated to do so.  If you do not
 * wish to do so, delete this exception statement from your version.
 * If you delete this exception statement from all source files in the
 * program, then also delete it here.
 */

#include "common-internal.h"
#include "cryptor_libssl.h"
#include "virtual_server.h"
#include "socket.h"
#include "util.h"
#include "server-protected.h"

#define ENTRIES "crypto,ssl"

static DH *dh_param_512  = NULL;
static DH *dh_param_1024 = NULL;
static DH *dh_param_2048 = NULL;
static DH *dh_param_4096 = NULL;

#include "cryptor_libssl_dh_512.c"
#include "cryptor_libssl_dh_1024.c"
#include "cryptor_libssl_dh_2048.c"
#include "cryptor_libssl_dh_4096.c"

static ret_t
_free (cherokee_cryptor_libssl_t *cryp)
{
	/* DH Parameters
	 */
	if (dh_param_512  != NULL) {
		DH_free (dh_param_512);
		dh_param_512 = NULL;
	}

	if (dh_param_1024 != NULL) {
		DH_free (dh_param_1024);
		dh_param_1024 = NULL;
	}

	if (dh_param_2048 != NULL) {
		DH_free (dh_param_2048);
		dh_param_2048 = NULL;
	}

	if (dh_param_4096 != NULL) {
		DH_free (dh_param_4096);
		dh_param_4096 = NULL;
	}

	/* Free loaded error strings
	 */
	ERR_free_strings();

	/* Free all ciphers and digests
	 */
	EVP_cleanup();

	cherokee_cryptor_free_base (CRYPTOR(cryp));
	return ret_ok;
}

static ret_t
try_read_dh_param(cherokee_config_node_t  *conf,
		  DH                     **dh,
		  int                      bitsize)
{
	ret_t              ret;
	cherokee_buffer_t *buf;
	FILE              *paramfile = NULL;
	cherokee_buffer_t  confentry = CHEROKEE_BUF_INIT;

	cherokee_buffer_add_va (&confentry, "dh_param%d", bitsize);

	/* Read the configuration parameter
	 */
	ret = cherokee_config_node_read (conf, confentry.buf, &buf);
	if (ret != ret_ok) {
		ret = ret_ok;
		goto out;
	}

	/* Read the file
	 */
	paramfile = fopen (buf->buf, "r");
	if (paramfile == NULL) {
		TRACE(ENTRIES, "Cannot open file %s\n", buf->buf);
		ret = ret_file_not_found;
		goto out;
	}

	/* Process the content
	 */
	*dh = PEM_read_DHparams (paramfile, NULL, NULL, NULL);
	if (*dh == NULL) {
		TRACE(ENTRIES, "Failed to load PEM %s\n", buf->buf);
		ret = ret_error;
		goto out;
	}

	ret = ret_ok;

out:
	/* Clean up
	 */
	if (paramfile != NULL) {
		fclose (paramfile);
	}

	cherokee_buffer_mrproper (&confentry);
	return ret;
}

static ret_t
_configure (cherokee_cryptor_t     *cryp,
	    cherokee_config_node_t *conf,
	    cherokee_server_t      *srv)
{
	ret_t ret;

	UNUSED(cryp);
	UNUSED(srv);

	ret = try_read_dh_param (conf, &dh_param_512, 512);
	if (ret != ret_ok)
		return ret;

	ret = try_read_dh_param (conf, &dh_param_1024, 1024);
	if (ret != ret_ok)
		return ret;

	ret = try_read_dh_param (conf, &dh_param_2048, 2048);
	if (ret != ret_ok)
		return ret;

	ret = try_read_dh_param (conf, &dh_param_4096, 4096);
	if (ret != ret_ok)
		return ret;

	return ret_ok;
}

static ret_t
_vserver_free (cherokee_cryptor_vserver_libssl_t *cryp_vsrv)
{
	if (cryp_vsrv->context != NULL) {
		SSL_CTX_free (cryp_vsrv->context);
		cryp_vsrv->context = NULL;
	}

	free (cryp_vsrv);
	return ret_ok;
}

#ifndef OPENSSL_NO_TLSEXT
static int
openssl_sni_servername_cb (SSL *ssl, int *ad, void *arg)
{
	ret_t                      ret;
	const char                *servername;
	cherokee_socket_t         *socket;
	cherokee_buffer_t          tmp;
	SSL_CTX                   *ctx;
	cherokee_server_t         *srv       = SRV(arg);
	cherokee_virtual_server_t *vsrv      = NULL;

	UNUSED(ad);

	/* Get the pointer to the socket
	 */
	socket = SSL_get_app_data (ssl);
	if (unlikely (socket == NULL)) {
		LOG_ERROR (CHEROKEE_ERROR_SSL_SOCKET, ssl);
		return SSL_TLSEXT_ERR_ALERT_FATAL;
	}

	/* Read the SNI server name
	 */
	servername = SSL_get_servername (ssl, TLSEXT_NAMETYPE_host_name);
	if (servername == NULL) {
		TRACE (ENTRIES, "No SNI: Did not provide a server name%s", "\n");
		return SSL_TLSEXT_ERR_NOACK;
	}

	TRACE (ENTRIES, "SNI: Switching to servername='%s'\n", servername);

	/* Try to match the name
	 */
	cherokee_buffer_fake (&tmp, servername, strlen(servername));

	ret = cherokee_server_get_vserver (srv, &tmp, NULL, &vsrv);
	if ((ret != ret_ok) || (vsrv == NULL)) {
		LOG_ERROR (CHEROKEE_ERROR_SSL_SRV_MATCH, servername);
		return SSL_TLSEXT_ERR_NOACK;
	}

	TRACE (ENTRIES, "SNI: Setting new TLS context. Virtual host='%s'\n",
	       vsrv->name.buf);

	/* Check whether the Virtual Server supports TLS
	 */
	if ((vsrv->cryptor == NULL) ||
	    (CRYPTOR_VSRV_SSL(vsrv->cryptor)->context == NULL))
	{
		TRACE (ENTRIES, "Virtual server '%s' does not support SSL\n", servername);
		return SSL_TLSEXT_ERR_NOACK;
	}

	/* Set the new SSL context
	 */
	ctx = SSL_set_SSL_CTX (ssl, CRYPTOR_VSRV_SSL(vsrv->cryptor)->context);
	if (ctx != CRYPTOR_VSRV_SSL(vsrv->cryptor)->context) {
		LOG_ERROR (CHEROKEE_ERROR_SSL_CHANGE_CTX, servername);
	}

	/* SSL_set_SSL_CTX() only change certificates. We need to
	 * changes more options by hand.
	 */
	SSL_set_options(ssl, SSL_CTX_get_options(ssl->ctx));

	if ((SSL_get_verify_mode(ssl) == SSL_VERIFY_NONE) ||
	    (SSL_num_renegotiations(ssl) == 0)) {

		SSL_set_verify(ssl, SSL_CTX_get_verify_mode(ssl->ctx),
		               SSL_CTX_get_verify_callback(ssl->ctx));
	}

	return SSL_TLSEXT_ERR_OK;
}
#endif

static DH *
tmp_dh_cb (SSL *ssl, int export, int keylen)
{
	UNUSED(ssl);
	UNUSED(export);

	switch (keylen) {
	case 512:
		if (dh_param_512 == NULL) {
			dh_param_512 = get_dh512();
		}
		return dh_param_512;

	case 1024:
		if (dh_param_1024 == NULL) {
			dh_param_1024 = get_dh1024();
		}
		return dh_param_1024;

	case 2048:
		if (dh_param_2048 == NULL) {
			dh_param_2048 = get_dh2048();
		}
		return dh_param_2048;

	case 4096:
		if (dh_param_4096 == NULL) {
			dh_param_4096 = get_dh4096();
		}
		return dh_param_4096;
	}
	return NULL;
}

static ret_t
_vserver_new (cherokee_cryptor_t          *cryp,
	      cherokee_virtual_server_t   *vsrv,
	      cherokee_cryptor_vserver_t **cryp_vsrv)
{
	ret_t              ret;
	int                rc;
	const char        *error;
	int                verify_mode = SSL_VERIFY_NONE;
	cherokee_buffer_t  session_id  = CHEROKEE_BUF_INIT;
	CHEROKEE_NEW_STRUCT (n, cryptor_vserver_libssl);


	UNUSED(cryp);

	/* Init
	 */
	ret = cherokee_cryptor_vserver_init_base (CRYPTOR_VSRV(n));
	if (ret != ret_ok)
		return ret;

	CRYPTOR_VSRV(n)->free = (cryptor_vsrv_func_free_t) _vserver_free;

	/* Init the OpenSSL context
	 */
	n->context = SSL_CTX_new (SSLv23_server_method());
	if (n->context == NULL) {
		LOG_ERROR_S(CHEROKEE_ERROR_SSL_ALLOCATE_CTX);
		return ret_error;
	}

	/* Callback to be used when a DH parameters are required
	 */
	SSL_CTX_set_tmp_dh_callback (n->context, tmp_dh_cb);

	/* Work around all known bugs
	 */
	SSL_CTX_set_options (n->context, SSL_OP_ALL);

	/* Set other OpenSSL context options
	 */
	SSL_CTX_set_options (n->context, SSL_OP_SINGLE_DH_USE);

#ifdef SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS
	SSL_CTX_set_options (n->context, SSL_OP_DONT_INSERT_EMPTY_FRAGMENTS);
#endif

	/* Set cipher list that vserver will accept.
	 */
	if (! cherokee_buffer_is_empty (&vsrv->ciphers)) {
		rc = SSL_CTX_set_cipher_list (n->context, vsrv->ciphers.buf);
		if (rc != 1) {
			OPENSSL_LAST_ERROR(error);
			LOG_ERROR(CHEROKEE_ERROR_SSL_CIPHER,
				  vsrv->ciphers.buf, error);
			return ret_error;
		}
	}

#if (OPENSSL_VERSION_NUMBER < 0x0090808fL)
	/* OpenSSL < 0.9.8h
	 */
	ERR_clear_error();
#endif

	/* Certificate
	 */
	TRACE(ENTRIES, "Vserver '%s'. Reading certificate file '%s'\n",
	      vsrv->name.buf, vsrv->server_cert.buf);

	rc = SSL_CTX_use_certificate_chain_file (n->context, vsrv->server_cert.buf);
	if (rc != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR(CHEROKEE_ERROR_SSL_CERTIFICATE,
			  vsrv->server_cert.buf, error);
		return ret_error;
	}

	/* Private key
	 */
	TRACE(ENTRIES, "Vserver '%s'. Reading key file '%s'\n",
	      vsrv->name.buf, vsrv->server_key.buf);

	rc = SSL_CTX_use_PrivateKey_file (n->context, vsrv->server_key.buf, SSL_FILETYPE_PEM);
	if (rc != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR(CHEROKEE_ERROR_SSL_KEY,
			  vsrv->server_key.buf, error);
		return ret_error;
	}

	/* Check private key
	 */
	rc = SSL_CTX_check_private_key (n->context);
	if (rc != 1) {
		LOG_ERROR_S(CHEROKEE_ERROR_SSL_KEY_MATCH);
		return ret_error;
	}

	if (! cherokee_buffer_is_empty (&vsrv->req_client_certs)) {
		STACK_OF(X509_NAME) *X509_clients;

		verify_mode = SSL_VERIFY_PEER|SSL_VERIFY_CLIENT_ONCE;
		if (cherokee_buffer_cmp_str (&vsrv->req_client_certs, "required") == 0) {
			verify_mode |= SSL_VERIFY_FAIL_IF_NO_PEER_CERT;
		}

		/* Trusted CA certificates
		*/
		if (! cherokee_buffer_is_empty (&vsrv->certs_ca)) {
			rc = SSL_CTX_load_verify_locations (n->context, vsrv->certs_ca.buf, NULL);
			if (rc != 1) {
				OPENSSL_LAST_ERROR(error);
				LOG_CRITICAL(CHEROKEE_ERROR_SSL_CA_READ,
					     vsrv->certs_ca.buf, error);
				return ret_error;
			}

			X509_clients = SSL_load_client_CA_file (vsrv->certs_ca.buf);
			if (X509_clients == NULL) {
				OPENSSL_LAST_ERROR(error);
				LOG_CRITICAL (CHEROKEE_ERROR_SSL_CA_LOAD,
					      vsrv->certs_ca.buf, error);
				return ret_error;
			}

			ERR_clear_error();

			SSL_CTX_set_client_CA_list (n->context, X509_clients);
			TRACE (ENTRIES, "Setting client CA list: %s on '%s'\n", vsrv->certs_ca.buf, vsrv->name.buf);
		} else {
			verify_mode = SSL_VERIFY_NONE;
		}
	}

	SSL_CTX_set_verify (n->context, verify_mode, NULL);
	SSL_CTX_set_verify_depth (n->context, vsrv->verify_depth);

	SSL_CTX_set_mode (n->context, SSL_MODE_ENABLE_PARTIAL_WRITE);

	/* Set the SSL context cache
	 */
	cherokee_buffer_add_ulong10  (&session_id, getpid());
	cherokee_buffer_add_char     (&session_id, '-');
	cherokee_buffer_add_buffer   (&session_id, &vsrv->name);
	cherokee_buffer_add_char     (&session_id, '-');
	cherokee_buffer_add_ullong10 (&session_id, random());

	TRACE(ENTRIES, "VServer '%s' session: %s\n", vsrv->name.buf, session_id.buf);

	rc = SSL_CTX_set_session_id_context (n->context,
					     (unsigned char *) session_id.buf,
					     (unsigned int)    session_id.len);

	cherokee_buffer_mrproper (&session_id);

	if (rc != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_SESSION_ID,
			   vsrv->name.buf, error);
	}

#ifndef OPENSSL_NO_TLSEXT
	/* Enable SNI
	 */
	rc = SSL_CTX_set_tlsext_servername_callback (n->context, openssl_sni_servername_cb);
	if (rc != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_SNI, vsrv->name.buf, error);
		return ret_error;
	}

	rc = SSL_CTX_set_tlsext_servername_arg (n->context, VSERVER_SRV(vsrv));
	if (rc != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_SNI, vsrv->name.buf, error);
		return ret_error;
	}
#endif /* OPENSSL_NO_TLSEXT */

	*cryp_vsrv = CRYPTOR_VSRV(n);

	return ret_ok;
}


static ret_t
socket_initialize (cherokee_cryptor_socket_libssl_t *cryp,
		   cherokee_socket_t                *socket,
		   cherokee_virtual_server_t        *vserver)
{
	int                                re;
	const char                        *error;
	cherokee_cryptor_vserver_libssl_t *vsrv_crytor = CRYPTOR_VSRV_SSL(vserver->cryptor);

	/* Set the virtual server object reference
	 */
	CRYPTOR_SOCKET(cryp)->vserver_ref = vserver;

	/* Check whether the virtual server supports SSL
	 */
	if (vserver->cryptor == NULL) {
		return ret_not_found;
	}

	if (vsrv_crytor->context == NULL) {
		return ret_not_found;
	}

	/* New session
	 */
	cryp->session = SSL_new (vsrv_crytor->context);
	if (cryp->session == NULL) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_CONNECTION, error);
		return ret_error;
	}

	/* Set the socket file descriptor
	 */
	re = SSL_set_fd (cryp->session, socket->socket);
	if (re != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_FD,
			   socket->socket, error);
		return ret_error;
	}

#ifndef OPENSSL_NO_TLSEXT
	SSL_set_app_data (cryp->session, socket);
#endif

	return ret_ok;
}


static ret_t
_socket_init_tls (cherokee_cryptor_socket_libssl_t *cryp,
		  cherokee_socket_t                *sock,
		  cherokee_virtual_server_t        *vsrv)
{
	int   re;
	ret_t ret;

	/* Initialize
	 */
	if (CRYPTOR_SOCKET(cryp)->initialized == false) {
		ret = socket_initialize (cryp, sock, vsrv);
		if (ret != ret_ok) {
			return ret_error;
		}

		CRYPTOR_SOCKET(cryp)->initialized = true;
	}

	/* TLS Handshake
	 */
	re = SSL_accept (cryp->session);
	if (re <= 0) {
		int         err;
		const char *error;

		err = SSL_get_error (cryp->session, re);
		switch (err) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
			return ret_eagain;
		case SSL_ERROR_SSL:
		case SSL_ERROR_SYSCALL:
		case SSL_ERROR_ZERO_RETURN:
			return ret_error;
		default:
			OPENSSL_LAST_ERROR(error);
			LOG_ERROR (CHEROKEE_ERROR_SSL_INIT, error);
			return ret_error;
		}
	}

	return ret_ok;
}

static ret_t
_socket_close (cherokee_cryptor_socket_libssl_t *cryp)
{
	int re;

	if (cryp->session != NULL) {
		re = SSL_shutdown (cryp->session);
		if (re == 0) {
			/* Call it again to finish the shutdown */
			re = SSL_shutdown (cryp->session);
		}

		if (re < 0) {
			return ret_error;
		}
	}

	return ret_ok;
}

static ret_t
_socket_write (cherokee_cryptor_socket_libssl_t *cryp,
	       char                             *buf,
	       int                               buf_len,
	       size_t                           *pcnt_written)
{
	int     re;
	ssize_t len;
	int     error;

	len = SSL_write (cryp->session, buf, buf_len);
	if (likely (len > 0) ) {
		TRACE (ENTRIES",write", "write len=%d, written=%d\n", buf_len, len);

		/* Return info
		 */
		*pcnt_written = len;
		return ret_ok;
	}

	if (len == 0) {
		/* maybe socket was closed by client, no write was performed
		int re = SSL_get_error (socket->session, len);
		if (re == SSL_ERROR_ZERO_RETURN)
			socket->status = socket_closed;
		 */

		TRACE (ENTRIES",write", "write len=%d, EOF\n", buf_len);
		return ret_eof;
	}

	/* len < 0 */
	error = errno;

	re = SSL_get_error (cryp->session, len);
	switch (re) {
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
		TRACE (ENTRIES",write", "write len=%d, EAGAIN\n", buf_len);
		return ret_eagain;

	case SSL_ERROR_SYSCALL:
		switch (error) {
		case EAGAIN:
			TRACE (ENTRIES",write", "write len=%d, EAGAIN\n", buf_len);
			return ret_eagain;
#ifdef ENOTCONN
		case ENOTCONN:
#endif
		case EPIPE:
		case ECONNRESET:
			TRACE (ENTRIES",write", "write len=%d, EOF\n", buf_len);
			return ret_eof;
		default:
			LOG_ERRNO_S (error, cherokee_err_error,
				     CHEROKEE_ERROR_SSL_SW_DEFAULT);
		}

		TRACE (ENTRIES",write", "write len=%d, ERROR: %s\n", buf_len, ERR_error_string(re, NULL));
		return ret_error;

	case SSL_ERROR_SSL:
		TRACE (ENTRIES",write", "write len=%d, ERROR: %s\n", buf_len, ERR_error_string(re, NULL));
		return ret_error;
	}

	LOG_ERROR (CHEROKEE_ERROR_SSL_SW_ERROR,
		   SSL_get_fd(cryp->session), (int)len, ERR_error_string(re, NULL));

	TRACE (ENTRIES",write", "write len=%d, ERROR: %s\n", buf_len, ERR_error_string(re, NULL));
	return ret_error;
}


static ret_t
_socket_read (cherokee_cryptor_socket_libssl_t *cryp,
	      char                             *buf,
	      int                               buf_size,
	      size_t                           *pcnt_read)
{
	int     re;
	int     error;
	ssize_t len;

	len = SSL_read (cryp->session, buf, buf_size);
	if (likely (len > 0)) {
		*pcnt_read = len;
		if (SSL_pending (cryp->session))
			return ret_eagain;
		return ret_ok;
	}

	if (len == 0) {
		return ret_eof;
	}

	/* len < 0 */
	error = errno;
	re = SSL_get_error (cryp->session, len);
	switch (re) {
	case SSL_ERROR_WANT_READ:
	case SSL_ERROR_WANT_WRITE:
		return ret_eagain;
	case SSL_ERROR_ZERO_RETURN:
		return ret_eof;
	case SSL_ERROR_SSL:
		return ret_error;
	case SSL_ERROR_SYSCALL:
		switch (error) {
		case EAGAIN:
			return ret_eagain;
		case EPIPE:
		case ECONNRESET:
			return ret_eof;
		default:
			LOG_ERRNO_S (error, cherokee_err_error,
				     CHEROKEE_ERROR_SSL_SR_DEFAULT);
		}
		return ret_error;
	}

	LOG_ERROR (CHEROKEE_ERROR_SSL_SR_ERROR,
		   SSL_get_fd(cryp->session), (int)len, ERR_error_string(re, NULL));
	return ret_error;
}

static int
_socket_pending (cherokee_cryptor_socket_libssl_t *cryp)
{
	return (SSL_pending (cryp->session) > 0);
}

static ret_t
_socket_clean (cherokee_cryptor_socket_libssl_t *cryp_socket)
{
	cherokee_cryptor_socket_clean_base (CRYPTOR_SOCKET(cryp_socket));

	if (cryp_socket->session != NULL) {
		SSL_free (cryp_socket->session);
		cryp_socket->session = NULL;
	}

	if (cryp_socket->ssl_ctx != NULL) {
		SSL_CTX_free (cryp_socket->ssl_ctx);
		cryp_socket->ssl_ctx = NULL;
	}

	return ret_ok;
}

static ret_t
_socket_free (cherokee_cryptor_socket_libssl_t *cryp_socket)
{
	_socket_clean (cryp_socket);

	free (cryp_socket);
	return ret_ok;
}

static ret_t
_socket_new (cherokee_cryptor_libssl_t         *cryp,
	     cherokee_cryptor_socket_libssl_t **cryp_socket)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, cryptor_socket_libssl);

	UNUSED(cryp);

	ret = cherokee_cryptor_socket_init_base (CRYPTOR_SOCKET(n));
	if (unlikely (ret != ret_ok))
		return ret;

	/* Socket properties */
	n->session = NULL;
	n->ssl_ctx = NULL;

	/* Virtual methods */
	CRYPTOR_SOCKET(n)->free     = (cryptor_socket_func_free_t) _socket_free;
	CRYPTOR_SOCKET(n)->clean    = (cryptor_socket_func_clean_t) _socket_clean;
	CRYPTOR_SOCKET(n)->init_tls = (cryptor_socket_func_init_tls_t) _socket_init_tls;
	CRYPTOR_SOCKET(n)->close    = (cryptor_socket_func_close_t) _socket_close;
	CRYPTOR_SOCKET(n)->read     = (cryptor_socket_func_read_t) _socket_read;
	CRYPTOR_SOCKET(n)->write    = (cryptor_socket_func_write_t) _socket_write;
	CRYPTOR_SOCKET(n)->pending  = (cryptor_socket_func_pending_t) _socket_pending;

	*cryp_socket = n;
	return ret_ok;
}

static int
_client_init_tls (cherokee_cryptor_client_libssl_t *cryp,
		  cherokee_buffer_t                *host,
		  cherokee_socket_t                *socket)
{
	int         re;
	const char *error;

	/* New context
	 */
	cryp->ssl_ctx = SSL_CTX_new (SSLv23_client_method());
	if (cryp->ssl_ctx == NULL) {
		OPENSSL_LAST_ERROR(error);
		LOG_CRITICAL (CHEROKEE_ERROR_SSL_CREATE_CTX, error);
		return ret_error;
	}

	/* CA verifications
	re = cherokee_buffer_is_empty (&cryp->vserver_ref->certs_ca);
	if (! re) {
		re = SSL_CTX_load_verify_locations (socket->ssl_ctx,
						    socket->vserver_ref->certs_ca.buf, NULL);
		if (! re) {
			OPENSSL_LAST_ERROR(error);
			LOG_ERROR (CHEROKEE_ERROR_SSL_CTX_LOAD,
			           socket->vserver_ref->certs_ca.buf, error);
			return ret_error;
		}

		re = SSL_CTX_set_default_verify_paths (socket->ssl_ctx);
		if (! re) {
			OPENSSL_LAST_ERROR(error);
			LOG_ERROR (CHEROKEE_ERROR_SSL_CTX_SET, error);
			return ret_error;
		}
	}
	 */

	SSL_CTX_set_verify (cryp->ssl_ctx, SSL_VERIFY_NONE, NULL);
	SSL_CTX_set_mode (cryp->ssl_ctx, SSL_MODE_ENABLE_PARTIAL_WRITE);

	/* New session
	 */
	cryp->session = SSL_new (cryp->ssl_ctx);
	if (cryp->session == NULL) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_CONNECTION, error);
		return ret_error;
	}

	/* Set the socket file descriptor
	 */
	re = SSL_set_fd (cryp->session, socket->socket);
	if (re != 1) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_FD, socket->socket, error);
		return ret_error;
	}

	SSL_set_connect_state (cryp->session);

#ifndef OPENSSL_NO_TLSEXT
	if ((host != NULL) &&
	    (! cherokee_buffer_is_empty (host)))
	{
		re = SSL_set_tlsext_host_name (cryp->session, host->buf);
		if (re <= 0) {
			OPENSSL_LAST_ERROR(error);
			LOG_ERROR (CHEROKEE_ERROR_SSL_SNI_SRV, error);
			return ret_error;
		}
	}
#endif

	re = SSL_connect (cryp->session);
	if (re <= 0) {
		OPENSSL_LAST_ERROR(error);
		LOG_ERROR (CHEROKEE_ERROR_SSL_CONNECT, error);
		return ret_error;
	}

	return ret_ok;
}

static ret_t
_client_free (cherokee_cryptor_client_libssl_t *cryp)
{
	if (cryp->session != NULL) {
		SSL_free (cryp->session);
		cryp->session = NULL;
	}

	if (cryp->ssl_ctx != NULL) {
		SSL_CTX_free (cryp->ssl_ctx);
		cryp->ssl_ctx = NULL;
	}

	free (cryp);
	return ret_ok;
}

static ret_t
_client_new (cherokee_cryptor_t         *cryp,
	     cherokee_cryptor_client_t **cryp_client)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, cryptor_client_libssl);

	UNUSED(cryp);

	ret = cherokee_cryptor_socket_init_base (CRYPTOR_SOCKET(n));
	if (unlikely (ret != ret_ok))
		return ret;

	/* Client properties */
	n->session = NULL;
	n->ssl_ctx = NULL;

	/* Socket */
	CRYPTOR_SOCKET(n)->close    = (cryptor_socket_func_close_t) _socket_close;
	CRYPTOR_SOCKET(n)->read     = (cryptor_socket_func_read_t) _socket_read;
	CRYPTOR_SOCKET(n)->write    = (cryptor_socket_func_write_t) _socket_write;
	CRYPTOR_SOCKET(n)->pending  = (cryptor_socket_func_pending_t) _socket_pending;
	CRYPTOR_SOCKET(n)->clean    = (cryptor_socket_func_clean_t) _socket_clean;

	/* Client */
	CRYPTOR_SOCKET(n)->free     = (cryptor_socket_func_free_t) _client_free;
	CRYPTOR_SOCKET(n)->init_tls = (cryptor_client_func_init_t) _client_init_tls;

	*cryp_client = CRYPTOR_CLIENT(n);
	return ret_ok;
}


PLUGIN_INFO_INIT (libssl,
		  cherokee_cryptor,
		  cherokee_cryptor_libssl_new,
		  NULL);


ret_t
cherokee_cryptor_libssl_new (cherokee_cryptor_libssl_t **cryp)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, cryptor_libssl);

	/* Init
	 */
	ret = cherokee_cryptor_init_base (CRYPTOR(n), PLUGIN_INFO_PTR(libssl));
	if (ret != ret_ok)
		return ret;

	MODULE(n)->free         = (module_func_free_t) _free;
	CRYPTOR(n)->configure   = (cryptor_func_configure_t) _configure;
	CRYPTOR(n)->vserver_new = (cryptor_func_vserver_new_t) _vserver_new;
	CRYPTOR(n)->socket_new  = (cryptor_func_socket_new_t) _socket_new;
	CRYPTOR(n)->client_new  = (cryptor_func_client_new_t) _client_new;

	*cryp = n;
	return ret_ok;
}



/* Private low-level functions for OpenSSL
 */
static pthread_mutex_t *locks;
static size_t           locks_num;

static unsigned long
__get_thread_id (void)
{
	return (unsigned long) pthread_self();
}


static void
__lock_thread (int mode, int n, const char *file, int line)
{
	UNUSED (file);
	UNUSED (line);

	if (mode & CRYPTO_LOCK) {
		CHEROKEE_MUTEX_LOCK (&locks[n]);
	} else {
		CHEROKEE_MUTEX_UNLOCK (&locks[n]);
	}
}


/* Plug-in initialization
 */

cherokee_boolean_t PLUGIN_IS_INIT(libssl) = false;

void
PLUGIN_INIT_NAME(libssl) (cherokee_plugin_loader_t *loader)
{
#if HAVE_OPENSSL_ENGINE_H
	ENGINE *e;
#endif
	UNUSED(loader);

	/* Do not initialize the library twice
	 */
	PLUGIN_INIT_ONCE_CHECK (libssl);

	/* Init OpenSSL
	 */
	CRYPTO_malloc_init();
	SSL_load_error_strings();
	SSL_library_init();
	OpenSSL_add_all_algorithms();

	/* Init concurrency related stuff
	 */
	if ((CRYPTO_get_id_callback()      == NULL) &&
	    (CRYPTO_get_locking_callback() == NULL))
	{
		cuint_t n;

		CRYPTO_set_id_callback (__get_thread_id);
		CRYPTO_set_locking_callback (__lock_thread);

		locks_num = CRYPTO_num_locks();
		locks     = malloc (locks_num * sizeof(*locks));

		for (n = 0; n < locks_num; n++) {
			CHEROKEE_MUTEX_INIT (&locks[n], NULL);
		}
	}


# if HAVE_OPENSSL_ENGINE_H
#  if OPENSSL_VERSION_NUMBER >= 0x00907000L
        ENGINE_load_builtin_engines();
#  endif
        e = ENGINE_by_id("pkcs11");
        while (e != NULL) {
                if(! ENGINE_init(e)) {
                        ENGINE_free (e);
                        LOG_CRITICAL_S (CHEROKEE_ERROR_SSL_PKCS11);
			break;
                }

                if(! ENGINE_set_default(e, ENGINE_METHOD_ALL)) {
                        ENGINE_free (e);
                        LOG_CRITICAL_S (CHEROKEE_ERROR_SSL_DEFAULTS);
			break;
                }

                ENGINE_finish(e);
                ENGINE_free(e);
		break;
        }
#endif
}
