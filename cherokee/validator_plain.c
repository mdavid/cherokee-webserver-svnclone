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

#include "common.h"
#include "validator_plain.h"

cherokee_module_info_t cherokee_plain_info = {
	cherokee_validator,             /* type     */
	cherokee_validator_plain_new    /* new func */
};


ret_t 
cherokee_validator_plain_new (cherokee_validator_plain_t **plain, cherokee_table_t *properties)
{
	CHEROKEE_NEW_STRUCT(n,validator_plain);

	/* Init 		
	 */
	cherokee_validator_init_base (VALIDATOR(n));

	MODULE(n)->free     = (module_func_free_t)     cherokee_validator_plain_free;
	VALIDATOR(n)->check = (validator_func_check_t) cherokee_validator_plain_check;
	   
	n->plain_file_ref = NULL;

	/* Get the properties
	 */
	if (properties) {
		ret_t ret;
			 
		ret = cherokee_table_get (properties, "file", (void **) &n->plain_file_ref);
		if (ret < ret_ok) {
			PRINT_ERROR_S ("plain validator needs a \"File\" property\n");
			return ret_error;
		}
	}
	   
	*plain = n;
	return ret_ok;
}


ret_t 
cherokee_validator_plain_free (cherokee_validator_plain_t *plain)
{
	free (plain);
	return ret_ok;
}


ret_t 
cherokee_validator_plain_check (cherokee_validator_plain_t *plain, cherokee_connection_t *conn)
{
	FILE  *f;
	ret_t  ret;
	int    len;
	char  *pass;
	CHEROKEE_TEMP(line, 128);

        if (cherokee_buffer_is_empty(conn->user)) {
                return ret_error;
        }

	f = fopen (plain->plain_file_ref, "r");
	if (f == NULL) {
		return ret_error;
	}

	ret = ret_error;
	while (!feof(f)) {
		if (fgets (line, line_size, f) == NULL)
			continue;

		len = strlen (line);

		if (len <= 3) 
			continue;
			 
		if (line[0] == '#')
			continue;

		if (line[len-1] == '\n') 
			line[len-1] = '\0';
			 
		/* Split into user and encrypted password. 
		 */
		pass = strchr (line, ':');
		if (pass == NULL) continue;
		*pass++ = '\0';
			 
		/* Is this the right user? 
		 */
		if (strcmp (conn->user->buf, line) != 0) {
			continue;
		}
		
		/* Check the password
		 */
		if (strcmp (conn->passwd->buf, pass) == 0) {
			ret = ret_ok;
			break;
		}
	}

	fclose(f);

	return ret;
}

