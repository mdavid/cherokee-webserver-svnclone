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

/* Mini-Howto compile PHP5:
 *
 * $ ./configure ./configure --with-mysql=/usr/include/mysql/ --with-mysql-sock=/var/run/mysqld/mysqld.sock && make
 * $ sudo cp sapi/cgi/php /usr/lib/cgi-bin/php5
 */

#include "common-internal.h"
#include "handler_phpcgi.h"

#ifdef HAVE_SYS_TYPES_H
# include <sys/types.h>
#endif 

#ifdef HAVE_SYS_STAT_H
# include <sys/stat.h>
#endif

#ifdef HAVE_UNISTD_H
# include <unistd.h>
#endif

#ifdef HAVE_STDLIB_H
# include <stdlib.h>
#endif

#include "module.h"
#include "module_loader.h"
#include "handler_cgi.h"
#include "connection.h"
#include "connection-protected.h"


static char *php_paths[] = {
	"/usr/lib/cgi-bin/",
	"/usr/local/bin/",
	NULL
};

static char *php_names[] = {
	"php", 
	"php5", 
	"php4", 
	"php3",
	NULL
};


cherokee_module_info_t MODULE_INFO(phpcgi) = {
	cherokee_handler,           /* type     */
	cherokee_handler_phpcgi_new /* new func */
};


static ret_t check_interpreter (char *path);


static ret_t
search_php_executable (char **ret_path)
{
	cuint_t            npath;
	cuint_t            nname;
	cherokee_buffer_t  tmppath = CHEROKEE_BUF_INIT;

	for (npath = 0; php_paths[npath]; npath++) {
		for (nname = 0; php_names[nname]; nname++) {
			int re;

			cherokee_buffer_add_va (&tmppath, "%s%s", php_paths[npath], php_names[nname]);
			re = access (tmppath.buf, R_OK | X_OK);
			
			if (re == 0) {
				*ret_path = strdup (tmppath.buf);
				goto out;
			}

			cherokee_buffer_clean (&tmppath);
		}
	}

out:
	cherokee_buffer_mrproper (&tmppath);
	return ret_ok;
}


static ret_t
check_interpreter (char *path)
{
	int re;
	
	/* Sanity check
	 */
	if (path == NULL)
		return ret_not_found;

	/* Check for the PHP executable 
	 */
	re = access (path, R_OK | X_OK);
	if (re != 0) {
		return ret_not_found;
	}

	return ret_ok;
}


ret_t 
cherokee_handler_phpcgi_new  (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	ret_t  ret;
	char  *interpreter = NULL;

	/* Create the new handler CGI object
	 */
	ret = cherokee_handler_cgi_new (hdl, cnt, properties);
	if (unlikely(ret != ret_ok)) return ret;
	   
	/* Redefine the init method
	 */
	MODULE(*hdl)->init = (handler_func_init_t) cherokee_handler_phpcgi_init;

	/* Look for the interpreter in the properties
	 */
	if (properties) {
		cherokee_typed_table_get_str (properties, "interpreter", &interpreter);
	}

	if (interpreter == NULL) 
		search_php_executable (&interpreter);

	/* Check the interpreter
	 */
	if (check_interpreter(interpreter) != ret_ok) {
		PRINT_ERROR ("ERROR: PHP interpreter not found (%s). Please install it.\n", interpreter);
		return ret_error;
	}

	/* Set it up in the CGI handler
	 */
	if (CGIHANDLER(*hdl)->filename == NULL) {
		cherokee_buffer_new (&CGIHANDLER(*hdl)->filename);
		cherokee_buffer_add ( CGIHANDLER(*hdl)->filename, interpreter, strlen(interpreter));
	}	
	
	/* If it has to fake the effective directory, set the -C paramter:
	 * Do not chdir to the script's directory
	 */
	if (!cherokee_buffer_is_empty (&CONN(cnt)->effective_directory)) {
		cherokee_handler_cgi_add_parameter (CGIHANDLER(*hdl), "-C");
	}

	return ret_ok;
}


ret_t 
cherokee_handler_phpcgi_init (cherokee_handler_t *hdl)
{
	cherokee_connection_t *conn = HANDLER_CONN(hdl);

	/* Special case:
	 * The CGI handler could return a ret_eagain value, so the connection
	 * will keep trying call this funcion.  The right action on this case
	 * is to call again the CGI handler
	 */
	if (CGIHANDLER(hdl)->init_phase != hcgi_phase_init) {
		return cherokee_handler_cgi_init (CGIHANDLER(hdl));
	}

	/* Add parameter to CGI handler
	*/
	if (CGIHANDLER(hdl)->parameter == NULL) {
		cherokee_buffer_t *ld = &conn->local_directory;
		
		cherokee_buffer_new (&CGIHANDLER(hdl)->parameter);
		cherokee_buffer_add (CGIHANDLER(hdl)->parameter, ld->buf, ld->len - 1);
		cherokee_buffer_add_buffer (CGIHANDLER(hdl)->parameter, &conn->request);
		
		cherokee_handler_cgi_split_pathinfo (CGIHANDLER(hdl), 
						     CGIHANDLER(hdl)->parameter,
						     ld->len + 1);
	}
	
	cherokee_handler_cgi_add_env_pair (CGIHANDLER(hdl), "REDIRECT_STATUS", 15, "200", 3); 
	cherokee_handler_cgi_add_env_pair (CGIHANDLER(hdl), "SCRIPT_FILENAME", 15,
					   CGIHANDLER(hdl)->parameter->buf, CGIHANDLER(hdl)->parameter->len);	

	return cherokee_handler_cgi_init (CGIHANDLER(hdl));
}


/*   Library init function
 */
static cherokee_boolean_t _phpcgi_is_init = false;

void
MODULE_INIT(phpcgi) (cherokee_module_loader_t *loader)
{
	/* Is init?
	 */
	if (_phpcgi_is_init) return;
	_phpcgi_is_init = true;
	
	/* Load the dependences
	 */
	cherokee_module_loader_load (loader, "cgi");
}


