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
#include "sha1.h"
#include "md5crypt.h"

#define CRYPT_SALT_LENGTH 2


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
	VALIDATOR(n)->support = http_auth_basic;

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



#if !defined(HAVE_CRYPT_R) && defined(HAVE_PTHREAD)
# define USE_CRYPT_R_EMU
static pthread_mutex_t __global_crypt_r_emu_mutex = PTHREAD_MUTEX_INITIALIZER;

static ret_t
crypt_r_emu (const char *key, const char *salt, char *compared)
{
	ret_t  ret;
	char  *tmp;

	CHEROKEE_MUTEX_LOCK(&__global_crypt_r_emu_mutex);
	tmp = crypt (key, salt);
	ret = (strcmp (tmp, compared) == 0) ? ret_ok : ret_deny;
	CHEROKEE_MUTEX_UNLOCK(&__global_crypt_r_emu_mutex);	

	return ret;
}

#endif


ret_t
check_crypt (char *passwd, char *salt, const char *compared)
{
	ret_t  ret;
	char  *tmp;

#ifndef HAVE_PTHREAD
	tmp = crypt (passwd, salt);
	ret = (strcmp (tmp, compared) == 0) ? ret_ok : ret_deny;
#else
# ifdef HAVE_CRYPT_R
	struct crypt_data data;

	memset (&data, 0, sizeof(data));
	tmp = crypt_r (passwd, salt, &data);
	ret = (strcmp (tmp, compared) == 0) ? ret_ok : ret_deny;

# elif defined(USE_CRYPT_R_EMU)
	ret = crypt_r_emu (passwd, salt, compared);
# else
# error "No crypt() implementation found"
# endif
#endif

	return ret;
}



static ret_t
validate_plain (cherokee_connection_t *conn, const char *crypted)
{
	if (cherokee_buffer_is_empty (&conn->validator->passwd))
		return ret_error;

	return (strcmp (conn->validator->passwd.buf, crypted) == 0) ? ret_ok : ret_error;
}


static ret_t
validate_crypt (cherokee_connection_t *conn, const char *crypted)
{
	ret_t ret;
	char  salt[CRYPT_SALT_LENGTH];

	if (cherokee_buffer_is_empty (&conn->validator->passwd))
		return ret_error;

	memcpy (salt, crypted, CRYPT_SALT_LENGTH);

	ret = check_crypt (conn->validator->passwd.buf, salt, crypted);

	return ret;
}


static ret_t
validate_md5 (cherokee_connection_t *conn, const char *magic, char *crypted)
{
	ret_t  ret;
	char  *new_md5_crypt;
	char   space[120];

	new_md5_crypt = md5_crypt (conn->validator->passwd.buf, crypted, magic, space);
	if (new_md5_crypt == NULL)
		return ret_error;
	
	ret = (strcmp (new_md5_crypt, crypted) == 0) ? ret_ok : ret_error;

	/* There is no need to free new_md5_crypt, it is 'space' (in the stack)..
	 */
	return ret;
}


static ret_t
validate_non_salted_sha (cherokee_connection_t *conn, char *crypted)
{	
	cherokee_buffer_t sha1_client = CHEROKEE_BUF_INIT;
	cuint_t           c_len       = strlen (crypted);

	/* Check the size. It should be: "{SHA1}" + Base64(SHA1(info))
	 */
	if (c_len != 28)
		return ret_error;

	if (cherokee_buffer_is_empty (&conn->validator->passwd))
		return ret_error;
 
	/* Decode user
	 */ 
	cherokee_buffer_add_buffer (&sha1_client, &conn->validator->passwd);
	cherokee_buffer_encode_sha1_base64 (&sha1_client);

	if (strcmp (sha1_client.buf, crypted) == 0)
		return ret_ok;

	return ret_error;
}


static ret_t
request_isnt_passwd_file (cherokee_validator_htpasswd_t *htpasswd, cherokee_connection_t *conn)
{
	ret_t   ret;
	cuint_t file_ref_len;

	cherokee_buffer_add (conn->local_directory, conn->request->buf+1, conn->request->len-1);  /* 1: add    */

	ret = ret_ok;
	file_ref_len = strlen (htpasswd->file_ref);

	if (file_ref_len == conn->local_directory->len) {
		ret = (strncmp (htpasswd->file_ref, 
				conn->local_directory->buf, 
				conn->local_directory->len) == 0) ? ret_error : ret_ok;
	}

	cherokee_buffer_drop_endding (conn->local_directory, conn->request->len-1);              /* 1: remove */

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

		/* Check the type of the crypted password:
		 * It recognizes: Apache MD5, MD5, SHA, old crypt and plain text
		 */
		if (strncmp (cryp, "$apr1$", 6) == 0) {
			const char *magic = "$apr1$";
			ret_auth = validate_md5 (conn, magic, cryp);
			
		} else if (strncmp (cryp, "$1$", 3) == 0) {
			const char *magic = "$1$";
			ret_auth = validate_md5 (conn, magic, cryp);

		} else if (strncmp (cryp, "{SHA}", 5) == 0) {
			ret_auth = validate_non_salted_sha (conn, cryp + 5);

		} else if (cryp_len == 13) {
			ret_auth = validate_crypt (conn, cryp);

			if (ret_auth != ret_ok)
				ret_auth = validate_plain (conn, cryp);

		} else {
			ret_auth = validate_plain (conn, cryp);
		}

		if (ret_auth == ret_ok)
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
	ret = request_isnt_passwd_file (htpasswd, conn);	
	if (ret != ret_ok) return ret;

	return ret_ok;
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
