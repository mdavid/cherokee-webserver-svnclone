/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * This piece of code by:
 *      Ricardo Cardenes Medina <ricardo@conysis.com>
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

#include "socket.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif

#ifdef HAVE_SYS_FILIO_H
# include <sys/filio.h>     /* defines FIONBIO and FIONREAD */
#endif
#ifdef HAVE_SYS_SOCKIO_H
# include <sys/sockio.h>    /* defines SIOCATMARK */
#endif

#ifdef HAVE_NETDB_H
# include <netdb.h>         /* defines gethostbyname()  */
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h> 
#endif

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_NETDB_H
# include <netdb.h>
#endif


/* sendfile() function related includes
 */
#if defined(LINUX_SENDFILE_API) || defined(SOLARIS_SENDFILE_API)
# include <sys/sendfile.h>

#elif defined(HPUX_SENDFILE_API)
# include <sys/socket.h>
# include <sys/uio.h>

#elif defined(FREEBSD_SENDFILE_API)
# include <sys/types.h>
# include <sys/socket.h>
# include <sys/uio.h>

#elif defined(LINUX_BROKEN_SENDFILE_API)
 extern int32 sendfile (int out_fd, int in_fd, int32 *offset, uint32 count);
#endif


/* gethostbyname_r () emulation
 */
#if defined(HAVE_PTHREAD) && !defined(HAVE_GETHOSTBYNAME_R)
static pthread_mutex_t __global_gethostbyname_mutex = PTHREAD_MUTEX_INITIALIZER;
#endif

#include <errno.h>
#include <unistd.h>
#include <fcntl.h>

#include "buffer.h"
#include "virtual_server.h"


ret_t
cherokee_socket_new (cherokee_socket_t **socket)
{
 	CHEROKEE_NEW_STRUCT (n, socket);

	/* Init 
	 */
	cherokee_socket_clean (n);

	/* Client address
	 */
	memset (&n->client_addr, 0, sizeof(struct sockaddr));
	n->client_addr_len = -1;

	/* Return it
	 */
	*socket = n;
	return ret_ok;
}


ret_t 
cherokee_socket_clean (cherokee_socket_t *socket)
{
	socket->socket   = -1;
	socket->status   = socket_closed;
	socket->is_tls   = non_TLS;

	/* Client address
	 */
	socket->client_addr_len = -1;
	memset (&socket->client_addr, 0, sizeof(struct sockaddr));

#ifdef HAVE_TLS
	socket->initialized = false;
	socket->vserver_ref = NULL;
#endif
	
	return ret_ok;
}


#ifdef HAVE_GNUTLS

static gnutls_datum
db_retrieve (void *ptr, gnutls_datum key)
{
	ret_t                     ret;
	gnutls_datum              new    = { NULL, 0 };
	cherokee_socket_t        *socket = SOCKET(ptr);
	cherokee_session_cache_t *cache;

	// printf ("db::retrieve\n");

	if (socket->vserver_ref == NULL) {
		PRINT_ERROR ("Assert failed %s, %d\n", __FILE__, __LINE__);
		return new;
	}

	cache = socket->vserver_ref->session_cache;

	/* Retrieve (get and remove) the object from the session cache
	 */
	ret = cherokee_session_cache_retrieve (cache, key.data, key.size, 
					       (void **)&new.data, &new.size);
	if (unlikely(ret != ret_ok)) return new;

	return new;
}

static int
db_remove (void *ptr, gnutls_datum key)
{
	ret_t                     ret;
	cherokee_socket_t        *socket = SOCKET(ptr);
	cherokee_session_cache_t *cache;

	// printf ("db::remove\n");

	if (socket->vserver_ref == NULL) {
		PRINT_ERROR ("ERROR: Assert failed %s, %d\n", __FILE__, __LINE__);
		return 1;
	}

	cache = socket->vserver_ref->session_cache;
	ret = cherokee_session_cache_del (cache, key.data, key.size);
	
	return (ret == ret_ok) ? 0 : 1;
}

static int
db_store (void *ptr, gnutls_datum key, gnutls_datum data)
{
	ret_t                     ret;
	cherokee_socket_t        *socket = SOCKET(ptr);
	cherokee_session_cache_t *cache;

	// printf ("db::store\n");

	if (socket->vserver_ref == NULL) {
		PRINT_ERROR ("ERROR: Assert failed %s, %d\n", __FILE__, __LINE__);
		return 1;
	}

	cache = socket->vserver_ref->session_cache;
	ret = cherokee_session_cache_add (cache, key.data, key.size, data.data, data.size);

	return (ret == ret_ok) ? 0 : 1;
}

#endif /* HAVE_GNUTLS */



static ret_t
initialize_tls_session (cherokee_socket_t *socket, cherokee_virtual_server_t *vserver)
{
	int re;

#ifdef HAVE_TLS

	/* Set the virtual server object reference
	 */
	socket->vserver_ref = vserver;

# if defined(HAVE_GNUTLS)
	/* Init the TLS session
	 */
        re = gnutls_init (&socket->session, GNUTLS_SERVER);
	if (unlikely (re != GNUTLS_E_SUCCESS)) return ret_error;

	/* Set the socket file descriptor
	 */
	gnutls_transport_set_ptr (socket->session, (gnutls_transport_ptr)socket->socket);

	/* Load the credentials
	 */
	gnutls_set_default_priority (socket->session);   

	/* Set the virtual host credentials
	 */
 	gnutls_credentials_set (socket->session, GNUTLS_CRD_CERTIFICATE, vserver->credentials);
	
	/* Request client certificate if any.
	 */
	gnutls_certificate_server_set_request (socket->session, GNUTLS_CERT_REQUEST);

	/* Set the number of bits, for use in an Diffie Hellman key
	 * exchange: minimum size of the prime that will be used for
	 * the handshake.
	 */
	gnutls_dh_set_prime_bits (socket->session, 1024);

	/* Set the session DB manage functions
	 */
	gnutls_db_set_retrieve_function (socket->session, db_retrieve);
	gnutls_db_set_remove_function   (socket->session, db_remove);
	gnutls_db_set_store_function    (socket->session, db_store);
	gnutls_db_set_ptr               (socket->session, socket);

# elif defined (HAVE_OPENSSL)

	/* New context
	 */
	socket->session = SSL_new (vserver->context);
	if (socket->session == NULL) {
		PRINT_ERROR ("ERROR: OpenSSL: Unable to create a new SSL connection from the SSL context: %s\n",
			     ERR_error_string(ERR_get_error(), NULL));
		return ret_error;
	}
	
	/* Set the socket file descriptor
	 */
	re = SSL_set_fd (socket->session, socket->socket);
	if (re != 1) {
		PRINT_ERROR ("ERROR: OpenSSL: Can not set fd(%d): %s\n", 
			     socket->socket, ERR_error_string(ERR_get_error(), NULL));
		return ret_error;
	}

	/* Set the SSL context cache
	 */
	re = SSL_CTX_set_session_id_context (vserver->context, "SSL", 3);
	if (re != 1) {
		PRINT_ERROR_S("ERROR: OpenSSL: Unable to set SSL session-id context\n");
	}
# endif
#endif
	return ret_ok;
}


ret_t 
cherokee_socket_init_tls (cherokee_socket_t *socket, cherokee_virtual_server_t *vserver)
{
#ifdef HAVE_TLS
	int rc;
	
	socket->is_tls = TLS;

	if (socket->initialized == false) {
		initialize_tls_session (socket, vserver);
		socket->initialized = true;
	}

# ifdef HAVE_GNUTLS
	rc = gnutls_handshake (socket->session);

	switch (rc) {
	case GNUTLS_E_AGAIN:
		return ret_eagain;
	case GNUTLS_E_INTERRUPTED:
		return ret_error;
	}
	
	if (rc < 0) {
		PRINT_ERROR ("ERROR: Init GNUTLS: Handshake has failed: %s\n", gnutls_strerror(rc));
		return ret_error;
	}
# endif

# ifdef HAVE_OPENSSL
	rc = SSL_accept (socket->session);

	if (rc <= 0) {
		switch (SSL_get_error(socket->session, rc)) {
		case SSL_ERROR_WANT_READ:
		case SSL_ERROR_WANT_WRITE:
		case SSL_ERROR_WANT_CONNECT:
			return ret_eagain;
		default:
			PRINT_ERROR ("ERROR: OpenSSL: %s\n", ERR_error_string(ERR_get_error(), NULL));
			return ret_error;
		}
	}
# endif
#endif
	return ret_ok;
}


ret_t
cherokee_socket_free (cherokee_socket_t *socket)
{
#ifdef HAVE_GNUTLS
	if (socket->session != NULL) {
		
	}
#endif

#ifdef HAVE_OPENSSL
	if (socket->session != NULL) {
		SSL_free (socket->session);
		socket->session = NULL;
	}
#endif

	free (socket);
	return ret_ok;
}

ret_t       
cherokee_socket_close (cherokee_socket_t *socket)
{
	int re;

	if (socket->socket <= 0) {
		return ret_error;
	}

	if (socket->is_tls == TLS) {
#ifdef HAVE_GNUTLS
		gnutls_bye (socket->session, GNUTLS_SHUT_WR);
		gnutls_deinit (socket->session);
#endif

#ifdef HAVE_OPENSSL
		SSL_shutdown (socket->session);
#endif
	}

	re = close (socket->socket);

	socket->socket = -1;
	socket->status = socket_closed;
	socket->is_tls = non_TLS;

	return (re == 0) ? ret_ok : ret_error;
}


ret_t
cherokee_socket_ntop (cherokee_socket_t *socket, char *dst, size_t cnt)
{
	const char *str = NULL;

	errno = EAFNOSUPPORT;

	if (SOCKET_FD(socket) <= 0) {
		return ret_error;
	}

	/* Only old systems without inet_ntop() function
	 */
#ifndef HAVE_INET_NTOP
	{
		struct sockaddr_in *addr = (struct sockaddr_in *) &SOCKET_ADDR(socket);
		
		str = inet_ntoa (addr->sin_addr);
		memcpy(dst, str, strlen(str));

		return ret_ok;
	}
#endif


#ifdef HAVE_IPV6
	if (SOCKET_AF(socket) == AF_INET6) {
		struct sockaddr_in6 *addr = (struct sockaddr_in6 *) &SOCKET_ADDR(socket);

		str = (char *) inet_ntop (AF_INET6, &addr->sin6_addr, dst, cnt);
		if (str == NULL) {
			return ret_error;
		}
	} else
#endif
	{
		struct sockaddr_in *addr = (struct sockaddr_in *) &SOCKET_ADDR(socket);
		
		str = inet_ntop (AF_INET, &addr->sin_addr, dst, cnt);
		if (str == NULL) {
			return ret_error;
		}
	}

	return ret_ok;
}


ret_t 
cherokee_socket_pton (cherokee_socket_t *socket, cherokee_buffer_t *host)
{
	int r;

#ifdef HAVE_IPV6
	if (SOCKET_AF(socket) == AF_INET6) {
		r = inet_pton (AF_INET6, host->buf, &SOCKET_SIN_ADDR(socket));
	} else 
#endif
	{
		r = inet_pton (AF_INET, host->buf, &SOCKET_SIN_ADDR(socket));
	}

	return (r > 0) ? ret_ok : ret_error;
}


ret_t 
cherokee_socket_accept (cherokee_socket_t *socket, int server_socket)
{
	ret_t           ret;
	int             fd;
	struct sockaddr sa;

	ret = cherokee_socket_accept_fd (server_socket, &fd, &sa);
	if (unlikely(ret < ret_ok)) return ret;

	ret = cherokee_socket_set_sockaddr (socket, fd, &sa);
	if (unlikely(ret < ret_ok)) return ret;

	return ret_ok;
}

ret_t 
cherokee_socket_set_sockaddr (cherokee_socket_t *socket, int fd, struct sockaddr *sa)
{
	/* Copy the client address
	 */
	switch (sa->sa_family) {
	case AF_INET: 
		socket->client_addr_len = sizeof(struct sockaddr_in);
		break;
#ifdef HAVE_IPV6
	case AF_INET6:
		socket->client_addr_len = sizeof(struct sockaddr_in6);
		break;
#endif
	default:
		SHOULDNT_HAPPEN;
		return ret_error;
	}

	memcpy (&socket->client_addr, sa, socket->client_addr_len);
	
	SOCKET_FD(socket) = fd;
	return ret_ok;
}


ret_t
cherokee_socket_accept_fd (int server_socket, int *new_fd, struct sockaddr *sa)
{
	int tmp = 1;
	int new_socket;

	/* Get the new connection
	 */
	tmp = sizeof (struct sockaddr);
	new_socket = accept (server_socket, sa, &tmp);
	if (new_socket <= 0) {
		return ret_error;
	}		

	/* <EXPERIMENTAL>
	 */
#ifdef _WIN32
        WSAResetEvent (socket->hSelectEvent);
        WSAEventSelect (new_socket, socket->hSelectEvent, FD_READ | FD_CLOSE);
#else
	fcntl (new_socket, F_SETFD, FD_CLOEXEC);  /* close-on-exec */
#endif
	
	/* Disable Nagle's algorithm for this connection.
	 * Written data to the network is not buffered pending
	 * acknowledgement of previously written data.
	 */
 	setsockopt (new_socket, IPPROTO_TCP, TCP_NODELAY, (const void *)&tmp, sizeof(tmp));

	/*  </EXPERIMENTAL>
	 */
	
	/* Enables nonblocking I/O.
	 */
	tmp = ioctl (new_socket, FIONBIO, &tmp);
	if (tmp < 0) {
		PRINT_ERROR ("ERROR: Setting 'FIONBIO' in socked fd=%d\n", new_socket);
		return ret_error;
	}

	*new_fd = new_socket;
	return ret_ok;
}


ret_t 
cherokee_socket_set_client (cherokee_socket_t *sock)
{
	sock->socket = socket (AF_INET, SOCK_STREAM, 0);
	if (sock->socket < 0) {
		return ret_error;
	}
	
	SOCKET_AF(sock) = AF_INET;

	return ret_ok;
}


ret_t       
cherokee_socket_set_status (cherokee_socket_t *socket, cherokee_socket_status_t status)
{
	socket->status = status;

	if (status == socket_closed) {
		cherokee_socket_close (socket);
	}

	return ret_ok;
}


ret_t 
cherokee_write (cherokee_socket_t *socket, const char *buf, int buf_len, size_t *writed)
{
	ssize_t len;

	return_if_fail (buf != NULL, ret_error);


	if (socket->is_tls == TLS) {
#ifdef HAVE_GNUTLS
		len = gnutls_record_send (socket->session, buf, buf_len);
		
		if (len < 0) {
			switch (len) {
			case GNUTLS_E_PUSH_ERROR:      return ret_eof;
			case GNUTLS_E_INTERRUPTED:     return ret_eof;
			case GNUTLS_E_INVALID_SESSION: return ret_eof;
			case GNUTLS_E_AGAIN:           return ret_eagain;
			}

			PRINT_ERROR ("ERROR: GNUTLS: gnutls_record_send(%d, ..) -> err=%d '%s'\n", 
				     SOCKET_FD(socket), len, gnutls_strerror(len));

			*writed = 0;
			return ret_error;

		} else if (len == 0) {
			return ret_eof;
		}
#endif

#ifdef HAVE_OPENSSL
		len = SSL_write (socket->session, buf, buf_len);

		if (len < 0) {
			int re;

			re = SSL_get_error (socket->session, len);
			switch (re) {
			case SSL_ERROR_WANT_WRITE: return ret_eagain;
			case SSL_ERROR_SSL:        return ret_error;
			}

			PRINT_ERROR ("ERROR: SSL_write (%d, ..) -> err=%d '%s'\n", 
				     SOCKET_FD(socket), len, ERR_error_string(re, NULL));

			*writed = 0;
			return ret_error;

		} else if (len == 0) {
			return ret_eof;
		}
#endif		

		goto out;
	}


#ifdef _WIN32
	len = send (SOCKET_FD(socket), buf, buf_len, 0);
#else 	
	len = write (SOCKET_FD(socket), buf, buf_len);
#endif

	if (len < 0) {
		switch (errno) {
		case EAGAIN:      
		case EINTR:       
			return ret_eagain;
		case EPIPE:       
		case ECONNRESET:  
			return ret_eof;
		}
	
		PRINT_ERROR ("ERROR: write(%d, ..) -> errno=%d '%s'\n", 
			     SOCKET_FD(socket), errno, strerror(errno));

		*writed = 0;
		return ret_error;

	}  else if (len == 0) {
		return ret_eof;
	}

out:
	/* Return info
	 */
	*writed = len;
	return ret_ok;
}


ret_t
cherokee_read (cherokee_socket_t *socket, char *buf, int buf_size, size_t *readed)
{
	ssize_t len;

	if (socket->is_tls == TLS) {
#ifdef HAVE_GNUTLS
		if (unlikely (buf == NULL)) {
			char tmp[buf_size+1];
			len = gnutls_record_recv (socket->session, tmp, buf_size);
		} else {
			len = gnutls_record_recv (socket->session, buf, buf_size);
		}

		if (len < 0) {
			switch (len) {
			case GNUTLS_E_INTERRUPTED:              
			case GNUTLS_E_UNEXPECTED_PACKET_LENGTH: 
				return ret_eof;
			case GNUTLS_E_AGAIN:
				return ret_eagain;
			}
			
			PRINT_ERROR ("ERROR: GNUTLS: gnutls_record_recv(%d, ..) -> err=%d '%s'\n", 
				     SOCKET_FD(socket), len, gnutls_strerror(len));

			*readed = 0;
			return ret_error;
			
		} else if (len == 0) {
			return ret_eof;
		}
#endif

#ifdef HAVE_OPENSSL
		if (unlikely (buf == NULL)) {
			char tmp[buf_size+1];
			len = SSL_read (socket->session, tmp, buf_size);
		} else {
			len = SSL_read (socket->session, buf, buf_size);
		}
		
		if (len < 0) {
			int re;

			re = SSL_get_error (socket->session, len);
			switch (re) {
			case SSL_ERROR_WANT_READ:   return ret_eagain;
			case SSL_ERROR_ZERO_RETURN: return ret_eof;
			case SSL_ERROR_SSL:         return ret_error;
			}

			PRINT_ERROR ("ERROR: OpenSSL: SSL_read (%d, ..) -> err=%d '%s'\n", 
				     SOCKET_FD(socket), len, ERR_error_string(re, NULL));

			*readed = 0;
			return ret_error;

		} else if (len == 0) {
			return ret_eof;
		}
#endif
		
		goto out;
	}


		if (unlikely (buf == NULL)) {
			char tmp[buf_size+1];
			len = read (SOCKET_FD(socket), tmp, buf_size);
		} else {
			len = read (SOCKET_FD(socket), buf, buf_size);
		}

	if (len < 0) {
		switch (errno) {
		case EAGAIN:    
		case EINTR:      
			return ret_eagain;
		case EPIPE: 
		case ECONNRESET:
			return ret_eof;
		}

		PRINT_ERROR ("ERROR: read(%d, ..) -> errno=%d '%s'\n", 
			     SOCKET_FD(socket), errno, strerror(errno));
		
		*readed = 0;
		return ret_error;

	} else if (len == 0) {
		return ret_eof;
	}
	
out:
	/* Return info
	 */
	if (readed != NULL)
		*readed = len;

	return ret_ok;
}


ret_t 
cherokee_writev (cherokee_socket_t *socket, const struct iovec *vector, uint16_t vector_len, size_t *writed)
{
#ifdef _WIN32
	DWORD re;
	WSASend (SOCKET_FD(socket), (LPWSABUF)vector, vector_len, &re, 0, NULL, NULL);
#else
	int re;
	re = writev (SOCKET_FD(socket), vector, vector_len);
#endif

	if (re <= 0) {
		switch (errno) {
		case EAGAIN:
		case EINTR: 
			return ret_eagain;
		case EPIPE:
		case ECONNRESET:
			return ret_eof;
		}
		
		PRINT_ERROR ("ERROR: writev(%d, ..) -> errno=%d '%s'\n", 
			     SOCKET_FD(socket), errno, strerror(errno));

		*writed = 0;
		return ret_error;
	}

	*writed = re;
	return ret_ok;
}


ret_t       
cherokee_socket_write (cherokee_socket_t *socket, cherokee_buffer_t *buf, size_t *writed)
{
	ret_t   ret;
	size_t _writed;

	ret = cherokee_write (socket, buf->buf, buf->len, &_writed);

	*writed = _writed;
	return ret;
}


ret_t      
cherokee_socket_read (cherokee_socket_t *socket, cherokee_buffer_t *buf, size_t count, size_t *readed)
{
	ret_t    ret;
	char    *starting;
	
	/* Special case: read to /dev/null
	 */
	if (buf == NULL) {
		return cherokee_read (socket, NULL, count, NULL);
	}

	/* Read
	 */
	ret = cherokee_buffer_ensure_size (buf, buf->len + count +2);
	if (unlikely(ret < ret_ok)) return ret;

	starting = buf->buf + buf->len;

	ret = cherokee_read (socket, starting, count, readed);
	if (ret == ret_ok) {
		buf->len += *readed;
		buf->buf[buf->len] = '\0';
	}

	return ret;
}


ret_t 
cherokee_socket_sendfile (cherokee_socket_t *socket, int fd, size_t size, off_t *offset, ssize_t *sent)
{
#ifndef HAVE_SENDFILE
	SHOULDNT_HAPPEN;
	return ret_error;
#else

# ifdef FREEBSD_SENDFILE_API
	int            re;
	struct sf_hdtr hdr;
	struct iovec   hdtrl;

	hdr.headers    = &hdtrl;
	hdr.hdr_cnt    = 1;
	hdr.trailers   = NULL;
	hdr.trl_cnt    = 0;
	
	hdtrl.iov_base = NULL;
	hdtrl.iov_len  = 0;


	/* FreeBSD sendfile: in_fd and out_fd are reversed
	 *
	 * int
	 * sendfile (int fd, int s, off_t offset, size_t nbytes,
	 *           struct sf_hdtr *hdtr, off_t *sbytes, int flags);
	 */
	
	do {
		re = sendfile (fd,                        /* int             fd     */
			       SOCKET_FD(socket),         /* int             s      */
			       *offset,                   /* off_t           offset */
			       size,                      /* size_t          nbytes */
			       &hdr,                      /* struct sf_hdtr *hdtr   */
			       sent,                      /* off_t          *sbytes */ 
			       0);                        /* int             flags  */

	}  while (re == -1 && errno == EINTR);

	if (*sent < 0) {
		if (errno == EAGAIN) {
			return ret_eagain;
		}
		return ret_error;
	}
	*offset = *offset + *sent;

# elif HPUX_SENDFILE_API
	
	/* HP-UX:
	 *
	 * sbsize_t sendfile(int s, int fd, off_t offset, bsize_t nbytes, 
	 *                   const struct iovec *hdtrl, int flags); 
	 *
	 * HPUX guarantees that if any data was written before
	 * a signal interrupt then sendfile returns the number of
	 * bytes written (which may be less than requested) not -1.
	 * nwritten includes the header data sent.
	 */
	do {
		*sent = sendfile (SOCKET_FD(socket),      /* socket          */
				  fd,                     /* fd to send      */
				  *offset,                /* where to start  */
				  size,                   /* bytes to send   */
				  NULL,                   /* Headers/footers */
				  0);                     /* flags           */
	} while ((*sent == -1) && (errno == EINTR));

	if (*sent < 0) {
		if (errno == EAGAIN) {
			return ret_eagain;
		}
		return ret_error;
	}
	*offset = *offset + *sent;

# else
	/* Linux sendfile
	 *
	 * ssize_t 
	 * sendfile (int out_fd, int in_fd, off_t *offset, size_t *count);
	 */
	*sent = sendfile (SOCKET_FD(socket),              /* int     out_fd */
			  fd,                             /* int     in_fd  */
			  offset,                         /* off_t  *offset */
			  size);                          /* size_t  count  */

	if (*sent < 0) {
		if (errno == EAGAIN) 
			return ret_eagain;
		
		return ret_error;
	}
# endif

	return ret_ok;

#endif /* HAVE_SENDFILE */
}


ret_t
cherokee_socket_gethostbyname (cherokee_socket_t *socket, cherokee_buffer_t *hostname)
{
#if !defined(HAVE_PTHREAD) || (defined(HAVE_PTHREAD) && !defined(HAVE_GETHOSTBYNAME_R))

	struct hostent *host;

	CHEROKEE_MUTEX_LOCK (&__global_gethostbyname_mutex);
	/* Resolv the host name
	 */
	host = gethostbyname (hostname->buf);
	if (host == NULL) {
		CHEROKEE_MUTEX_UNLOCK (&__global_gethostbyname_mutex);
		return ret_error;
	}

	/* Copy the address
	 */
	memcpy (&SOCKET_SIN_ADDR(socket), host->h_addr, host->h_length);
	CHEROKEE_MUTEX_UNLOCK (&__global_gethostbyname_mutex);
	return ret_ok;

#elif defined(HAVE_PTHREAD) && defined(HAVE_GETHOSTBYNAME_R)

/* Maximum size that should use gethostbyname_r() function.
 * It will return ERANGE, if more space is needed.
 */
# define GETHOSTBYNAME_R_BUF_LEN 512

	int             r;
	int             h_errnop;
	struct hostent  hs;
	struct hostent *hp;
	char   tmp[GETHOSTBYNAME_R_BUF_LEN];
	

#ifdef SOLARIS
	/* Solaris 10:
	 * struct hostent *gethostbyname_r
	 *        (const char *, struct hostent *, char *, int, int *h_errnop);
	 */
	hp = gethostbyname_r (hostname->buf, &hs, 
			      tmp, GETHOSTBYNAME_R_BUF_LEN - 1, &h_errnop);
	if (hp == NULL) {
		return ret_error;
	}	
#else
	/* Linux glibc2:
	 *  int gethostbyname_r (const char *name,
	 *         struct hostent *ret, char *buf, size_t buflen,
	 *         struct hostent **result, int *h_errnop);
	 */
	r = gethostbyname_r (hostname->buf, 
			     &hs, tmp, GETHOSTBYNAME_R_BUF_LEN - 1, 
			     &hp, &h_errnop);
	if (r != 0) {
		return ret_error;
	}
#endif	

	/* Copy the address
	 */
	memcpy (&SOCKET_SIN_ADDR(socket), hp->h_addr, hp->h_length);

	return ret_ok;
#else

	SHOULDNT_HAPPEN;
	return ret_error;
#endif
}



ret_t 
cherokee_socket_connect (cherokee_socket_t *socket)
{
	int r;

	r = connect (SOCKET_FD(socket), (struct sockaddr *) &SOCKET_ADDR(socket), sizeof(cherokee_sockaddr_t));
	if (r < 0) {
		switch (r) {
		case ECONNREFUSED: return ret_deny;
		case EAGAIN:       return ret_eagain;
		default:           
#if 1
			PRINT_ERROR ("ERROR: Can not connect: %s\n", strerror(errno));
#endif
			return ret_error;
		}
	}

	return ret_ok;
}
