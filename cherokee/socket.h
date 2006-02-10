/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * This piece of code by:
 *      Ricardo Cardenes Medina <ricardo@conysis.com>
 *
 * Copyright (C) 2001-2006 Alvaro Lopez Ortega
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

#ifndef CHEROKEE_SOCKET_H
#define CHEROKEE_SOCKET_H

#include "common-internal.h"


#include <sys/types.h>

#ifdef HAVE_SYS_TIME_H
#include <sys/time.h>
#else 
#include <time.h>
#endif

#include <sys/types.h>	

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
# include <sys/un.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#if defined(HAVE_GNUTLS)
# include <gnutls/gnutls.h>
#elif defined(HAVE_OPENSSL)
# include <openssl/lhash.h>
# include <openssl/ssl.h>
# include <openssl/err.h>
# include <openssl/rand.h>
#endif


#include "buffer.h"
#include "virtual_server.h"


#ifdef INET6_ADDRSTRLEN
# define CHE_INET_ADDRSTRLEN INET6_ADDRSTRLEN
#else
#  ifdef INET_ADDRSTRLEN
#    define CHE_INET_ADDRSTRLEN INET_ADDRSTRLEN
#  else
#    define CHE_INET_ADDRSTRLEN 16
#  endif
#endif


/* Socket status
 */
typedef enum {
	socket_reading,
	socket_writing,
	socket_closed
} cherokee_socket_status_t;


/* Socket crypt
 */
typedef enum {
	non_TLS,
	TLS
} cherokee_socket_type_t;


/* Socket address
 */
typedef union {
	struct sockaddr    sa;
	struct sockaddr_in sa_in;

#ifdef HAVE_SOCKADDR_UN
	struct sockaddr_un sa_un;
#endif
#ifdef HAVE_SOCKADDR_IN6
	struct sockaddr_in6 sa_in6;
#endif
#ifdef HAVE_SOCKADDR_STORAGE
	struct sockaddr_storage sa_stor;
#endif
} cherokee_sockaddr_t;


/* Socket
 */
typedef struct {
	int                       socket;
	cherokee_sockaddr_t       client_addr;
	socklen_t                 client_addr_len;
	cherokee_socket_status_t  status;
	cherokee_socket_type_t    is_tls;

#ifdef HAVE_TLS
	cherokee_boolean_t         initialized;
	cherokee_virtual_server_t *vserver_ref;
#endif
#ifdef HAVE_GNUTLS	
	gnutls_session  session;
#endif
#ifdef HAVE_OPENSSL
	SSL            *session;
	SSL_CTX        *ssl_ctx;  /* Only client socket */
#endif
} cherokee_socket_t;


#define SOCKET(s)              ((cherokee_socket_t *)(s))
#define SOCKET_FD(s)           (SOCKET(s)->socket)
#define SOCKET_AF(s)           (SOCKET(s)->client_addr.sa.sa_family)
#define SOCKET_ADDR(s)         (SOCKET(s)->client_addr)
#define SOCKET_ADDR_UNIX(s)    (SOCKET(s)->client_addr.sa_un)
#define SOCKET_ADDR_IPv4(s)    ((struct sockaddr_in  *)&SOCKET(s)->client_addr)
#define SOCKET_ADDR_IPv6(s)    ((struct sockaddr_in6 *)&SOCKET(s)->client_addr)
#define SOCKET_STATUS(s)       (SOCKET(s)->status)

#define SOCKET_SIN_PORT(s)     (SOCKET(s)->client_addr.sa_in.sin_port)
#define SOCKET_SIN_ADDR(s)     (SOCKET(s)->client_addr.sa_in.sin_addr)
#define SOCKET_SUN_PATH(s)     (SOCKET(s)->client_addr.sa_un.sun_path)

#define SOCKET_ADDRESS_IPv4(s) (SOCKET_ADDR_IPv4(s)->sin_addr.s_addr)
#define SOCKET_ADDRESS_IPv6(s) (SOCKET_ADDR_IPv6(s)->sin6_addr.s6_addr)


#ifdef HAVE_OPENSSL
# define OPENSSL_LAST_ERROR(error)  do { int n;                              \
                                         while ((n = ERR_get_error()))       \
                                          error = ERR_error_string(n, NULL); \
                                    } while (0)
#endif


ret_t cherokee_socket_new           (cherokee_socket_t **socket);
ret_t cherokee_socket_free          (cherokee_socket_t  *socket);
ret_t cherokee_socket_clean         (cherokee_socket_t  *socket);

ret_t cherokee_socket_init_tls        (cherokee_socket_t *socket, cherokee_virtual_server_t *vserver);
ret_t cherokee_socket_init_client_tls (cherokee_socket_t *socket);

ret_t cherokee_socket_close         (cherokee_socket_t *socket);
ret_t cherokee_socket_shutdown      (cherokee_socket_t *socket, int how);
ret_t cherokee_socket_accept        (cherokee_socket_t *socket, int server_socket);
ret_t cherokee_socket_set_client    (cherokee_socket_t *socket, int type);

ret_t cherokee_socket_write         (cherokee_socket_t *socket, cherokee_buffer_t *buf, size_t *written);
ret_t cherokee_socket_read          (cherokee_socket_t *socket, cherokee_buffer_t *buf, size_t count, size_t *read);
ret_t cherokee_socket_sendfile      (cherokee_socket_t *socket, int fd, size_t size, off_t *offset, ssize_t *sent);
ret_t cherokee_socket_connect       (cherokee_socket_t *socket);
ret_t cherokee_socket_set_timeout   (cherokee_socket_t *socket, cuint_t timeout);

ret_t cherokee_socket_ntop          (cherokee_socket_t *socket, char *buf, size_t buf_size);
ret_t cherokee_socket_pton          (cherokee_socket_t *socket, cherokee_buffer_t *buf);
ret_t cherokee_socket_gethostbyname (cherokee_socket_t *socket, cherokee_buffer_t *hostname);
ret_t cherokee_socket_set_status    (cherokee_socket_t *socket, cherokee_socket_status_t status);


/* Low level functions
 */
ret_t cherokee_read   (cherokee_socket_t *socket, char *buf, int buf_size, size_t *read);
ret_t cherokee_write  (cherokee_socket_t *socket, const char *buf, int len, size_t *written);
ret_t cherokee_writev (cherokee_socket_t *socket, const struct iovec *vector, uint16_t vector_len, size_t *written);

/* Extra
 */
ret_t cherokee_socket_accept_fd    (int server_socket, int *new_fd, cherokee_sockaddr_t *sa);
ret_t cherokee_socket_set_sockaddr (cherokee_socket_t *socket, int fd, cherokee_sockaddr_t *sa);

#endif /* CHEROKEE_SOCKET_H */
