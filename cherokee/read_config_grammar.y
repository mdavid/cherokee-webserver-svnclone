%{
/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001, 2002, 2003 Alvaro Lopez Ortega
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

#include <pwd.h>
#include <grp.h>
#include <stdio.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <unistd.h>
#include <dirent.h>
#include <errno.h>
#include <ctype.h>
#include <strings.h>

#include "table.h"
#include "mime.h"
#include "server.h"
#include "server-protected.h"
#include "virtual_server.h"
#include "handler_table_entry.h"
#include "encoder.h"
#include "logger_table.h"
#include "access.h"
#include "list_ext.h"


/* Define the parameter name of the yyparse() argument
 */
#define YYPARSE_PARAM server

%}


%union {
	   int   number;
	   char *string;
	   void *ptr;

	   struct {
			 char *name;
			 void *ptr;
	   } name_ptr;

	   void *list;
}


%{
/* What is the right way to import this prototipe?
 */
extern int yylex (void);


extern char *yytext;
extern int   yylineno;

char                                  *current_yacc_file           = NULL;
static cherokee_handler_table_t       *current_handler_table       = NULL;
static cherokee_handler_table_entry_t *current_handler_table_entry = NULL;
static cherokee_virtual_server_t      *current_virtual_server      = NULL;
static cherokee_encoder_table_entry_t *current_encoder_entry       = NULL;
static cherokee_module_info_t         *current_module_info         = NULL;
// static list_t                         *current_list                = NULL;

struct {
	   char                          *handler_name;
	   cherokee_handler_table_entry_t *entry;
	   cherokee_virtual_server_t     *vserver;
	   cherokee_handler_table_t      *plugins;
	   char                          *document_root;
	   char                          *directory_name;
} directory_content_tmp;

typedef struct {
	   void *next;
	   char *string;
} linked_list_t;


#define auto_virtual_server ((current_virtual_server) ? current_virtual_server : SRV(server)->vserver_default)
#define auto_handler_table  ((current_handler_table) ? current_handler_table : (auto_virtual_server)->plugins)


static void
free_linked_list (linked_list_t *list, void (*free_func) (void *))
{
	   linked_list_t *i = list;

	   while (i != NULL) {
			 linked_list_t *prev = i;

			 if ((free_func) && (i->string)) {
				    free_func (i->string);
			 }

			 prev = i;
			 i = i->next;
			 free (prev);
	   }	   
}

static char *
make_finish_with_slash (char *string, int *len)
{
	   char *new;
	   int   new_len;

	   if (string[(*len)-1] == '/') {
			 return string;
	   }

	   new_len = (*len) + 2;
	   new  = (char*) malloc (new_len);

	   *len = snprintf (new, new_len, "%s/", string);

	   free (string);
	   return new;
}

static char *
make_slash_end (char *string)
{
	   int len = strlen(string);
	   return make_finish_with_slash (string, &len);
}

cherokee_handler_table_entry_t *
handler_table_entry_new (void)
{
	   cherokee_handler_table_entry_t *entry;

	   cherokee_handler_table_entry_new (&entry);
	   current_handler_table_entry = entry;

	   return entry;
}


char *
new_string_to_lowercase (const char *in)
{
	   int   i;
	   char *tmp;
	   
	   i = strlen(in);
	   tmp = (char *) malloc (i+1);
	   tmp[i] = '\0';

	   while (i--) {
			 tmp[i] = tolower(in[i]);
	   }

	   return tmp;
}


void
yyerror (char* msg)
{
	   char *config;

	   config = (current_yacc_file) ? current_yacc_file : "";

        PRINT_ERROR ("Error parsing file %s:%d '%s', symbol '%s'\n", 
				 config, yylineno, msg, yytext);
}

%}



%token T_QUOTE T_DENY T_THREAD_NUM T_SSL_CERT_KEY_FILE T_SSL_CERT_FILE T_KEEPALIVE_MAX_REQUESTS T_ERROR_HANDLER
%token T_TIMEOUT T_KEEPALIVE T_DOCUMENT_ROOT T_LOG T_MIME_FILE T_DIRECTORY T_HANDLER T_USER T_GROUP T_POLICY
%token T_SERVER T_USERDIR T_URL T_PIDFILE T_LISTEN T_FILEDIR T_SERVER_TOKENS T_ENCODER T_ALLOW 
%token T_BGCOLOR T_TEXT T_LINK T_ALINK T_VLINK T_BACKGROUND T_DIRECTORYINDEX T_IPV6 T_SHOW T_CHROOT T_HEADER_FILE
%token T_ICONS T_AUTH T_NAME T_METHOD T_PASSWDFILE T_SSL_CA_LIST_FILE T_FROM T_SOCKET T_LOG_FLUSH_INTERVAL
%token T_INCLUDE T_PANIC_ACTION T_JUST_ABOUT T_LISTEN_QUEUE_SIZE T_SENDFILE T_MINSIZE T_MAXSIZE T_MAX_FDS
%token T_INTERPRETER T_SCRIPT_ALIAS T_ONLY_SECURE T_MAX_CONNECTION_REUSE T_REWRITE

%token <number> T_NUMBER T_PORT 
%token <string> T_QSTRING T_FULLDIR T_ID T_HTTP_URL T_HTTPS_URL T_HOSTNAME T_IP T_DOMAIN_NAME

%type <name_ptr> directory_option handler
%type <string> host_name http_generic
%type <list> id_list ip_list domain_list

%%

conffile : /* Empty */
         | lines
         ; 

lines : line
	 | lines line
	 ;

server_lines : server_line
             | server_lines server_line
             ;

line : server_line
     | common_line
     ;

common_line : server
            | port
            | listen
            | server_tokens
            | mime
            | icons
            | timeout
            | keepalive
            | keepalive_max_requests
            | pidfile
            | user1 
            | user2
            | group1 
            | group2
            | chroot
            | thread_number
            | ipv6
            | directoryindex
            | errorhandler
            | log_flush_interval
            | include
            | panic_action
            | listen_queue_size
            | sendfile
            | maxfds
            | maxconnectionreuse
            ;

server_line : directory
            | document_root
            | log
            | encoder
            | ssl_file
            | ssl_key_file
            | ssl_ca_list_file
            | userdir
            ;

directory_options : 
                  | directory_options directory_option;

sendfile_options : 
                 | sendfile_options sendfile_option;

handler_options : 
                | handler_options handler_option;

encoder_options :
                | encoder_options encoder_option;

thread_options : 
               | thread_options thread_option;

auth_options :
             | auth_options auth_option;

directories : 
            | directories directory;


/* ID List
 */
id_list : T_ID 
{
	   linked_list_t *n = (linked_list_t *) malloc(sizeof(linked_list_t));
	   n->next = NULL;
	   n->string = $1;
	   
	   $$ = n;
};

id_list : T_ID ',' id_list
{
	   linked_list_t *n = (linked_list_t *) malloc(sizeof(linked_list_t));

	   n->next = $3;
	   n->string = $1;

	   $$ = n;
};


/* Domain names list
 */
domain_list : host_name
{
	   linked_list_t *n = (linked_list_t *) malloc(sizeof(linked_list_t));
	   n->next = NULL;
	   n->string = $1;
	   
	   $$ = n;
};

domain_list : host_name ',' domain_list
{
	   linked_list_t *n = (linked_list_t *) malloc(sizeof(linked_list_t));

	   n->next = $3;
	   n->string = $1;

	   $$ = n;
};


/* IP List
 */
ip_list : T_IP 
{
	   linked_list_t *n = (linked_list_t *) malloc (sizeof(linked_list_t));
	   n->next   = NULL;
	   n->string = $1;

	   $$ = n;
};

ip_list : T_IP ',' ip_list
{
	   linked_list_t *n = (linked_list_t *) malloc (sizeof(linked_list_t));
	   n->next   = $3;
	   n->string = $1;

	   $$ = n;
};

port : T_PORT T_NUMBER 
{
	   SRV(server)->port = $2;
};

listen : T_LISTEN host_name
{
	   SRV(server)->listen_to = $2;
};

log_flush_interval : T_LOG_FLUSH_INTERVAL T_NUMBER
{
	   SRV(server)->log_flush_elapse = $2;
};

document_root : T_DOCUMENT_ROOT T_FULLDIR
{
	   char                      *root;
	   int                        root_len;
	   cherokee_virtual_server_t *vserver;

	   vserver = auto_virtual_server;

	   root     = $2;
	   root_len = strlen($2);

	   /* Check for the endding slash
	    */
	   root = make_finish_with_slash (root, &root_len);

	   /* Add the virtual root path to the virtual server struct
	    */
	   cherokee_buffer_add (vserver->root, root, root_len);
};


log : T_LOG T_ID
{
	   ret_t ret;

	   /* Maybe load the module
	    */
	   ret = cherokee_module_loader_load (SRV(server)->loader, $2);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Can't load logger module '%s'\n", $2);
			 return 1;
	   }

	   cherokee_module_loader_get (SRV(server)->loader, $2, &current_module_info);
}
log_optional
{
	   cherokee_virtual_server_t *vserver;
	   vserver = auto_virtual_server;

	   /* Instance the logger object
	    */
	   cherokee_logger_table_new_logger (SRV(server)->loggers, $2, current_module_info,
								  vserver->logger_props, &vserver->logger);
	   current_module_info = NULL;
};

log_optional : 
             | '{' tuple_list '}';

tuple_list : 
           | tuple_list tuple
           ;

tuple : T_ID T_FULLDIR
{
	   cherokee_virtual_server_t *vserver;
	   vserver = auto_virtual_server;

	   if (vserver->logger_props == NULL) {
			 cherokee_table_new (&vserver->logger_props);
	   }

	   cherokee_table_add (vserver->logger_props, $1, $2);
};


server_tokens : T_SERVER_TOKENS T_ID
{
	   if (!strncasecmp("Product", $2, 7)) {
			 SRV(server)->server_token = cherokee_version_product;
	   } else if (!strncasecmp("Minor", $2, 5)) {
			 SRV(server)->server_token = cherokee_version_minor;
	   } else if (!strncasecmp("Minimal", $2, 7)) {
			 SRV(server)->server_token = cherokee_version_minimal;
	   } else if (!strncasecmp("OS", $2, 2)) {
			 SRV(server)->server_token = cherokee_version_os;
	   } else if (!strncasecmp("Full", $2, 4)) {
			 SRV(server)->server_token = cherokee_version_full;
	   } else {
			 PRINT_ERROR ("ERROR: Unknown server token '%s'\n", $2);
			 return 1;
	   }
};

mime : T_MIME_FILE T_FULLDIR
{
	   SRV(server)->mime_file = $2;
};

icons : T_ICONS T_FULLDIR
{
	   SRV(server)->icons_file = $2;
};

timeout : T_TIMEOUT T_NUMBER
{
	   SRV(server)->timeout = $2;

	   cherokee_buffer_clean  (SRV(server)->timeout_header);
	   cherokee_buffer_add_va (SRV(server)->timeout_header, "Keep-Alive: timeout=%d"CRLF, $2);
};

keepalive : T_KEEPALIVE T_NUMBER
{
	   SRV(server)->keepalive = ($2 == 0) ? false : true;
};

keepalive_max_requests : T_KEEPALIVE_MAX_REQUESTS T_NUMBER
{
	   SRV(server)->keepalive_max = $2;
};

ssl_file : T_SSL_CERT_FILE T_FULLDIR
{
#ifdef HAVE_TLS
	   cherokee_virtual_server_t *vsrv = auto_virtual_server;

	   if (vsrv->server_cert != NULL) {
			 PRINT_ERROR ("ERROR: \"SSLCertificateFile\" overlaps: '%s' <- '%s'\n", vsrv->server_cert, $2);
			 free (vsrv->server_cert);
	   }

	   vsrv->server_cert = $2;

#else
	   PRINT_ERROR_S ("WARNING: Ignoring SSL configuration entry: \"SSLCertificateFile\"\n");
#endif
};

ssl_key_file : T_SSL_CERT_KEY_FILE T_FULLDIR
{
#ifdef HAVE_TLS
	   cherokee_virtual_server_t *vsrv = auto_virtual_server;

	   if (vsrv->server_key != NULL) {
			 PRINT_ERROR ("ERROR: \"SSLCertificateKeyFile\" overlaps: '%s' <- '%s'\n", vsrv->server_key, $2);
			 free (vsrv->server_key);
	   }

	   vsrv->server_key = $2;

#else
	   PRINT_ERROR_S ("WARNING: Ignoring SSL configuration entry: \"SSLCertificateKeyFile\"\n");
#endif
};

ssl_ca_list_file : T_SSL_CA_LIST_FILE T_FULLDIR
{
#ifdef HAVE_TLS
	   cherokee_virtual_server_t *vsrv = auto_virtual_server;

	   if (vsrv->ca_cert != NULL) {
			 PRINT_ERROR ("ERROR: \"SSLCAListFile\" overlaps: '%s' <- '%s'\n", vsrv->ca_cert, $2);
			 free (vsrv->ca_cert);
	   }

	   vsrv->ca_cert = $2;

#else
	   PRINT_ERROR_S ("WARNING: Ignoring SSL configuration entry: \"SSLCAListFile\"\n");
#endif
};


encoder : T_ENCODER T_ID
{
	   ret_t ret;
	   cherokee_module_info_t *info;
	   cherokee_encoder_table_entry_t *enc;

	   /* Load the module
	    */
	   ret = cherokee_module_loader_load (SRV(server)->loader, $2);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Can't load encoder module '%s'\n", $2);
			 return 1;
	   }

	   cherokee_module_loader_get  (SRV(server)->loader, $2, &info);

	   /* Set the info in the new entry
	    */
	   cherokee_encoder_table_entry_new (&enc);
	   cherokee_encoder_table_entry_get_info (enc, info);

	   /* Set in the encoders table
	    */
	   cherokee_encoder_table_set (SRV(server)->encoders, $2, enc);
	   current_encoder_entry = enc;
} 
maybe_encoder_options
{
	   current_encoder_entry = NULL;
};

maybe_encoder_options : '{' encoder_options '}' 
                      |;

encoder_option : T_ALLOW id_list
{
	   linked_list_t *i;
	   cherokee_matching_list_t *matching;

	   cherokee_matching_list_new (&matching);
	   cherokee_encoder_entry_set_matching_list (current_encoder_entry, matching);

	   i = $2;
	   while (i!=NULL) {
			 linked_list_t *prev;

			 cherokee_matching_list_add_allow (matching, i->string);

			 free(i->string);
			 prev = i;
			 i = i->next;
			 free(prev);
	   }
};

encoder_option : T_DENY id_list
{
	   linked_list_t *i;
	   cherokee_matching_list_t *matching;

	   cherokee_matching_list_new (&matching);
	   cherokee_encoder_entry_set_matching_list (current_encoder_entry, matching);

	   i = $2;
	   while (i!=NULL) {
			 linked_list_t *prev;

			 cherokee_matching_list_add_deny (matching, i->string);

			 free(i->string);
			 prev = i;
			 i = i->next;
			 free(prev);
	   }
};

pidfile : T_PIDFILE T_FULLDIR
{
	   FILE *file;
	   CHEROKEE_TEMP(buffer, 10);

	   file = fopen ($2, "w");
	   if (file == NULL) {
			 PRINT_ERROR ("ERROR: Can't write PID file '%s'\n", $2);
			 return 1;
	   }

	   snprintf (buffer, buffer_size, "%d\n", getpid());
	   fwrite (buffer, 1, strlen(buffer), file);
	   fclose (file);

	   free ($2);
};


include : T_INCLUDE T_FULLDIR
{
	   cherokee_list_add_tail (&SRV(server)->include_list, $2);
};

panic_action : T_PANIC_ACTION T_FULLDIR
{
	   if (SRV(server)->panic_action != NULL) {
			 PRINT_ERROR ("WARNING: Overwriting panic action '%s' by '%s'\n", SRV(server)->panic_action, $2);
			 free (SRV(server)->panic_action);
	   }

	   SRV(server)->panic_action = $2;
};

listen_queue_size : T_LISTEN_QUEUE_SIZE T_NUMBER
{
	   SRV(server)->listen_queue = $2;
};

sendfile : T_SENDFILE '{' sendfile_options '}';

sendfile_option : T_MINSIZE T_NUMBER 
{
	   SRV(server)->sendfile.min = $2;
};

sendfile_option : T_MAXSIZE T_NUMBER 
{
	   SRV(server)->sendfile.max = $2;
};

maxfds : T_MAX_FDS T_NUMBER
{
	   SRV(server)->max_fds = $2;
};

maxconnectionreuse : T_MAX_CONNECTION_REUSE T_NUMBER
{
	   SRV(server)->max_conn_reuse = $2;
};

chroot : T_CHROOT T_FULLDIR
{
	   SRV(server)->chroot = $2;
};

thread_number : T_THREAD_NUM T_NUMBER maybe_thread_options
{
#ifdef HAVE_PTHREAD
	   SRV(server)->thread_num = $2;
#endif
};

maybe_thread_options : '{' thread_options '}'
                     |;

thread_option : T_POLICY T_ID
{
#ifdef HAVE_PTHREAD
	   if (strcasecmp($2, "fifo") == 0) {
			 SRV(server)->thread_policy = SCHED_FIFO;
	   } else if (strcasecmp($2, "rr") == 0) {
			 SRV(server)->thread_policy = SCHED_RR;
	   } else if (strcasecmp($2, "other") == 0) {
			 SRV(server)->thread_policy = SCHED_OTHER;
	   } else {
			 PRINT_ERROR ("ERROR: unknown scheduling policy '%s'\n", $2);
	   }
#endif
};


ipv6 : T_IPV6 T_NUMBER
{
	   SRV(server)->ipv6 = $2;
};

user1 : T_USER T_ID
{
	   struct passwd *pwd;
	   
	   pwd = (struct passwd *) getpwnam ($2);
	   if (pwd == NULL) {
			 PRINT_ERROR ("ERROR: User '%s' not found in the system", $2);
			 return 1;
	   }

	   SRV(server)->user = pwd->pw_uid;

	   free ($2);
};

user2 : T_USER T_NUMBER
{
	   SRV(server)->user = $2;
};

group1 : T_GROUP T_ID
{
	   struct group *grp;

	   grp = (struct group *) getgrnam ($2);
	   if (grp == NULL) {
			 PRINT_ERROR ("ERROR: Group '%s' not found in the system", $2);
			 return 1;
	   }

	   SRV(server)->group = grp->gr_gid;

	   free ($2);
};

group2 : T_GROUP T_NUMBER
{
	   SRV(server)->group = $2;
};

handler : T_HANDLER T_ID '{' handler_options '}' 
{
	   $$.name = $2;
	   $$.ptr = current_handler_table_entry;
};

handler : T_HANDLER T_ID 
{
	   $$.name = $2;
	   $$.ptr = current_handler_table_entry;
};

http_generic : T_HTTP_URL  { $$ = $1; } 
             | T_HTTPS_URL { $$ = $1; };

handler_option : T_URL http_generic
{
	   cherokee_handler_table_entry_set (current_handler_table_entry, "url", $2);
};

handler_option : T_REWRITE T_QSTRING T_QSTRING
{
	int *count = NULL;
	char name[32];

	if (current_handler_table_entry->properties != NULL)
		count = cherokee_table_get_val(current_handler_table_entry->properties, "regex_count");

	if (count == NULL)
	{
		count = (int*)malloc(sizeof(int));
		*count = 1;

		cherokee_handler_table_entry_set (current_handler_table_entry, "regex_count", count);
	} else {
		++*count;
	}

	snprintf(name, sizeof(name), "regex_%d_expr", *count - 1);
	cherokee_handler_table_entry_set (current_handler_table_entry, name, $2);

	snprintf(name, sizeof(name), "regex_%d_subs", *count - 1);
	cherokee_handler_table_entry_set (current_handler_table_entry, name, $3);
};

handler_option : T_URL T_FULLDIR
{
	   cherokee_handler_table_entry_set (current_handler_table_entry, "url", $2);
};

handler_option : T_FILEDIR T_FULLDIR
{
	   cherokee_handler_table_entry_set (current_handler_table_entry, "filedir", $2);
};

handler_option : T_BGCOLOR T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "bgcolor", $2); };

handler_option : T_TEXT T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "text", $2); };

handler_option : T_LINK T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "link", $2); };

handler_option : T_VLINK T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "vlink", $2); };

handler_option : T_ALINK T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "alink", $2); };

handler_option : T_HEADER_FILE T_ID
{ cherokee_handler_table_entry_set (current_handler_table_entry, "headerfile", $2); };

handler_option : T_SOCKET T_FULLDIR
{ cherokee_handler_table_entry_set (current_handler_table_entry, "socket", $2); };

handler_option : T_INTERPRETER T_FULLDIR
{ cherokee_handler_table_entry_set (current_handler_table_entry, "interpreter", $2); };

handler_option : T_JUST_ABOUT
{ cherokee_handler_table_entry_set (current_handler_table_entry, "about", NULL); };

handler_option : T_SCRIPT_ALIAS T_FULLDIR
{ cherokee_handler_table_entry_set (current_handler_table_entry, "scriptalias", $2); };

handler_option : T_NUMBER http_generic
{
	   char code[4];

	   if (($1 < 100) || ($1 >= http_type_500_max)) {
			 PRINT_ERROR("ERROR: Incorrect HTTP code number %d\n", $1);
			 return 1;
	   }

	   snprintf (code, 4, "%d", $1);
	   cherokee_handler_table_entry_set (current_handler_table_entry, code, $2);
};

handler_option : T_SHOW id_list
{
	   linked_list_t *i;

	   i = $2;
	   while (i != NULL) {
			 if ((!strncasecmp (i->string, "date",  4)) ||
				(!strncasecmp (i->string, "size",  4)) ||
				(!strncasecmp (i->string, "group", 5)) ||
				(!strncasecmp (i->string, "owner", 5)))
			 {
				    char *lower;

				    lower = new_string_to_lowercase (i->string);
				    free (i->string);
				    i->string = lower;

				    cherokee_handler_table_entry_set (current_handler_table_entry, i->string, i->string);				    
			 } else {
				    PRINT_ERROR ("ERROR: Unknown parameter '%s' for \"Show\"", i->string);
			 }
				
			 i = i->next;
	   }

	   free_linked_list ($2, free);
};


host_name : T_ID
          | T_IP 
          | T_DOMAIN_NAME
{
	   $$ = $1;
};

server : T_SERVER domain_list '{' 
{
	   linked_list_t *i = $2;
	   CHEROKEE_NEW(vsrv, virtual_server);

	   current_virtual_server = vsrv;
	   current_handler_table  = vsrv->plugins;

	   /* Add the virtual server to the list
	    */
	   list_add ((list_t *)vsrv, &SRV(server)->vservers);

	   /* Add default virtual server name
	    */
	   if (i->string != NULL) {
			 cherokee_buffer_t *name = current_virtual_server->name;

			 if (cherokee_buffer_is_empty (name)) {
				    cherokee_buffer_add_va (name, "%s", i->string);
			 }
	   }

	   /* Add all the alias to the table
	    */
	   while (i != NULL) {
			 cherokee_table_add (SRV(server)->vservers_ref, i->string, vsrv);
			 i = i->next;
	   }
	   free_linked_list ($2, NULL);

} server_lines '}' {

	   current_virtual_server = NULL;
	   current_handler_table  = NULL;
};


directory : T_DIRECTORY T_FULLDIR '{' 
{
	   /* Fill the tmp struct
	    */
	   directory_content_tmp.directory_name = $2;
	   directory_content_tmp.vserver        = auto_virtual_server;
	   directory_content_tmp.plugins        = auto_handler_table;
	   directory_content_tmp.entry          = handler_table_entry_new (); /* new! */
	   directory_content_tmp.handler_name   = NULL;
	   directory_content_tmp.document_root  = NULL;
} 
directory_options '}'
{
	   ret_t ret;
	   cherokee_module_info_t *info;

	   /* Basic checks
	    */
	   if (directory_content_tmp.handler_name == NULL) {
			 PRINT_ERROR ("ERROR: Directory %s needs a handler; this directory entry is ignored.\n", directory_content_tmp.directory_name);
			 goto out;
	   }

	   /* Set the document_root in the entry
	    */
	   if (directory_content_tmp.document_root != NULL) {
			 cherokee_buffer_make_empty(directory_content_tmp.entry->document_root);
			 cherokee_buffer_add_va (directory_content_tmp.entry->document_root, "%s",
								directory_content_tmp.document_root);
	   }

	   /* Load the module 
	    */
	   ret = cherokee_module_loader_load (SRV(server)->loader, directory_content_tmp.handler_name);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Loading module '%s'\n", directory_content_tmp.handler_name);
			 return 1;
	   }
	   
	   ret = cherokee_module_loader_get  (SRV(server)->loader, directory_content_tmp.handler_name, &info);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Loading module '%s'\n", directory_content_tmp.handler_name);
			 return 1;
	   }
	   
	   cherokee_handler_table_enty_get_info (directory_content_tmp.entry, info);
	   
	   /* Add "web_dir -> entry" in the plugins table
	    */
	   ret = cherokee_handler_table_add (directory_content_tmp.plugins,
								  directory_content_tmp.directory_name,
								  directory_content_tmp.entry);
	   if (ret != ret_ok) {
			 switch (ret) {
			 case ret_file_not_found:
				    PRINT_ERROR ("ERROR: Can't load handler '%s': File not found\n",
							  directory_content_tmp.handler_name);
				    break;
			 default:
				    PRINT_ERROR ("ERROR: Can't load handler '%s': Unknown error\n",
							  directory_content_tmp.handler_name);
			 }
	   }


	   /* Clean
	    */
out:
	   if (directory_content_tmp.document_root != NULL) {
			 free (directory_content_tmp.document_root);
			 directory_content_tmp.document_root = NULL;
	   }
	   directory_content_tmp.vserver       = NULL;
	   directory_content_tmp.plugins       = NULL;
	   directory_content_tmp.entry         = NULL;
	   directory_content_tmp.handler_name  = NULL;

	   current_handler_table_entry = NULL;
};


directory_option : handler
{	   
	   directory_content_tmp.handler_name = $1.name;
};


directory_option : T_DOCUMENT_ROOT T_FULLDIR
{
	   if (directory_content_tmp.document_root != NULL) {
			 PRINT_ERROR ("WARNING: Overwriting DocumentRoot '%s' by '%s'\n",
					    directory_content_tmp.document_root, $2);

			 free (directory_content_tmp.document_root);
	   }

	   directory_content_tmp.document_root = make_slash_end ($2);
};


directory_option : T_AUTH id_list '{' auth_options '}'
{
	   linked_list_t                  *i     = $2;
	   cherokee_handler_table_entry_t *entry = directory_content_tmp.entry;

	   while (i != NULL) {
			 if (strncasecmp(i->string, "basic", 5) == 0) {
				    entry->authentication |= http_auth_basic;
			 } 
			 else if (strncasecmp(i->string, "digest", 6) == 0) {
				    entry->authentication |= http_auth_digest;			 
			 }
			 else {
				    PRINT_ERROR ("ERROR: Unknown authentication type '%s'\n", i->string);
				    return 1;
			 }

			 i = i->next;
	   }

	   free_linked_list ($2, free);
};


directory_option : T_ALLOW T_FROM ip_list 
{
	   linked_list_t *i, *prev;
	   CHEROKEE_NEW(n, access);

	   i = $3;
	   while (i != NULL) {
			 cherokee_access_add (n, i->string);
			 
			 free (i->string);
			 prev = i;
			 i = i->next;
			 free (prev);
	   }
	   directory_content_tmp.entry->access = n;
};

directory_option : T_ONLY_SECURE
{
#ifndef HAVE_TLS
	   PRINT_ERROR_S ("ERROR: Cherokee is compiled without TLS support, so\n"
				   "       it isn't possible to use «OnlySecure»\n");
#endif

	   directory_content_tmp.entry->only_secure = true;
};

auth_option : T_NAME T_QSTRING 
{
	   cherokee_buffer_t *realm;
	   realm = directory_content_tmp.entry->auth_realm;

	   cherokee_buffer_add (realm, $2, strlen($2));
	   free ($2);
};

auth_option : T_METHOD T_ID maybe_auth_option_params
{
	   ret_t ret;
	   cherokee_module_info_t *info;

	   ret = cherokee_module_loader_load (SRV(server)->loader, $2);
	   if (ret != ret_ok) {
			 PRINT_ERROR ("ERROR: Can't load validator module '%s'\n", $2);
			 return 1;
	   }

	   cherokee_module_loader_get  (SRV(server)->loader, $2, &info);

	   if (info->type != cherokee_validator) {
			 PRINT_ERROR ("ERROR: %s is not a validator module!!\n", $2);
	   }

	   directory_content_tmp.entry->validator_new_func = info->new_func;
};

maybe_auth_option_params : '{' auth_option_params '}'
                         |
                         ;

auth_option_params : 
                   | auth_option_params auth_option_param
                   ;

auth_option_param : T_PASSWDFILE T_FULLDIR 
{ cherokee_handler_table_entry_set (current_handler_table_entry, "file", $2); };



userdir : T_USERDIR T_ID 
{
	   int   len;
	   char *tmp;
	   cherokee_virtual_server_t *vsrv = auto_virtual_server;

	   /* Set the users public directory
	    */
	   if (!cherokee_buffer_is_empty (vsrv->userdir)) {
			 PRINT_ERROR ("WARNING: Overwriting userdir '%s'\n", vsrv->userdir->buf);
			 cherokee_buffer_clean (vsrv->userdir);
	   }

	   len = strlen($2);
	   tmp = make_finish_with_slash ($2, &len);
	   cherokee_buffer_add (vsrv->userdir, tmp, len);

	   /* Set the plugin table reference
	    */
	   current_handler_table = vsrv->userdir_plugins;

}  '{' directories '}' {

	   /* Remove the references
	    */
	   current_handler_table = NULL;
};


directoryindex : T_DIRECTORYINDEX id_list
{
	   linked_list_t *i = $2;

	   while (i != NULL) {
			 cherokee_list_add_tail (&SRV(server)->index_list, i->string);
			 i = i->next;
	   }

	   free_linked_list ($2, NULL);
};


maybe_handlererror_options : 
                           | '{' handler_options '}';

errorhandler : T_ERROR_HANDLER T_ID
{
	   ret_t                      ret;
	   cherokee_module_info_t    *info;
	   cherokee_virtual_server_t *vsrv = auto_virtual_server;

	   /* Load the module
	    */
	   ret = cherokee_module_loader_load (SRV(server)->loader, $2);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Loading module '%s'\n", $2);
			 return 1;
	   }

	   ret = cherokee_module_loader_get (SRV(server)->loader, $2, &info);
	   if (ret < ret_ok) {
			 PRINT_ERROR ("ERROR: Loading module '%s'\n", $2);
			 return 1;
	   }
	   
	   /* Remove the old (by default) error handler and cretate a new one
	    */
	   vsrv->error_handler = handler_table_entry_new();

	   /* Setup the loaded module
	    */
	   cherokee_handler_table_enty_get_info (vsrv->error_handler, info);
} 
maybe_handlererror_options;

%%

