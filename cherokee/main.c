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

#include "config.h"
#include <stdio.h>
#include <stdlib.h>
#include <signal.h>
#include <unistd.h>
#include <string.h>

#ifdef HAVE_GETOPT_H
# include <getopt.h>
#endif

#include "server.h"

static cherokee_server_t  *srv         = NULL;
static char               *config_file = NULL;
static cherokee_boolean_t  daemon_mode = false;


static void
restart_server (int code)
{	
	cherokee_server_handle_HUP (srv);
}

static void
panic_handler (int code)
{
	cherokee_server_handle_panic (srv);
}

static void
process_parameters (int argc, char **argv)
{
	int c;

	while ((c = getopt(argc, argv, "C:b")) != -1) {
		switch(c) {
		case 'C':
			config_file = strdup(optarg);
			break;

		case 'b':
			daemon_mode = true;
			break;

		default:
			fprintf (stderr, "Usage: %s [-C configfile] [-b]\n", argv[0]);
			exit(1);
		}
	}
}


int
main (int argc, char **argv)
{
	ret_t ret;
	
	ret = cherokee_server_new (&srv);
	if (ret < ret_ok) return 1;
	
	process_parameters (argc, argv);

	signal (SIGPIPE, SIG_IGN);
	signal (SIGHUP,  restart_server);
	signal (SIGSEGV, panic_handler);

	ret = cherokee_server_read_config_file (srv, config_file);
	if (ret != ret_ok) return 2;
		
	ret = cherokee_server_init (srv);
	if (ret != ret_ok) return 3;

	if (daemon_mode) {
		cherokee_server_daemonize (srv);
	}

	cherokee_server_unlock_threads (srv);

	for (;;) {
		cherokee_server_step (srv);
	}
	
	cherokee_server_free (srv);

	if (config_file)
		free (config_file);

	return 0;
}
