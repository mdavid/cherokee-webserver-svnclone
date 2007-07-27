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
#include "post.h"
#include "util.h"

#include <stdlib.h>
#include <unistd.h>
#include <errno.h>

#define ENTRIES "post"


ret_t 
cherokee_post_init (cherokee_post_t *post)
{
	post->type        = post_undefined;
	post->size        = 0;
	post->received    = 0;
	post->tmp_file_fd = -1;
	post->walk_offset = 0;
	
	cherokee_buffer_init (&post->info);
	cherokee_buffer_init (&post->tmp_file);
	
	return ret_ok;
}


ret_t 
cherokee_post_mrproper (cherokee_post_t *post)
{
	int re;

	post->type        = post_undefined;
	post->size        = 0;
	post->received    = 0;
	post->walk_offset = 0;
	
	if (post->tmp_file_fd != -1) {
		close (post->tmp_file_fd);
		post->tmp_file_fd = -1;
	}
	
	if (! cherokee_buffer_is_empty (&post->tmp_file)) {
		re = unlink (post->tmp_file.buf);
		if (unlikely (re != 0)) {
			char buferr[ERROR_MAX_BUFSIZE];
			PRINT_MSG ("Couldn't remove temporal post file '%s'\n", 
				   cherokee_strerror_r(errno, buferr, sizeof(buferr)));
		}

		cherokee_buffer_mrproper (&post->tmp_file);
	}

	cherokee_buffer_mrproper (&post->info);
       	
	return ret_ok;
}


ret_t 
cherokee_post_set_len (cherokee_post_t *post, off_t len)
{
	ret_t ret;

	post->type = (len > POST_SIZE_TO_DISK) ? post_in_tmp_file : post_in_memory;
	post->size = len;

	if (post->type == post_in_tmp_file) {
		cherokee_buffer_add_str (&post->tmp_file, "/tmp/cherokee_post_XXXXXX");

		/* Generate a unique name
		 */
		ret = cherokee_mkstemp (&post->tmp_file, &post->tmp_file_fd);
		if (unlikely (ret != ret_ok)) return ret;
	}

	return ret_ok;
}


int
cherokee_post_is_empty (cherokee_post_t *post)
{
	return (post->size <= 0);
}


int
cherokee_post_got_all (cherokee_post_t *post)
{
	return (post->received >= post->size);
}

ret_t 
cherokee_post_get_len (cherokee_post_t *post, off_t *len)
{
	*len = post->size;
	return ret_ok;
}


ret_t 
cherokee_post_append (cherokee_post_t *post, char *str, size_t len)
{
	cherokee_buffer_add (&post->info, str, len);
	cherokee_post_commit_buf (post, len);
	return ret_ok;
}


ret_t 
cherokee_post_commit_buf (cherokee_post_t *post, size_t size)
{
	size_t written;

	if (size <= 0)
		return ret_ok;

	switch (post->type) {
	case post_undefined:
		return ret_error;

	case post_in_memory:
		post->received += size;
		return ret_ok;
		
	case post_in_tmp_file:
		if (post->tmp_file_fd == -1)
			return ret_error;

		written = write (post->tmp_file_fd, post->info.buf, post->info.len); 
		if (written < 0) return ret_error;

		cherokee_buffer_move_to_begin (&post->info, written);
		post->received += written;

		return ret_ok;
	}

	return ret_error;
}


ret_t 
cherokee_post_walk_reset (cherokee_post_t *post)
{
	post->walk_offset = 0;

	if (post->tmp_file_fd != -1) {
		lseek (post->tmp_file_fd, 0L, SEEK_SET);
	}

	return ret_ok;
}


ret_t 
cherokee_post_walk_finished (cherokee_post_t *post)
{
	switch (post->type) {
	case post_in_memory:
		return (post->walk_offset >= post->info.len) ? ret_ok : ret_eagain;
	case post_in_tmp_file:
		return (post->walk_offset >= post->size) ? ret_ok : ret_eagain;
	default:
		break;
	}
	
	SHOULDNT_HAPPEN;
	return ret_error;
}


ret_t 
cherokee_post_walk_to_fd (cherokee_post_t *post, int fd, int *eagain_fd, int *mode)
{
	ssize_t r;
	size_t  ur;
	
	/* Sanity check
	 */
	if (fd < 0) return ret_error;

	/* Send a chunk..
	 */
	switch (post->type) {
	case post_in_memory:
		r = write (fd, post->info.buf + post->walk_offset,
			   post->info.len - post->walk_offset);
		if (r < 0) {
			if (errno == EAGAIN)
				return ret_eagain;

			TRACE(ENTRIES, "errno %d\n", errno);
			return  ret_error; 			
		}

		TRACE(ENTRIES, "wrote %d\n", r);

		post->walk_offset += r;
		return cherokee_post_walk_finished (post);

	case post_in_tmp_file:
		cherokee_buffer_ensure_size (&post->info, DEFAULT_READ_SIZE+1);

		/* Read from the temp file is needed
		 */
		if (cherokee_buffer_is_empty (&post->info)) {
			ur = read (post->tmp_file_fd, post->info.buf, DEFAULT_READ_SIZE);
			if (ur == 0) {
				/* EOF */
				return ret_ok;
			} else if (ur < 0) {
				/* Couldn't read */
				return ret_error;
			}
			
			post->info.len     = ur;
			post->info.buf[ur] = '\0';
		}

		/* Write it to the fd
		 */
		r = write (fd, post->info.buf, post->info.len);
		if (r < 0) {
			if (errno == EAGAIN) {
				if (eagain_fd) *eagain_fd = fd;
				if (mode)      *mode      = 1;
				return ret_eagain;
			}

			return ret_error;	
		} 
		else if (r == 0)
			return ret_eagain;

		cherokee_buffer_move_to_begin (&post->info, r);

		post->walk_offset += r;
		return cherokee_post_walk_finished (post);

	default:
		SHOULDNT_HAPPEN;
		return ret_error;
	}

	return ret_ok;
}


ret_t 
cherokee_post_walk_read (cherokee_post_t *post, cherokee_buffer_t *buf, cuint_t len)
{
	size_t ur;

	switch (post->type) {
	case post_in_memory:
		if (len > (post->info.len - post->walk_offset))
			len = post->info.len - post->walk_offset;

		cherokee_buffer_add (buf, post->info.buf + post->walk_offset, len);
		post->walk_offset += len;
		break;

	case post_in_tmp_file:
		cherokee_buffer_ensure_size (buf, buf->len + len + 1);

		ur = read (post->tmp_file_fd, buf->buf + buf->len, len);
		if (ur == 0) {
			/* EOF */
			return ret_ok;
		} else if (unlikely (ur < 0)) {
			/* Couldn't read */
			return ret_error;
		}

		buf->len           += ur;
		buf->buf[buf->len]  = '\0';
		post->walk_offset  += ur;
		break;

	default:
		SHOULDNT_HAPPEN;
		return ret_error;
	}

	return cherokee_post_walk_finished (post);
}

