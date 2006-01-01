/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
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

#ifndef CHEROKEE_HANDLER_CGI_H
#define CHEROKEE_HANDLER_CGI_H

#include "common-internal.h"

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <fcntl.h>

#include "buffer.h"
#include "handler.h"
#include "cgi.h"
#include "list.h"

#define ENV_VAR_NUM 30


typedef enum {
	hcgi_phase_init,
	hcgi_phase_sent_post
} cgi_init_phase_t;


typedef struct {
	cherokee_handler_t handler;

	int     pipeInput;         /* read from the CGI */
	int     pipeOutput;        /* write to the CGI */
	int     post_data_sent;    /* amount POSTed to the CGI */
	pid_t   pid;               /* CGI pid */
	char   *script_alias;
	char   *extra_param;
	list_t *system_env;
	size_t  content_length;
	cuint_t is_error_handler;
	cuint_t change_user;

	char *envp[ENV_VAR_NUM]; /* Environ variables for execve() */
	int   envp_last;

	cgi_init_phase_t   init_phase;
	cherokee_boolean_t cgi_fd_in_poll;

	cherokee_buffer_t *filename;
	cherokee_buffer_t *parameter; 
	cherokee_buffer_t *data; 

} cherokee_handler_cgi_t;

#define CGIHANDLER(x)  ((cherokee_handler_cgi_t *)(x))

/* Library init function
 */
void cgi_init ();


ret_t cherokee_handler_cgi_new   (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties);

/* virtual methods implementation
 */
ret_t cherokee_handler_cgi_init        (cherokee_handler_cgi_t *hdl);
ret_t cherokee_handler_cgi_free        (cherokee_handler_cgi_t *hdl);
ret_t cherokee_handler_cgi_step        (cherokee_handler_cgi_t *hdl, cherokee_buffer_t *buffer);
ret_t cherokee_handler_cgi_add_headers (cherokee_handler_cgi_t *hdl, cherokee_buffer_t *buffer);


/* This handler export these extra functions to allow phpcgi
 * set enviroment variables, work with pathinfo, etc..
 */
ret_t cherokee_handler_cgi_split_pathinfo (cherokee_handler_cgi_t *cgi, 
					   cherokee_buffer_t      *buf, 
					   int                     pos);

void  cherokee_handler_cgi_add_parameter  (cherokee_handler_cgi_t *cgi, char *name);

void  cherokee_handler_cgi_add_env_pair   (cherokee_handler_cgi_t *cgi, 
				          char *name,    int name_len,
					  char *content, int content_len);

#endif /* CHEROKEE_HANDLER_CGI_H */
