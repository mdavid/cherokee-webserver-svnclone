/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2007 Alvaro Lopez Ortega
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
#include "dtm.h"
#include "mime.h"
#include "header.h"
#include "header-protected.h"
#include "handler_file.h"
#include "connection.h"
#include "connection-protected.h"
#include "module.h"
#include "iocache.h"
#include "util.h"

#define ENTRIES "handler,file"


/* Plug-in initialization
 */
PLUGIN_INFO_HANDLER_EASIEST_INIT (file, http_get | http_head);


/* Methods implementation
 */
ret_t 
cherokee_handler_file_props_free (cherokee_handler_file_props_t *props)
{
	return cherokee_handler_props_free_base (HANDLER_PROPS(props));
}


ret_t 
cherokee_handler_file_configure (cherokee_config_node_t *conf, cherokee_server_t *srv, cherokee_module_props_t **_props)
{
	cherokee_list_t               *i;
	cherokee_handler_file_props_t *props;

	if (*_props == NULL) {
		CHEROKEE_NEW_STRUCT (n, handler_file_props);

		cherokee_handler_props_init_base (HANDLER_PROPS(n), 
						  MODULE_PROPS_FREE(cherokee_handler_file_props_free));

		n->use_cache = true;
		*_props = MODULE_PROPS(n);
	}
	
	props = PROP_FILE(*_props);

	cherokee_config_node_foreach (i, conf) {
		cherokee_config_node_t *subconf = CONFIG_NODE(i);
		
		if (equal_buf_str (&subconf->key, "iocache")) {
			props->use_cache = atoi (subconf->val.buf);
		} else {
			PRINT_MSG ("ERROR: Handler file: Unknown key: '%s'\n", subconf->key.buf);
			return ret_deny;
		}
	}

	return ret_ok;
}


ret_t
cherokee_handler_file_new  (cherokee_handler_t **hdl, cherokee_connection_t *cnt, cherokee_module_props_t *props)
{
	CHEROKEE_NEW_STRUCT (n, handler_file);
	
	/* Init the base class object
	 */
	cherokee_handler_init_base (HANDLER(n), cnt, HANDLER_PROPS(props), PLUGIN_INFO_HANDLER_PTR(file));

	MODULE(n)->free         = (module_func_free_t) cherokee_handler_file_free;
	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_file_init;
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
	n->info           = NULL;
	n->using_sendfile = false;
	n->not_modified   = false;

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
check_cached (cherokee_handler_file_t *fhdl)
{
	ret_t                  ret;
	char                  *header;
	cuint_t                header_len;
	cherokee_boolean_t     has_modified_since = false;
	cherokee_boolean_t     has_etag           = false;
	cherokee_boolean_t     not_modified_ms    = false;
	cherokee_boolean_t     not_modified_etag  = false;
	cherokee_connection_t *conn               = HANDLER_CONN(fhdl);
	cherokee_thread_t     *thread             = HANDLER_THREAD(fhdl);

	/* Based in time
	 */
	ret = cherokee_header_get_known (&conn->header, header_if_modified_since, &header, &header_len);
	if (ret == ret_ok)  {
		time_t req_time;
		char   tmp;
		char  *end = header + header_len;

		tmp = *end;    /* save */
		*end = '\0';   /* set  */

		has_modified_since = true;

		req_time = cherokee_dtm_str2time (header);			
		if (unlikely (req_time == DTM_TIME_EVAL)) {
			cherokee_logger_write_string (
				CONN_VSRV(conn)->logger, 
				"Warning: Unparseable time '%s'",
				header);
			/* restore end of line */
			*end = tmp;
			return ret_ok;
		}
		/* restore end of line */
		*end = tmp;
		
		/* The file is cached in the client
		 */
		if (fhdl->info->st_mtime <= req_time) {
			not_modified_ms = true;
		}
	}
	
	/* HTTP/1.1 only headers from now on
	 */
	if (conn->header.version < http_version_11) {
		fhdl->not_modified = not_modified_ms;
		return ret_ok;
	}

	/* Based in ETag
	 */
	ret = cherokee_header_get_known (&conn->header, header_if_none_match, &header, &header_len);
	if (ret == ret_ok)  {
		cherokee_buffer_t *tmp = THREAD_TMP_BUF1(thread);

		/* Temporary buffer has already been pre-allocated,
		 * so here its length is only reset.
		 */
		cherokee_buffer_clean (tmp);

		/* Build ETag value
		 */
		cherokee_buffer_add_ullong16 (tmp, (cullong_t) fhdl->info->st_mtime);
		cherokee_buffer_add_str      (tmp, "=");
		cherokee_buffer_add_ullong16 (tmp, (cullong_t) fhdl->info->st_size);

		/* Compare ETag(s)
		 */
		if ((header_len == tmp->len) && 
		    (strncmp (header, tmp->buf, tmp->len) == 0)) 
		{
			not_modified_etag = true;
		}

		has_etag = true;
	}

	/* If both If-Modified-Since and ETag have been found then
	 * both must match in order to return a not_modified response.
	 */
	if (has_modified_since && has_etag) {
		if (not_modified_ms && not_modified_etag) {
			fhdl->not_modified = true;		
			return ret_ok;
		}
	} else  {
		if (not_modified_ms || not_modified_etag) {
			fhdl->not_modified = true;		
			return ret_ok;
		}
	}
	
	/* If-Range
	 */
	ret = cherokee_header_get_known (&conn->header, header_if_range, &header, &header_len);
	if (ret == ret_ok)  {
		time_t req_time;
		char   tmp;
		char  *end = header + header_len;
		
		tmp = *end; 
		*end = '\0'; 
		
		req_time = cherokee_dtm_str2time (header);			
		if (unlikely (req_time == DTM_TIME_EVAL)) {
			cherokee_logger_write_string (
				CONN_VSRV(conn)->logger, 
				"Warning: Unparseable time '%s'",
				header);
			*end = tmp;
			return ret_ok;
		}
		*end = tmp;

		/* If the entity tag given in the If-Range header
		 * matches the current entity tag for the entity, then
		 * the server SHOULD provide the specified sub-range
		 * of the entity using a 206 (Partial content)
		 * response. If the entity tag does not match, then
		 * the server SHOULD return the entire entity using a
		 * 200 (OK) response.
		 */
		if (fhdl->info->st_mtime > req_time) {
			conn->error_code = http_ok;

			conn->range_start = 0;
			conn->range_end = 0;
		}
	}

	return ret_ok;
}


static ret_t 
open_local_directory (cherokee_handler_file_t *fhdl, cherokee_connection_t *conn)
{
	/* Check if it is already open
	 */
	if (fhdl->fd > 0)
		return ret_ok;

	/* Open it
	 */
	fhdl->fd = open (conn->local_directory.buf, CHE_O_READ);
	if (fhdl->fd > 0) return ret_ok;

	/* Manage errors
	 */
	switch (errno) {
	case EACCES:
		conn->error_code = http_access_denied;
		break;
	case ENOENT:
		conn->error_code = http_not_found;
		break;
	default:
		conn->error_code = http_internal_error;
	}
	return ret_error;
}


static ret_t
stat_local_directory (cherokee_handler_file_t *fhdl, cherokee_connection_t *conn, cherokee_iocache_entry_t **io_entry, struct stat **info)
{	
	int                re;
	ret_t              ret;
	cherokee_server_t *srv = CONN_SRV(conn);

	/* I/O cache
	 */
#ifndef CHEROKEE_EMBEDDED
	if (HDL_FILE_PROP(fhdl)->use_cache) {
		ret = cherokee_iocache_get_or_create_w_stat (srv->iocache, &conn->local_directory, io_entry);
		switch (ret) {
		case ret_ok:
			*info = &(*io_entry)->state;
			return ret_ok;

		case ret_no_sys:
			goto without;

		case ret_not_found:
			conn->error_code = http_not_found;
			break;
		case ret_deny:
			conn->error_code = http_access_denied;
			break;
		default:
			conn->error_code = http_internal_error;
		}
		
		return ret_error;		
	}
#endif 

	/* Without cache
	 */
without:
	re = stat (conn->local_directory.buf, &fhdl->cache_info);
	if (re >= 0) {
		*info = &fhdl->cache_info;		
		return ret_ok;
	}

	switch (errno) {
	case ENOENT: 
		conn->error_code = http_not_found;
		break;
	case EACCES: 
		conn->error_code = http_access_denied;
		break;
	default:     
		conn->error_code = http_internal_error;
	}

	return ret_error;
}


ret_t 
cherokee_handler_file_init (cherokee_handler_file_t *fhdl)
{
	ret_t                     ret;
	cherokee_boolean_t        use_io   = false;
	cherokee_connection_t    *conn     = HANDLER_CONN(fhdl);
	cherokee_server_t        *srv      = HANDLER_SRV(fhdl);

	/* Build the local file path                         1.- BUILD
	 * Take care with "return"s until 2.
	 */
	if (conn->request.len > 1) {
		cherokee_buffer_add (&conn->local_directory, conn->request.buf+1, conn->request.len-1); 
	}

	/* Query the I/O cache
	 */
	ret = stat_local_directory (fhdl, conn, &conn->io_entry_ref, &fhdl->info);
	if (ret != ret_ok) {
		cherokee_buffer_drop_endding (&conn->local_directory, conn->request.len);		
		return ret;
	}

	/* Ensure it is a file
	 */
	if (S_ISDIR(fhdl->info->st_mode)) {
		conn->error_code = http_access_denied;
		return ret_error;
	}

	/* Look for the mime type
	 */
#ifndef CHEROKEE_EMBEDDED
	if (srv->mime != NULL) {
		char *ext;

		ext = strrchr (conn->request.buf, '.');
		if (ext != NULL) {
			ret = cherokee_mime_get_by_suffix (srv->mime, ext+1, &fhdl->mime);
		}
	}
#endif

	/* Is it cached on the client?
	 */
	ret = check_cached (fhdl);
	if ((ret != ret_ok) ||
	    (fhdl->not_modified)) 
	{
		cherokee_buffer_drop_endding (&conn->local_directory, conn->request.len);
		return ret;
	}

	/* Is this file cached in the io cache?
	 */
#ifndef CHEROKEE_EMBEDDED
	use_io = ((HDL_FILE_PROP(fhdl)->use_cache) &&
		  (conn->encoder == NULL) &&
		  (conn->socket.is_tls == non_TLS) &&
		  (fhdl->info->st_size <= IOCACHE_MAX_FILE_SIZE) &&
		  (http_method_with_body (conn->header.method)));
	

	TRACE(ENTRIES, "Using iocache %d\n", use_io);
	
	if (use_io) {
		if (conn->io_entry_ref)
			cherokee_iocache_mmap_release (srv->iocache, conn->io_entry_ref);
		ret = cherokee_iocache_get_or_create_w_mmap (srv->iocache,
							     &conn->local_directory,
							     &conn->io_entry_ref, 
							     &fhdl->fd);

		TRACE (ENTRIES, "iocache looked up, local=%s ret=%d\n", 
		       conn->local_directory.buf, ret);

		switch (ret) {
		case ret_ok:
			break;
		case ret_no_sys:
			use_io = false;
			break;
		default:
			cherokee_buffer_drop_endding (&conn->local_directory, conn->request.len);
			return ret_error;
		}
	}
#endif

	/* Maybe open the file
	 */
	if ((fhdl->fd < 0) && (!use_io)) {
		ret = open_local_directory (fhdl, conn);
		if (ret != ret_ok) {
			cherokee_buffer_drop_endding (&conn->local_directory, conn->request.len);
			return ret;
		}
	}

	/* Undo the local file path                          2.- UNDO
	 * "return" is allowed again
	 */
	cherokee_buffer_drop_endding (&conn->local_directory, conn->request.len);

	/* Is it a directory?
	 */
	if (S_ISDIR(fhdl->info->st_mode)) {
		conn->error_code = http_access_denied;
		return ret_error;		
	}

	/* Range 1: Check the range and file size
	 */
	if (unlikely ((conn->range_start > fhdl->info->st_size) ||
		      (conn->range_end   > fhdl->info->st_size)))
	{
		conn->range_end  = fhdl->info->st_size;
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
		conn->range_end = fhdl->info->st_size;
	} else {
		conn->range_end++;
	}

	/* Set mmap or file position
	 */
	if ((conn->io_entry_ref != NULL) &&
	    (conn->io_entry_ref->mmaped != NULL)) 
	{
		/* Set the mmap info
		 */
		conn->mmaped     = conn->io_entry_ref->mmaped + conn->range_start;
		conn->mmaped_len = conn->io_entry_ref->mmaped_len - (
			conn->range_start + (conn->io_entry_ref->mmaped_len - conn->range_end));
	} else {
		/* Seek the file if needed
		 */
		if ((conn->range_start != 0) && (conn->mmaped == NULL)) {
			fhdl->offset = conn->range_start;
			lseek (fhdl->fd, fhdl->offset, SEEK_SET);
		}
	}

	/* Maybe use sendfile
	 */
#ifdef WITH_SENDFILE
	fhdl->using_sendfile = ((conn->mmaped == NULL) &&
				(conn->encoder == NULL) &&
				(fhdl->info->st_size >= srv->sendfile.min) && 
				(fhdl->info->st_size <  srv->sendfile.max) &&
				(conn->socket.is_tls == non_TLS));

	if (fhdl->using_sendfile) {
		cherokee_connection_set_cork (conn, 1);
	}
#endif
	
	return ret_ok;
}


ret_t
cherokee_handler_file_add_headers (cherokee_handler_file_t *fhdl,
				   cherokee_buffer_t       *buffer)
{
	ret_t                  ret;
	char                   bufstr[DTM_SIZE_GMTTM_STR];
	size_t                 szlen          = 0;
	off_t                  content_length = 0;
	struct tm              modified_tm    = { 0 };
	cherokee_connection_t *conn           = HANDLER_CONN(fhdl);

	/* ETag:
	 */
	if (conn->header.version >= http_version_11) { 
		/*
		 * "ETag: %lx=" FMT_OFFSET_HEX CRLF
		 */
		cherokee_buffer_add_str     (buffer, "ETag: ");
		cherokee_buffer_add_ullong16(buffer, (cullong_t) fhdl->info->st_mtime);
		cherokee_buffer_add_str     (buffer, "=");
		cherokee_buffer_add_ullong16(buffer, (cullong_t) fhdl->info->st_size);
		cherokee_buffer_add_str     (buffer, CRLF);
	}

	/* Last-Modified:
	 */
	cherokee_gmtime (&fhdl->info->st_mtime, &modified_tm);

	szlen = cherokee_dtm_gmttm2str(bufstr, DTM_SIZE_GMTTM_STR, &modified_tm);

	cherokee_buffer_add_str(buffer, "Last-Modified: ");
	cherokee_buffer_add    (buffer, bufstr, szlen);
	cherokee_buffer_add_str(buffer, CRLF);

#ifndef CHEROKEE_EMBEDDED
	/* Add MIME related headers: 
	 * "Content-Type:" and "Cache-Control: max-age="
	 */
	if (fhdl->mime != NULL) {
		cherokee_buffer_t *mime;
		cuint_t            maxage;
		
		cherokee_mime_entry_get_type (fhdl->mime, &mime);
		cherokee_buffer_add_str   (buffer, "Content-Type: ");
		cherokee_buffer_add_buffer(buffer, mime);
		cherokee_buffer_add_str   (buffer, CRLF);
		
		ret = cherokee_mime_entry_get_maxage (fhdl->mime, &maxage);             
		if (ret == ret_ok) {
			cherokee_buffer_add_str    (buffer, "Cache-Control: max-age=");
			cherokee_buffer_add_ulong10(buffer, (culong_t) maxage);
			cherokee_buffer_add_str    (buffer, CRLF);
		}
	}
#endif

	/* If it's replying "304 Not Modified", we're done here
	 */
	if (fhdl->not_modified) {
		/* The handler will manage this special reply
		 */
		HANDLER(fhdl)->support |= hsupport_error;

		conn->error_code = http_not_modified;
		return ret_ok;
	}

	/* We stat()'ed the file in the handler constructor
	 */
	content_length = conn->range_end - conn->range_start;		
	if (unlikely (content_length < 0))
		content_length = 0;

	if (conn->encoder == NULL) {
		if (conn->error_code == http_partial_content) {
			/*
			 * "Content-Range: bytes " FMT_OFFSET "-" FMT_OFFSET
			 *                                    "/" FMT_OFFSET CRLF
			 */
			cherokee_buffer_add_str     (buffer, "Content-Range: bytes ");
			cherokee_buffer_add_ullong10(buffer, (cullong_t)conn->range_start);
			cherokee_buffer_add_str     (buffer, "-");
			cherokee_buffer_add_ullong10(buffer, (cullong_t)(conn->range_end - 1));
			cherokee_buffer_add_str     (buffer, "/");
			cherokee_buffer_add_ullong10(buffer, (cullong_t)fhdl->info->st_size);
			cherokee_buffer_add_str     (buffer, CRLF);
		}

		/*
		 * "Content-Length: " FMT_OFFSET CRLF
		 */
		cherokee_buffer_add_str     (buffer, "Content-Length: ");
		cherokee_buffer_add_ullong10(buffer, (cullong_t) content_length);
		cherokee_buffer_add_str     (buffer, CRLF);

	} else {
		/* Can't use Keep-alive w/o "Content-length:", so disable it.
		 */
		conn->keepalive = 0;
	}

	return ret_ok;
}


ret_t
cherokee_handler_file_step (cherokee_handler_file_t *fhdl, cherokee_buffer_t *buffer)
{
	off_t                  total;
	int                    size;
	cherokee_connection_t *conn = HANDLER_CONN(fhdl);

#ifdef WITH_SENDFILE
	if (fhdl->using_sendfile) {
		ret_t   ret;
		ssize_t sent;
		
		ret = cherokee_socket_sendfile (&conn->socket,                     /* cherokee_socket_t *socket */
						fhdl->fd,                          /* int                fd     */
						conn->range_end - fhdl->offset,    /* size_t             size   */
						&fhdl->offset,                     /* off_t             *offset */
						&sent);                            /* ssize_t           *sent   */

		/* cherokee_handler_file_init() activated the TCP_CORK flags.
		 * After it, the header was sent.  And now, the first
		 * chunk of the file with sendfile().  It's time to turn
		 * off again the TCP_CORK flag
		 */
		if (conn->options & conn_op_tcp_cork) {
			cherokee_connection_set_cork (conn, false);
		}

		if (ret == ret_no_sys) {
			fhdl->using_sendfile = false;
			goto exit_sendfile;
		}

		if (ret < ret_ok) return ret;

		/* This connection is not using the cherokee_connection_send() method,
		 * so we have to update the connection traffic counter here.
		 */
		cherokee_connection_tx_add (conn, sent);

		if (fhdl->offset >= conn->range_end) {
			return ret_eof;
		}

		return ret_ok_and_sent;
	}

exit_sendfile:
#endif

	/* Check the amount to read
	 */
	if ((fhdl->offset + buffer->size) > conn->range_end) {
		size = conn->range_end - fhdl->offset + 1;
	} else {
		/* Align read size on a 4 byte limit
		 */
		size = (buffer->size & ~3);
	}

	/* Read
	 */
	total = read (fhdl->fd, buffer->buf, size);	
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

