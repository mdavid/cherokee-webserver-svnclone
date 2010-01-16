/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2009 Alvaro Lopez Ortega
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "common-internal.h"
#include "post.h"

#include "connection-protected.h"
#include "util.h"

#define ENTRIES           "post"
#define HTTP_100_RESPONSE "HTTP/1.1 100 Continue" CRLF CRLF


/* Base functions
 */

ret_t
cherokee_post_init (cherokee_post_t *post)
{
	post->len               = 0;
	post->has_info          = false;
	post->encoding          = post_enc_regular;
	post->read_header_phase = cherokee_post_read_header_init;

	post->send.phase        = cherokee_post_send_phase_read;
	post->send.read         = 0;

	cherokee_buffer_init (&post->send.buffer);
	cherokee_buffer_init (&post->read_header_100cont);
	cherokee_buffer_init (&post->header_surplus);

	return ret_ok;
}

ret_t
cherokee_post_clean (cherokee_post_t *post)
{
	post->len               = 0;
	post->has_info          = false;
	post->encoding          = post_enc_regular;
	post->read_header_phase = cherokee_post_read_header_init;

	post->send.phase        = cherokee_post_send_phase_read;
	post->send.read         = 0;

	cherokee_buffer_mrproper (&post->send.buffer);
	cherokee_buffer_mrproper (&post->read_header_100cont);
	cherokee_buffer_mrproper (&post->header_surplus);

	return ret_ok;
}

ret_t
cherokee_post_mrproper (cherokee_post_t *post)
{
	cherokee_buffer_mrproper (&post->send.buffer);
	cherokee_buffer_mrproper (&post->read_header_100cont);
	cherokee_buffer_mrproper (&post->header_surplus);

	return ret_ok;
}


/* Methods
 */


static ret_t
parse_header (cherokee_post_t       *post,
	      cherokee_connection_t *conn)
{
	ret_t    ret;
	char    *info      = NULL;
	cuint_t  info_len  = 0;
	CHEROKEE_TEMP(buf, 64);

	/* RFC 2616:
	 *
	 * If a message is received with both a Transfer-Encoding
	 * header field and a Content-Length header field, the latter
	 * MUST be ignored.
	 */

	/* Check "Transfer-Encoding"
	 */
	ret = cherokee_header_get_known (&conn->header, header_transfer_encoding, &info, &info_len);
	if (ret == ret_ok) {
		if (strncasecmp (info, "chunked", MIN(info_len, 7)) == 0) {
			post->encoding = post_enc_chunked;
			return ret_ok;
		}
	}

	/* Check "Content-Length"
	 */
	ret = cherokee_header_get_known (&conn->header, header_content_length, &info, &info_len);
	if (unlikely (ret != ret_ok)) {
		conn->error_code = http_length_required;
		return ret_error;
	}

	/* Parse the POST length
	 */
	if (unlikely ((info == NULL)  ||
		      (info_len == 0) ||
		      (info_len >= buf_size)))
	{
		conn->error_code = http_bad_request;
		return ret_error;
	}

	memcpy (buf, info, info_len);
	buf[info_len] = '\0';

	/* Check: Post length >= 0
	 */
	post->len = (off_t) atoll(buf);
	if (unlikely (post->len < 0)) {
		conn->error_code = http_bad_request;
		return ret_error;
	}

	TRACE (ENTRIES, "Post claims to be %llu bytes long\n", post->len);
	return ret_ok;
}


static ret_t
reply_100_continue (cherokee_post_t       *post,
		    cherokee_connection_t *conn)
{
	ret_t  ret;
	size_t written;

	ret = cherokee_socket_bufwrite (&conn->socket, &post->read_header_100cont, &written);
	if (ret != ret_ok) {
		TRACE(ENTRIES, "Could not send a '100 Continue' response. Error=500.\n");
		return ret;
	}

	if (written >= post->read_header_100cont.len) {
		TRACE(ENTRIES, "Sent a '100 Continue' response.\n");
		return ret_ok;
	}

	TRACE(ENTRIES, "Sent partial '100 Continue' response: %d bytes\n", written);
	cherokee_buffer_move_to_begin (&post->read_header_100cont, written);

	return ret_eagain;
}


static ret_t
remove_surplus (cherokee_post_t       *post,
		cherokee_connection_t *conn)
{
	ret_t   ret;
	cuint_t header_len;
	cint_t  post_chunk_len;

	/* Check the HTTP request length
	 */
	ret = cherokee_header_get_length (&conn->header, &header_len);
	if (unlikely(ret != ret_ok)) {
		return ret;
	}

	post_chunk_len = MIN (conn->incoming_header.len - header_len, post->len);
	if (post_chunk_len <= 0) {
		return ret_ok;
	}

	TRACE (ENTRIES, "POST surplus: moving %d bytes\n", post_chunk_len);

	/* Move the surplus
	 */
	cherokee_buffer_add (&post->header_surplus,
			     conn->incoming_header.buf + header_len,
			     post_chunk_len);

	cherokee_buffer_remove_chunk (&conn->incoming_header,
				      header_len, post_chunk_len);

	TRACE (ENTRIES, "POST surplus: incoming_header is %d bytes (header len=%d)\n",
	       conn->incoming_header.len, header_len);

	return ret_ok;
}


ret_t
cherokee_post_read_header (cherokee_post_t *post,
			   void            *cnt)
{
	ret_t                  ret;
	char                  *info     = NULL;
	cuint_t                info_len = 0;
	cherokee_connection_t *conn     = CONN(cnt);

	switch (post->read_header_phase) {
	case cherokee_post_read_header_init:
		/* Read the header
		 */
		ret = parse_header (post, conn);
		if (unlikely (ret != ret_ok)) {
			return ret;
		}

		post->has_info = true;

		ret = remove_surplus (post, conn);
		if (unlikely (ret != ret_ok)) {
			return ret;
		}

		/* Expect: 100-continue
		 * http://www.w3.org/Protocols/rfc2616/rfc2616-sec8.html
		 */
		ret = cherokee_header_get_known (&conn->header, header_expect, &info, &info_len);
		if (likely (ret != ret_ok)) {
			return ret_ok;
		}

		cherokee_buffer_add_str (&post->read_header_100cont, HTTP_100_RESPONSE);
		post->read_header_phase = cherokee_post_read_header_100cont;

	case cherokee_post_read_header_100cont:
		return reply_100_continue (post, conn);
	}

	SHOULDNT_HAPPEN;
	return ret_error;
}

static ret_t
do_read (cherokee_post_t   *post,
	 cherokee_socket_t *sock_in,
	 cherokee_buffer_t *buffer)
{
	ret_t  ret;
	off_t  to_read;
	size_t len;

	/* Surplus from header read
	 */
	if (! cherokee_buffer_is_empty (&post->header_surplus)) {
		TRACE (ENTRIES, "Post appending %d surplus bytes\n", post->header_surplus.len);
		post->send.read += post->header_surplus.len;

		cherokee_buffer_add_buffer (buffer, &post->header_surplus);
		cherokee_buffer_clean (&post->header_surplus);

		return ret_ok;
	}

	/* Read
	 */
	to_read = MIN((post->len - post->send.read), POST_READ_SIZE);

	TRACE (ENTRIES, "Post reading from client (to_read=%d)\n", to_read);
	ret = cherokee_socket_bufread (sock_in, buffer, to_read, &len);
	TRACE (ENTRIES, "Post read from client: ret=%d len=%d\n", ret, len);

	if (ret != ret_ok) {
		return ret;
	}

	post->send.read += len;
	return ret_ok;
}


static ret_t
do_send_socket (cherokee_socket_t        *sock,
		cherokee_buffer_t        *buffer,
		cherokee_socket_status_t *blocking)
{
	ret_t  ret;
	size_t written = 0;

	ret = cherokee_socket_bufwrite (sock, buffer, &written);
	switch (ret) {
	case ret_ok:
		break;
	case ret_eagain:
		if (written > 0) {
			break;
		}

		*blocking = socket_writing;
		return ret_eagain;
	default:
		return ret_error;
	}

	cherokee_buffer_move_to_begin (buffer, written);
	TRACE (ENTRIES, "sent=%d, remaining=%d\n", written, buffer->len);

	if (! cherokee_buffer_is_empty (buffer)) {
		return ret_eagain;
	}

	return ret_ok;
}


ret_t
cherokee_post_send_to_socket (cherokee_post_t          *post,
			      cherokee_socket_t        *sock_in,
			      cherokee_socket_t        *sock_out,
			      cherokee_buffer_t        *tmp,
			      cherokee_socket_status_t *blocking)
{
	ret_t              ret;
	cherokee_buffer_t *buffer = tmp ? tmp : &post->send.buffer;

	switch (post->send.phase) {
	case cherokee_post_send_phase_read:
		TRACE (ENTRIES, "Post send, phase: %s\n", "read");

		ret = do_read (post, sock_in, buffer);
		switch (ret) {
		case ret_ok:
			break;
		case ret_eagain:
			*blocking = socket_reading;
			return ret_eagain;
		default:
			return ret;
		}

		TRACE (ENTRIES, "Post buffer.len %d\n", buffer->len);
		post->send.phase = cherokee_post_send_phase_write;

	case cherokee_post_send_phase_write:
		TRACE (ENTRIES, "Post send, phase: %s\n", "write");

		if (! cherokee_buffer_is_empty (buffer)) {
			ret = do_send_socket (sock_out, buffer, blocking);
			switch (ret) {
                        case ret_ok:
                                break;
                        case ret_eagain:
                                return ret_eagain;
                        case ret_eof:
                        case ret_error:
                                return ret_error;
                        default:
				RET_UNKNOWN(ret);
				return ret_error;
                        }
		}

		if (! cherokee_buffer_is_empty (buffer)) {
			return ret_eagain;
		}

		if (post->send.read < post->len) {
			post->send.phase = cherokee_post_send_phase_read;
			return ret_eagain;
		}

		TRACE (ENTRIES, "Post send: %s\n", "finished");

		cherokee_buffer_mrproper (&post->send.buffer);
		return ret_ok;

	default:
		SHOULDNT_HAPPEN;
	}

	return ret_error;
}


ret_t
cherokee_post_send_to_fd (cherokee_post_t   *post,
			  cherokee_socket_t *sock_in,
			  int                fd,
			  int               *fd_eagain,
			  cherokee_buffer_t *tmp)
{
	ret_t              ret;
	int                r;
	cherokee_buffer_t *buffer = tmp ? tmp : &post->send.buffer;


	switch (post->send.phase) {
	case cherokee_post_send_phase_read:
		TRACE (ENTRIES, "Post send, phase: %s\n", "read");

		ret = do_read (post, sock_in, buffer);
		if (ret != ret_ok) {
			return ret;
		}

		TRACE (ENTRIES, "Post buffer.len %d\n", buffer->len);
		post->send.phase = cherokee_post_send_phase_write;

	case cherokee_post_send_phase_write:
		TRACE (ENTRIES, "Post send, phase: write. Has %d bytes to send\n", buffer->len);

		if (! cherokee_buffer_is_empty (buffer)) {
			r = write (fd, buffer->buf, buffer->len);
			if (r < 0) {
				if (errno == EAGAIN) {
					if (fd_eagain) {
						*fd_eagain = fd;
					}
					return ret_eagain;
				}

				TRACE(ENTRIES, "errno %d: %s\n", errno, strerror(errno));
				return ret_error;

			} else if (r == 0) {
				return ret_eagain;
			}

			cherokee_buffer_move_to_begin (buffer, r);
		}

		if (! cherokee_buffer_is_empty (buffer)) {
			return ret_eagain;
		}

		if (post->send.read < post->len) {
			post->send.phase = cherokee_post_send_phase_read;
			return ret_eagain;
		}

		TRACE (ENTRIES, "Post send: %s\n", "finished");

		cherokee_buffer_mrproper (&post->send.buffer);
		return ret_ok;

	default:
		SHOULDNT_HAPPEN;
	}

	return ret_error;
}


ret_t
cherokee_post_send_reset (cherokee_post_t *post)
{
	post->send.read  = 0;
	post->send.phase = cherokee_post_send_phase_read;

	return ret_ok;
}
