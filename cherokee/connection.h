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

#ifndef CHEROKEE_CONNECTION_H
#define CHEROKEE_CONNECTION_H

#include "common-internal.h"

#ifdef HAVE_SYS_TIME_H
# include <sys/time.h>
#else 
# include <time.h>
#endif

#ifdef HAVE_SYS_SOCKET_H
# include <sys/socket.h>
#endif

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif

#ifdef HAVE_ARPA_INET_H
# include <arpa/inet.h>
#endif

#ifdef HAVE_NETINET_IN_H
# include <netinet/in.h>
#endif

#include "list.h"
#include "http.h"
#include "buffer.h"
#include "handler.h"
#include "handler_table.h"
#include "virtual_server.h"
#include "encoder_table.h"
#include "handler_table_entry.h"
#include "socket.h"
#include "header.h"
#include "buffer_escape.h"
#include "mmap2.h"


typedef enum {
	phase_nothing,
	phase_switching_headers,
	phase_tls_handshake,
	phase_reading_header,
	phase_processing_header,
	phase_read_post,
	phase_setup_connection,
	phase_init,
	phase_add_headers,
	phase_send_headers,
	phase_steping,
	phase_lingering
} cherokee_connection_phase_t;

typedef struct {
	struct list_head              list_entry;

	/* References
	 */
	void                         *server;
	void                         *vserver;
	void                         *thread;

	/* Socket stuff
	 */
	cherokee_socket_t            *socket;
	int                           tcp_cork;
	int                           extra_polling_fd;
		
	cherokee_buffer_t            *query_string;	
	cherokee_table_t             *arguments;

	cherokee_logger_t            *logger_ref;
	int                           log_at_end;
	
	cherokee_handler_t           *handler;
	cherokee_http_upgrade_t       upgrade;
	
	/* Buffers
	 */
	cherokee_buffer_t            *incoming_header;  /* -> header               */
	cherokee_buffer_t            *header_buffer;    /* <- header, -> post data */
	cherokee_buffer_t            *buffer;           /* <- data                 */

	/* State
	 */
	cherokee_connection_phase_t   phase;
	cherokee_connection_phase_t   phase_return;
	cherokee_http_t               error_code;
	
	/* Headers
	 */
	cherokee_header_t            *header;

	/* Encoders
	 */
	cherokee_encoder_t           *encoder;
	cherokee_buffer_t            *encoder_buffer;

	/* Eg:
	 * http://www.alobbs.com/cherokee/dir/file
	 */
	cherokee_buffer_t            *local_directory;  /* /var/www/  or  /home/alo/public_html/ */
	cherokee_buffer_t            *web_directory;    /* /cherokee/ */
	cherokee_buffer_t            *request;          /* /dir/file */
	cherokee_buffer_t            *host;
	cherokee_buffer_t            *userdir;          /* 'alo' in http://www.alobbs.com/~alo/thing */

	/* Espace versions
	 */
	cherokee_buffer_escape_t     *request_escape;   /* Buffer escape for the request */

	/* Authentication
	 */
	cherokee_buffer_t            *user;             /* alo    */
	cherokee_buffer_t            *passwd;           /* secret */
	cherokee_buffer_t            *realm_ref;        /* "My private data" */
	cherokee_http_auth_t          auth_type;        /* Digest, Basic */

	/* Traffic
	 */
	size_t                        rx;               /* Bytes received */
	size_t                        tx;               /* Bytes sent */
	time_t                        traffic_next;     /* Time to update traffic */

	/* Post info
	 */
	cherokee_buffer_t            *post;
	size_t                        post_len;

	cherokee_buffer_t            *redirect;
	
	time_t                        timeout;
	uint32_t                      keepalive;

	off_t                         range_start;
	off_t                         range_end;

	void                         *mmaped;
	off_t                         mmaped_len;
	cherokee_mmap2_entry_t       *mmap_entry_ref;
} cherokee_connection_t;


#define CONN(c)        ((cherokee_connection_t *)(c))
#define CONN_SRV(c)    (SRV(CONN(c)->server))
#define CONN_HDR(c)    (HDR(CONN(c)->header))
#define CONN_SOCK(c)   (SOCKET(CONN(c)->socket))
#define CONN_VSRV(c)   (VSERVER(CONN(c)->vserver))
#define CONN_THREAD(c) (THREAD(CONN(c)->thread))

ret_t cherokee_connection_new        (cherokee_connection_t **cnt);
ret_t cherokee_connection_free       (cherokee_connection_t  *cnt);
ret_t cherokee_connection_clean      (cherokee_connection_t  *cnt);
ret_t cherokee_connection_mrproper   (cherokee_connection_t  *cnt);

/*	Socket
 */
ret_t cherokee_connection_set_cork   (cherokee_connection_t *cnt, int enable);

/* 	Send & Recv
 */
ret_t cherokee_connection_recv  (cherokee_connection_t *cnt, cherokee_buffer_t *buffer, off_t *len);
ret_t cherokee_connection_send  (cherokee_connection_t *cnt);

/*	Close
 */
ret_t cherokee_connection_close               (cherokee_connection_t *cnt);
ret_t cherokee_connection_pre_lingering_close (cherokee_connection_t *cnt);

/* 	Sending
 */
ret_t cherokee_connection_send_header            (cherokee_connection_t *cnt);
ret_t cherokee_connection_send_header_and_mmaped (cherokee_connection_t *cnt);
ret_t cherokee_connection_send_switching         (cherokee_connection_t *cnt);

/*	Internal
 */
ret_t cherokee_connection_get_plugin_entry     (cherokee_connection_t *cnt, cherokee_handler_table_t *plugins, cherokee_handler_table_entry_t **plugin_entry);
ret_t cherokee_connection_create_handler       (cherokee_connection_t *cnt, cherokee_handler_table_entry_t *plugin_entry);
ret_t cherokee_connection_setup_error_handler  (cherokee_connection_t *cnt);
ret_t cherokee_connection_check_authentication (cherokee_connection_t *cnt, cherokee_handler_table_entry_t *plugin_entry);
ret_t cherokee_connection_check_ip_validation  (cherokee_connection_t *cnt, cherokee_handler_table_entry_t *plugin_entry);
ret_t cherokee_connection_check_only_secure    (cherokee_connection_t *cnt, cherokee_handler_table_entry_t *plugin_entry);

/*	Iteration
 */
ret_t cherokee_connection_open_request  (cherokee_connection_t *cnt);
ret_t cherokee_connection_reading_check (cherokee_connection_t *cnt);
ret_t cherokee_connection_step          (cherokee_connection_t *cnt);


/* 	Headers
 */
ret_t cherokee_connection_get_request  (cherokee_connection_t *cnt);
ret_t cherokee_connection_build_header (cherokee_connection_t *cnt);
ret_t cherokee_connection_parse_args   (cherokee_connection_t *cnt);
ret_t cherokee_connection_parse_header (cherokee_connection_t *cnt, cherokee_encoder_table_t *encoders);

/* 	Log
 */
ret_t cherokee_connection_log_or_delay         (cherokee_connection_t *cnt);
ret_t cherokee_connection_log_delayed          (cherokee_connection_t *cnt);
ret_t cherokee_connection_update_vhost_traffic (cherokee_connection_t *cnt);


int   cherokee_connection_is_userdir                    (cherokee_connection_t *cnt);
ret_t cherokee_connection_build_local_directory         (cherokee_connection_t *cnt, cherokee_virtual_server_t *vsrv, cherokee_handler_table_entry_t *entry);
ret_t cherokee_connection_build_local_directory_userdir (cherokee_connection_t *cnt, cherokee_virtual_server_t *vsrv, cherokee_handler_table_entry_t *entry);

#endif /* __CHEROKEE_CONNECTION_H__  */
