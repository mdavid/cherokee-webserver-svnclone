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
#include "validator_htdigest.h"
#include "module_loader.h"
#include "connection-protected.h"


cherokee_module_info_t MODULE_INFO(htdigest) = {
	cherokee_validator,                /* type     */
	cherokee_validator_htdigest_new    /* new func */
};

ret_t 
cherokee_validator_htdigest_new (cherokee_validator_htdigest_t **htdigest, cherokee_table_t *properties)
{
	CHEROKEE_NEW_STRUCT(n,validator_htdigest);

	/* Init 	
	 */
	cherokee_validator_init_base (VALIDATOR(n));
	VALIDATOR(n)->support = http_auth_basic | http_auth_digest;

	MODULE(n)->free           = (module_func_free_t)           cherokee_validator_htdigest_free;
	VALIDATOR(n)->check       = (validator_func_check_t)       cherokee_validator_htdigest_check;
	VALIDATOR(n)->add_headers = (validator_func_add_headers_t) cherokee_validator_htdigest_add_headers;

	/* Properties
	 */
	n->file_ref = NULL;

	/* Get the properties
	 */
	if (properties) {
		cherokee_typed_table_get_str (properties, "file", &n->file_ref);
	}
	
	if (n->file_ref == NULL) {
		PRINT_ERROR_S ("htdigest validator needs a \"File\" property\n");
	}

	*htdigest = n;
	return ret_ok;
}


ret_t 
cherokee_validator_htdigest_free (cherokee_validator_htdigest_t *htdigest)
{
	cherokee_validator_free_base (VALIDATOR(htdigest));
	return ret_ok;
}

static ret_t
build_HA1 (cherokee_connection_t *conn, cherokee_buffer_t *buf)
{
	cherokee_buffer_add_va (buf, "%s:%s:%s", conn->user.buf, conn->realm_ref->buf, conn->passwd.buf);
	cherokee_buffer_encode_md5_digest (buf);
	return ret_ok;
}

static ret_t
read_data_from_line (char *line, char *uid, char **user, char **realm, char **passwd)
{
	/* Read line
	 */
	while ((*line == '\r') || (*line == '\n')) line++;
	*user = line;
		
	*realm = strchr(line, ':');
	if (!*realm) return ret_error;
	*realm++;
	
	*passwd = strchr (*realm, ':');
	if (!*passwd) return ret_error;
	*passwd++;
	
	/* Look for the right user
	 */
	if (strncmp (*user, uid, (*realm-*user)-1) != 0)
		return ret_not_found;

	return ret_ok;
}


static ret_t
validate_basic (cherokee_validator_htdigest_t *htdigest, cherokee_connection_t *conn, cherokee_buffer_t *file)
{
	ret_t  ret;
	char  *line;
	char  *user, *realm, *passwd;
	cherokee_buffer_t *ha1 = NULL;
		
	/* Look for the user entry
	 */
	line = file->buf;
	do {
		cherokee_boolean_t equal;

		ret = read_data_from_line (line, conn->user.buf, &user, &realm, &passwd);
		if (ret != ret_ok) continue;

		printf ("user: '%s', realm: '%s', passwd: '%s'\n", user, realm, passwd);

                /* This is the right line
		 */
		cherokee_buffer_new (&ha1);
		build_HA1 (conn, ha1);
		equal = (strncmp(ha1->buf, passwd, ha1->len) == 0);
		cherokee_buffer_free (ha1);

		if (equal) return ret_ok;

	} while ((line = strchr(line, '\n')) != NULL);

	return ret_not_found;
}


static ret_t
validate_digest (cherokee_validator_htdigest_t *htdigest, cherokee_connection_t *conn, cherokee_buffer_t *file)
{
	
	return ret_not_found;
}


ret_t 
cherokee_validator_htdigest_check (cherokee_validator_htdigest_t *htdigest, cherokee_connection_t *conn)
{
	ret_t  ret;
	CHEROKEE_NEW(file, buffer);

	/* Ensure that we have all what we need
	 */
	if (cherokee_buffer_is_empty (&conn->user)) 
		return ret_error;

	if (htdigest->file_ref == NULL)
		return ret_error;

	/* Read the whole file
	 */
	ret = cherokee_buffer_read_file (file, htdigest->file_ref);
	if (ret != ret_ok) {
		ret = ret_error;
		goto out;
	}

	/* Authenticate
	 */
	if (conn->req_auth_type & http_auth_basic) {
		ret = validate_basic (htdigest, conn, file);

	} else if (conn->req_auth_type & http_auth_basic) {
		ret = validate_digest (htdigest, conn, file);

	} else {
		SHOULDNT_HAPPEN;
	}

out:
	cherokee_buffer_free (file);
	return ret;
}


ret_t 
cherokee_validator_htdigest_add_headers (cherokee_validator_htdigest_t *plain, cherokee_connection_t *conn, cherokee_buffer_t *buf)
{
	return ret_ok;
}



/* Library init function
 */
static cherokee_boolean_t _htdigest_is_init = false;

void
MODULE_INIT(htdigest) (cherokee_module_loader_t *loader)
{
	/* Is init?
	 */
	if (_htdigest_is_init) return;
	_htdigest_is_init = true;
}
