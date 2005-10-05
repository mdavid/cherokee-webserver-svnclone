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
#include "validator_htpasswd.h"

#ifdef HAVE_CRYPT_H
# include <crypt.h>
#endif

#include "module_loader.h"
#include "connection.h"
#include "connection-protected.h"


#if !defined(HAVE_CRYPT_R) && defined(HAVE_PTHREAD)
# define USE_CRYPT_R_EMU
#endif


/* How to recognizing the Crypt Mechanism: 
 * Passwords encrypted with the MD5 hash are longer than those
 * encrypted with the DES hash and also begin with the characters
 * $1$. Passwords starting with $2$ are encrypted with the Blowfish
 * hash function. DES password strings do not have any particular
 * identifying characteristics, but they are shorter than MD5
 * passwords, and are coded in a 64-character alphabet which does not
 * include the $ character, so a relatively short string which does
 * not begin with a dollar sign is very likely a DES password.
 */

cherokee_module_info_t MODULE_INFO(htpasswd) = {
	cherokee_validator,                /* type     */
	cherokee_validator_htpasswd_new    /* new func */
};

ret_t 
cherokee_validator_htpasswd_new (cherokee_validator_htpasswd_t **htpasswd, cherokee_table_t *properties)
{	  
	CHEROKEE_NEW_STRUCT(n,validator_htpasswd);

	/* Init 	
	 */
	cherokee_validator_init_base (VALIDATOR(n));
	MODULE(n)->free           = (module_func_free_t)           cherokee_validator_htpasswd_free;
	VALIDATOR(n)->check       = (validator_func_check_t)       cherokee_validator_htpasswd_check;
	VALIDATOR(n)->add_headers = (validator_func_add_headers_t) cherokee_validator_htpasswd_add_headers;
	   
	n->file_ref = NULL;

	/* Get the properties
	 */
	if (properties) {
		cherokee_typed_table_get_str (properties, "file", (char **)&n->file_ref);
	}

	if (n->file_ref == NULL) {
		PRINT_ERROR_S ("htdigest validator needs a \"File\" property\n");
	}

	*htpasswd = n;
	return ret_ok;
}


ret_t 
cherokee_validator_htpasswd_free (cherokee_validator_htpasswd_t *htpasswd)
{
	cherokee_validator_free_base (VALIDATOR(htpasswd));
	return ret_ok;
}




#ifdef USE_CRYPT_R_EMU
static pthread_mutex_t __global_crypt_r_emu_mutex = PTHREAD_MUTEX_INITIALIZER;

static char *
crypt_r_emu (const char *key, const char *salt)
{
	char *ret;
	char *tmp_crypt;

	CHEROKEE_MUTEX_LOCK(&__global_crypt_r_emu_mutex);
	tmp_crypt = crypt (key, salt);
	ret = strdup (tmp_crypt);
	CHEROKEE_MUTEX_UNLOCK(&__global_crypt_r_emu_mutex);	

	return ret;
}
#endif




static ret_t
validate_crypt (cherokee_connection_t *conn, const char *crypted)
{
	ret_t ret;
	char *tmp;
	char *passwd = "";

	if (conn->validator && (! cherokee_buffer_is_empty (&conn->validator->passwd))) {
		passwd = conn->validator->passwd.buf;
	}

#if defined(HAVE_CRYPT_R) && defined(HAVE_PTHREAD)
	{
		struct crypt_data data;

		memset (&data, 0, sizeof(data));
		tmp = crypt_r (passwd, crypted, &data);
		ret = (strcmp(tmp, crypted) == 0) ? ret_ok : ret_error;
	}

#elif defined(USE_CRYPT_R_EMU)
	tmp = crypt_r_emu (passwd, crypted);
	ret = (strcmp(tmp, crypted) == 0) ? ret_ok : ret_error;
	free (tmp);

#elif defined(HAVE_CRYPT)
	tmp = crypt (passwd, crypted);
	ret = (strcmp(tmp, crypted) == 0) ? ret_ok : ret_error;
#else
# error "No crypt() implementation found"
#endif

	return ret;
}

static ret_t
validate_md5_apache (cherokee_connection_t *conn, const char *crypted)
{
	/* Format:
	 * "$apr1$..salt..$.......md5hash..........\0" 
	 */
	// char out_buf[6 + 9 + 24 + 2]; 

	PRINT_ERROR_S ("Apache special MD5 is still not supported\n");
	return ret_error;
}


static ret_t
validate_md5_digest (cherokee_connection_t *conn, char *crypted)
{	ret_t ret;
	cherokee_buffer_t *tmp;
	
	cherokee_buffer_new (&tmp);
	cherokee_buffer_add_buffer (tmp, &conn->validator->passwd);
	cherokee_buffer_encode_md5_digest (tmp);

	ret = (strcmp(crypted, tmp->buf)) ? ret_error : ret_ok;

	cherokee_buffer_free (tmp);

	return ret;
}

static ret_t
validate_md5 (cherokee_connection_t *conn, char *crypted)
{
	ret_t ret;
	cherokee_buffer_t *tmp;
	
	cherokee_buffer_new (&tmp);

	cherokee_buffer_add_buffer (tmp, &conn->validator->user);
	cherokee_buffer_add (tmp, ":", 1);
	cherokee_buffer_add_buffer (tmp, &conn->validator->passwd);
	printf ("1->'%s'\n", tmp->buf);

	cherokee_buffer_encode_md5_digest (tmp);
//	printf ("2->'%s'\n", tmp->buf);
//	cherokee_buffer_encode_hex (tmp);
//	printf ("3->'%s'\n", tmp->buf);

	{
		cherokee_buffer_t *n;

		cherokee_buffer_new (&n);
		cherokee_buffer_add (n, crypted, strlen(crypted));
		cherokee_buffer_encode_hex (n);
		printf ("crypted->hex: %s\n", n->buf);
		cherokee_buffer_clean (n);

		cherokee_buffer_add (n, crypted, strlen(crypted));
		cherokee_buffer_encode_md5_digest (n);
		cherokee_buffer_encode_hex (n);
		printf ("crypted->md5->hex: %s\n", n->buf);
		cherokee_buffer_clean (n);
	}

	printf ("'%s' == '%s'\n", crypted, tmp->buf);
	ret = (strcmp(crypted, tmp->buf) == 0) ? ret_ok : ret_error;
	
	cherokee_buffer_free (tmp);

	return ret;
}


ret_t 
cherokee_validator_htpasswd_check (cherokee_validator_htpasswd_t *htpasswd, cherokee_connection_t *conn)
{
	FILE *f;
	int   len;
	char *cryp;
	int   cryp_len;
	ret_t ret;
	ret_t ret_auth;
	CHEROKEE_TEMP(line, 128);

	if ((conn->validator == NULL) || cherokee_buffer_is_empty (&conn->validator->user)) {
		return ret_error;
	}

	/* 1.- Check the login/passwd
	 */	  
	f = fopen (htpasswd->file_ref, "r");
	if (f == NULL) {
		return ret_error;
	}

	ret = ret_error;
	while (!feof(f)) {
		if (fgets (line, line_size, f) == NULL)
			continue;

		len = strlen (line);

		if (len <= 0) 
			continue;

		if (line[0] == '#')
			continue;

		if (line[len-1] == '\n') 
			line[len-1] = '\0';
			 
		/* Split into user and encrypted password. 
		 */
		cryp = strchr (line, ':');
		if (cryp == NULL) continue;
		*cryp++ = '\0';
		cryp_len = strlen(cryp);

		/* Is this the right user? 
		 */
		if (strcmp (conn->validator->user.buf, line) != 0) {
			continue;
		}

		/* Check the crypted password
		 */
		if (strncmp (cryp, "$apr1$", 6) == 0)  
		{    
			/* Apache MD5 variant
			 */
			ret_auth = validate_md5_apache (conn, cryp);
		} 
		else if (cryp_len == 32) 
		{   
			ret_auth = validate_md5_digest (conn, cryp);
		} 
		else {
			ret_auth = validate_crypt (conn, cryp);
		}

		break;
	}

	fclose(f);

	/* Check the authentication returned value
	 */
	if (ret_auth < ret_ok) {
		return ret_auth;
	}


	/* 2.- Security check:
	 * Is the client trying to download the passwd file?
	 */
	cherokee_buffer_add (conn->local_directory, conn->request->buf+1, conn->request->len-1);  /* 1: add    */

	ret = (strncmp (htpasswd->file_ref, 
			conn->local_directory->buf, 
			conn->local_directory->len) == 0) ? ret_error : ret_ok;

	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len-1);              /* 1: remove */

	if (ret < ret_ok) {
		return ret_error;
	}

	return ret;
}


ret_t 
cherokee_validator_htpasswd_add_headers (cherokee_validator_htpasswd_t *htpasswd, cherokee_connection_t *conn, cherokee_buffer_t *buf)
{
	return ret_ok;
}


/* Library init function
 */
static cherokee_boolean_t _htpasswd_is_init = false;

void
MODULE_INIT(htpasswd) (cherokee_module_loader_t *loader)
{
	/* Is init?
	 */
	if (_htpasswd_is_init) return;
	_htpasswd_is_init = true;
}
