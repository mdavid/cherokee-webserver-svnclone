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

#include "encoder_gzip.h"
#include "common-internal.h"
#include "module_loader.h"

/* Specs:
 * GZIP file format specification version 4.3:
 * RFC-1952: http://www.ietf.org/rfc/rfc1952.txt
 */

/* Links:
 * http://leknor.com/code/php/class.gzip_encode.php.txt
 */

/* Flags
 */
enum {
	GZ_ASCII_FLAG   = 0x01,
	GZ_HEAD_CRC     = 0x02,
	GZ_EXTRA_FIELD  = 0x04,
	GZ_ORIG_NAME    = 0x08,
	GZ_COMMENT      = 0x10,
	GZ_RESERVED     = 0xE0
};


/* Extra flags
 */
enum {
	GZ_SLOWEST = 2,
	GZ_FASTEST = 4
};


/* Some operating systems
 */
enum {
	OS_FAT       =   0,
	OS_UNIX      =   3,
	OS_OS2       =   6,
	OS_MACINTOSH =   7,
	OS_NTFS      =  11,
	OS_UNKNOWN   = 255
};


#define gzip_header_len 10
static char gzip_header[gzip_header_len] = {0x1F, 0x8B,   /* 16 bits: IDentification     */
					    Z_DEFLATED,   /*  b bits: Compression Method */
					    0,            /*  8 bits: FLags              */
					    0, 0, 0, 0,   /* 32 bits: Modification TIME  */
					    GZ_FASTEST,   /*  8 bits: Extra Flags        */
					    OS_UNKNOWN};  /*  8 bits: Operating System   */  

/* GZIP
 * ====
 * gzip_header (10 bytes) + [gzip_encoder_content] + crc32 (4 bytes) + length (4 bytes)
 *
 */

cherokee_module_info_t MODULE_INFO(gzip) = {
	cherokee_encoder,            /* type     */
	cherokee_encoder_gzip_new    /* new func */
};

ret_t 
cherokee_encoder_gzip_new (cherokee_encoder_gzip_t **encoder)
{
	CHEROKEE_NEW_STRUCT (n, encoder_gzip);

	/* Init 	
	 */
	cherokee_encoder_init_base (ENCODER(n));

	MODULE(n)->free         = (encoder_func_free_t) cherokee_encoder_gzip_free;
	MODULE(n)->init         = (encoder_func_encode_t) cherokee_encoder_gzip_init;
	ENCODER(n)->add_headers = (encoder_func_add_headers_t) cherokee_encoder_gzip_add_headers;
	ENCODER(n)->encode      = (encoder_func_encode_t) cherokee_encoder_gzip_encode;

	/* Return the object
	 */
	*encoder = n;
	return ret_ok;
}


ret_t 
cherokee_encoder_gzip_free (cherokee_encoder_gzip_t *encoder)
{
	free (encoder);
	return ret_ok;
}


ret_t 
cherokee_encoder_gzip_add_headers (cherokee_encoder_gzip_t *encoder,
				   cherokee_buffer_t       *buf)
{
	cherokee_buffer_add (buf, "Content-Encoding: gzip"CRLF, 24);
	return ret_ok;
}


static char *
get_gzip_error_string (int err)
{
	switch (err) {
	case Z_NEED_DICT:     return "Need dict.";
	case Z_ERRNO:         return "Errno";
	case Z_STREAM_ERROR:  return "Stream error";
	case Z_DATA_ERROR:    return "Data error";
	case Z_MEM_ERROR:     return "Memory error";
	case Z_BUF_ERROR:     return "Buffer error";
	case Z_VERSION_ERROR: return "Version error";
	default:
		SHOULDNT_HAPPEN;
	}

	return "unknown";
}


static ret_t
init_gzip_stream (z_stream *stream)
{
	int err;

	/* Comment from the PHP source code:
	 * windowBits is passed < 0 to suppress zlib header & trailer 
	 */
	err = zlib_deflateInit2 (stream, 
				 Z_BEST_SPEED, 
				 Z_DEFLATED, 
				 -MAX_WBITS, 
				 MAX_MEM_LEVEL, 
				 Z_DEFAULT_STRATEGY);

	if (err != Z_OK) {
		PRINT_ERROR("Error in deflateInit2() = %s\n", get_gzip_error_string(err));
		return ret_error;
	}

	return ret_ok;
}


ret_t 
cherokee_encoder_gzip_init (cherokee_encoder_gzip_t *encoder)
{
	return ret_ok;
}


/* "A gzip file consists of a series of "members" (compressed data
 *  sets).  The format of each member is specified in the following
 *  section.  The members simply appear one after another in the file,
 *  with no additional information before, between, or after them."
 *
 * The step() method build a "member":
 */

/* Pseudo-code with PHP:
 *
 * $t = "\x1f\x8b\x08\x00\x00\x00\x00\x00" . substr(gzcompress($t,$l),0,-4) . \
 *  pack('V',crc32($t)) . pack('V',strlen($t));
 */

ret_t 
cherokee_encoder_gzip_encode (cherokee_encoder_gzip_t *encoder, 
			      cherokee_buffer_t       *in, 
			      cherokee_buffer_t       *out)
{
	ret_t ret;
	int   err;
	uLong the_crc;
	uLong the_size;
	char  footer[8];

	/* Init the GZip stream
	 */
	ret = init_gzip_stream (&encoder->stream);
	if (unlikely(ret < ret_ok)) return ret;

	/* Set the data to be compress
	 */
	the_size = 10 + in->len + (in->len * .1) + 22; 
	ret = cherokee_buffer_ensure_size (out, the_size);
	if (unlikely(ret < ret_ok)) return ret;
	
	encoder->stream.next_in   = (void *)in->buf;
	encoder->stream.avail_in  = in->len;
	encoder->stream.next_out  = (void *)out->buf;
	encoder->stream.avail_out = out->size;

	/* Compress it
	 */
	err = zlib_deflate (&encoder->stream, Z_FINISH);
	zlib_deflateEnd (&encoder->stream);
	
	if (err != Z_STREAM_END) {
		PRINT_ERROR("Error in deflate(): err=%s avail=%d\n", 
			    get_gzip_error_string(err), encoder->stream.avail_in);
		return ret_error;		
	}

	out->len = encoder->stream.total_out;


	/* Add the GZip header:
	 * +---+---+---+---+---+---+---+---+---+---+
         * |ID1|ID2|CM |FLG|     MTIME     |XFL|OS |
         * +---+---+---+---+---+---+---+---+---+---+
	 */
        cherokee_buffer_prepend (out, gzip_header, gzip_header_len);

	/* Add the endding:
	 * +---+---+---+---+---+---+---+---+
	 * |     CRC32     |     ISIZE     |
         * +---+---+---+---+---+---+---+---+
	 */
	the_crc  = cherokee_buffer_crc32 (in);
	the_size = in->len;

	/* Build the GZip footer
	 */
	footer[0] =  the_crc         & 0xFF;
	footer[1] = (the_crc >> 8)   & 0xFF;
	footer[2] = (the_crc >> 16)  & 0xFF;
	footer[3] = (the_crc >> 24)  & 0xFF;
	footer[4] =  the_size        & 0xFF;
	footer[5] = (the_size >> 8)  & 0xFF;
	footer[6] = (the_size >> 16) & 0xFF;
	footer[7] = (the_size >> 24) & 0xFF;

	cherokee_buffer_add (out, footer, 8);
//	cherokee_buffer_add (out, (char *)&the_crc, 4);
//	cherokee_buffer_add (out, (char *)&the_size, 4);

#if 0
 	cherokee_buffer_print_debug (out, -1); 
#endif	

	return ret_ok;
}



/*   Library init function
 */

static cherokee_boolean_t _gzip_is_init = false;

void
MODULE_INIT(gzip) (cherokee_module_loader_t *loader)
{
	/* Init flag
	 */
	if (_gzip_is_init) return;
	_gzip_is_init = true;
}
