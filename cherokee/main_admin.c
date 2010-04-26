/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2010 Alvaro Lopez Ortega
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
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

#include "common-internal.h"

#include <signal.h>
#include <sys/stat.h>

#include "init.h"
#include "server.h"
#include "socket.h"
#include "spawner.h"
#include "config_reader.h"
#include "util.h"

#ifdef HAVE_GETOPT_LONG
# include <getopt.h>
#else
# include "getopt/getopt.h"
#endif

#define APP_NAME        \
	"Cherokee Web Server: Admin"

#define APP_COPY_NOTICE \
	"Written by Alvaro Lopez Ortega <alvaro@alobbs.com>\n\n"	               \
	"Copyright (C) 2001-2010 Alvaro Lopez Ortega.\n"                               \
	"This is free software; see the source for copying conditions.  There is NO\n" \
	"warranty; not even for MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.\n"

#define ALPHA_NUM            "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789"
#define PASSWORD_LEN         16
#define DEFAULT_PORT         9090
#define TIMEOUT              "25"
#define DEFAULT_DOCUMENTROOT CHEROKEE_DATADIR "/admin"
#define DEFAULT_CONFIG_FILE  CHEROKEE_CONFDIR "/cherokee.conf"
#define DEFAULT_UNIX_SOCKET  "/tmp/cherokee-admin-scgi.socket"
#define DEFAULT_BIND         "127.0.0.1"
#define RULE_PRE             "vserver!1!rule!"

static int         port          = DEFAULT_PORT;
static const char *document_root = DEFAULT_DOCUMENTROOT;
static const char *config_file   = DEFAULT_CONFIG_FILE;
static const char *bind_to       = DEFAULT_BIND;
static int         debug         = 0;
static int         unsecure      = 0;
static int         scgi_port     = 0;

static ret_t
find_empty_port (int starting, int *port)
{
	ret_t             ret;
	cherokee_socket_t s;
	int               p     = starting;
	cherokee_buffer_t bind_ = CHEROKEE_BUF_INIT;

	cherokee_buffer_add_str (&bind_, "127.0.0.1");

	cherokee_socket_init (&s);
	cherokee_socket_set_client (&s, AF_INET);

	while (true) {
		ret = cherokee_socket_bind (&s, p, &bind_);
		if (ret == ret_ok)
			break;

		if (++p > 0XFFFF)
			return ret_error;
	}

	cherokee_socket_close (&s);

	cherokee_socket_mrproper (&s);
	cherokee_buffer_mrproper (&bind_);

	*port = p;
	return ret_ok;
}

static ret_t
generate_admin_password (cherokee_buffer_t *buf)
{
	cuint_t i;
	cuint_t n;

	srand(getpid()*time(NULL));

	for (i=0; i<PASSWORD_LEN; i++) {
		n = rand()%(sizeof(ALPHA_NUM)-1);
		cherokee_buffer_add_char (buf, ALPHA_NUM[n]);
	}

	return ret_ok;
}

static ret_t
remove_old_socket (const char *path)
{
	int         re;
	struct stat info;

	/* It might not exist
	 */
	re = stat (path, &info);
	if (re != 0) {
		return ret_ok;
	}

	/* If exist, it must be a socket
	 */
	if (! S_ISSOCK(info.st_mode)) {
		PRINT_MSG ("ERROR: Something happens; '%s' isn't a socket.\n", path);
		return ret_error;
	}

	/* Remove it
	 */
	re = unlink (path);
	if (re != 0) {
		PRINT_MSG ("ERROR: Couldn't remove unix socket '%s'.\n", path);
		return ret_error;
	}

	return ret_ok;
}


static ret_t
config_server (cherokee_server_t *srv)
{
	ret_t                  ret;
	cherokee_config_node_t conf;
	cherokee_buffer_t      buf       = CHEROKEE_BUF_INIT;
	cherokee_buffer_t      password  = CHEROKEE_BUF_INIT;
	cherokee_buffer_t      rrd_dir   = CHEROKEE_BUF_INIT;
	cherokee_buffer_t      rrd_bin   = CHEROKEE_BUF_INIT;
	cherokee_buffer_t      fake;

	/* Print some information
	 */
	printf ("\n");

	if (unsecure == 0) {
		ret = generate_admin_password (&password);
		if (ret != ret_ok)
			return ret;

		printf ("Login:\n"
			"  User:              admin\n"
			"  One-time Password: %s\n\n", password.buf);
	}

	printf ("Web Interface:\n"
		"  URL:               http://%s:%d/\n\n",
		(bind_to) ? bind_to : "localhost", port);

	/* Configure the embedded server
	 */
	if (scgi_port > 0) {
		ret = find_empty_port (scgi_port, &scgi_port);
	} else {
		ret = remove_old_socket (DEFAULT_UNIX_SOCKET);
	}
	if (ret != ret_ok) {
		return ret_error;
	}

	cherokee_buffer_add_va  (&buf, "server!bind!1!port = %d\n", port);
	cherokee_buffer_add_str (&buf, "server!thread_number = 1\n");
	cherokee_buffer_add_str (&buf, "server!ipv6 = 1\n");
	cherokee_buffer_add_str (&buf, "server!max_connection_reuse = 0\n");

	if (bind_to) {
		cherokee_buffer_add_va (&buf, "server!bind!1!interface = %s\n", bind_to);
	}

	cherokee_buffer_add_str (&buf, "vserver!1!nick = default\n");
	cherokee_buffer_add_va  (&buf, "vserver!1!document_root = %s\n", document_root);

	if (scgi_port <= 0) {
		cherokee_buffer_add_va  (&buf,
					 "source!1!nick = app-logic\n"
					 "source!1!type = interpreter\n"
					 "source!1!timeout = " TIMEOUT "\n"
					 "source!1!host = %s\n"
					 "source!1!interpreter = %s/server.py %s %s %s\n"
					 "source!1!env_inherited = 1\n",
					 DEFAULT_UNIX_SOCKET, document_root,
					 DEFAULT_UNIX_SOCKET, config_file,
					 (debug) ? "-x" : "");

	} else {
		cherokee_buffer_add_va  (&buf,
					 "source!1!nick = app-logic\n"
					 "source!1!type = interpreter\n"
					 "source!1!timeout = " TIMEOUT "\n"
					 "source!1!host = localhost:%d\n"
					 "source!1!interpreter = %s/server.py %d %s %s\n"
					 "source!1!env_inherited = 1\n",
					 scgi_port, document_root,
					 scgi_port, config_file,
					 (debug) ? "-x" : "");
	}

	if (debug) {
		cherokee_buffer_add_str  (&buf, "source!1!debug = 1\n");
	}

	cherokee_buffer_add_str  (&buf,
				  RULE_PRE "1!match = default\n"
				  RULE_PRE "1!handler = scgi\n"
				  RULE_PRE "1!encoder!gzip = 1\n"
				  RULE_PRE "1!timeout = " TIMEOUT "\n"
				  RULE_PRE "1!handler!balancer = round_robin\n"
				  RULE_PRE "1!handler!balancer!source!1 = 1\n");

	if ((unsecure == 0) &&
	    (!cherokee_buffer_is_empty (&password)))
	{
		cherokee_buffer_add_va (&buf,
					RULE_PRE "1!auth = authlist\n"
					RULE_PRE "1!auth!methods = digest\n"
					RULE_PRE "1!auth!realm = Cherokee-admin\n"
					RULE_PRE "1!auth!list!1!user = admin\n"
					RULE_PRE "1!auth!list!1!password = %s\n",
					password.buf);
	}

	cherokee_buffer_add_str (&buf,
				 RULE_PRE "2!match = directory\n"
				 RULE_PRE "2!match!directory = /about\n"
				 RULE_PRE "2!handler = server_info\n");

	cherokee_buffer_add_str (&buf,
				 RULE_PRE "3!match = directory\n"
				 RULE_PRE "3!match!directory = /static\n"
				 RULE_PRE "3!handler = file\n"
				 RULE_PRE "3!handler!iocache = 0\n");

	if (! debug) {
		cherokee_buffer_add_str (&buf, RULE_PRE "3!expiration = time\n");
		cherokee_buffer_add_str (&buf, RULE_PRE "3!expiration!time = 30d\n");
	}

	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "4!match = request\n"
				 RULE_PRE "4!match!request = ^/favicon.ico$\n"
				 RULE_PRE "4!document_root = %s/static/images\n"
				 RULE_PRE "4!handler = file\n", document_root);

	if (! debug) {
		cherokee_buffer_add_str (&buf, RULE_PRE "4!expiration = time\n");
		cherokee_buffer_add_str (&buf, RULE_PRE "4!expiration!time = 30d\n");
	}

	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "5!match = directory\n"
				 RULE_PRE "5!match!directory = /icons_local\n"
				 RULE_PRE "5!handler = file\n"
				 RULE_PRE "5!handler!iocache = 0\n"
				 RULE_PRE "5!document_root = %s\n", CHEROKEE_ICONSDIR);

	if (! debug) {
		cherokee_buffer_add_str (&buf, RULE_PRE "5!expiration = time\n");
		cherokee_buffer_add_str (&buf, RULE_PRE "5!expiration!time = 30d\n");
	}


	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "6!match = directory\n"
				 RULE_PRE "6!match!directory = /CTK\n"
				 RULE_PRE "6!handler = file\n"
				 RULE_PRE "6!handler!iocache = 0\n"
				 RULE_PRE "6!document_root = %s/CTK/static\n", document_root);

	if (! debug) {
		cherokee_buffer_add_str (&buf, RULE_PRE "6!expiration = time\n");
		cherokee_buffer_add_str (&buf, RULE_PRE "6!expiration!time = 30d\n");
	}


	/* Embedded help
	 */
	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "7!match = and\n"
				 RULE_PRE "7!match!left = directory\n"
				 RULE_PRE "7!match!left!directory = /help\n"
				 RULE_PRE "7!match!right = not\n"
				 RULE_PRE "7!match!right!right = extensions\n"
				 RULE_PRE "7!match!right!right!extensions = html\n"
				 RULE_PRE "7!handler = file\n");

	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "8!match = fullpath\n"
				 RULE_PRE "8!match!fullpath!1 = /static/help_404.html\n"
				 RULE_PRE "8!handler = file\n"
				 RULE_PRE "8!handler!iocache = 0\n"
				 RULE_PRE "8!document_root = %s\n", document_root);

	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "9!match = and\n"
				 RULE_PRE "9!match!left = directory\n"
				 RULE_PRE "9!match!left!directory = /help\n"
				 RULE_PRE "9!match!right = not\n"
				 RULE_PRE "9!match!right!right = exists\n"
				 RULE_PRE "9!match!right!right!iocache = 0\n"
				 RULE_PRE "9!match!right!right!match_any = 1\n"
				 RULE_PRE "9!handler = redir\n"
				 RULE_PRE "9!handler!rewrite!1!show = 1\n"
				 RULE_PRE "9!handler!rewrite!1!substring = /static/help_404.html\n");

	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "10!match = directory\n"
				 RULE_PRE "10!match!directory = /help\n"
				 RULE_PRE "10!match!final = 0\n"
				 RULE_PRE "10!document_root = %s\n", CHEROKEE_DOCDIR);

	/* GZip
	 */
	cherokee_buffer_add_va  (&buf,
				 RULE_PRE "15!match = extensions\n"
				 RULE_PRE "15!match!extensions = css,js,html\n"
				 RULE_PRE "15!match!final = 0\n"
				 RULE_PRE "15!encoder!gzip = 1\n");

	/* RRDtool graphs
	 */
	cherokee_config_node_init (&conf);
	cherokee_buffer_fake (&fake, config_file, strlen(config_file));

	ret = cherokee_config_reader_parse (&conf, &fake);
	if (ret == ret_ok) {
		cherokee_config_node_copy (&conf, "server!collector!rrdtool_path", &rrd_bin);
		cherokee_config_node_copy (&conf, "server!collector!database_dir", &rrd_dir);
	}

	if (! cherokee_buffer_is_empty (&rrd_bin)) {
		cherokee_buffer_add_va  (&buf,
					 RULE_PRE "20!handler!rrdtool_path = %s\n", rrd_bin.buf);
	}

	if (! cherokee_buffer_is_empty (&rrd_dir)) {
		cherokee_buffer_add_va  (&buf,
					 RULE_PRE "20!handler!database_dir = %s\n", rrd_dir.buf);
	}

	cherokee_buffer_add_str (&buf,
				 RULE_PRE "20!match = directory\n"
				 RULE_PRE "20!match!directory = /graphs\n"
				 RULE_PRE "20!handler = render_rrd\n");

	cherokee_buffer_add_str (&buf, RULE_PRE "20!document_root = ");
	cherokee_tmp_dir_copy   (&buf);
	cherokee_buffer_add_va  (&buf, "/cherokee/rrd-cache\n");

	/* MIME types
	 */
	cherokee_buffer_add_str (&buf,
				 "mime!text/javascript!extensions = js\n"
				 "mime!text/css!extensions = css\n"
				 "mime!image/png!extensions = png\n"
				 "mime!image/jpeg!extensions = jpeg,jpg\n"
				 "mime!image/svg+xml!extensions = svg,svgz\n"
				 "mime!image/gif!extensions = gif\n");

	ret = cherokee_server_read_config_string (srv, &buf);
	if (ret != ret_ok) {
		PRINT_ERROR_S ("Could not initialize the server\n");
		return ret;
	}

	cherokee_config_node_mrproper (&conf);

	cherokee_buffer_mrproper (&rrd_bin);
	cherokee_buffer_mrproper (&rrd_dir);
	cherokee_buffer_mrproper (&password);
	cherokee_buffer_mrproper (&buf);

	return ret_ok;
}


static void
print_help (void)
{
	printf (APP_NAME "\n"
		"Usage: cherokee-admin [options]\n\n"
		"  -h,  --help                   Print this help\n"
		"  -V,  --version                Print version and exit\n"
		"  -x,  --debug                  Enables debug\n"
		"  -u,  --unsecure               Turn off the authentication\n"
		"  -b,  --bind[=IP]              Bind net iface; no arg means all\n"
		"  -d,  --appdir=DIR             Application directory\n"
		"  -p,  --port=NUM               TCP port\n"
		"  -t,  --internal-tcp           Use TCP for internal communications\n"
		"  -C,  --target=PATH            Configuration file to modify\n\n"
		"Report bugs to " PACKAGE_BUGREPORT "\n");
}

static void
process_parameters (int argc, char **argv)
{
	int c;

	struct option long_options[] = {
		{"help",         no_argument,       NULL, 'h'},
		{"version",      no_argument,       NULL, 'V'},
		{"debug",        no_argument,       NULL, 'x'},
		{"unsecure",     no_argument,       NULL, 'u'},
		{"internal-tcp", no_argument,       NULL, 't'},
		{"bind",         optional_argument, NULL, 'b'},
		{"appdir",       required_argument, NULL, 'd'},
		{"port",         required_argument, NULL, 'p'},
		{"target",       required_argument, NULL, 'C'},
		{NULL, 0, NULL, 0}
	};

	while ((c = getopt_long(argc, argv, "hVxutb::d:p:C:", long_options, NULL)) != -1) {
		switch(c) {
		case 'b':
			if (optarg)
				bind_to = strdup(optarg);
			else
				bind_to = NULL;
			break;
		case 'p':
			port = atoi(optarg);
			break;
		case 'd':
			document_root = strdup(optarg);
			break;
		case 'C':
			config_file = strdup(optarg);
			break;
		case 'x':
			debug = 1;
			break;
		case 'u':
			unsecure = 1;
			break;
		case 't':
			scgi_port = 4000;
			break;
		case 'V':
			printf (APP_NAME " " PACKAGE_VERSION "\n" APP_COPY_NOTICE);
			exit(0);
		case 'h':
		case '?':
		default:
			print_help();
			exit(0);
		}
	}
}


int
main (int argc, char **argv)
{
	ret_t              ret;
	cherokee_server_t *srv;

#ifdef SIGPIPE
        signal (SIGPIPE, SIG_IGN);
#endif
#ifdef SIGCHLD
        signal (SIGCHLD, SIG_IGN);
#endif

	cherokee_init();
	cherokee_spawner_set_active (false);
	process_parameters (argc, argv);

	ret = cherokee_server_new (&srv);
	if (ret != ret_ok)
		exit (EXIT_ERROR);

	ret = config_server (srv);
	if (ret != ret_ok)
		exit (EXIT_ERROR);

	ret = cherokee_server_initialize (srv);
	if (ret != ret_ok)
		exit (EXIT_ERROR);

	for (;;) {
		cherokee_server_step (srv);
	}

	cherokee_server_stop (srv);
	cherokee_server_free (srv);

	cherokee_mrproper();
	return EXIT_OK;
}
