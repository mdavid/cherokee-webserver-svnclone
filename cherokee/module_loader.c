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

#include "module_loader.h"
#include "module_loader-protected.h"

#include "table.h"
#include "buffer.h"

#include <stdlib.h>
#include <string.h>
#include <dlfcn.h>


#ifdef CHEROKEE_EMBEDDED

ret_t
cherokee_module_loader_new  (cherokee_module_loader_t **loader)
{
	*loader = NULL;
	return ret_ok;
}

ret_t 
cherokee_module_loader_free (cherokee_module_loader_t *loader)
{  
	return ret_ok; 
}

ret_t 
cherokee_module_loader_load (cherokee_module_loader_t *loader, char *modname)
{
	if      (strcmp(modname, "common")  == 0) common_init (NULL);
	else if (strcmp(modname, "file")    == 0) file_init (NULL);
	else if (strcmp(modname, "dirlist") == 0) dirlist_init (NULL);
	else if (strcmp(modname, "cgi")     == 0) cgi_init (NULL);
	else if (strcmp(modname, "phpcgi")  == 0) phpcgi_init (NULL);
	else return ret_error;

	return ret_ok;
}

ret_t 
cherokee_module_loader_get (cherokee_module_loader_t *loader, char *modname, cherokee_module_info_t **info)
{
	extern cherokee_module_info_t cherokee_common_info;
	extern cherokee_module_info_t cherokee_file_info;
	extern cherokee_module_info_t cherokee_dirlist_info;
	extern cherokee_module_info_t cherokee_cgi_info;
	extern cherokee_module_info_t cherokee_phpcgi_info;

	if      (strcmp(modname, "common")  == 0) *info = &cherokee_common_info;
	else if (strcmp(modname, "file")    == 0) *info = &cherokee_file_info;
	else if (strcmp(modname, "dirlist") == 0) *info = &cherokee_dirlist_info;
	else if (strcmp(modname, "cgi")     == 0) *info = &cherokee_cgi_info;
	else if (strcmp(modname, "phpcgi")  == 0) *info = &cherokee_phpcgi_info;
	else return ret_error;

	return ret_ok;
}

#else 

/* This is the non-embedded implementation
 */

#include "loader.autoconf.h"

#ifdef HAVE_RTLDNOW	
# define RTLD_BASE RTLD_NOW
#else
# define RTLD_BASE RTLD_LAZY
#endif

#ifdef HAVE_RTLDGLOBAL
# define RTLD_OPTIONS 	| RTLD_GLOBAL;
#else 
# define RTLD_OPTIONS
#endif

typedef void *func_new_t;


static ret_t
load_static_linked_modules (cherokee_module_loader_t *loader)
{
/* Yeah, yeah.. I know, it's horrible. :-(
 * If you know a better way to manage dynamic/static modules,
 * please, let me know.
 */
#include "loader.autoconf.inc"

	return ret_ok;
}


ret_t
cherokee_module_loader_new  (cherokee_module_loader_t **loader)
{
	ret_t ret;
	
	CHEROKEE_NEW_STRUCT (n, module_loader);
	
	ret = cherokee_table_new(&n->table);
	if (unlikely(ret < ret_ok)) return ret;
	
	ret = load_static_linked_modules (n);
	if (unlikely(ret < ret_ok)) return ret;

	*loader = n;
	return ret_ok;
}


ret_t 
cherokee_module_loader_free (cherokee_module_loader_t *loader)
{
	cherokee_table_free (loader->table);

	free (loader);
	return ret_ok;
}


static void *
get_sym_from_enviroment (const char *sym)
{
	return dlsym (NULL, sym);
}


static void *
get_sym_from_dlopen_handler (void *dl_handle, const char *sym)
{
	void *re;
	   
	/* Get the sym
	 */
	re = (void *) dlsym(dl_handle, sym);
	if (re == NULL) {
		PRINT_ERROR ("ERROR: %s\n", dlerror());
		return NULL;
	}
	
	return re;
}

static ret_t
dylib_open (const char *libname, void **handler_out) 
{
	ret_t  ret;
	void  *lib;
	int    flags;
	CHEROKEE_NEW(tmp, buffer);
	
	flags = RTLD_BASE RTLD_OPTIONS;

	/* Build the path string
	 */
	ret = cherokee_buffer_add_va (tmp, CHEROKEE_PLUGINDIR "/libplugin_%s." SO_SUFFIX, libname);
	if (unlikely(ret < ret_ok)) return ret;
	
	/* Open the library	
	 */
	lib = dlopen (tmp->buf, flags);
	if (lib == NULL) {
		PRINT_ERROR ("ERROR: dlopen(): %s\n", dlerror());
		goto error;
	}

	/* Free the memory
	 */
	cherokee_buffer_free (tmp);

	*handler_out = lib;
	return ret_ok;

error:
	cherokee_buffer_free(tmp);
	return ret_error;
}


static ret_t
execute_init_func (cherokee_module_loader_t *loader, const char *module, void *dl_handler)
{
	ret_t ret;
	void (*init_func) (cherokee_module_loader_t *);
	CHEROKEE_NEW(init_name, buffer);
	
	/* Build the init function name
	 */
	ret = cherokee_buffer_add_va (init_name, "%s_init", module);
	if (unlikely(ret < ret_ok)) return ret;

	/* Get the function
	 */
	if (dl_handler == NULL) 
		init_func = get_sym_from_dlopen_handler (dl_handler, init_name->buf);
	else 
		init_func = get_sym_from_enviroment (init_name->buf);
	

	/* Free the init function name string
	 */
	cherokee_buffer_free (init_name);
	
	/* Only try to execute if it exists
	 */
	if (init_func == NULL) {
		return ret_not_found;
	}
	
	/* Execute the init func
	 */
	init_func(loader);
	return ret_ok;
}


static ret_t
get_info (const char *module, cherokee_module_info_t **info, void **dl_handler)
{
	ret_t ret;
	CHEROKEE_NEW(info_name,buffer);
	
	/* Build the info struct string
	 */
	cherokee_buffer_add_va (info_name, "cherokee_%s_info", module);

	/* Maybe it's statically linked
	 */
	*info = get_sym_from_enviroment (info_name->buf);
	printf ("%s -> %p\n", info_name->buf, *info);

	/* Or maybe we have to load a dinamic library
	 */	
	if (*info == NULL) {
		ret = dylib_open (module, dl_handler);
		if (ret != ret_ok) {
			ret = ret_error;
			goto error;
		}

		*info = get_sym_from_dlopen_handler (*dl_handler, info_name->buf);
		if (*info == NULL) {
			ret = ret_not_found;
			goto error;
		}
	}
	
	/* Free the info struct string
	 */
	cherokee_buffer_free (info_name);
	return ret_ok;

error:
	cherokee_buffer_free (info_name);
	return ret;	
}


ret_t
check_deps_file (cherokee_module_loader_t *loader, char *modname)
{
	FILE  *file;
	char   temp[128];
	CHEROKEE_NEW (filename, buffer);

	cherokee_buffer_add_va (filename, "%s/%s.deps", CHEROKEE_DEPSDIR, modname);
	file = fopen (filename->buf, "r");
	if (file == NULL) goto exit;

	while (!feof(file)) {
		int   len;
		char *ret;

		ret = fgets (temp, 127, file);
		if (ret == NULL) break;

		len = strlen (temp); 

		if (len < 2) continue;
		if (temp[0] == '#') continue;
		
		if (temp[len-1] == '\n')
			temp[len-1] = '\0';

		cherokee_module_loader_load (loader, temp);
		temp[0] = '\0';
	}

	fclose (file);

exit:
	cherokee_buffer_free (filename);
	return ret_ok;
}


ret_t 
cherokee_module_loader_load (cherokee_module_loader_t *loader, char *modname)
{
	ret_t                   ret;
	cherokee_module_info_t *info      = NULL;
	void                   *dl_handle = NULL;
	
	ret = cherokee_table_get (loader->table, modname, &dl_handle);
	if (ret == ret_ok) return ret_ok;
	
	/* Check deps
	 */
	ret = check_deps_file (loader, modname);
	if (ret != ret_ok) return ret;

	/* Get the module info
	 */
	ret = get_info (modname, &info, &dl_handle);
	switch (ret) {
	case ret_ok:
		break;
	case ret_error:
		PRINT_ERROR ("ERROR: Can't open \"%s\" module\n", modname);		
		return ret;
	case ret_not_found:
		PRINT_ERROR ("ERROR: Can't read \"info\" structure from %s\n", modname);
		return ret;
	default:
		SHOULDNT_HAPPEN;
		return ret_error;
	}
	
	/* Execute init function
	 */
	execute_init_func (loader, modname, dl_handle);
	
	ret = cherokee_table_add (loader->table, modname, info);
	if (unlikely(ret != ret_ok)) return ret;
	
	return ret_ok;
}


ret_t 
cherokee_module_loader_get (cherokee_module_loader_t *loader, char *modname, cherokee_module_info_t **info)
{
	return cherokee_table_get (loader->table, modname, (void **)info);
}

#endif
