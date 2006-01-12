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

#include "handler_gnomevfs.h"
#include "connection.h"


cherokee_module_info_handler_t MODULE_INFO(gnomevfs) = {
	.module.type     = cherokee_handler,                  /* type         */
	.module.new_func = cherokee_handler_gnomevfs_new,     /* new func     */
	.valid_methods   = http_get | http_head               /* http methods */
};


ret_t 
cherokee_handler_gnomevfs_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	CHEROKEE_NEW(n, handler_gnomevfs);

	/* Init the base class object
	 */
	cherokee_handler_init_base(HANDLER(n), cnt);

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_gnomevfs_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_gnomevfs_free;
	HANDLER(n)->step        = (handler_func_step_t) cherokee_handler_gnomevfs_step;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_gnomevfs_add_headers;

	HANDLER(n)->connection  = cnt;
	HANDLER(n)->support     = hsupport_length | hsupport_range;
	
	/* Init
	 */
	cherokee_table_get (properties, "filedir", &n->filedir);
	n->filedir_len = strlen (n->filedir);

	n->vfs_handler = NULL;
	n->openning    = open_none;
	n->reading     = read_none;

	/* Return the object
	 */
	*hdl = HANDLER(n);
	return ret_ok;	
}


ret_t 
cherokee_handler_gnomevfs_free (cherokee_handler_gnomevfs_t *hdl)
{
	if (hdl->vfs_handler != NULL) {
		gnome_vfs_async_close (hdl->vfs_handler,
				       close_cb, 
				       NULL);
	}

	return ret_ok;
}


static void
open_cb (GnomeVFSAsyncHandle *handle,
	 GnomeVFSResult result,
	 cherokee_handler_gnomevfs_t *hdl)
{	
	if (result != GNOME_VFS_OK) {
		CONN(HANDLER(hdl)->connection)->error_code = http_not_found;
	}

	hdl->openning = open_finished;
}


static cherokee_buffer_t *
build_gnomevfs_uri (cherokee_handler_gnomevfs_t *hdl)
{
	char                  *protocol;
	cherokee_buffer_t     *ret;
	cherokee_connection_t *conn;

	conn = CONN(HANDLER(hdl)->connection);

	cherokee_buffer_new (&ret);
       
	protocol = conn->request->buf + conn->web_directory->len;
	if (! strncmp (protocol, "file/", 5)) {
		cherokee_buffer_add (ret, "file://", 7);
		cherokee_buffer_add (ret, hdl->filedir, hdl->filedir_len);
		cherokee_buffer_add (ret, protocol+5, strlen(protocol+5));
		
		cherokee_buffer_swap_chars (ret, '$', '#');

	} else if (! strncmp (protocol, "http/", 5)) {
		cherokee_buffer_add (ret, "http://", 7);
		cherokee_buffer_add (ret, protocol+5, strlen(protocol+5));

	} else if (! strncmp (protocol, "ftp/", 4)) {
		cherokee_buffer_add (ret, "ftp://", 6);
		cherokee_buffer_add (ret, protocol+4, strlen(protocol+4));

	} else if (! strncmp (protocol, "smb/", 4)) {
		cherokee_buffer_add (ret, "smb://", 6);
		cherokee_buffer_add (ret, protocol+4, strlen(protocol+4));
	}

	return ret;
}


ret_t 
cherokee_handler_gnomevfs_init (cherokee_handler_gnomevfs_t *hdl)
{	
	switch (hdl->openning) {
	case open_none: {
		cherokee_buffer_t *uri;

		uri = build_gnomevfs_uri (hdl);
		if (uri->len <= 0) goto error;

		hdl->openning = open_requested;
		gnome_vfs_async_open (&hdl->vfs_handler,
				      uri->buf,
				      GNOME_VFS_OPEN_READ,
				      GNOME_VFS_PRIORITY_DEFAULT,
				      open_cb,
				      hdl);

		cherokee_buffer_free (uri);
	}

	case open_requested: {
		int i = 5;

		while (g_main_context_iteration(NULL, FALSE) && i--);
		return ret_eagain;
	}
	case open_finished:
		if (CONN(HANDLER(hdl)->connection)->error_code != http_ok) {
			return ret_error;
		}
	}

	return ret_ok;


error:
	CONN(HANDLER(hdl)->connection)->error_code = http_internal_error;
	return ret_ok;
}

static void
close_cb (GnomeVFSAsyncHandle *handle,
	  GnomeVFSResult result,
	  gpointer callback_data)
{
}


static void 
read_cb (GnomeVFSAsyncHandle *handle,
	 GnomeVFSResult result,
	 gpointer buffer,
	 GnomeVFSFileSize bytes_requested,
	 GnomeVFSFileSize bytes_read,
	 cherokee_handler_gnomevfs_t *hdl)
{
	hdl->bytes_read = bytes_read;
	hdl->reading = read_finished;
}


ret_t 
cherokee_handler_gnomevfs_step (cherokee_handler_gnomevfs_t *hdl, cherokee_buffer_t *buffer)
{
	switch (hdl->reading) {
	case read_none: 
		hdl->reading = read_requested;
		gnome_vfs_async_read (hdl->vfs_handler,
				      buffer->buf,
				      buffer->size,
				      read_cb,
				      hdl);

	case read_requested: {
		int i = 5;

		while (g_main_context_iteration(NULL, FALSE) && i--);	      
		return ret_eagain;
	} 

	case read_finished:
		hdl->reading = read_none;
		buffer->len = hdl->bytes_read;
	}

	return ret_ok;
}


ret_t 
cherokee_handler_gnomevfs_add_headers (cherokee_handler_gnomevfs_t *hdl, cherokee_buffer_t *buffer)
{
	return ret_ok;
}




/*   Library init function
 */

static cherokee_boolean_t _gnomevfs_is_init = false;

void 
gnomevfs_init ()
{
	if (_gnomevfs_is_init) {
		return;
	}
	_gnomevfs_is_init = true;

	PRINT_ERROR ("WARNING: The GNOME-VFS handler is just a proof of "
		     "concept and it's not ready for production!!\n");

	gnome_vfs_init();
}
