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
#include "encoder.h"


ret_t 
cherokee_encoder_init_base (cherokee_encoder_t *enc)
{
	cherokee_module_init_base (MODULE(enc));

	enc->encode      = NULL;
	enc->add_headers = NULL;

	return ret_ok;
}


ret_t 
cherokee_encoder_free (cherokee_encoder_t *enc)
{
	if (MODULE(enc)->free) {
		return MODULE(enc)->free (enc);
	}

	return ret_error;
}


ret_t
cherokee_encoder_add_headers (cherokee_encoder_t *enc, cherokee_buffer_t *buf)
{
	if (enc->add_headers) {
		return enc->add_headers (enc, buf);
	}

	return ret_error;
}


ret_t 
cherokee_encoder_init (cherokee_encoder_t *enc, void *conn)
{
	encoder_func_init_t init_func;

	enc->conn = conn;
	init_func = (encoder_func_init_t) MODULE(enc)->init;
		
	if (init_func) {
		return init_func (enc);
	}

	return ret_error;
}

ret_t 
cherokee_encoder_encode (cherokee_encoder_t *enc, 
			 cherokee_buffer_t  *in, 
			 cherokee_buffer_t  *out)
{
	if (enc->encode) {
		return enc->encode (enc, in, out);
	}

	return ret_error;
}

