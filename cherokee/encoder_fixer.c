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

#include "encoder_fixer.h"

ret_t 
cherokee_encoder_fixer_new (cherokee_encoder_fixer_t **encoder)
{
	   CHEROKEE_NEW_STRUCT(n, encoder_fixer);
	   
	   cherokee_encoder_init_base (ENCODER(n));

	   ENCODER(n)->free        = (encoder_func_free_t) cherokee_encoder_fixer_free;
	   ENCODER(n)->add_headers = (encoder_func_add_headers_t) cherokee_encoder_fixer_add_headers;
	   ENCODER(n)->init        = (encoder_func_encode_t) cherokee_encoder_fixer_init;
	   ENCODER(n)->encode      = (encoder_func_encode_t) cherokee_encoder_fixer_encode;

	   /* Init
	    */
	   tidyBufInit (&n->output);
	   n->tdoc = tidyCreate();
	   
	   /* Return the object
	    */
	   *encoder = n;
	   
	   return ret_ok;
}


ret_t 
cherokee_encoder_fixer_free (cherokee_encoder_fixer_t *encoder)
{
	     tidyBufFree (&encoder->output);
		tidyRelease (encoder->tdoc);

		free (encoder);
		return ret_ok;
}


ret_t 
cherokee_encoder_fixer_add_headers (cherokee_encoder_fixer_t *encoder,
							 cherokee_buffer_t        *buf)
{
	   cherokee_buffer_add (buf, "Content-Encoding: text/html\r\n", 29);
	   return ret_ok;
}


ret_t 
cherokee_encoder_fixer_init (cherokee_encoder_fixer_t *encoder, 
					    cherokee_buffer_t        *in, 
					    cherokee_buffer_t        *out)
{
	   return ret_ok;
}


ret_t 
cherokee_encoder_fixer_encode (cherokee_encoder_fixer_t *encoder, 
						 cherokee_buffer_t        *in, 
						 cherokee_buffer_t        *out)
{
	printf ("En encoder fixer\n");


	   /* Convert to XHTML
	    */
	   tidyOptSetBool (encoder->tdoc, TidyXhtmlOut, yes);
	
	   /* Parse the input
	    */
	   tidyParseString (encoder->tdoc, in->buf);
	   
	   /* Tidy it up!
	    */
	   tidyCleanAndRepair (encoder->tdoc);

	   /* Pretty Print
	    */
	   tidyOptSetBool (encoder->tdoc, TidyForceOutput, yes);
	   tidySaveBuffer (encoder->tdoc, &encoder->output);

	   cherokee_buffer_add (out, encoder->output.bp, encoder->output.size);

	   return ret_ok;
}



/*   Library init function
 */

static int _fixer_is_init = 0;

void
fixer_init ()
{
	/* Init flag
	 */
	if (_fixer_is_init) {
		return;
	}
	_fixer_is_init = 1;
}
