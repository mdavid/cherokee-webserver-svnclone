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
#include "server-protected.h"
#include "server.h"

#include <fcntl.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <pwd.h>
#include <grp.h>

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_TCP_H
# include <netinet/tcp.h>
#endif

#include <errno.h>

#ifdef HAVE_SYS_IOCTL_H
# include <sys/ioctl.h>
#endif

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#ifdef HAVE_GNUTLS
# include <gnutls/gnutls.h>	
# include <gcrypt.h>
#endif

#include <signal.h>
#include <dirent.h>
#include <unistd.h>

#include "socket.h"
#include "connection.h"
#include "ncpus.h"
#include "mime.h"
#include "thread.h"
#include "list_ext.h"
#include "util.h"
#include "fdpoll.h"
#include "fdpoll-protected.h"


static ret_t init_tls_library          (void);
static ret_t read_default_config_files (cherokee_server_t *srv, char *filename);

#ifdef HAVE_GNUTLS
# ifdef GCRY_THREAD_OPTION_PTHREAD_IMPL
GCRY_THREAD_OPTION_PTHREAD_IMPL;
# endif
#endif


ret_t
cherokee_server_new  (cherokee_server_t **srv)
{
	ret_t ret;

	/* Get memory
	 */
	CHEROKEE_NEW_STRUCT(n, server);
	
	/* Thread list
	 */
	INIT_LIST_HEAD(&(n->thread_list));

	/* Default Index files list
	 */
	INIT_LIST_HEAD(&(n->index_list));

	/* Include config files list
	 */
	INIT_LIST_HEAD(&(n->include_list));

	/* Sockets
	 */
	n->socket          = -1;
	n->socket_tls      = -1;
	n->ipv6            =  1;

	/* Config files
	 */
	n->config_file     = NULL;	
	n->icons_file      = NULL;
	n->mime_file       = NULL;

	/* Exit related
	 */
	n->wanna_exit      = 0;
	n->panic_action    = NULL;
	
	/* Server config
	 */
	n->port            = 80;
	n->port_tls        = 443;
	n->tls_enabled     = false;

	n->listen_to       = NULL;
	n->fdwatch_msecs   = 999;

	n->start_time      = time(NULL);

	n->keepalive       = true;
	n->keepalive_max   = MAX_KEEPALIVE;

	n->ncpus           = -1;
	n->thread_num      = -1;
	n->thread_policy   = -1;

	n->chroot          = NULL;
	n->chrooted        = 0;

	n->user_orig       = getuid();
	n->user            = n->user_orig;
	n->group_orig      = getgid();
	n->group           = n->group_orig;

	n->timeout         = 15;

	n->max_fds         = -1;
	n->system_fd_limit = -1;
	n->max_conn_reuse  = -1;

	n->listen_queue    = 1024;
	n->sendfile.min    = 32768;
	n->sendfile.max    = 2147483647;

	/* Time managing hack
	 */
	cherokee_buffer_new (&n->timeout_header);
	cherokee_buffer_add (n->timeout_header, "Keep-Alive: timeout=15"CRLF, 48);

	/* Bogo now
	 */
	CHEROKEE_RWLOCK_INIT (&n->bogo_now_mutex, NULL);

	CHEROKEE_RWLOCK_WRITER (&n->bogo_now_mutex);
	cherokee_buffer_new (&n->bogo_now_string);
	cherokee_buffer_ensure_size (n->bogo_now_string, 100);	
	CHEROKEE_RWLOCK_UNLOCK (&n->bogo_now_mutex);

	/* Accepting mutexes
	 */
#ifdef HAVE_TLS
	CHEROKEE_MUTEX_INIT (&n->accept_tls_mutex, NULL);
#endif
	CHEROKEE_MUTEX_INIT (&n->accept_mutex, NULL);

#ifndef CHEROKEE_EMBEDDED
	/* Icons 
	 */
	cherokee_icons_new (&n->icons);
	return_if_fail (n->icons != NULL, ret_nomem);

	/* Mmap cache
	 */
	cherokee_mmap2_new (&n->mmap_cache);
	return_if_fail (n->mmap_cache != NULL, ret_nomem);	
	n->mmap_cache_clean_next = 0;
#endif

	/* Module loader
	 */
	cherokee_module_loader_new (&n->loader);
#ifndef CHEROKEE_EMBEDDED
	return_if_fail (n->loader != NULL, ret_nomem);	
#endif

	/* Virtual servers table
	 */
	INIT_LIST_HEAD (&n->vservers);

	cherokee_table_new (&n->vservers_ref);
	return_if_fail (n->vservers_ref!=NULL, ret_nomem);

	cherokee_virtual_server_new (&n->vserver_default);
	return_if_fail (n->vserver_default!=NULL, ret_nomem);
		
	/* Encoders 
	 */
	cherokee_encoder_table_new (&n->encoders);
	return_if_fail (n->encoders != NULL, ret_nomem);

	/* Server string
	 */
	n->server_token = cherokee_version_full;
	cherokee_buffer_new (&n->server_string);

	/* Loggers
	 */
	n->log_flush_next   = 0;
	n->log_flush_elapse = 10;

	cherokee_logger_table_new (&n->loggers);
	return_if_fail (n->loggers != NULL, ret_nomem);

	/* TLS
	 */
	ret = init_tls_library();
	if (ret < ret_ok) {
		return ret;
	}

	/* Return the object
	 */
	*srv = n;
	return ret_ok;
}


static inline void
close_all_connections (cherokee_server_t *srv)
{
	list_t *i;

	cherokee_thread_close_all_connections (srv->main_thread);
	
	list_for_each (i, &srv->thread_list) {
		cherokee_thread_close_all_connections (THREAD(i));
	}
}


static void
free_virtual_servers (cherokee_server_t *srv)
{
	list_t *i, *j;

	list_for_each_safe (i, j, &srv->vservers) {
		cherokee_virtual_server_free (VSERVER(i));
		list_del(i);
	}
}


ret_t 
cherokee_server_clean (cherokee_server_t *srv)
{
	/* Close all active connections
	 */
	close_all_connections (srv);

	/* Clean the configuration
	 */
	if (srv->listen_to != NULL) {
		free (srv->listen_to);
		srv->listen_to = NULL;
	}

	if (srv->chroot != NULL) {
		free (srv->chroot);
		srv->chroot = NULL;
	}


	/* Clean config files entries
	 */
	cherokee_list_free (&srv->include_list, free);
	
	/* Exit related
	 */
 	srv->wanna_exit = 0;

	if (srv->panic_action != NULL) {
		free (srv->panic_action);
		srv->panic_action = NULL;
	}

	/* Clean the loader
	 */
	cherokee_module_loader_free (srv->loader);
	srv->loader = NULL;

#ifndef CHEROKEE_EMBEDDED
	/* Icons 
	 */
	cherokee_icons_clean (srv->icons);

	if (srv->icons_file != NULL) {
		free (srv->icons_file);
		srv->icons_file = NULL;
	}

	/* Mmap cache
	 */
	cherokee_mmap2_clean (srv->mmap_cache);

	/* Mime
	 */
	if (srv->mime_file != NULL) {
		free (srv->mime_file);
		srv->mime_file = NULL;
	}
#endif

	/* Clean the encoders table
	 */
	cherokee_encoder_table_clean (srv->encoders);

	/* Clean the loggers table
	 */
	cherokee_logger_table_clean (srv->loggers);

	/* Clean the virtual servers table
	 */
	cherokee_virtual_server_clean (srv->vserver_default);

	/* Clean the virtual servers table
	 */	
	free_virtual_servers (srv);
	cherokee_table_clean (srv->vservers_ref);

	/* Time-out pre-builded header
	 */
	cherokee_buffer_clean (srv->timeout_header);

	/* Server string
	 */
	srv->server_token = cherokee_version_full;
	cherokee_buffer_clean (srv->server_string);

	/* Clean the index list
	 */
	cherokee_list_free (&srv->index_list, free);
	INIT_LIST_HEAD(&srv->index_list);

	srv->system_fd_limit = -1;
	srv->max_fds         = -1;
	srv->max_conn_reuse  = -1;

	srv->listen_queue  = 1024;
	srv->sendfile.min  = 32768;
	srv->sendfile.max  = 2147483647;

	srv->keepalive_max = MAX_KEEPALIVE;

	return ret_ok;
}


ret_t
cherokee_server_free (cherokee_server_t *srv)
{
	close (srv->socket);

	if (srv->socket_tls != -1) {
		close (srv->socket_tls);		
	}

	cherokee_encoder_table_free (srv->encoders);
	cherokee_logger_table_free (srv->loggers);

	cherokee_list_free (&srv->index_list, free);

#ifdef HAVE_TLS
	CHEROKEE_MUTEX_DESTROY (&srv->accept_tls_mutex);
#endif
	CHEROKEE_MUTEX_DESTROY (&srv->accept_mutex);

#ifndef CHEROKEE_EMBEDDED
	/* Icons 
	 */
	cherokee_icons_free (srv->icons);
	if (srv->icons_file != NULL) {
		free (srv->icons_file);
		srv->icons_file = NULL;
	}

	/* Mime
	 */
	if (srv->mime_file != NULL) {
		free (srv->mime_file);
		srv->mime_file = NULL;
	}

	cherokee_mmap2_free (srv->mmap_cache);
#endif
	
	/* Module loader
	 */
	cherokee_module_loader_free (srv->loader);

	/* Virtual servers
	 */
	cherokee_virtual_server_free (srv->vserver_default);
	srv->vserver_default = NULL;

	free_virtual_servers (srv);
	cherokee_table_free (srv->vservers_ref);

	cherokee_buffer_free (srv->bogo_now_string);
	cherokee_buffer_free (srv->server_string);

	cherokee_buffer_free (srv->timeout_header);

	if (srv->listen_to != NULL) {
		free (srv->listen_to);
		srv->listen_to = NULL;
	}

	if (srv->chroot != NULL) {
		free (srv->chroot);
		srv->chroot = NULL;
	}

	/* Clean config files entries
	 */
	cherokee_list_free (&srv->include_list, free);

	if (srv->config_file != NULL) {
		free (srv->config_file);
		srv->config_file = NULL;
	}

	if (srv->panic_action != NULL) {
		free (srv->panic_action);
		srv->panic_action = NULL;
	}

	free (srv);	
	return ret_ok;
}


static ret_t
init_tls_library (void)
{
	int rc;
	
#ifdef HAVE_GNUTLS
# ifdef GCRY_THREAD_OPTION_PTHREAD_IMPL
	/* Although the GnuTLS library is thread safe by design, some
	 * parts of the crypto backend, such as the random generator,
	 * are not; hence, it needs to initialize some semaphores.
	 */
	gcry_control (GCRYCTL_SET_THREAD_CBS, &gcry_threads_pthread); 
# endif

	/* Gnutls library-width initialization
	 */
	rc = gnutls_global_init();
	if (rc < 0) {
		PRINT_ERROR_S ("Global GNUTLS state initialisation failed.\n");
		return ret_error;
	}
#endif

#ifdef HAVE_OPENSSL
	SSL_load_error_strings();
	SSL_library_init(); 
#endif

	return ret_ok;
}


static inline ret_t
change_execution_user (cherokee_server_t *srv)
{
	int            error;
	struct passwd *ent;

        /* Get user information
	 */
	ent = getpwuid (srv->user);
	if (ent == NULL) {
		PRINT_ERROR ("Can't get username for UID %d\n", srv->user);
		return ret_error;
	}
	
	/* Reset `groups' attributes.
	 */
	if (srv->user_orig == 0) {
		error = initgroups (ent->pw_name, srv->group);
		if (error == -1) {
			PRINT_ERROR ("initgroups: Unable to set groups for user `%s' and GID %d\n", 
				     ent->pw_name, srv->group);
		}
	}

	/* Change of group requested
	 */
	if (srv->group != srv->group_orig) {
		error = setgid (srv->group);
		if (error != 0) {
			PRINT_ERROR ("Can't change group to GID %d, running with GID=%d\n",
				     srv->group, srv->group_orig);
		}
	}

	/* Change of user requested
	 */
	if (srv->user != srv->user_orig) {
		error = setuid (srv->user);		
		if (error != 0) {
			PRINT_ERROR ("Can't change user to UID %d, running with UID=%d\n",
				     srv->user, srv->user_orig);
		}
	}

	return ret_ok;
}


void  
cherokee_server_set_min_latency (cherokee_server_t *srv, int msecs)
{
	srv->fdwatch_msecs = msecs;
}


static ret_t
set_server_socket_opts (int socket)
{
	int           re;
	int           on;
        struct linger ling = {0, 0};

	/* Set 'close-on-exec'
	 */
	fcntl (socket, F_SETFD, FD_CLOEXEC);
		
	/* To re-bind without wait to TIME_WAIT
	 */
	on = 1;
	re = setsockopt (socket, SOL_SOCKET, SO_REUSEADDR, &on, sizeof(on));
	if (re != 0) return ret_error;

	/* TCP_MAXSEG:
	 * The maximum size of a TCP segment is based on the network MTU for des-
	 * tinations on local networks or on a default MTU of 576 bytes for desti-
	 * nations on nonlocal networks.  The default behavior can be altered by
	 * setting the TCP_MAXSEG option to an integer value from 1 to 65,535.
	 * However, TCP will not use a maximum segment size smaller than 32 or
	 * larger than the local network MTU.  Setting the TCP_MAXSEG option to a
	 * value of zero results in default behavior.  The TCP_MAXSEG option can
	 * only be set prior to calling listen or connect on the socket.  For pas-
	 * sive connections, the TCP_MAXSEG option value is inherited from the
	 * listening socket. This option takes an int value, with a range of 0 to
	 * 65535.
	 */
	on = 64000;
	re = setsockopt (socket, SOL_SOCKET, TCP_MAXSEG, &on, sizeof(on));
	if (re != 0) return ret_error;
	
	/* SO_LINGER
	 * kernels that map pages for IO end up failing if the pipe is full
         * at exit and we take away the final buffer.  this is really a kernel
         * bug but it's harmless on systems that are not broken, so...
	 *
	 * http://www.apache.org/docs/misc/fin_wait_2.html
         */
	setsockopt (socket, SOL_SOCKET, SO_LINGER, &ling, sizeof(ling));

	/* TCP_DEFER_ACCEPT:
	 * Allows a listener to be awakened only when data arrives on the socket.
	 * Takes an integer value (seconds), this can bound the maximum number of
	 * attempts TCP will make to complete the connection. This option should 
	 * not be used in code intended to be portable.
	 *
	 * Give clients 10s to send first data packet 
	 */
#if defined (TCP_DEFER_ACCEPT)
	on = 10;
	re = setsockopt (socket, SOL_SOCKET, TCP_DEFER_ACCEPT, &on, sizeof(on));
	if (re != 0) return ret_error;
#endif

	/* TODO:
	 * Maybe add support for the FreeBSD accept filters:
	 * http://www.freebsd.org/cgi/man.cgi?query=accf_http
	 */

	return ret_ok;
}


static ret_t
initialize_server_socket4 (cherokee_server_t *srv, unsigned short port, int *srv_socket_ret)
{
	int re;
	int srv_socket;
	struct sockaddr_in sockaddr;

	srv_socket = socket (AF_INET, SOCK_STREAM, 0);
	if (srv_socket <= 0) {
		PRINT_ERROR_S ("Error creating IPv4 server socket\n");
		exit(EXIT_CANT_CREATE_SERVER_SOCKET4);
	}

	*srv_socket_ret = srv_socket;
	set_server_socket_opts(srv_socket);

	/* Bind the socket
	 */
	memset (&sockaddr, 0, sizeof(struct sockaddr_in));

	sockaddr.sin_port   = htons (port);
	sockaddr.sin_family = AF_INET;

	if (srv->listen_to == NULL) {
		sockaddr.sin_addr.s_addr = INADDR_ANY; 
	} else {
#ifdef HAVE_INET_PTON
		inet_pton (sockaddr.sin_family, srv->listen_to, &sockaddr.sin_addr);
#else
		/* IPv6 needs inet_pton. inet_addr just doesn't work without
		 * it. We'll suppose then that we haven't IPv6 support 
		 */
		sockaddr.sin_addr.s_addr = inet_addr (srv->listen_to);
#endif
	}

	re = bind (srv_socket, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in));
	return (re == 0) ? ret_ok : ret_error;
}


static ret_t
initialize_server_socket6 (cherokee_server_t *srv, unsigned short port, int *srv_socket_ret)
{
#ifdef HAVE_IPV6
	int re;
	int srv_socket;
	struct sockaddr_in6 sockaddr;

	/* Create the socket
	 */
	srv_socket = socket (AF_INET6, SOCK_STREAM, 0);
	if (srv_socket <= 0) {
		PRINT_ERROR_S ("Error creating IPv6 server socket.. switching to IPv4\n");
		srv->ipv6 = 0;
		return ret_error;
	}

	*srv_socket_ret = srv_socket;
	set_server_socket_opts(srv_socket);

	/* Bind the socket
	 */
	memset (&sockaddr, 0, sizeof(struct sockaddr_in6));

	sockaddr.sin6_port   = htons (port);  /* Transport layer port #   */
	sockaddr.sin6_family = AF_INET6;      /* AF_INET6                 */

	if (srv->listen_to == NULL) {
		sockaddr.sin6_addr = in6addr_any; 
	} else {
		inet_pton (sockaddr.sin6_family, srv->listen_to, &sockaddr.sin6_addr);
	}

	re = bind (srv_socket, (struct sockaddr *)&sockaddr, sizeof(struct sockaddr_in6));
	return (re == 0) ? ret_ok : ret_error;
#endif
}


static ret_t
print_banner (cherokee_server_t *srv)
{
	char *p;
	char *method;
	CHEROKEE_NEW(n,buffer);

	/* First line
	 */
	cherokee_buffer_add_va (n, "Cherokee Web Server %s: ", PACKAGE_VERSION);
	cherokee_buffer_add_va (n, "Listening on port %d", srv->port);
		
	if (srv->chrooted) {
		cherokee_buffer_add (n, ", chrooted", 10);
	}

	/* TLS / SSL
	 */
	if (srv->tls_enabled) {
#if defined(HAVE_GNUTLS)
		cherokee_buffer_add (n, ", with TLS support via GNUTLS", 30);
#elif defined(HAVE_OPENSSL)
		cherokee_buffer_add (n, ", with TLS support via OpenSSL", 31);
#endif
	} else {
		cherokee_buffer_add (n, ", TLS disabled", 14);		
	}

	/* IPv6
	 */
#ifdef HAVE_IPV6
	if (srv->ipv6) {
		cherokee_buffer_add (n, ", IPv6 enabled", 14);		
	} else
#endif
		cherokee_buffer_add (n, ", IPv6 disabled", 14);

	/* Polling method
	 */
	cherokee_fdpoll_get_method_str (srv->main_thread->fdpoll, &method);
	cherokee_buffer_add_va (n, ", using %s", method);

	/* File descriptor limit
	 */
	cherokee_buffer_add_va (n, ", %d fds limit", srv->system_fd_limit);

	/* Threading stuff
	 */
	if (srv->thread_num <= 1) {
		cherokee_buffer_add (n, ", single thread", 15);
	} else {
		cherokee_buffer_add_va (n, ", %d threads", srv->thread_num);
		cherokee_buffer_add_va (n, ", %d fds in each", srv->system_fd_limit / (srv->thread_num));	

		switch (srv->thread_policy) {
		case SCHED_FIFO:
			cherokee_buffer_add (n, ", FIFO scheduling policy", 24);
			break;
		case SCHED_RR:
			cherokee_buffer_add (n, ", RR scheduling policy", 22);
			break;
		default:
			cherokee_buffer_add (n, ", standard scheduling policy", 28);
			break;
		}
	}

	/* Print it!
	 */
	for (p = n->buf+TERMINAL_WIDTH; p < n->buf+n->len; p+=80) {
		while (*p != ',') p--;
		*p = '\n';
	}

	printf ("%s\n", n->buf);
	cherokee_buffer_free (n);
	
	return ret_ok;
}


static ret_t
initialize_server_socket (cherokee_server_t *srv, unsigned short port, int *srv_socket_ptr)
{
	int   flags;
	int   srv_socket;
	ret_t ret = ret_error;

#ifdef HAVE_IPV6
	if (srv->ipv6) {
		ret = initialize_server_socket6 (srv, port, srv_socket_ptr);
	}
#endif

	if ((srv->ipv6 == 0) || (ret != ret_ok)) {
		ret = initialize_server_socket4 (srv, port, srv_socket_ptr);
	}

	if (ret != 0) {
		uid_t uid = getuid();
		gid_t gid = getgid();

		PRINT_ERROR ("Can't bind() socket (port=%d, UID=%d, GID=%d)\n", port, uid, gid);
		return ret_error;
	}
	   
	srv_socket = *srv_socket_ptr;

	/* Set no-delay mode
	 */
	flags = fcntl (srv_socket, F_GETFL, 0);
	return_if_fail (flags != -1, ret_error);
	
	ret = fcntl (srv_socket, F_SETFL, flags | O_NDELAY);
	return_if_fail (ret >= 0, ret_error);
	
	/* Listen
	 */
	ret = listen (srv_socket, srv->listen_queue);
	if (ret < 0) {
		close (srv_socket);
		return ret_error;
	}

	return ret_ok;
}


static ret_t
initialize_server_threads (cherokee_server_t *srv)
{	
	ret_t ret;
	int   i, fds_per_thread;

	/* Get the system fd number limit
	 */
	fds_per_thread = srv->system_fd_limit / srv->thread_num;

	/* Create the main thread
	 */
	ret = cherokee_thread_new (&srv->main_thread, srv, thread_sync, 
				   srv->system_fd_limit, fds_per_thread);
	if (unlikely(ret < ret_ok)) return ret;

	/* If Cherokee is compiled in single thread mode, it has to
	 * add the server socket to the fdpoll of the sync thread
	 */
#ifndef HAVE_PTHREAD
	ret = cherokee_fdpoll_add (srv->main_thread->fdpoll, srv->socket, 0);
	if (unlikely(ret < ret_ok)) return ret;

	if (srv->tls_enabled) {
		ret = cherokee_fdpoll_add (srv->main_thread->fdpoll, srv->socket_tls, 0);
		if (unlikely(ret < ret_ok)) return ret;
	}
#endif

	/* If Cherokee is compiles in multi-thread mode, it has to
	 * launch the threads
	 */
#ifdef HAVE_PTHREAD
	for (i=0; i<srv->thread_num - 1; i++) {
		cherokee_thread_t *thread;

		ret = cherokee_thread_new (&thread, srv, thread_async, 
					   srv->system_fd_limit, fds_per_thread);
		if (unlikely(ret < ret_ok)) return ret;
		
		thread->thread_pref = (i % 2)? thread_normal_tls : thread_tls_normal;

		list_add ((list_t *)thread, &srv->thread_list);
	}
#endif

	return ret_ok;
}


static void
for_each_vserver_init_tls_func (const char *key, void *value)
{
	ret_t ret;
	cherokee_virtual_server_t *vserver = VSERVER(value);

	ret = cherokee_virtual_server_init_tls (vserver);
	if (ret < ret_ok) {
		PRINT_ERROR ("Can not initialize TLS for `%s' virtual host\n", 
			     cherokee_buffer_is_empty(vserver->name) ? "unknown" : vserver->name->buf);
	}
}


static int  
while_vserver_check_tls_func (const char *key, void *value, void *param)
{
	cherokee_boolean_t found;
	
	found = (cherokee_virtual_server_have_tls (VSERVER(value)) == ret_ok);

	return (found) ? 0 : 1;
}


static ret_t
init_vservers_tls (cherokee_server_t *srv)
{
	ret_t ret;

#ifdef HAVE_TLS
	/* Initialize the server TLS socket
	 */
	if (srv->socket_tls == -1) {
		ret = initialize_server_socket (srv, srv->port_tls, &srv->socket_tls);
		if (unlikely(ret != ret_ok)) return ret;
	}

	/* Initialize TLS in all the virtual servers
	 */
	ret = cherokee_virtual_server_init_tls (srv->vserver_default);
	if (ret < ret_ok) {
		PRINT_ERROR_S ("Can not init TLS for the default virtual server\n");
		return ret_error;
	}

	cherokee_table_foreach (srv->vservers_ref, for_each_vserver_init_tls_func);
#endif

	return ret_ok;	
}


static ret_t
set_fdmax_limit (cherokee_server_t *srv)
{
	ret_t ret;

	/* Try to raise the fd number system limit
	 */
	if (srv->max_fds != -1) {
		/* Set it	
		 */
		ret = cherokee_sys_fdlimit_set (srv->max_fds);
		if (ret < ret_ok) {
			PRINT_ERROR ("WARNING: Unable to set file descriptor limit to %d\n",
				     srv->max_fds);
		}
	}

	/* Get system fd limimt.. has it been increased?
	 */
	ret = cherokee_sys_fdlimit_get (&srv->system_fd_limit);
	if (ret < ret_ok) {
		PRINT_ERROR_S ("ERROR: Unable to get file descriptor limit\n");
		return ret;
	}

	return ret_ok;
}


static void
build_server_string (cherokee_server_t *srv)
{
	cherokee_buffer_clean (srv->server_string);

	/* Cherokee
	 */
	cherokee_buffer_add (srv->server_string, "Cherokee", 8); 
	if (srv->server_token <= cherokee_version_product) 
		return;

	/* Cherokee/x.y
	 */
	cherokee_buffer_add (srv->server_string, 
			     "/"PACKAGE_MAJOR_VERSION"."PACKAGE_MINOR_VERSION,
			     2 + PACKAGE_MAJOR_VERSION_LEN + PACKAGE_MINOR_VERSION_LEN);
	if (srv->server_token <= cherokee_version_minor) 
		return;

	/* Cherokee/x.y.z-betaXX
	 */
	cherokee_buffer_add (srv->server_string, 
			     "."PACKAGE_MICRO_VERSION PACKAGE_PATCH_VERSION,
			     1 + PACKAGE_MICRO_VERSION_LEN + PACKAGE_PATCH_VERSION_LEN);
	if (srv->server_token <= cherokee_version_minimal) 
		return;

	/* Cherokee/x.y.z-betaXX (UNIX)
	 */
	cherokee_buffer_add_va (srv->server_string, " (%s)", OS_TYPE);
	if (srv->server_token <= cherokee_version_os) return;

	/* Cherokee/x.y.z-betaXX (UNIX) Ext1/x.y Ext2/x.y
	 * TODO
	 */
}


ret_t
cherokee_server_init (cherokee_server_t *srv) 
{   
	ret_t ret;

	/* Build the server string
	 */
	build_server_string (srv);

	/* Set the FD number limit
	 */
	ret = set_fdmax_limit (srv);
	if (unlikely(ret < ret_ok)) return ret;

	/* If the server has a previous server socket opened, Eg:
	 * because a SIGHUP, it shouldn't init the server socket.
	 */
	if (srv->socket == -1) {
		ret = initialize_server_socket (srv, srv->port, &srv->socket);
		if (unlikely(ret != ret_ok)) return ret;
	}

	/* Look if TLS is enabled
	 */
	srv->tls_enabled = (cherokee_virtual_server_have_tls (srv->vserver_default) == ret_ok);

	if (srv->tls_enabled == false) {
		ret = cherokee_table_while (srv->vservers_ref, while_vserver_check_tls_func, NULL, NULL, NULL);
		srv->tls_enabled = (ret == ret_ok);
	}

	if (srv->tls_enabled) {
		ret = init_vservers_tls (srv);
		if (unlikely(ret != ret_ok)) return ret;
	}

        /* Get the CPU number
	 */
#ifndef CHEROKEE_EMBEDDED
	dcc_ncpus (&srv->ncpus);
#else
	srv->ncpus = 1;
#endif
	if (srv->ncpus == -1) {
		PRINT_ERROR_S ("Can not deternime the number of processors\n");
		srv->ncpus = 1;
	}

	/*  Maybe recalculate the thread number
	 */
	if (srv->thread_num == -1) {
#ifdef HAVE_PTHREAD
		srv->thread_num = srv->ncpus * 5;
#else
		srv->thread_num = 1;
#endif
	}
		
	/* Check the number of reusable connections
	 */
	if (srv->max_conn_reuse == -1) {
		srv->max_conn_reuse = DEFAULT_CONN_REUSE;
	}

	/* Chroot
	 */
	if (srv->chroot) {
		srv->chrooted = (chroot (srv->chroot) == 0);
		if (srv->chrooted == 0) {
			PRINT_ERROR ("Cannot chroot() to '%s': %s\n", srv->chroot, strerror(errno));
		}
	} 

	/* Change the user
	 */
	ret = change_execution_user (srv);
	if (ret != ret_ok) {
		return ret;
	}

	/* Change current directory
	 */
	chdir ("/");

	/* Create the threads
	 */
	ret = initialize_server_threads (srv);
	if (unlikely(ret < ret_ok)) return ret;

	/* Print the server banner
	 */
	print_banner (srv);

	return ret_ok;
}


static void
flush_vserver (const char *key, void *value)
{
	/* There's no logger in this virtual server
	 */
	if ((value == NULL) || (VSERVER_LOGGER(value) == NULL))
		return;

	cherokee_logger_flush (VSERVER_LOGGER(value));
}


void static 
flush_logs (cherokee_server_t *srv)
{
	flush_vserver (NULL, srv->vserver_default);
	cherokee_table_foreach (srv->vservers_ref, flush_vserver);
}

ret_t 
cherokee_server_reinit (cherokee_server_t *srv)
{
	ret_t  ret;
	
	if (srv->chrooted) {
		fprintf (stderr, 
			 "WARNING: Chrooted cherokee cannot be reloaded. "
			 "Please, stop and restart it again.\n");
		return ret_ok;
	}


#if 1
	{
		int tmp;
		printf ("Handling server reinit. Press any key to continue..\n");
		read (0, &tmp, 1);
	}
#endif

	ret = cherokee_server_clean (srv);
	if (ret != ret_ok) {
		exit(EXIT_SERVER_CLEAN);
	}

	ret = read_default_config_files (srv, NULL);
	if (ret != ret_ok) {
		exit(EXIT_SERVER_READ_CONFIG);
	}

	ret = cherokee_server_init (srv);
	if (ret != ret_ok) {
		exit(EXIT_SERVER_INIT);	
	}

	return ret_ok;
}


static void
update_bogo_now (cherokee_server_t *srv)
{
	static unsigned char to_wait = 1;
	int unlocked;

	unlocked = CHEROKEE_RWLOCK_TRYWRITER (&srv->bogo_now_mutex);     /* 1.- lock as writer */
	if (unlocked) return;

	srv->bogo_now = time (NULL);
	cherokee_gmtime (&srv->bogo_now, &srv->bogo_now_tm);
	
	if (--to_wait == 0) {
		srv->bogo_now_string->len = strftime (srv->bogo_now_string->buf, srv->bogo_now_string->size,
						      "%a, %d %b %Y %H:%M:%S GMT", &srv->bogo_now_tm);
		to_wait = 1000 / srv->fdwatch_msecs;
	}

	CHEROKEE_RWLOCK_UNLOCK (&srv->bogo_now_mutex);                   /* 2.- release */
}


ret_t
cherokee_server_unlock_threads (cherokee_server_t *srv)
{
	ret_t   ret;
	list_t *i;

	/* Update bogonow before launch the threads
	 */
	update_bogo_now (srv);

	/* Launch all the threads
	 */
	list_for_each (i, &srv->thread_list) {
		ret = cherokee_thread_unlock (THREAD(i));
		if (unlikely(ret < ret_ok)) return ret;
	}

	return ret_ok;
}


void
cherokee_server_step (cherokee_server_t *srv)
{
	ret_t ret;

	/* Get the time
	 */
	update_bogo_now (srv);
	ret = cherokee_thread_step (srv->main_thread, true);

	/* Logger flush 
	 */
	if (srv->log_flush_next < srv->bogo_now) {
		flush_logs (srv);
		srv->log_flush_next = srv->bogo_now + srv->log_flush_elapse;
	}

#ifndef CHEROKEE_EMBEDDED
	/* Clean mmap
	 */
	if (srv->mmap_cache_clean_next < srv->bogo_now) {
		cherokee_mmap2_clean_up (srv->mmap_cache);	
		srv->mmap_cache_clean_next = srv->bogo_now + MMAP_DEFAULT_CLEAN_ELAPSE;
	}
#endif

	/* Wanna exit?
	 */
	if (srv->wanna_exit) {
		cherokee_server_reinit (srv);
		srv->wanna_exit = 0;
		printf ("Cherokee reloaded\n");
	}
}


static ret_t
read_config_file (cherokee_server_t *srv, char *filename)
{
	int   error;
	void *bufstate;
	extern FILE *yyin;
	extern char *current_yacc_file;

	extern int   yyparse             (void *);
	extern void  yyrestart           (FILE *);
	extern void *yy_create_buffer    (FILE *file, int size);
	extern void  yy_switch_to_buffer (void *);
	extern void  yy_delete_buffer    (void *);

	/* Maybe set a default configuration filename
	 */
	if (filename == NULL) {
		filename = (srv->config_file)? srv->config_file : CHEROKEE_CONFDIR"/cherokee.conf";

	} else {
		/* Maybe free an old filename
		 */
		if (srv->config_file != NULL) {
			free (srv->config_file);
		}
		
		/* Store last configuration filename
		 */
		srv->config_file = strdup(filename);
	}

	/* Set the file to read
	 */
	current_yacc_file = filename;

	yyin = fopen (filename, "r");
	if (yyin == NULL) {
		PRINT_ERROR("Can't read the configuration file: '%s'\n", filename);
		return ret_error;
	}

	/* Cooooome on :-)
	 */
	yyrestart(yyin);

	bufstate = (void *) yy_create_buffer (yyin, 65535);
	yy_switch_to_buffer (bufstate);
	error = yyparse ((void *)srv);
	yy_delete_buffer (bufstate);

	fclose (yyin);

	return (error == 0) ? ret_ok : ret_error;
}


static ret_t
read_config_path (cherokee_server_t *srv, char *path)
{
	int         re;
	ret_t       ret;
	struct stat info;
	
	re = stat (path, &info);
	if (re < 0) return ret_not_found;
	
	if (S_ISREG(info.st_mode)) {
		ret = read_config_file (srv, path);
		if (unlikely(ret < ret_ok)) return ret;
		
	} else if (S_ISDIR(info.st_mode)) {
		DIR *dir;
		int entry_len;
		struct dirent *entry;
		
		dir = opendir(path);
		if (dir == NULL) return ret_error;
		
		while ((entry = readdir(dir)) != NULL) {
			char *full_new;
			int   full_new_len;

			/* Ignore backup files
			 */
			entry_len = strlen(entry->d_name);

			if ((entry->d_name[0] == '.') ||
			    (entry->d_name[0] == '#') ||
			    (entry->d_name[entry_len-1] == '~')) 
			{
				continue;
			}
			
			full_new_len = strlen(path) + strlen(entry->d_name) + 2;
			full_new = (char *)malloc(full_new_len);
			if (full_new == NULL) return ret_error;

			snprintf (full_new, full_new_len, "%s/%s", path, entry->d_name);
			cherokee_list_add_tail (&srv->include_list, full_new);
		}
		
		closedir (dir);
	} else {
		SHOULDNT_HAPPEN;
		return ret_error;
	}
	
	return ret_ok;
}


static ret_t
read_default_config_files (cherokee_server_t *srv, char *filename)
{
	ret_t   ret;
	list_t *i;
	
	/* Support null filename as default config file
	 */
	if (filename == NULL) {
		filename = CHEROKEE_CONFDIR"/cherokee.conf";
	}

	/* Read the main config file
	 */
	ret = read_config_path (srv, filename);
	if (unlikely(ret < ret_ok)) return ret;

	/* Process the includes form the config file
	 */
	list_for_each (i, &srv->include_list) {
		char *path = LIST_ITEM_INFO(i);

		ret = read_config_path (srv, path);
		if (unlikely(ret < ret_ok)) return ret;
	}


#ifndef CHEROKEE_EMBEDDED
	/* Maybe read the Icons file
	 */
	if (srv->icons_file != NULL) {
		ret = cherokee_icons_read_config_file (srv->icons, srv->icons_file);
		if (ret < ret_ok) {
			PRINT_ERROR_S ("Cannot read the icons file\n");
		}
	}

	/* Maybe read the MIME file
	 */
	if (srv->mime_file != NULL) {
		do {
			cherokee_mime_t *mime = NULL;

			ret = cherokee_mime_get_default (&mime);

			if (ret == ret_not_found) {
				ret = cherokee_mime_init (&mime);
			}

			if (ret < ret_ok) {
				PRINT_ERROR_S ("Can not get default MIME configuration file\n");
				break;
			}
			
			ret = cherokee_mime_load (mime, srv->mime_file);
			if (ret < ret_ok) {
				PRINT_ERROR ("Can not load MIME configuration file %s\n", srv->mime_file);
			}
		} while (0);
	}
#endif

	return ret;
}


ret_t 
cherokee_server_read_config_file (cherokee_server_t *srv, char *path)
{
	ret_t ret;

	ret = read_default_config_files (srv, path);
	if (ret == ret_not_found) {
		PRINT_ERROR ("Configuration file not found: %s\n", path);
	}

	return ret;
}


ret_t 
cherokee_server_read_config_string (cherokee_server_t *srv, char *config_string)
{
	ret_t ret;
	int   error;
	void *bufstate;

	extern int  yyparse             (void *);
	extern int  yy_scan_string      (const char *);
	extern void yy_switch_to_buffer (void *);
	extern void yy_delete_buffer    (void *);

	bufstate = (void *) yy_scan_string (config_string);
	yy_switch_to_buffer(bufstate);

	error = yyparse((void *)srv);

	yy_delete_buffer (bufstate);

	ret = (error == 0) ? ret_ok : ret_error;
	if (ret < ret_ok) {
		return ret;
	}
	
#ifndef CHEROKEE_EMBEDDED
	/* Maybe read the Icons file
	 */
	if (srv->icons_file != NULL) {
		ret = cherokee_icons_read_config_file (srv->icons, srv->icons_file);
		if (ret < ret_ok) {
			PRINT_ERROR_S ("Cannot read the icons file\n");
		}
	}

	/* Maybe read the MIME file
	 */
	if (srv->mime_file != NULL) {
		do {
			cherokee_mime_t *mime = NULL;

			ret = cherokee_mime_get_default (&mime);

			if (ret == ret_not_found) {
				ret = cherokee_mime_init (&mime);
			}

			if (ret < ret_ok) {
				PRINT_ERROR_S ("Can not get default MIME configuration file\n");
				break;
			}
			
			ret = cherokee_mime_load (mime, srv->mime_file);
			if (ret < ret_ok) {
				PRINT_ERROR ("Can not load MIME configuration file %s\n", srv->mime_file);
			}
		} while (0);
	}
#endif
	
	return ret;
}


ret_t 
cherokee_server_daemonize (cherokee_server_t *srv)
{
        pid_t child_pid;

        switch (child_pid = fork()) {
	case -1:
                PRINT_ERROR_S ("Could not fork\n");
		break;

	case 0:
		setsid();
		close(2);
		close(1);
		close(0);
		break;

	default:
		exit(0);
        }

	return ret_ok;
}


ret_t 
cherokee_server_get_active_conns (cherokee_server_t *srv, int *num)
{
	int     active = 0;
	list_t *thread;

	/* Active connections number
	 */
	list_for_each (thread, &srv->thread_list) {
		active += THREAD(thread)->active_list_num;
	}
	
	active += srv->main_thread->active_list_num;

	/* Return out parameters
	 */
	*num = active;

	return ret_ok;
}


ret_t 
cherokee_server_get_reusable_conns (cherokee_server_t *srv, int *num)
{
	int     reusable = 0;
	list_t *thread, *i;

	/* Reusable connections
	 */
	list_for_each (thread, &srv->thread_list) {
		list_for_each (i, &THREAD(thread)->reuse_list) {
			reusable++;
		}
	}
	list_for_each (i, &THREAD(srv->main_thread)->reuse_list) {
		reusable++;
	}

	/* Return out parameters
	 */
	*num = reusable;
	return ret_ok;
}


ret_t 
cherokee_server_get_total_traffic (cherokee_server_t *srv, size_t *rx, size_t *tx)
{
	list_t *i;

	*rx = srv->vserver_default->data.rx;
	*tx = srv->vserver_default->data.tx;

	list_for_each (i, &srv->vservers) {
		*rx += VSERVER(i)->data.rx;
		*tx += VSERVER(i)->data.tx;
	}

	return ret_ok;
}


ret_t 
cherokee_server_handle_HUP (cherokee_server_t *srv)
{
	srv->wanna_exit = 1;
	return ret_ok;
}


ret_t
cherokee_server_handle_panic (cherokee_server_t *srv)
{
	int                re;
	cherokee_buffer_t *cmd;

	PRINT_ERROR_S ("Cherokee feels panic!\n");
	
	if ((srv == NULL) || (srv->panic_action == NULL)) {
		goto fin;
	}

	cherokee_buffer_new (&cmd);
	cherokee_buffer_add_va (cmd, "%s %d", srv->panic_action, getpid());

	re = system (cmd->buf);
	if (re < 0) {
		PRINT_ERROR ("PANIC: re-panic: '%s', status %d\n", cmd->buf, WEXITSTATUS(re));
	}

	cherokee_buffer_free (cmd);

fin:
	abort();
}
