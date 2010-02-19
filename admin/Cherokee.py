# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2001-2010 Alvaro Lopez Ortega
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of version 2 of the GNU General Public
# License as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import CTK

# Python
import os
import time
import errno
import signal

from subprocess import *
from select import select

# Cheroke-admin
from consts import *
from configured import *
from config_version import *


DEFAULT_PATH = ['/usr/local/sbin', '/usr/local/bin', '/usr/sbin', '/usr/bin', '/sbin', '/bin']
LAUNCH_DELAY = 2



class PID:
    def __init__ (self):
        self.pid       = None
        self._pid_file = None

        # Initialize
        self.refresh()

    def refresh (self):
        # It might be alive
        if self.pid and _pid_is_alive (self.pid):
            return

        self.pid = None

        # Need a PID
        self._read_pid_file()
        if not self.pid:
            self._figure_pid()

    def _read_pid_file (self):
        # Check the configuration
        self._pid_file = CTK.cfg.get_val("server!pid_file")
        if not self._pid_file:
            return

        # Read the file
        try:
            f = open(self._pid_file, 'r')
        except: return

        self.pid = int(f.readline())

        try: f.close()
        except: pass

    def _figure_pid (self):
        # Execture ps
        try:
            f = os.popen ("ps aux")
        except: return

        ps = f.read()

        try: f.close()
        except: pass

        # Try to find the Cherokee process
        for l in ps.split("\n"):
            if "cherokee " in l and "-C %s"%(CTK.cfg.file) in l:
                pid = filter (lambda x: x.isdigit(), l.split())[0]
                self.pid = int(pid)


class Server:
    def is_alive (self):
        return _pid_is_alive(pid.pid)

    def stop (self):
        if not pid.pid: return

        # Kill the process
        return _pid_kill(pid.pid)

    def restart (self, graceful=True):
        if not pid.pid: return

        try:
            os.kill (pid.pid, (signal.SIGUSR1,signal.SIGHUP)[graceful])
        except:
            pass

    def launch (self):
        def daemonize():
            os.setsid()

        # Ensure the a minimum $PATH is set
        environ = os.environ.copy()
        if not "PATH" in environ:
            environ["PATH"] = ':'.join(DEFAULT_PATH)

        # Launch the process
        p = Popen ([CHEROKEE_SERVER, '--admin_child', '-C', CTK.cfg.file],
                   stdout=PIPE, stderr=PIPE, env=environ,
                   preexec_fn=daemonize, close_fds=True)

        stdout_f,  stderr_f  = (p.stdout, p.stderr)
        stdout_fd, stderr_fd = stdout_f.fileno(), stderr_f.fileno()
        stdout,    stderr    = '', ''

        # Check the first few lines of the output
        while True:
            r,w,e = select([stdout_fd, stderr_fd], [], [stdout_fd, stderr_fd], 1)

            if e:
                return _("Could not access file descriptors: ") + str(e)

            if stdout_fd in r:
                stdout += stdout_f.read(1)
            if stderr_fd in r:
                stderr += stderr_f.read(1)

            nl = stderr.find('\n')
            if nl != -1:
                for e in ["{'type': ", 'ERROR', '(error) ', '(critical) ']:
                    if e in stderr:
                        _pid_kill (p.pid)
                        return stderr
                stderr = stderr[nl+1:]

            if stdout.count('\n') > 1:
                break

        time.sleep (LAUNCH_DELAY)
        return None


class Modules:
    def __init__ (self):
        # Get server info
        try:
            f = os.popen ("%s -i" % (CHEROKEE_WORKER))
            self._server_info = f.read()
        except:
            self._server_info = ''

        try: f.close()
        except: pass

    def get_info_section (self, filter):
        filter_string = " %s: " % (filter)

        for line in self._server_info.split("\n"):
            if line.startswith (filter_string):
                line = line.replace (filter_string, "")
                return line.split(" ")
        return []

    def has_plugin (self, name):
        try:
            mods = filter(lambda x: name in x, os.listdir(CHEROKEE_PLUGINDIR))
            if len(mods) >= 1:
                return True
        except:
            pass

        return name in self.get_info_section("Built-in")

    def has_polling_method (self, name):
        return name in self.get_info_section("Polling methods")

    def filter_available (self, module_list):
        new_module_list = []

        for entry in module_list:
            assert (type(entry) == tuple)
            assert (len(entry) == 2)
            plugin, name = entry

            if (not len(plugin) or
                self.has_plugin (plugin)):
                new_module_list.append(entry)

        return new_module_list


#
# Globals
#
pid     = PID()
server  = Server()
modules = Modules()


#
# Helper functions
#
def _pid_is_alive (pid):
    if not pid:
        return False

    try:
        os.kill (pid, 0)
    except OSError:
        return False

    return True

def _pid_kill (pid):
    # Kill it
    try:
        os.kill (pid, signal.SIGTERM)
    except:
        return not _pid_is_alive(pid)

    # Ensure it died
    retries = 3
    while retries:
        try:
            os.waitpid (pid, 0)
            return True
        except OSError, e:
            if e[0] == errno.ECHILD:
                return True
            time.sleep(1)
            retries -= 1

    # Did not succeed
    return False
