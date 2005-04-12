/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Ayose Cazorla León <setepo@gulic.org>
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
#include <sys/wait.h>
#include <ctype.h>
#include <fcntl.h>
#include <signal.h>

#include "module.h"
#include "connection.h"
#include "connection-protected.h"
#include "socket.h"
#include "server.h"
#include "server-protected.h"
#include "header.h"
#include "header-protected.h"


cherokee_module_info_t cherokee_cgi_info = {
	cherokee_handler,           /* type     */
	cherokee_handler_cgi_new    /* new func */
};


#define dbg PRINT_DEBUG

#define set_env(c,s,l)           cherokee_handler_cgi_add_env(c,s,l) 
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
	HANDLER(n)->support     = hsupport_complex_headers;

	/* Process the request_string, and build the arguments table..
	 * We'll need it later
	 */
	ret = cherokee_connection_parse_args (cnt);
	if (unlikely(ret < ret_ok)) return ret;
		
	/* Init
	 */
	n->pipeInput         = 0;
	n->pipeOutput        = 0;
	n->post_data_sent    = 0;
	n->pid               = -1;
	n->filename          = NULL;
	n->data              = NULL;
	n->parameter         = NULL;
	n->cgi_fd_in_poll    = false;
	n->script_alias      = NULL;
	n->extra_param       = NULL;

	/* envp
	 */
	n->envp_last         = 0;

	for (i=0; i<ENV_VAR_NUM; i++)
		n->envp[i] = NULL;

	/* Get properties
	 */
	if (properties) {
		n->script_alias = cherokee_table_get_val (properties, "scriptalias");
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
	int   child_count;
	int   status;

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

	return child_count;
}


ret_t
cherokee_handler_cgi_free (cherokee_handler_cgi_t *cgi)
{
	int i;

	dbg("CGI: closing\n\n");

	/* Close the connection with the CGI
	 */
	if (cgi->pipeInput >= 0)
		close (cgi->pipeInput);

	if (cgi->pipeOutput >= 0)
		close (cgi->pipeOutput);

        /* Maybe kill the CGI
	 */
	if (cgi->pid > 0) {
		pid_t pid;

		do {
			pid = waitpid (cgi->pid, NULL, WNOHANG);
		} while ((pid == 1) && (errno == EINTR));

		if (pid <= 0) {
			kill (cgi->pid, SIGTERM);
		}
	}

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

	/* Free the envp
	 */
	for (i=0; i<cgi->envp_last; i++) {
		free (cgi->envp[i]);
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


void
cherokee_handler_cgi_add_env (cherokee_handler_cgi_t *cgi, char *string, int string_len)
{
	char *entry;

	/* Build the new envp entry
	 */
	entry = (char *) malloc (string_len+1); 
	if (entry == NULL) return;

	memcpy (entry, string, string_len);
	entry[string_len] = '\0';

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
	int                r;
	ret_t              ret;
	cherokee_buffer_t *tmp;
	char              *p;
	const char        *p_const;
	int                p_len;

	char remote_ip[CHE_INET_ADDRSTRLEN+1];
	CHEROKEE_TEMP(temp, 32);

/*                    0         1         2         3         4         5         6         7
		      01234567890123456789012345678901234567890123456789012345678901234567890 */
/*                        0         1         2         3         4         5         6         7
		          01234567890123456789012345678901234567890123456789012345678901234567890 */

	ret = cherokee_buffer_new (&tmp);
	if (unlikely(ret < ret_ok)) return ret;

	/* Set the basic variables
	 */
	set_env (cgi, "SERVER_SIGNATURE=<address>Cherokee web server</address>", 55);
	set_env (cgi, "SERVER_SOFTWARE=Cherokee " PACKAGE_VERSION, 25 + PACKAGE_VERSION_LEN);
	set_env (cgi, "GATEWAY_INTERFACE=CGI/1.1", 25);
	set_env (cgi, "PATH=/bin:/usr/bin:/sbin:/usr/sbin", 34);

	/* Servers MUST supply this value to scripts. The QUERY_STRING
	 * value is case-sensitive. If the Script-URI does not include a
	 * query component, the QUERY_STRING metavariable MUST be defined
	 * as an empty string ("").
	 */
	set_env_pair (cgi, "DOCUMENT_ROOT", 13, conn->local_directory->buf, conn->local_directory->len);

	/* The IP address of the client sending the request to the
	 * server. This is not necessarily that of the user agent (such
	 * as if the request came through a proxy).
	 */
	memset (remote_ip, 0, sizeof(remote_ip));
	cherokee_socket_ntop (conn->socket, remote_ip, sizeof(remote_ip)-1);
	set_env_pair (cgi, "REMOTE_ADDR", 11, remote_ip, strlen(remote_ip));

	/* HTTP_HOST and SERVER_NAME. The difference between them is that
	 * HTTP_HOST can include the «:PORT» text, and SERVER_NAME only
	 * the name 
	 */
	cherokee_header_copy_known (conn->header, header_host, tmp);
	if (! cherokee_buffer_is_empty(tmp)) {
		set_env_pair (cgi, "HTTP_HOST", 9, tmp->buf, tmp->len);

		p = strchr (tmp->buf, ':');
		if (p != NULL) *p = '\0';

		set_env_pair (cgi, "SERVER_NAME", 11, tmp->buf, tmp->len);
	}

	/* Cookies :-)
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Cookie", 6, tmp);
	if (ret == ret_ok) {
		set_env_pair (cgi, "HTTP_COOKIE", 11, tmp->buf, tmp->len);
	}

	/* User Agent
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_known (conn->header, header_user_agent, tmp);
	if (ret == ret_ok) {
		set_env_pair (cgi, "HTTP_USER_AGENT", 15, tmp->buf, tmp->len);
	}

	/* Content-type and Content-lenght (if available) 
	 */
	cherokee_buffer_clean (tmp);
	ret = cherokee_header_copy_unknown (conn->header, "Content-Type", 12, tmp);
	if (ret == ret_ok)
		set_env_pair (cgi, "CONTENT_TYPE", 12, tmp->buf, tmp->len);

	cherokee_buffer_clean (tmp); 
	ret = cherokee_header_copy_known (conn->header, header_content_length, tmp);
	if (ret == ret_ok)
		set_env_pair (cgi, "CONTENT_LENGTH", 14, tmp->buf, tmp->len);

	/* If there is a query_string, set the environment variable
	 */
	if (conn->query_string->len > 0) 
		set_env_pair (cgi, "QUERY_STRING", 12, conn->query_string->buf, conn->query_string->len);
	else
		set_env_pair (cgi, "QUERY_STRING", 12, "", 0);

	/* Set the server port
	 */
	r = snprintf (temp, temp_size, "%d", CONN_SRV(conn)->port);
	set_env_pair (cgi, "SERVER_PORT", 11, temp, r);

	/* Set the HTTP version
	 */
	ret = cherokee_http_version_to_string (conn->header->version, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (cgi, "SERVER_PROTOCOL", 15, (char *)p_const, p_len);

	/* Set the method
	 */
	ret = cherokee_http_method_to_string (conn->header->method, &p_const, &p_len);
	if (ret >= ret_ok)
		set_env_pair (cgi, "REQUEST_METHOD", 14, (char *)p_const, p_len);

	/* Set the host name
	 */
	if (!cherokee_buffer_is_empty (conn->host)) {
		p = strchr (conn->host->buf, ':');
		if (p != NULL) *p = '\0';
		
		set_env_pair (cgi, "SERVER_NAME", 11, conn->host->buf, conn->host->len);

		if (p != NULL) *p = ':';
	}

	/* Set PATH_INFO 
	 */
	if (! cherokee_buffer_is_empty(conn->pathinfo)) {
		set_env_pair (cgi, "PATH_INFO", 9, conn->pathinfo->buf, conn->pathinfo->len);
	}

	/* Set REQUEST_URI 
	 */
	cherokee_buffer_clean (tmp);
	cherokee_header_copy_request_w_args (conn->header, tmp);
	set_env_pair(cgi, "REQUEST_URI", 11, tmp->buf, tmp->len);

	/* Set SCRIPT_NAME
	 */
	if (cgi->parameter) {
		p = cgi->parameter->buf + conn->local_directory->len -1;
		set_env_pair (cgi, "SCRIPT_NAME", 11, p, (cgi->parameter->buf + cgi->parameter->len) - p);
	} else {	
		cherokee_buffer_clean (tmp);
		cherokee_header_copy_request (conn->header, tmp);
		set_env_pair(cgi, "REQUEST_URI", 11, tmp->buf, tmp->len);
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
	cherokee_buffer_free (tmp);
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
	cherokee_buffer_add (conn->pathinfo, pathinfo, pathinfo_len);
	
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
		return ret_ok;
	}

	/* Maybe the request contains pathinfo
	 */
	if ((cgi->parameter == NULL) &&
	    cherokee_buffer_is_empty (conn->pathinfo)) 
	{
		int req_len;
		int local_len;

		req_len   = conn->request->len;
		local_len = conn->local_directory->len;

		cherokee_buffer_add_buffer (conn->local_directory, conn->request); 

		ret = cherokee_handler_cgi_split_pathinfo (cgi, conn->local_directory, local_len +1);
		if (unlikely(ret < ret_ok)) goto bye;
		
		/* Is the filename set? 
		 */
		if (cgi->filename == NULL) {		
			/* We have to check if the file exists 
			 */
			if (stat(conn->local_directory->buf, &st) == -1) {
				conn->error_code = http_not_found;
				return ret_error;
			}
			
			cherokee_buffer_new (&cgi->filename);
			cherokee_buffer_add_buffer (cgi->filename, conn->local_directory);
		}
		
	bye:
		cherokee_buffer_drop_endding (conn->local_directory, req_len);		
	}

	return ret;
}


static ret_t
_send_post_data (cherokee_handler_cgi_t *cgi)
{
	int                    r;
	cherokee_connection_t *conn;

	conn = HANDLER_CONN(cgi);

	if ((conn->post == NULL) || (cgi->post_data_sent >= conn->post_len)) {
		return ret_ok;
	}

	r = write (cgi->pipeOutput,
		   conn->post->buf + cgi->post_data_sent,
		   conn->post_len - cgi->post_data_sent);

	if (r == -1) {
		dbg("Can't write to the client\n");
		return (errno == EAGAIN) ? ret_eagain : ret_error;
	}

	cgi->post_data_sent += r;
	dbg("Write %d bytes of POST\n", r);

	/* Are all the data sent? 
	 */
	if (cgi->post_data_sent >= conn->post_len) {
		return ret_ok;
	}

	return ret_eagain;
}


static ret_t
_fd_set_properties (int fd, int add_flags, int remove_flags)
{
	int flags;

	flags = fcntl (fd, F_GETFL, 0);

	flags |= add_flags;
	flags &= ~remove_flags;

	if (fcntl (fd, F_SETFL, flags) == -1) {
		PRINT_ERROR ("ERROR: Setting pipe properties fd=%d: %s\n", fd, strerror(errno));
		return ret_error;
	}	

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

	/* Extracts PATH_INFO and filename from request uri 
	 */
	ret = _extract_path(cgi);
	if (ret < ret_ok) { 
		return ret;
	}

	/* Creates the pipes ...
	 */
	re   = pipe (pipes.cgi);
	ret |= pipe (pipes.server);

	if (re != 0) {
		conn->error_code = http_internal_error;
		return ret_error;
	}

	/* .. and fork the process 
	 */
	pid = fork();
	if (pid == 0) 
	{
		/* Child process
		 */
		int   re;
		char *absolute_path = cgi->filename->buf;
		char *argv[4]       = { NULL, NULL, NULL };

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

#if 0
		/* Set unbuffered
		 */
		setvbuf (stdin,  NULL, _IONBF, 0);
		setvbuf (stdout, NULL, _IONBF, 0);
#endif

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
		if (!cherokee_buffer_is_empty (conn->effective_directory)) {
			chdir (conn->effective_directory->buf);
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

	dbg("CGI: pid %d\n", pid);

	close (pipes.server[0]);
	close (pipes.cgi[1]);

	cgi->pid        = pid;
	cgi->pipeInput  = pipes.cgi[0];
	cgi->pipeOutput = pipes.server[1];

	/* Set to Input to NON-BLOCKING
	 */
	_fd_set_properties (cgi->pipeInput, O_NDELAY|O_NONBLOCK, 0);

	/* TODO: Yeah, I know, this is not a perfect solution
	 */
	cherokee_buffer_new (&cgi->data);
	cherokee_buffer_ensure_size (cgi->data, 2 * 1024); /* 2 kb for headers */

	/* POST management
	 */
	if (conn->post != NULL) {
		_fd_set_properties (cgi->pipeOutput, O_NONBLOCK, 0);

		/* Sent POST data 
		 */
		if ((conn->post != NULL) && 
		    (cgi->post_data_sent < conn->post_len)) 
		{
			do {
				ret = _send_post_data (cgi);
				if (ret == ret_ok)     break;
				if (ret == ret_eagain) continue;
				
				return ret;
			} while (1);
		}

	} 

	/* We don't need to send anything to the client,
	 * so we close the pipe
	 */
	close (cgi->pipeOutput);
	cgi->pipeOutput = -1;

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
		if (cgi->cgi_fd_in_poll == false) {
			/* Add the CGI pipe file descriptor to the thread's poll
			 */
			ret = cherokee_fdpoll_add (HANDLER_THREAD(cgi)->fdpoll, cgi->pipeInput, 0);
			if (unlikely(ret != ret_ok)) return ret;
	
			dbg ("CGI: _read_from_cgi - added fd=%d al poll\n", cgi->pipeInput);		
			cgi->cgi_fd_in_poll = true;
		}

		ret = cherokee_thread_deactive_to_polling (HANDLER_THREAD(cgi), HANDLER_CONN(cgi), cgi->pipeInput);
		if (unlikely(ret != ret_ok)) return ret;	
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
	
	return ret_ok;
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
cgi_init ()
{
	if (_cgi_is_init == true) {
		return;
	}

	_cgi_is_init = true;

#if 0
	signal(SIGCHLD, child_finished);
#endif
}

