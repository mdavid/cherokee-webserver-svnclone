/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *      Ayose Cazorla León <setepo@gulic.org>
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

#include <config.h>

#include "handler_redir.h"
#include "connection.h"
#include "connection-protected.h"

#ifdef HAVE_PCRE
# include <pcre.h>
#endif


#ifdef HAVE_PCRE
static pcre* compile_regex (char* pattern);
static void  substitute_groups (cherokee_buffer_t* url, int groupsfound,
			       const char* subject, const char *subs,
			       int strings[]);

struct cre_list {
	pcre* re;
	char* subs;
	struct cre_list *next;
};
#endif /* HAVE_PCRE */


cherokee_module_info_t cherokee_redir_info = {
	cherokee_handler,             /* type     */
	cherokee_handler_redir_new    /* new func */
};

ret_t 
cherokee_handler_redir_new (cherokee_handler_t **hdl, void *cnt, cherokee_table_t *properties)
{
	int i;
	int *count;
	CHEROKEE_NEW_STRUCT (n, handler_redir);
	
	/* Init the base class object
	 */
	cherokee_handler_init_base(HANDLER(n), cnt);

	MODULE(n)->init         = (handler_func_init_t) cherokee_handler_redir_init;
	MODULE(n)->free         = (handler_func_free_t) cherokee_handler_redir_free;
	HANDLER(n)->add_headers = (handler_func_add_headers_t) cherokee_handler_redir_add_headers;

	HANDLER(n)->connection  = cnt;
	HANDLER(n)->support     = hsupport_nothing;

	n->regex_list           = NULL;
	n->target_url           = NULL;
	n->target_url_len       = 0;
	
	/* It needs at least the "URL" configuration parameter..
	 */
	if (cherokee_buffer_is_empty (CONN(cnt)->redirect) && properties) {

		/* Get the URL property, if needed
		 */
		n->target_url     = cherokee_table_get_val (properties, "url");
		n->target_url_len = (n->target_url == NULL ? 0 : strlen(n->target_url));
	}

	/* Create the list of compiled regexs
	 */
#ifdef HAVE_PCRE
	if (cherokee_table_get (properties, "regex_count", &count) == ret_ok)
	{
		struct cre_list **last_item;
		last_item = (struct cre_list**)&(n->regex_list);

		for (i = 0; i < *count; i++)
		{
			char name[32];
			char *pattern, *subs;
			pcre *re;
			struct cre_list *new;

			snprintf(name, sizeof(name), "regex_%d_expr", i);
			if ((pattern = cherokee_table_get_val(properties, name)) == NULL)
				continue;

			snprintf(name, sizeof(name), "regex_%d_subs", i);
			if ((subs = cherokee_table_get_val(properties, name)) == NULL)
				continue;

			if ((re = compile_regex(pattern)) == NULL)
				continue;

			/* Add to the list in the same order that they are read
			 */
			new = (struct cre_list*)malloc(sizeof(struct cre_list));
			new->re = re;
			new->subs = subs;
			new->next = NULL;

			*last_item = new;
			last_item = &(new->next);
		}
	}
#endif

	/* Return the new handler obj
	 */
	*hdl = HANDLER(n);
	return ret_ok;	   
}


ret_t 
cherokee_handler_redir_free (cherokee_handler_redir_t *rehdl)
{
#ifdef HAVE_PCRE
	struct cre_list *n, *list;

	list = (struct cre_list *)rehdl->regex_list;
	while (list != NULL) {
		n = list->next;
		free(list);
		list = n;
	}
#endif

	return ret_ok;
}


ret_t 
cherokee_handler_redir_init (cherokee_handler_redir_t *n)
{
	cherokee_connection_t *conn = HANDLER_CONN(n);
    
	conn->error_code = http_moved_permanently;
	
#ifdef HAVE_PCRE
        /* First, look up in the regex list
	 */
	{
		struct cre_list *list;
		
		list = (struct cre_list*)n->regex_list;
		while (list != NULL)
		{
			int strings[30], rc;
			char *subject = conn->request->buf + conn->web_directory->len;
			
			rc = pcre_exec (list->re, NULL, subject, strlen(subject), 0, 0, strings, 30);
			
			if (rc == 0) {
				/* TODO: write to error.log 
				 */
				PRINT_ERROR_S("Too many groups in the regex\n");
				
			} else if (rc > 0) {
				/* Eureka!
				 */
				substitute_groups (conn->redirect, rc, subject, list->subs, strings);
				return ret_error;
			}
			
			list = list->next;
		}
	}
#endif

	/* Try with URL directive
	 */
	if (cherokee_buffer_is_empty (conn->redirect)) {
		int   request_end;
		char *request_endding;

		request_end = (conn->request->len - conn->web_directory->len);
		request_endding = conn->request->buf + conn->web_directory->len;

		cherokee_buffer_ensure_size (conn->redirect, request_end + n->target_url_len +1);

		cherokee_buffer_add (conn->redirect, n->target_url, n->target_url_len);
		cherokee_buffer_add (conn->redirect, request_endding, request_end);
	} 

	return ret_ok;
}


ret_t 
cherokee_handler_redir_add_headers (cherokee_handler_redir_t *rehdl, cherokee_buffer_t *buffer)
{
	cherokee_connection_t *conn = CONN(HANDLER(rehdl)->connection);
	
	if (!cherokee_buffer_is_empty (conn->redirect)) {
		cherokee_buffer_add (buffer, "Location: ", 10);
		cherokee_buffer_add_buffer (buffer, conn->redirect);
		cherokee_buffer_add (buffer, CRLF, 2);
	}

	return ret_ok;
}


/* PCRE code
 */
#ifdef HAVE_PCRE


static struct {
# ifdef HAVE_PTHREAD
	pthread_rwlock_t rwlock;
# endif
	cherokee_table_t *cache;
} regex_cache;


static pcre* 
compile_regex (char* pattern)
{
	pcre* re;

	CHEROKEE_RWLOCK_READER(&regex_cache.rwlock);
	re = (pcre*)cherokee_table_get_val (regex_cache.cache, pattern);
	CHEROKEE_RWLOCK_UNLOCK(&regex_cache.rwlock);

	if (re == NULL)
	{
		CHEROKEE_RWLOCK_WRITER(&regex_cache.rwlock);

		/* Check again, because another thread can create it after the
		 * previous unlock.
		 */
		re = (pcre*)cherokee_table_get_val (regex_cache.cache, pattern);
		if (re == NULL)
		{
			const char* error_msg;
			int         error_offset;

			re = pcre_compile (pattern, 0, &error_msg, &error_offset, NULL);

			if (re == NULL) {
				/* TODO: Write the error to error.log
				 */
				PRINT_ERROR ("Error in regex <<%s>>: \"%s\", offset %d\n", 
					     pattern, error_msg, error_offset);
			} else {
				cherokee_table_add (regex_cache.cache, pattern, re);
			}
		}
		CHEROKEE_RWLOCK_UNLOCK(&regex_cache.rwlock);
	}

	return re;
}


static void 
substitute_groups (cherokee_buffer_t* url, int groupsfound, 
		   const char* subject, const char* subs, int strings[])
{
	char buff[512];
	int dollar;

	cherokee_buffer_ensure_size(url, strlen(subs));
	dollar = 0;

	for(; *subs; subs++) {
		if (dollar) {
			if (*subs >= '0' && *subs <= '9') {
				pcre_copy_substring (subject, strings, groupsfound,
						     *subs - '0', buff, sizeof(buff));
				cherokee_buffer_add (url, buff, strlen(buff));

			} else {
				/* If it is not a number, add both characters 
				 */
				cherokee_buffer_add (url, "$", 1);
				cherokee_buffer_add (url, subs, 1);
			}

			dollar = 0;
		} else {
			if (*subs == '$')
				dollar = 1;
			else 
				cherokee_buffer_add (url, subs, 1);
		}
	}
}

#endif /* HAVE_PCRE */


/* Library init function
 */

void 
redir_init (cherokee_module_loader_t *loader)
{
#ifdef HAVE_PCRE
	CHEROKEE_RWLOCK_INIT (&regex_cache.rwlock, NULL);
	cherokee_table_new(&regex_cache.cache);
#endif /* HAVE_PCRE */
}
