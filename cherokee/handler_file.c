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

#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <errno.h>
#include <time.h>

#include "server.h"
#include "server-protected.h"
#include "tdate_parse.h"
#include "mime.h"
#include "handler_file.h"
#include "connection.h"
#include "module.h"
#include "mmap2.h"
#include "util.h"


cherokee_module_info_t cherokee_file_info = {
	cherokee_handler,            /* type     */
	cherokee_handler_file_new    /* new func */
};

ret_t
cherokee_handler_file_new  (cherokee_handler_t **hdl, cherokee_connection_t *cnt, cherokee_table_t *properties)
{
	CHEROKEE_NEW_STRUCT (n, handler_file);
	
	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt);

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_file_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_file_free;
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_file_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_file_add_headers;

	/* Supported features
	 */
	HANDLER(n)->support     = hsupport_length | hsupport_range;

	/* Init
	 */
	n->fd             = -1;
	n->offset         = 0;
	n->mime           = NULL;
	n->using_sendfile = false;

	/* Return the object
	 */
	*hdl = HANDLER(n);
	return ret_ok;
}


ret_t
cherokee_handler_file_free (cherokee_handler_file_t *fhdl)
{
	if (fhdl->fd != -1) {
		close (fhdl->fd);
		fhdl->fd = -1;
	}

	return ret_ok;
}


static ret_t
check_cached (cherokee_handler_file_t *n)
{
	ret_t ret;
	char *header;
	int   header_len;
	cherokee_connection_t *conn = HANDLER_CONN(n);

	/* Based in time
	 */
	ret = cherokee_header_get_unknown (conn->header, "If-Modified-Since", 17, &header, &header_len);
	if (ret == ret_ok)  {
		time_t req_time;
		char   tmp;
		char  *end = header + header_len;

		tmp = *end;    /* save */
		*end = '\0';   /* set  */

		req_time = tdate_parse (header);			
		if (req_time == -1) {
			cherokee_logger_write_string (
				CONN_VSRV(conn)->logger, 
				"Warning: Unparseable time '%s'",
				header);
		}
		*end = tmp;   /* restore */
		
		if (req_time == -1) {
			return ret_ok;
		}
		
		/* The file is cached in the client
		 */
		if (n->info.st_mtime <= req_time) {
			conn->error_code = http_not_modified;
			return ret_error;	
		}
	}
	
	if (conn->header->version == http_version_11) {
		/* Based in ETag
		 */
		ret = cherokee_header_get_unknown (conn->header, "If-None-Match", 13, &header, &header_len);
		if (ret == ret_ok)  {
			int    tmp_len;
			CHEROKEE_TEMP(tmp,100);
			
			tmp_len = snprintf (tmp, tmp_size, "%lx=" FMT_OFFSET "x", n->info.st_mtime, n->info.st_size);

			if (strncmp (header, tmp, tmp_len) == 0) {
				conn->error_code = http_not_modified;
				return ret_error;				
			}
		}
	}

	return ret_ok;
}

ret_t 
cherokee_handler_file_init (cherokee_handler_file_t *n)
{
	int   ret;
	char *ext;
	cherokee_mmap2_entry_t *mmap_entry = NULL;
	cherokee_connection_t  *conn       = HANDLER_CONN(n);
	cherokee_server_t      *srv        = HANDLER_SRV(n);

	/* Build the local file path                         1.- BUILD
	 * "return" is forbbiden until 2.
	 */
	if (conn->request->len > 1) {
		cherokee_buffer_add (conn->local_directory, conn->request->buf+1, conn->request->len-1); 
	}

	/* Open the file and stat
	 */
	n->fd = open (conn->local_directory->buf, O_RDONLY);

	if (n->fd != -1) {
		ret = fstat (n->fd, &n->info);
	} else {
		/* We have no access to that file. Why? */
		if (errno == EACCES)
			conn->error_code = http_access_denied;
		else if (errno == ENOENT)
			conn->error_code = http_not_found;
		else
			conn->error_code = http_internal_error;

		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return ret_error;
	}

	/* Is it cached in the client?
	 */
	ret = check_cached(n);
	if (ret != ret_ok) {
		cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);
		return ret;
	}

	/* Is this file cached in the mmap table?
	 */
	if ((conn->encoder == NULL) &&
	    (conn->socket->is_tls == non_TLS) &&
	    (n->info.st_size <= MMAP_MAX_FILE_SIZE) &&
	    (http_mehod_with_body (conn->header->method))) 
	{
		ret = cherokee_mmap2_get (srv->mmap_cache,
					  conn->local_directory->buf,
					  &mmap_entry);
		if (ret == ret_ok) {
			conn->mmap_entry_ref = mmap_entry;
		}
	}

	/* Undo the local file path                          2.- UNDO
	 * "return" is allowed again
	 */
	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len);

	/* Is it a directory?
	 */
	if (S_ISDIR(n->info.st_mode)) {
		conn->error_code = http_access_denied;
		return ret_error;		
	}

	/* Range 1: Check the range and file size
	 */
	if ((conn->range_start > n->info.st_size) ||
	    (conn->range_end   > n->info.st_size)) 
	{
		conn->error_code = http_range_not_satisfiable;
		return ret_error;
	}

	/* Set the error code
	 */
	if (conn->range_start != 0 || conn->range_end != 0) {
		conn->error_code = http_partial_content;
	}

	/* Range 2: Set the file length as the range end
	 */
	if (conn->range_end == 0) {
		conn->range_end = n->info.st_size;
	} 

	/* Set mmap or file position
	 */
	if (conn->mmap_entry_ref != NULL) {
		/* Set the mmap info
		 */
		conn->mmaped     = conn->mmap_entry_ref->mmaped +  conn->range_start;
		conn->mmaped_len = conn->mmap_entry_ref->mmaped_len - (
			conn->range_start + (conn->mmap_entry_ref->mmaped_len - conn->range_end));
	} else {
		/* Seek the file is needed
		 */
		if ((conn->range_start != 0) && (conn->mmaped == NULL)) {
			n->offset = conn->range_start;
			lseek (n->fd, n->offset, SEEK_SET);
		}
	}

	/* Look for the mime type
	 */
	ext = strrchr (conn->request->buf, '.');
	if (ext != NULL) {
		cherokee_mime_t *m;

		ret = cherokee_mime_get_default (&m);
		if (ret >= ret_ok) {
			ret = cherokee_mime_get (m, ext+1, &n->mime);
		}
	}

	/* Maybe use sendfile
	 */
#ifdef HAVE_SENDFILE
	n->using_sendfile = ((conn->mmaped == NULL) &&
			     (conn->encoder == NULL) &&
			     (n->info.st_size >= srv->sendfile.min) && 
			     (n->info.st_size <  srv->sendfile.max) &&
			     (conn->socket->is_tls == non_TLS));
	
	if (n->using_sendfile) {
		cherokee_connection_set_cork(conn, 1);
	}
#endif
	
	return ret_ok;
}


ret_t
cherokee_handler_file_step (cherokee_handler_file_t *fhdl, 
			    cherokee_buffer_t       *buffer)
{
	off_t total;

#if HAVE_SENDFILE
	if (fhdl->using_sendfile) {
		ret_t   ret;
		ssize_t sent;
		cherokee_connection_t *conn = HANDLER_CONN(fhdl);
		
		ret = cherokee_socket_sendfile (conn->socket,                      /* cherokee_socket_t *socket */
						fhdl->fd,                          /* int                fd     */
						conn->range_end - fhdl->offset,    /* size_t             size   */
						&fhdl->offset,                     /* off_t             *offset */
						&sent);                            /* ssize_t           *sent   */
		if (ret < ret_ok) return ret;

		/* cherokee_handler_file_init() activated the TCP_CORK flags.
		 * After it, the header was sent.  And now, the first
		 * chunk of the file with sendfile().  It's time to turn
		 * off again the TCP_CORK flag
		 */
		if (conn->tcp_cork) {
			cherokee_connection_set_cork (conn, 0);
		}

		if (fhdl->offset >= conn->range_end) {
			return ret_eof;
		}

		return ret_ok_and_sent;
	}
#endif

	total = read (fhdl->fd, buffer->buf, buffer->size);	
	switch (total) {
	case 0:
		return ret_eof;
	case -1:
		return ret_error;
	default:
		buffer->len = total;
		fhdl->offset += total;
	}	

	/* Maybe it was the last file chunk
	 */
	if (fhdl->offset >= HANDLER_CONN(fhdl)->range_end) {
		return ret_eof_have_data;
	}

	return ret_ok;
}


ret_t
cherokee_handler_file_add_headers (cherokee_handler_file_t *fhdl,
				   cherokee_buffer_t       *buffer)
{
	int       len;
	off_t     length;
	struct tm modified_tm;

	CHEROKEE_TEMP(tmp,100);

	/* Add "Content-length:" header
	 */
	if (fhdl->fd != -1) {
		/* We stat()'ed the file in the handler constructor
		 */
		length = HANDLER_CONN(fhdl)->range_end - HANDLER_CONN(fhdl)->range_start;		
		if (length < 0) {
			length = 0;
		}

		if (HANDLER_SUPPORT_LENGTH(fhdl) && 
		    (HANDLER_CONN(fhdl)->encoder == NULL)) 
		{
			cherokee_buffer_add_va (buffer, "Content-length: " FMT_OFFSET CRLF, length);
		}

	} else {
		/* Can't use Keep-alive w/o "Content-length:", so
		 */
		HANDLER_CONN(fhdl)->keepalive = 0;
	}

	/* Add MIME related headers: 
	 * "Content-Type:" and "Cache-Control: max-age="
	 */
	if (fhdl->mime != NULL) {
		cherokee_buffer_add (buffer, "Content-Type: ", 14);
		cherokee_buffer_add_buffer (buffer, MIME_ENTRY_NAME(fhdl->mime));
		cherokee_buffer_add (buffer, CRLF, 2);

		if (MIME_ENTRY_AGE(fhdl->mime) != -1) {
			cherokee_buffer_add_va (buffer, "Cache-Control: max-age=%d"CRLF, 
						MIME_ENTRY_AGE(fhdl->mime));
		}
	}

	/* Etag
	 */
	if (HANDLER_CONN(fhdl)->header->version >= http_version_11) { 
		cherokee_buffer_add_va (buffer, "Etag: %lx=%lx"CRLF, fhdl->info.st_mtime, fhdl->info.st_size);
	}

	/* Last-Modified:
	 */
	len = strftime (tmp, tmp_size, "Last-Modified: %a, %d %b %Y %H:%M:%S GMT"CRLF, 
			cherokee_gmtime(&fhdl->info.st_mtime, &modified_tm));
	cherokee_buffer_add (buffer, tmp, len);

	return ret_ok;
}



/*   Library init function
 */
static cherokee_boolean_t _file_is_init = false;

void
file_init (cherokee_module_loader_t *loader)
{
	ret_t ret;
	cherokee_mime_t *mime;

	/* Init flag
	 */
	if (_file_is_init == true) {
		return;
	}
	_file_is_init = true;


	/* Library dependences: mime
	 */
	ret = cherokee_mime_init (&mime);
	if (ret < ret_ok) {
		PRINT_ERROR_S ("Can not init MIME module\n");
		return;
	}
}
