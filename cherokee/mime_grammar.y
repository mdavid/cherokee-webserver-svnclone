%{
/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2006 Alvaro Lopez Ortega
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
#include "list_ext.h"
#include "mime.h"
#include "mime-protected.h"


/* Define the parameter name of the yyparse() argument
 */
#define YYPARSE_PARAM mime

%}

%union {
	   unsigned int      number;
	   char             *string;
	   struct list_head *list;
}

%{

/* What is the right way to import these prototipes?
 * There's the same problem in read_config_grammar.y
 */
extern int   yy_mime_lex (void);
extern char *yy_mime_text;
extern int   yy_mime_lineno;

list_t                 current_list;
cherokee_mime_entry_t *current_mime_entry;



void
yy_mime_error(char* msg)
{
        PRINT_ERROR ("Error parsing configuration: '%s', line %d, symbol '%s' (0x%x)\n", 
				 msg, yy_mime_lineno, yy_mime_text, yy_mime_text[0]);
}

%}


%token T_MAXAGE T_SUFFIXES
%token <string> T_SUFFIX T_MIME
%token <number> T_AGE

%type <list>   list_ids
%type <string> id

%%

mime_file : lines;

lines : line
      | lines line;

line : mime
     ;


mime : T_MIME '{' 
{
	   /* Create a new mime_entry object
	    */
	   cherokee_mime_entry_new (&current_mime_entry);
	   cherokee_mime_add (MIME(mime), current_mime_entry);

	   /* Add the mime string to the mime_name buffer
	    */
	   cherokee_buffer_add (MIME_ENTRY_NAME(current_mime_entry), $1, strlen($1));
	   free ($1);
}
internal_mimes '}'
{
	   current_mime_entry = NULL;
};


internal_mimes : internal_mime
               | internal_mimes internal_mime;

internal_mime : maxage
              | suffixes;

maxage : T_MAXAGE T_AGE 
{   
	   MIME_ENTRY_AGE(current_mime_entry) = $2;
};

suffixes : T_SUFFIXES list_ids
{
	   ret_t   ret;
	   list_t *i;

	   list_for_each (i, $2) {
			 ret = cherokee_table_add (MIME_TABLE(mime), LIST_ITEM(i)->info, current_mime_entry);
			 if (ret < ret_ok) {
				    PRINT_ERROR ("Error adding MIME suffix %s\n", (char *)LIST_ITEM(i)->info);
			 }
	   }

	   cherokee_list_free ($2, free);
};


/* List
 */
id : T_SUFFIX;

list_ids : id
{
	   INIT_LIST_HEAD(&current_list);
	   cherokee_list_add (&current_list, $1);
	   $$ = &current_list;
};

list_ids : id ',' list_ids
{
	   cherokee_list_add (&current_list, $1);
	   $$ = &current_list;
};
