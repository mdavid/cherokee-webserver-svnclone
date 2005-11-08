/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Ayose Cazorla León <setepo@gulic.org>
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
#include "handler_cgi.h"
#include "util.h"

#ifdef HAVE_STDARG_H
# include <stdarg.h>
#endif

#include <stdio.h>
#include <stdlib.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif

#include "module.h"
#include "connection.h"
#include "connection-protected.h"
#include "socket.h"
#include "server.h"
#include "server-protected.h"
#include "header.h"
#include "header-protected.h"
#include "list_ext.h"
#include "post.h"


cherokee_module_info_t MODULE_INFO(cgi)= {
	cherokee_handler,           /* type     */
	cherokee_handler_cgi_new    /* new func */
};


#define dbg PRINT_DEBUG

#define set_env_pair(c,n,l,v,l2) cherokee_handler_cgi_add_env_pair(c,n,l,v,l2) 


ret_t
cherokee_handler_cgi_new  (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	int   i;
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, handler_cgi);
	
	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt);

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_cgi_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_cgi_free;
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_cgi_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_cgi_add_headers;

	/* Supported features
	 */
	HANDLER(n)->support     = hsupport_maybe_length;

	/* Process the request_string, and build the arguments table..
	 * We'll need it later
	 */
	ret = cherokee_connection_parse_args (cnt);
	if (unlikely(ret < ret_ok)) return ret;
		
	/* Init
	 */
	n->pipeInput         = -1;
	n->pipeOutput        = -1;
	n->post_data_sent    = 0;
	n->pid               = -1;
	n->filename          = NULL;
	n->data              = NULL;
	n->parameter         = NULL;
	n->cgi_fd_in_poll    = false;
	n->script_alias      = NULL;
	n->extra_param       = NULL;
	n->init_phase        = hcgi_phase_init;
	n->system_env        = NULL;
	n->content_length    = 0;
	n->is_error_handler  = 0;
	
	n->envp_last = 0;	
	for (i=0; i<ENV_VAR_NUM; i++)
		n->envp[i] = NULL;

	/* Get properties
	 */
	if (properties) {
		cherokee_typed_table_get_str (properties, "scriptalias", &n->script_alias);
		cherokee_typed_table_get_list (properties, "env", &n->system_env);
		cherokee_typed_table_get_int (properties, "error_handler", &n->is_error_handler);
	}

	if (n->is_error_handler) {
		HANDLER(n)->support |= hsupport_error;		
	}

	/* Return the object
	 */
	*hdl = HANDLER(n);
	return ret_ok;
}


static int
do_reap (void)
{
	pid_t pid;
	int   status;
	int   child_count = 0;

#ifndef _WIN32

	/* Reap defunct children until there aren't any more. 
	 */
	for (child_count = 0; ; ++child_count)
        {
		pid = waitpid (-1, &status, WNOHANG);

                /* none left */
		if (pid == 0) break;

		else if (pid < 0) {
			/* because of ptrace */
			if (errno == EINTR) continue;
			break;
		}
        }
#endif

	return child_count;
}


ret_t
cherokee_handler_cgi_free (cherokee_handler_cgi_t *cgi)
{
	int i;

	/* Close the connection with the CGI
	 */
	if (cgi->pipeInput > 0) {
		close (cgi->pipeInput);
		cgi->pipeInput = -1;
	}

	if (cgi->pipeOutput > 0) {
		close (cgi->pipeOutput);
		cgi->pipeOutput = -1;
	}

        /* Maybe kill the CGI
	 */
#ifndef _WIN32
	if (cgi->pid > 0) {
		pid_t pid;

		do {
			pid = waitpid (cgi->pid, NULL, WNOHANG);
		} while ((pid == 1) && (errno == EINTR));

		if (pid <= 0) {
			kill (cgi->pid, SIGTERM);
		}
	}
#endif

	/* Free the rest of the handler CGI memory
	 */
	if (cgi->data != NULL) {
		cherokee_buffer_free (cgi->data);
		cgi->data = NULL;
	}

	if (cgi->filename != NULL) {
		cherokee_buffer_free (cgi->filename);
		cgi->filename = NULL;
	}
	
	if (cgi->parameter != NULL) {
		cherokee_buffer_free (cgi->parameter);
		cgi->parameter = NULL;
	}

	for (i=0; i<cgi->envp_last; i++) {
		free (cgi->envp[i]);
		cgi->envp[i] = NULL;
	}

	/* for some reason, we have seen that the SIGCHLD signal does not call to
	 * our handler in a server with a lot of requests, so the wait() call,
	 * necessary to free the resources used by the CGI, is not called. So I
	 * think that a possible solution couble be to put the waitpid call in the
	 * _free method of this handler, so when the handler ends, this will free
	 * the resources used by our cool CGI.
	 */
	do_reap();

	return ret_ok;
}


void 
cherokee_handler_cgi_add_parameter (cherokee_handler_cgi_t *cgi, char *param)
{
	cgi->extra_param = param;
}


void
cherokee_handler_cgi_add_env_pair (cherokee_handler_cgi_t *cgi,
				   char *name,    int name_len,
				   char *content, int content_len)
{
	char *entry;

	/* Build the new envp entry
	 */
	entry = (char *) malloc (name_len + content_len + 2); 
	if (entry == NULL) return;

	memcpy (entry, name, name_len);
	entry[name_len] = '=';
	memcpy (entry + name_len + 1, content, content_len);

	entry[name_len+content_len+1] = '\0';

	/* Set it in the table
	 */
	cgi->envp[cgi->envp_last] = entry;
	cgi->envp_last++;

	/* Sanity check
	 */
	if (cgi->envp_last >= ENV_VAR_NUM) {
		SHOULDNT_HAPPEN;
	}
}


static ret_t
build_envp (cherokee_connection_t *conn, cherokee_handler_cgi_t* cgi)
{
	ret_t              ret;
	list_t            *i;
	char              *p;
	cherokee_buffer_t  tmp = CHEROKEE_BUF_INIT;

	/* Add user defined variables at the beginning,
	 * these have precedence..
	 */
	if (cgi->system_env != NULL) {
		list_for_each (i, cgi->system_env) {
			char    *name;
			cuint_t  name_len;
			char    *value;
			
			name     = LIST_ITEM_INFO(i);
			name_len = strlen(name);
			value    = name + name_len + 1;
			
			cherokee_handler_cgi_add_env_pair (cgi, name, name_len, value, strlen(value));
		}
	}

	/* Add the basic enviroment variables
	 */
	ret = cherokee_cgi_build_basic_env (conn, 
					    (cherokee_cgi_set_env_pair_t) cherokee_handler_cgi_add_env_pair, 
					    &tmp, cgi);
	if (unlikely (ret != ret_ok)) return ret;

	/* SCRIPT_NAME:
	 * It is the request without the pathinfo if it exists
	 */
	if (cgi->parameter == NULL) {
		cherokee_buffer_clean (&tmp);
		cherokee_header_copy_request (conn->header, &tmp);

		if (conn->pathinfo.len > 0) {
			set_env_pair (cgi, "SCRIPT_NAME", 11, tmp.buf, tmp.len - conn->pathinfo.len);
		} else {
			set_env_pair (cgi, "SCRIPT_NAME", 11, tmp.buf, tmp.len);
		}
	} else {
		p = cgi->parameter->buf + conn->local_directory.len -1;
		set_env_pair (cgi, "SCRIPT_NAME", 11, p, (cgi->parameter->buf + cgi->parameter->len) - p);
	}

	/* SCRIPT_FILENAME
	 */
	if (cgi->filename) {
		set_env_pair (cgi, "SCRIPT_FILENAME", 16, cgi->filename->buf, cgi->filename->len);
	}

	/* TODO: Fill the others CGI environment variables
	 * 
	 * http://hoohoo.ncsa.uiuc.edu/cgi/env.html
	 * http://cgi-spec.golux.com/cgi-120-00a.html
	 */
	cherokee_buffer_mrproper (&tmp);
	return ret_ok;
}


ret_t
cherokee_handler_cgi_split_pathinfo (cherokee_handler_cgi_t *cgi, cherokee_buffer_t *buf, int init_pos) 
{
	ret_t                  ret;
	char                  *pathinfo;
	int                    pathinfo_len;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	/* Look for the pathinfo
	 */
	ret = cherokee_split_pathinfo (buf, init_pos, &pathinfo, &pathinfo_len);
	if (ret == ret_not_found) {
		conn->error_code = http_not_found;
		return ret_error;
	}

	/* Build the PathInfo string 
	 */
	cherokee_buffer_add (&conn->pathinfo, pathinfo, pathinfo_len);
	
	/* Drop it out from the original string
	 */
	cherokee_buffer_drop_endding (buf, pathinfo_len);

	return ret_ok;
}


static ret_t
_extract_path (cherokee_handler_cgi_t *cgi)
{
	struct stat            st;
	ret_t                  ret  = ret_ok;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	/* ScriptAlias: If there is a ScriptAlias directive, it
	 * doesn't need to find the executable file..
	 */
	if (cgi->script_alias != NULL) {
		if (stat(cgi->script_alias, &st) == -1) {
			conn->error_code = http_not_found;
			return ret_error;
		}

		cherokee_buffer_new (&cgi->filename);
		cherokee_buffer_add (cgi->filename, cgi->script_alias, strlen(cgi->script_alias));

		/* Check the path_info even if it uses a
		 * scriptalias. the PATH_INFO is the rest of the
		 * substraction * of request - configured directory.
		 */
		if (cgi->script_alias != NULL) {
			cherokee_buffer_add (&conn->pathinfo, 
					     conn->request.buf + conn->web_directory.len, 
					     conn->request.len - conn->web_directory.len);
		}

		return ret_ok;
	}

	/* Maybe the request contains pathinfo
	 */
	if ((cgi->parameter == NULL) &&
	    cherokee_buffer_is_empty (&conn->pathinfo)) 
	{
		int req_len;
		int local_len;

		/* It is going to concatenate two paths like:
		 * local_directory = "/usr/share/cgi-bin/", and
		 * request = "/thing.cgi", so there will be two
		 * slashes in the middle of the request.
		 */
		req_len   = conn->request.len;
		local_len = conn->local_directory.len;

		if (conn->request.len > 0)
			cherokee_buffer_add (&conn->local_directory, 
					     conn->request.buf + 1, 
					     conn->request.len - 1); 

		ret = cherokee_handler_cgi_split_pathinfo (cgi, &conn->local_directory, local_len +1);
		if (unlikely(ret < ret_ok)) goto bye;
		
		/* Is the filename set? 
		 */
		if (cgi->filename == NULL) {		
			/* We have to check if the file exists 
			 */
			if (stat (conn->local_directory.buf, &st) == -1) {
				conn->error_code = http_not_found;
				return ret_error;
			}
			
			cherokee_buffer_new (&cgi->filename);
			cherokee_buffer_add_buffer (cgi->filename, &conn->local_directory);
		}
		
	bye:
		cherokee_buffer_drop_endding (&conn->local_directory, req_len - 1);
	}

	return ret;
}


static ret_t
_fd_set_properties (int fd, int add_flags, int remove_flags)
{
#ifndef _WIN32
	int flags;

	flags = fcntl (fd, F_GETFL, 0);

	flags |= add_flags;
	flags &= ~remove_flags;

	if (fcntl (fd, F_SETFL, flags) == -1) {
		PRINT_ERROR ("ERROR: Setting pipe properties fd=%d: %s\n", fd, strerror(errno));
		return ret_error;
	}	
#endif

	return ret_ok;
}


ret_t 
cherokee_handler_cgi_init (cherokee_handler_cgi_t *cgi)
{
	ret_t ret;
	int   re;
	int   pid;

	struct {
		int cgi[2];
		int server[2];
	} pipes;

	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	/* It might be trying to send the post information to the CGI:
	 * In the previus call to this function it could send
	 * everything so it returned a ret_eagain to continue, and
	 * here we are.
	 */
	if (cgi->init_phase == hcgi_phase_sent_post) {
		int eagain_fd = -1;
		int mode      =  0;

		ret = cherokee_post_walk_to_fd (&conn->post, cgi->pipeOutput, &eagain_fd, &mode);

		switch (ret) {
		case ret_ok:
			close (cgi->pipeOutput);
			cgi->pipeOutput = -1;
			return ret_ok;

		case ret_eagain:
			if (eagain_fd != -1) {
				cherokee_thread_deactive_to_polling (HANDLER_THREAD(cgi), HANDLER_CONN(cgi), eagain_fd, mode);
			}

			return ret_eagain;

		default:
			return ret;
		}
	}

	/* Extracts PATH_INFO and filename from request uri 
	 */
	ret = _extract_path(cgi);
	if (ret < ret_ok) { 
		return ret;
	}

	/* Check is the CGI is accessible
	 */
/* 	if (access(cgi->filename->buf, R_OK) != 0) { */
/* 		if (errno == ENOENT) */
/* 			conn->error_code = http_not_found; */
/* 		else */
/* 			conn->error_code = http_access_denied; */
		
/* 		return ret_error; */
/* 	} */

	/* Creates the pipes ...
	 */
	re   = pipe (pipes.cgi);
	ret |= pipe (pipes.server);

	if (re != 0) {
		conn->error_code = http_internal_error;
		return ret_error;
	}
	
	/* It has to update the timeout of the connection,
	 * otherwhise the server will drop it for the CGI
	 * isn't fast enough
	 */
	conn->timeout = CONN_THREAD(conn)->bogo_now + CGI_TIMEOUT;

	/* .. and fork the process 
	 */
#ifndef _WIN32
	pid = fork();
	if (pid == 0) 
	{
		/* Child process
		 */
		int     re;
		char   *absolute_path = cgi->filename->buf;
		char   *argv[4]       = { NULL, NULL, NULL };

		/* Close useless sides
		 */
		close (pipes.cgi[0]);
		close (pipes.server[1]);
		
		/* Change stdin and out
		 */
		dup2 (pipes.server[0], STDIN_FILENO);
		close (pipes.server[0]);

		dup2 (pipes.cgi[1], STDOUT_FILENO);
		close (pipes.cgi[1]);

# if 0
		/* Set unbuffered
		 */
		setvbuf (stdin,  NULL, _IONBF, 0);
		setvbuf (stdout, NULL, _IONBF, 0);
# endif

		/* Enable blocking mode
		 */
		_fd_set_properties (STDIN_FILENO,  0, O_NONBLOCK);
		_fd_set_properties (STDOUT_FILENO, 0, O_NONBLOCK);
		_fd_set_properties (STDERR_FILENO, 0, O_NONBLOCK);

		/* Set SIGPIPE
		 */
		signal (SIGPIPE, SIG_DFL);

		/* Sets the new environ. 
		 */			
		build_envp (conn, cgi);

		/* Change the directory 
		 */
		if (! cherokee_buffer_is_empty (&conn->effective_directory)) {
			chdir (conn->effective_directory.buf);
		} else {
			char *file = strrchr (absolute_path, '/');

			*file = '\0';
			chdir (absolute_path);
			*file = '/';
		}

		/* Build de argv array
		 */
		argv[0] = absolute_path;
		if (cgi->parameter != NULL) {
			argv[1] = cgi->parameter->buf;
			argv[2] = cgi->extra_param;
		} else {
			argv[1] = cgi->extra_param;
		}

		/* Lets go.. execute it!
		 */
		re = execve (absolute_path, argv, cgi->envp);
		if (re < 0) {
			switch (errno) {
			case ENOENT:
				printf ("Status: 404" CRLF CRLF);
				break;
			default:
				printf ("Status: 500" CRLF CRLF);
			}

			exit(1);
		}

		/* OH MY GOD!!! an error is here 
		 */
		SHOULDNT_HAPPEN;
		exit(1);

	} else if (pid < 0) {
		close (pipes.cgi[0]);
		close (pipes.cgi[1]);

		close (pipes.server[0]);
		close (pipes.server[1]);
		
		conn->error_code = http_internal_error;
		return ret_error;
	}
#else
	/* Win32: TODO
	 */
#endif
	dbg("CGI: pid %d\n", pid);

	close (pipes.server[0]);
	close (pipes.cgi[1]);

	cgi->pid        = pid;
	cgi->pipeInput  = pipes.cgi[0];
	cgi->pipeOutput = pipes.server[1];

	/* Set to Input to NON-BLOCKING
	 */
#ifndef _WIN32
	_fd_set_properties (cgi->pipeInput, O_NDELAY|O_NONBLOCK, 0);
#endif

	/* TODO: Yeah, I know, this is not a perfect solution
	 */
	cherokee_buffer_new (&cgi->data);
	cherokee_buffer_ensure_size (cgi->data, 2 * 1024); /* 2 kb for headers */

	/* POST management
	 */
	if (! cherokee_post_is_empty (&conn->post)) {
		cgi->init_phase = hcgi_phase_sent_post;
		cherokee_post_walk_reset (&conn->post);
		
		/* It returns eagain to send the POST information in
		 * the next call to this function.  It can not be send
		 * in the step() method because we need to get it done
		 * before call to add_headers().
		 */
		return ret_eagain;
	}

	return ret_ok;
}


static ret_t
_read_from_cgi (cherokee_handler_cgi_t *cgi, cherokee_buffer_t *buffer)
{
	ret_t  ret;
 	size_t readed;

	/* Read the data from the pipe:
	 */
	ret = cherokee_buffer_read_from_fd (buffer, cgi->pipeInput, 4096, &readed);
	dbg ("CGI: _read_from_cgi - cherokee_buffer_read_from_fd => ret = %d\n", ret);

	switch (ret) {
	case ret_eagain:
		cherokee_thread_deactive_to_polling (HANDLER_THREAD(cgi), HANDLER_CONN(cgi), cgi->pipeInput, 0);
		return ret_eagain;

	case ret_ok:
		dbg("CGI: _read_from_cgi - %d bytes read.\n", readed);
		return ret_ok;

	case ret_eof:
	case ret_error:
		return ret;

	default:
		RET_UNKNOWN(ret);
	}

	SHOULDNT_HAPPEN;
	return ret_error;
}


static ret_t
parse_header (cherokee_handler_cgi_t *cgi, cherokee_buffer_t *buffer)
{
	char                  *end;
	char                  *end1;
	char                  *end2;
	char                  *begin;
	cherokee_connection_t *conn = HANDLER_CONN(cgi);

	
	if (cherokee_buffer_is_empty (buffer) || buffer->len <= 5)
		return ret_ok;

	/* It is possible that the header ends with CRLF CRLF
	 * In this case, we have to remove the last two characters
	 */
	if ((buffer->len > 4) &&
	    (strncmp (CRLF CRLF, buffer->buf + buffer->len - 4, 4) == 0))
	{
		cherokee_buffer_drop_endding (&conn->header_buffer, 2);
	}
	
	/* Process the header line by line
	 */
	begin = buffer->buf;
	while (begin != NULL) {
		end1 = strchr (begin, '\r');
		end2 = strchr (begin, '\n');

		end = cherokee_min_str (end1, end2);
		if (end == NULL) break;

		end2 = end;
		while (((*end2 == '\r') || (*end2 == '\n')) && (*end2 != '\0')) end2++;

		if (strncasecmp ("Status: ", begin, 8) == 0) {
			int  code;
			char status[4];

			memcpy (status, begin+8, 3);
			status[3] = '\0';
		
			code = atoi (status);
			if (code <= 0) {
				conn->error_code = http_internal_error;
				return ret_error;
			}

			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);

			conn->error_code = code;			
			continue;
		}

		else if (strncasecmp ("Content-length: ", begin, 16) == 0) {
			cherokee_buffer_t tmp = CHEROKEE_BUF_INIT;

			cherokee_buffer_add (&tmp, begin+16, end - (begin+16));
			cgi->content_length = atoll (tmp.buf);
			cherokee_buffer_mrproper (&tmp);

			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);
		}

		else if (strncasecmp ("Location: ", begin, 10) == 0) {
			cherokee_buffer_add (&conn->redirect, begin+10, end - (begin+10));
			cherokee_buffer_remove_chunk (buffer, begin - buffer->buf, end2 - begin);
		}

		begin = end2;
	}
	
	return ret_ok;
}


ret_t
cherokee_handler_cgi_add_headers (cherokee_handler_cgi_t *cgi, cherokee_buffer_t *buffer)
{
	ret_t  ret;
	int    len;
	char  *content;
	int    end_len;

	/* Sanity check
	 */
	return_if_fail (buffer != NULL, ret_error);

	/* Read information from the CGI
	 */
	ret = _read_from_cgi (cgi, cgi->data);

	switch (ret) {
	case ret_ok:
	case ret_eof:
		break;

	case ret_error:
	case ret_eagain:
		return ret;

	default:
		RET_UNKNOWN(ret);
		return ret_error;
	}

	/* Look the end of headers
	 */
	content = strstr (cgi->data->buf, CRLF CRLF);
	if (content != NULL) {
		end_len = 4;
	} else {
		content = strstr (cgi->data->buf, "\n\n");
		end_len = 2;
	}

	if (content == NULL) {
		return (ret == ret_eof) ? ret_eof : ret_eagain;
	}

	/* Copy the header
	 */
	len = content - cgi->data->buf;	

	cherokee_buffer_ensure_size (buffer, len+6);
	cherokee_buffer_add (buffer, cgi->data->buf, len);
	cherokee_buffer_add (buffer, CRLF CRLF, 4);
	
	/* Drop out the headers, we already have a copy
	 */
	cherokee_buffer_move_to_begin (cgi->data, len + end_len);

	/* Parse the header.. it is likely we will have something to do with it.
	 */
	return parse_header (cgi, buffer);
}


ret_t
cherokee_handler_cgi_step (cherokee_handler_cgi_t *cgi, cherokee_buffer_t *buffer)
{
	ret_t ret;

	/* Maybe it has some stored data to be send
	 */
	if (cgi->data != NULL) {
		ret_t ret;
		
		dbg("CGI: sending stored data: %d bytes\n", cgi->data->len);
		
		/* Flush this buffer 
		 */
		if (! cherokee_buffer_is_empty (cgi->data)) {
			cherokee_buffer_add_buffer (buffer, cgi->data);
			ret = ret_ok;
		} else {
			ret = ret_eagain;
		}
		
		/* Free the data buffer
		 */
		cherokee_buffer_free (cgi->data);
		cgi->data = NULL;

		return ret;
	}
	

	/* Read some information from the CGI
	 */
	ret = _read_from_cgi (cgi, buffer);
	if (ret == ret_eof) goto finish;
	
	return ret;

finish:
	/* If it is the 'End of file', we have to remove the 
	 * pipe file descriptor from the thread's fdpoll
	 */
	if (cgi->cgi_fd_in_poll == true) {
		ret = cherokee_fdpoll_del (HANDLER_THREAD(cgi)->fdpoll, cgi->pipeInput);
		if (unlikely(ret != ret_ok)) return ret;
		
		cgi->cgi_fd_in_poll = false;
	}
	
	return ret_eof;
}




/* Library init function
 */

static cherokee_boolean_t _cgi_is_init = false;

#if 0
#include <signal.h>

static void 
child_finished(int sng)
{
	int status;
	while(waitpid (0, &status, WNOHANG) > 0);
}
#endif

void
MODULE_INIT(cgi) (cherokee_module_loader_t *loader)
{
	if (_cgi_is_init) return;
	_cgi_is_init = true;

#if 0
	signal(SIGCHLD, child_finished);
#endif
}

