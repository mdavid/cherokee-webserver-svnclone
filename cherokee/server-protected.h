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

#ifndef CHEROKEE_SERVER_PROTECTED_H
#define CHEROKEE_SERVER_PROTECTED_H

#include "common-internal.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else 
# include <time.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#include "server.h"
#include "list.h"
#include "fdpoll.h"
#include "handler_table.h"
#include "virtual_server.h"
#include "encoder_table.h"
#include "logger_table.h"
#include "thread.h"
#include "module_loader.h"
#include "icons.h"
#include "mmap2.h"

struct cherokee_server {
	/* Current time
	 */
	time_t             start_time;
	time_t             bogo_now;
	struct tm          bogo_now_tm;
	cherokee_buffer_t *bogo_now_string;
#ifdef HAVE_PTHREAD
	pthread_rwlock_t   bogo_now_mutex;
#endif

	/* Exit related
	 */
	char *panic_action;
	int   wanna_exit;
	
	/* Virtual servers
	 */
	struct list_head           vservers;
	cherokee_table_t          *vservers_ref;
	cherokee_virtual_server_t *vserver_default;
	
	/* Threads
	 */
	cherokee_thread_t *main_thread;
	int                thread_num;
	struct list_head   thread_list;
	int                thread_policy;

	/* Modules
	 */
	cherokee_logger_table_t  *loggers;
	cherokee_module_loader_t *loader;
	cherokee_encoder_table_t *encoders;

	/* Tables: icons, mmap
	 */
	cherokee_icons_t *icons;
	cherokee_mmap2_t *mmap_cache;
	time_t            mmap_cache_clean_next;

	/* Default index files
	 */
	list_t index_list;

	/* Logging
	 */
	int    log_flush_elapse;
	time_t log_flush_next;

	/* Main socket
	 */
	int socket;
	int socket_tls;

#ifdef HAVE_PTHREAD
	pthread_mutex_t accept_mutex;
# ifdef HAVE_TLS
	pthread_mutex_t accept_tls_mutex;	
# endif
#endif

	/* System related
	 */
 	int      ncpus;
	int      max_fds;
	uint32_t system_fd_limit;

	/* Networking config
	 */
	int   ipv6;
	char *listen_to;	
	int   fdwatch_msecs;
	int   listen_queue;

	unsigned short port;
	unsigned short port_tls;
	cherokee_boolean_t tls_enabled;

	cherokee_server_token_t  server_token;
	cherokee_buffer_t       *server_string;

	uid_t  user;
	uid_t  user_orig;
	gid_t  group;
	gid_t  group_orig;

	char  *chroot;
	int    chrooted;

	int                timeout;
	cherokee_buffer_t *timeout_header;

	cherokee_boolean_t keepalive;
	uint32_t           keepalive_max;

	struct {
		off_t min;
		off_t max;
	} sendfile;

	/* Another config files
	 */
	char             *config_file;
	char             *mime_file;
	char             *icons_file;
	struct list_head  include_list;

	/* Performance
	 */
	int max_conn_reuse;
};

#endif /* CHEROKEE_SERVER_PROTECTED_H */
