/* -*- Mode: C; tab-width: 8; indent-tabs-mode: t; c-basic-offset: 8 -*- */

/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2009 Alvaro Lopez Ortega
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
#include "collector_rrd.h"
#include "module.h"
#include "plugin_loader.h"
#include "bogotime.h"
#include "virtual_server.h"
#include "server-protected.h"
#include "util.h"

#include <unistd.h>
#include <errno.h>
#include <sys/types.h>
#include <signal.h>
#include <sys/stat.h>

#ifdef HAVE_SYS_WAIT_H
# include <sys/wait.h>
#endif


/* Plug-in initialization
 */
PLUGIN_INFO_COLLECTOR_EASIEST_INIT (rrd);

#define ELAPSE_UPDATE 60 
#define ELAPSE_RENDER 60

#define ENTRIES "collector,rrd"

static ret_t read_rrdtool  (cherokee_collector_rrd_t *rrd, cherokee_buffer_t *);
static ret_t write_rrdtool (cherokee_collector_rrd_t *rrd, cherokee_buffer_t *);

static struct interval_t {
	const char *interval;
	const char *description;
} intervals[] = {
	{ "1h", "1 Hour" },
	{ "6h", "6 Hours" },
	{ "1d", "1 Day" },
	{ "1w", "1 Week" },
	{ "1m", "1 Month" },
	{ NULL, NULL }
};


static ret_t
spawn_rrdtool (cherokee_collector_rrd_t *rrd)
{
#ifdef HAVE_FORK
	int    re;
        pid_t  pid;
	char  *argv[3];
	int    fds_to[2];
        int    fds_from[2];
	
	/* There might be a live process */
	if ((rrd->write_fd != -1) &&
	    (rrd->read_fd != -1) &&
	    (rrd->pid != -1))
	{
		return ret_ok;
	}

	TRACE (ENTRIES, "Spawning a new RRDtool instance: %s -\n", rrd->path_rrdtool.buf);

	/* Create communication pipes */
	re = pipe(fds_to);
	if (re != 0) {
		return ret_error;
	}

	re = pipe(fds_from);
	if (re != 0) {
		return ret_error;
	}

	/* Spawn the new child process */
	pid = fork();
	switch (pid) {
        case 0:
		argv[0] = rrd->path_rrdtool.buf;
		argv[1] = (char *) "-";
		argv[2] = NULL;

		/* Move stdout to fd_from[1] */
		dup2 (fds_from[1], STDOUT_FILENO);
                close (fds_from[1]);
                close (fds_from[0]);

                /* Move the stdin to fd_to[0] */
                dup2 (fds_to[0], STDIN_FILENO);
                close (fds_to[0]);
                close (fds_to[1]);

		/* Execute it */
		re = execv(argv[0], argv);

		LOG_ERRNO (errno, cherokee_err_error, "execv failed cmd='%s': ${errno}\n", argv[0]);
		exit (EXIT_ERROR);

        case -1:
		LOG_ERRNO (errno, cherokee_err_error, "Fork failed pid=%d: ${errno}\n", pid);
                break;

        default:
                close (fds_from[1]);
                close (fds_to[0]);
			 
                rrd->write_fd = fds_to[1];
                rrd->read_fd  = fds_from[0];
                rrd->pid      = pid;

                fcntl (rrd->write_fd, F_SETFD, FD_CLOEXEC);
                fcntl (rrd->read_fd,  F_SETFD, FD_CLOEXEC);
		break;
	}

	return ret_ok;
#else
	return ret_error;
#endif
}


static ret_t
kill_and_clean (cherokee_collector_rrd_t *rrd,
		cherokee_boolean_t        do_kill)
{
	int status;

	if (rrd->pid != -1) {
		if (do_kill) {
			kill (rrd->pid, SIGINT);
		}
		waitpid (rrd->pid, &status, 0);
		rrd->pid = -1;
	}

	if (rrd->write_fd) {
		close (rrd->write_fd);
		rrd->write_fd = -1;
	}

	if (rrd->read_fd) {
		close (rrd->read_fd);
		rrd->read_fd = -1;
	}

	return ret_ok;
}


static ret_t
check_and_create_db (cherokee_collector_rrd_t *rrd,
		     cherokee_buffer_t        *params)
{
	int          re;
	ret_t        ret;
	struct stat  info;
	char        *slash;

	/* It exists
	 */
	re = cherokee_stat (rrd->path_database.buf, &info);
	if ((re == 0) && (info.st_size > 0)) {
		return ret_ok;
	}

	/* Write access
	 */
	slash = strrchr (rrd->path_database.buf, '/');
	if (slash == NULL) {
		return ret_error;
	}

	*slash = '\0';
	re = access (rrd->path_database.buf, W_OK);
	if (re != 0) {
		LOG_ERRNO (errno, cherokee_err_error, "Cannot write in %s: ${errno}\n", rrd->path_database.buf);
		return ret_error;
	}
	*slash = '/';

	TRACE (ENTRIES, "Creating RRDtool database: %s", params->buf);

	/* Spawn rrdtool
	 */
	ret = spawn_rrdtool (rrd);
	if (unlikely (ret != ret_ok)) {
		return ret_error;
	}

	/* Write command
	 */
	ret = write_rrdtool (rrd, params);
	if (unlikely (ret != ret_ok)) {
		return ret_error;
	}

	return ret_ok;
}


static ret_t
read_rrdtool (cherokee_collector_rrd_t *rrd,
	      cherokee_buffer_t        *buffer)
{
	ret_t  ret;
	size_t got;

	do {
		ret = cherokee_buffer_read_from_fd (buffer, rrd->read_fd,
						    DEFAULT_RECV_SIZE, &got);
	} while (ret == ret_eagain);

	return ret;
}

static ret_t
write_rrdtool (cherokee_collector_rrd_t *rrd,
	       cherokee_buffer_t        *buffer)
{
	ssize_t written;

	while (true) {
		written = write (rrd->write_fd, buffer->buf, buffer->len);
		if (written >= (ssize_t) buffer->len) {
			cherokee_buffer_clean (buffer);
			return ret_ok;
				    
		} else if (written > 0) {
			cherokee_buffer_move_to_begin (buffer, written);
			continue;

		} else {
			switch (errno) {
			case EINTR:
				continue;
			default:
				return ret_error;
			}
		}
	}

	SHOULDNT_HAPPEN;
	return ret_error;
}

static ret_t
command_rrdtool (cherokee_collector_rrd_t *rrd,
		 cherokee_buffer_t        *buf)
{
	ret_t ret;

	TRACE (ENTRIES, "Refreshing images: %s", buf->buf);

	/* Spawn rrdtool
	 */
	ret = spawn_rrdtool (rrd);
	if (unlikely (ret != ret_ok)) {
		return ret_error;
	}

	/* Write command
	 */
	ret = write_rrdtool (rrd, buf);
	if (unlikely (ret != ret_ok)) {
		return ret_error;
	}

	/* Read response
	 */
	cherokee_buffer_clean (buf);

	ret = read_rrdtool (rrd, buf);
	if (unlikely (ret != ret_ok)) {
		kill_and_clean(rrd, false);
		return ret_error;
	}

	if (cherokee_buffer_is_empty (buf)) {
		LOG_ERROR_S ("RRDtool empty response\n");
		return ret_error;

	} else if (strncmp (buf->buf, "ERROR", 5) == 0) {
		LOG_ERROR ("RRDtool: %s\n", buf->buf);
		return ret_error;
	}

	return ret_ok;
}

static ret_t
update_generic (cherokee_collector_rrd_t *rrd,
		cherokee_buffer_t        *params)
{
	ret_t ret;

	/* Spawn new interpreter
	 */
	ret = spawn_rrdtool (rrd);
	if (ret != ret_ok) {
		LOG_ERROR ("Couldn't spawn rrdtool (%s)\n", rrd->path_rrdtool.buf);
		kill_and_clean(rrd, false);
		return ret_error;
	}

	/* Send it
	 */
	TRACE (ENTRIES, "Sending to RRDtool: %s", params->buf);

	ret = write_rrdtool (rrd, params);
	if (unlikely (ret != ret_ok)) {
		kill_and_clean(rrd, false);
		return ret_error;
	}

	/* Read response
	 */
	cherokee_buffer_clean (params);
	ret = read_rrdtool (rrd, params);
	if (unlikely (ret != ret_ok)) {
		kill_and_clean(rrd, false);
		return ret_error;
	}

	/* Check everything was alright
	 */
	if ((params->len < 3) &&
	    (strncmp (params->buf, "OK", 2) != 0))
	{
		kill_and_clean(rrd, false);
		return ret_error;
	}

	return ret_ok;
}

static void
check_img_dir (cherokee_collector_rrd_t *rrd)
{
	int re;

	cherokee_buffer_add_str (&rrd->database_dir, "/images");

	re = access (rrd->database_dir.buf, W_OK);
	if (re != 0) {
		mkdir (rrd->database_dir.buf, 0775);		
	}

	cherokee_buffer_drop_ending (&rrd->database_dir, 7);
}


static void 
render_srv_cb (void *param) 
{
	struct interval_t        *i;
	cherokee_collector_rrd_t *rrd = COLLECTOR_RRD(param);
	cherokee_buffer_t        *buf = &rrd->tmp;

	check_img_dir (rrd);
	cherokee_buffer_clean (buf);

	/* Accepts
	 */
	for (i = intervals; i->interval != NULL; i++) {
		cherokee_buffer_add_str    (buf, "graph ");
		cherokee_buffer_add_buffer (buf, &rrd->database_dir);
		cherokee_buffer_add_va     (buf, "/images/server_accepts_%s.png ", i->interval);
		cherokee_buffer_add_va     (buf, "--imgformat PNG --width 600 --height 350 --start -%s ", i->interval);
		cherokee_buffer_add_va     (buf, "--title \"Accepted Connections: %s\" ", i->interval);
		cherokee_buffer_add_str    (buf, "--vertical-label \"conn/s\" ");
		cherokee_buffer_add_va     (buf, "DEF:accepts=%s:Accepts:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:accepts_min=%s:Accepts:MIN ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:accepts_max=%s:Accepts:MAX ", rrd->path_database.buf);
		cherokee_buffer_add_str    (buf, "CDEF:accepts_minmax=accepts_max,accepts_min,- ");
		cherokee_buffer_add_str    (buf, "AREA:accepts_min#ffffff: ");
		cherokee_buffer_add_str    (buf, "STACK:accepts_minmax#00f000: ");
		cherokee_buffer_add_str    (buf, "LINE1:accepts_min#a0a0a0 ");
		cherokee_buffer_add_str    (buf, "LINE1:accepts_max#a0a0a0 ");
		cherokee_buffer_add_str    (buf, "LINE2:accepts#00a735:Connections ");
		cherokee_buffer_add_str    (buf, "GPRINT:accepts:MIN:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:accepts:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:accepts:MAX:%.2lf");
		cherokee_buffer_add_str    (buf, "\n");

		command_rrdtool (rrd, buf);
		cherokee_buffer_clean (buf);
	}

	/* Timeouts
	 */
	for (i = intervals; i->interval != NULL; i++) {
		cherokee_buffer_add_str    (buf, "graph ");
		cherokee_buffer_add_buffer (buf, &rrd->database_dir);
		cherokee_buffer_add_va     (buf, "/images/server_timeouts_%s.png ", i->interval);
		cherokee_buffer_add_va     (buf, "--imgformat PNG --width 600 --height 350 --start -%s ", i->interval);
		cherokee_buffer_add_va     (buf, "--title \"Timeouts: %s\" ", i->interval);
		cherokee_buffer_add_str    (buf, "--vertical-label \"timeouts/s\" ");
		cherokee_buffer_add_va     (buf, "DEF:timeouts=%s:Timeouts:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:timeouts_min=%s:Timeouts:MIN ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:timeouts_max=%s:Timeouts:MAX ", rrd->path_database.buf);
		cherokee_buffer_add_str    (buf, "CDEF:timeouts_minmax=timeouts_max,timeouts_min,- ");
		cherokee_buffer_add_str    (buf, "AREA:timeouts_min#ffffff: ");
		cherokee_buffer_add_str    (buf, "STACK:timeouts_minmax#00f000: ");
		cherokee_buffer_add_str    (buf, "LINE1:timeouts_min#a0a0a0 ");
		cherokee_buffer_add_str    (buf, "LINE1:timeouts_max#a0a0a0 ");
		cherokee_buffer_add_str    (buf, "LINE2:timeouts#00a735:Connections ");
		cherokee_buffer_add_str    (buf, "GPRINT:timeouts:MIN:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:timeouts:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:timeouts:MAX:%.2lf");
		cherokee_buffer_add_str    (buf, "\n");

		command_rrdtool (rrd, buf);
		cherokee_buffer_clean (buf);
	}

	/* Traffic
	 */
	for (i = intervals; i->interval != NULL; i++) {
		cherokee_buffer_add_str    (buf, "graph ");
		cherokee_buffer_add_buffer (buf, &rrd->database_dir);
		cherokee_buffer_add_va     (buf, "/images/server_traffic_%s.png ", i->interval);
		cherokee_buffer_add_va     (buf, "--imgformat PNG --width 600 --height 350 --start -%s ", i->interval);
		cherokee_buffer_add_va     (buf, "--title \"Traffic: %s\" ", i->interval);
		cherokee_buffer_add_str    (buf, "--vertical-label \"bytes/s\" ");
		cherokee_buffer_add_va     (buf, "DEF:rx=%s:RX:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:tx=%s:TX:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_str    (buf, "LINE1:rx#aa0000:In ");
		cherokee_buffer_add_str    (buf, "LINE1:tx#00aa00:Out ");
		cherokee_buffer_add_str    (buf, "GPRINT:rx:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:tx:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "\n");

		command_rrdtool (rrd, buf);
		cherokee_buffer_clean (buf);
	}
}

static void 
render_vsrv_cb (void *param) 
{
	struct interval_t             *i;
	cherokee_collector_vsrv_rrd_t *rrd  = COLLECTOR_VSRV_RRD(param);
	cherokee_buffer_t             *buf  = &rrd->tmp;
	cherokee_virtual_server_t     *vsrv = VSERVER(rrd->vsrv_ref);
	
	check_img_dir (rrd);
	cherokee_buffer_clean (buf);

	/* Traffic
	 */
	for (i = intervals; i->interval != NULL; i++) {
		cherokee_buffer_add_str    (buf, "graph ");
		cherokee_buffer_add_buffer (buf, &rrd->database_dir);
		cherokee_buffer_add_va     (buf, "/images/vserver_traffic_%s_%s.png ", vsrv->name.buf, i->interval);
		cherokee_buffer_add_va     (buf, "--imgformat PNG --width 600 --height 350 --start -%s ", i->interval);
		cherokee_buffer_add_va     (buf, "--title \"Traffic, %s: %s\" ", vsrv->name.buf, i->interval);
		cherokee_buffer_add_str    (buf, "--vertical-label \"bytes/s\" ");
		cherokee_buffer_add_va     (buf, "DEF:rx=%s:RX:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_va     (buf, "DEF:tx=%s:TX:AVERAGE ", rrd->path_database.buf);
		cherokee_buffer_add_str    (buf, "LINE1:rx#aa0000:In ");
		cherokee_buffer_add_str    (buf, "LINE1:tx#00aa00:Out ");
		cherokee_buffer_add_str    (buf, "GPRINT:rx:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "GPRINT:tx:AVERAGE:%.2lf ");
		cherokee_buffer_add_str    (buf, "\n");

		command_rrdtool (rrd, buf);
		cherokee_buffer_clean (buf);
	}

}


static void 
update_srv_cb (void *param) 
{
	ret_t                     ret;
	cherokee_collector_rrd_t *rrd = COLLECTOR_RRD(param);

	/* Build the RRDtool string
	 */
	cherokee_buffer_clean        (&rrd->tmp);
	cherokee_buffer_add_str      (&rrd->tmp, "update ");
	cherokee_buffer_add_buffer   (&rrd->tmp, &rrd->path_database);
	cherokee_buffer_add_str      (&rrd->tmp, " N:");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR(rrd)->accepts_partial);
	cherokee_buffer_add_str      (&rrd->tmp, ":");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR(rrd)->timeouts_partial);
	cherokee_buffer_add_str      (&rrd->tmp, ":");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR_BASE(rrd)->rx_partial);
	cherokee_buffer_add_str      (&rrd->tmp, ":");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR_BASE(rrd)->tx_partial);
	cherokee_buffer_add_str      (&rrd->tmp, "\n");

	/* Update
	 */
	ret = update_generic (rrd, &rrd->tmp);
	if (ret != ret_ok) {
		return;
	}

	/* Begin partial counting from scratch
	 */
	COLLECTOR(rrd)->accepts_partial  = 0;
	COLLECTOR(rrd)->timeouts_partial = 0;
	COLLECTOR_BASE(rrd)->rx_partial  = 0;
	COLLECTOR_BASE(rrd)->tx_partial  = 0;
}


static ret_t
srv_free (cherokee_collector_rrd_t *rrd)
{
	cherokee_buffer_mrproper (&rrd->path_rrdtool);
	cherokee_buffer_mrproper (&rrd->path_database);
	cherokee_buffer_mrproper (&rrd->tmp);

	return kill_and_clean (rrd, true);
}



/* Virtual Servers
 */

static void 
update_vsrv_cb (void *param) 
{
	ret_t                     ret;
	cherokee_collector_rrd_t *rrd = COLLECTOR_RRD(param);

	/* Build params
	 */
	cherokee_buffer_clean        (&rrd->tmp);
	cherokee_buffer_add_str      (&rrd->tmp, "update ");
	cherokee_buffer_add_buffer   (&rrd->tmp, &rrd->path_database);
	cherokee_buffer_add_str      (&rrd->tmp, " N:");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR_BASE(rrd)->rx_partial);
	cherokee_buffer_add_str      (&rrd->tmp, ":");
	cherokee_buffer_add_ullong10 (&rrd->tmp, COLLECTOR_BASE(rrd)->tx_partial);
	cherokee_buffer_add_str      (&rrd->tmp, "\n");

	/* Update
	 */
	ret = update_generic (rrd, &rrd->tmp);
	if (ret != ret_ok) {
		return;
	}
	
	/* Begin partial counting from scratch
	 */
	COLLECTOR_BASE(rrd)->rx_partial = 0;
	COLLECTOR_BASE(rrd)->tx_partial = 0;
}


static ret_t
vsrv_free (cherokee_collector_vsrv_rrd_t *rrd)
{
	UNUSED(rrd);
	return ret_ok;
}


static ret_t
vsrv_new (cherokee_collector_t           *collector,
	  cherokee_virtual_server_t      *vsrv,
	  cherokee_config_node_t         *config,
	  cherokee_collector_vsrv_rrd_t **collector_vsrv)
{
	ret_t ret;
	CHEROKEE_NEW_STRUCT (n, collector_vsrv_rrd);

	/* Base class initialization
	 */
	ret = cherokee_collector_vsrv_init (COLLECTOR_VSRV(n), config);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Virtual methods
	 */
	COLLECTOR(n)->new_vsrv  = NULL;
	COLLECTOR_BASE(n)->free = (collector_func_free_t) vsrv_free;

	/* Default values
	 */
	n->write_fd      = -1;
	n->read_fd       = -1;
	n->pid           = -1;
	n->vsrv_ref      = vsrv;
	n->render_elapse = COLLECTOR_RRD(collector)->render_elapse;

	cherokee_buffer_init (&n->tmp);

	/* Configuration
	 */
	cherokee_buffer_init       (&n->database_dir);
	cherokee_buffer_add_buffer (&n->database_dir,
				    &COLLECTOR_RRD(VSERVER_SRV(vsrv)->collector)->database_dir);

	cherokee_buffer_init       (&n->path_rrdtool);
	cherokee_buffer_add_buffer (&n->path_rrdtool, 
				    &COLLECTOR_RRD(VSERVER_SRV(vsrv)->collector)->path_rrdtool);

	cherokee_buffer_init       (&n->path_database);
	cherokee_buffer_add_buffer (&n->path_database, &n->database_dir);
	cherokee_buffer_add_str    (&n->path_database, "/vserver_");
	cherokee_buffer_add_buffer (&n->path_database, &vsrv->name);
	cherokee_buffer_add_str    (&n->path_database, ".rrd");

	/* Check whether the DB exists
	 */
	cherokee_buffer_clean      (&n->tmp);
	cherokee_buffer_add_str    (&n->tmp, "create ");
	cherokee_buffer_add_buffer (&n->tmp, &n->path_database);
	cherokee_buffer_add_str    (&n->tmp, " --step ");
	cherokee_buffer_add_long10 (&n->tmp, ELAPSE_UPDATE);
	cherokee_buffer_add_str    (&n->tmp, " ");

	/* Data Sources */
	cherokee_buffer_add_va     (&n->tmp, "DS:RX:ABSOLUTE:%d:U:U ", ELAPSE_UPDATE*10);
	cherokee_buffer_add_va     (&n->tmp, "DS:TX:ABSOLUTE:%d:U:U ", ELAPSE_UPDATE*10);

	/* Archives */
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:288:797 ");

	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:288:797 ");

	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:288:797 ");	   
	cherokee_buffer_add_str    (&n->tmp, "\n");

	ret = check_and_create_db (n, &n->tmp);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Add the commit callback
	 */
	ret = cherokee_bogotime_add_callback (update_vsrv_cb, n, ELAPSE_UPDATE);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Add the re-rendering callback
	 */
	ret = cherokee_bogotime_add_callback (render_vsrv_cb, n, ELAPSE_RENDER);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Return object
	 */
	*collector_vsrv = n;
	return ret_ok;
}



/* Plug-in constructor
 */

ret_t
cherokee_collector_rrd_new (cherokee_collector_rrd_t **rrd,
			    cherokee_plugin_info_t    *info,
			    cherokee_config_node_t    *config)
{
	ret_t                   ret;
	cherokee_config_node_t *subconf;
	CHEROKEE_NEW_STRUCT (n, collector_rrd);
	   
	/* Base class initialization
	 */
	ret = cherokee_collector_init (COLLECTOR(n), info, config);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Virtual methods
	 */
	COLLECTOR_BASE(n)->free = (collector_func_free_t) srv_free;
	COLLECTOR(n)->new_vsrv  = (collector_func_new_vsrv_t) vsrv_new;

	/* Default values
	 */
	n->write_fd      = -1;
	n->read_fd       = -1;
	n->pid           = -1;
	n->render_elapse = ELAPSE_RENDER;

	cherokee_buffer_init (&n->tmp);
	cherokee_buffer_init (&n->database_dir);
	cherokee_buffer_init (&n->path_rrdtool);
	cherokee_buffer_init (&n->path_database);

	/* Configuration
	 */
	cherokee_config_node_read_int (config, "render_elapse", &n->render_elapse);

	ret = cherokee_config_node_get (config, "rrdtool_path", &subconf);
	if (ret == ret_ok) {
		cherokee_buffer_add_buffer (&n->path_rrdtool, &subconf->val);
	} else {
		ret = cherokee_find_exec_in_path ("rrdtool", &n->path_rrdtool);
		if (ret != ret_ok) {
			LOG_ERROR ("Couldn't find rrdtool in PATH=%s\n", getenv("PATH"));
			return ret_error;
		}
	}
	   
	ret = cherokee_config_node_get (config, "database_dir", &subconf);
	if (ret == ret_ok) {
		cherokee_buffer_add_buffer (&n->database_dir, &subconf->val);
	} else {
		cherokee_buffer_add_str (&n->database_dir, CHEROKEE_RRD_DIR);
	}

	/* Path to the RRD file
	 */
	cherokee_buffer_add_buffer (&n->path_database, &n->database_dir);
	cherokee_buffer_add_str    (&n->path_database, "/server.rrd");

	/* Check whether the DB exists
	 */
	cherokee_buffer_clean      (&n->tmp);
	cherokee_buffer_add_str    (&n->tmp, "create ");
	cherokee_buffer_add_buffer (&n->tmp, &n->path_database);
	cherokee_buffer_add_str    (&n->tmp, " --step ");
	cherokee_buffer_add_long10 (&n->tmp, ELAPSE_UPDATE);
	cherokee_buffer_add_str    (&n->tmp, " ");

	/* Data Sources */
	cherokee_buffer_add_va     (&n->tmp, "DS:Accepts:ABSOLUTE:%d:U:U ",  ELAPSE_UPDATE*10);
	cherokee_buffer_add_va     (&n->tmp, "DS:Timeouts:ABSOLUTE:%d:U:U ", ELAPSE_UPDATE*10);
	cherokee_buffer_add_va     (&n->tmp, "DS:RX:ABSOLUTE:%d:U:U ", ELAPSE_UPDATE*10);
	cherokee_buffer_add_va     (&n->tmp, "DS:TX:ABSOLUTE:%d:U:U ", ELAPSE_UPDATE*10);

	/* Archives */
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:AVERAGE:0.5:288:797 ");

	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MAX:0.5:288:797 ");

	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:1:600 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:6:700 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:24:775 ");
	cherokee_buffer_add_str    (&n->tmp, "RRA:MIN:0.5:288:797 ");	   
	cherokee_buffer_add_str    (&n->tmp, "\n");

	ret = check_and_create_db (n, &n->tmp);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Add the commit callback
	 */
	ret = cherokee_bogotime_add_callback (update_srv_cb, n, ELAPSE_UPDATE);
	if (ret != ret_ok) {
		return ret_error;
	}

	/* Add the re-rendering callback
	 */
	ret = cherokee_bogotime_add_callback (render_srv_cb, n, ELAPSE_RENDER);
	if (ret != ret_ok) {
		return ret_error;
	}
	   
	*rrd = n;
	return ret_ok;
}
