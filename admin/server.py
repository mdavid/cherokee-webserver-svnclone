#!/usr/bin/env python

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

import os
import sys
import stat
import signal
import gettext
import config_version

# Import CTK
sys.path.append (os.path.abspath (os.path.realpath(__file__) + '/../CTK'))
import CTK

# Cherokee imports
from configured import *


def init (scgi_port, cfg_file):
    # Translation support
    gettext.install('cherokee', LOCALEDIR)

    import __builtin__
    __builtin__.__dict__['N_'] = lambda x: x

    # Try to avoid zombie processes
    if hasattr(signal, "SIGCHLD"):
        signal.signal(signal.SIGCHLD, signal.SIG_IGN)

    # Move to the server directory
    pathname, scriptname = os.path.split(sys.argv[0])
    os.chdir(os.path.abspath(pathname))

    # Let the user know what is going on
    version = VERSION
    pid     = os.getpid()

    if scgi_port.isdigit():
        print _("Server %(version)s running.. PID=%(pid)d Port=%(scgi_port)s") % (locals())
    else:
        print _("Server %(version)s running.. PID=%(pid)d Socket=%(scgi_port)s") % (locals())

    # Read configuration file
    CTK.cfg.file = cfg_file
    CTK.cfg.load()

    # Update config tree if required
    config_version.config_version_update_cfg (CTK.cfg)

    # Init CTK
    if scgi_port.isdigit():
        CTK.init (port=8000)

    else:
        # Remove the unix socket if it already exists
        try:
            mode = os.stat (scgi_port)[stat.ST_MODE]
            if stat.S_ISSOCK(mode):
                print "Removing an old '%s' unix socket.." %(scgi_port)
                os.unlink (scgi_port)
        except OSError:
            pass

        CTK.init (unix_socket=scgi_port)

    # At this moment, CTK must be forced to work as a syncronous
    # server. All the initial tests (config file readable, correct
    # installation, etc) must be performed in the safest possible way,
    # ensuring that race conditions don't cause trouble. It will be
    # upgraded to asyncronous mode just before the mainloop is reached
    CTK.set_synchronous (True)

def debug_set_up():
    def debug_callback (sig, frame):
        import code, traceback

        d = {'_frame':frame}       # Allow access to frame object.
        d.update(frame.f_globals)  # Unless shadowed by global
        d.update(frame.f_locals)

        i = code.InteractiveConsole(d)
        message  = "Signal recieved : entering python shell.\nTraceback:\n"
        message += ''.join(traceback.format_stack(frame))
        i.interact(message)

    def trace_callback (sig, stack):
        import traceback, threading

        id2name = dict([(th.ident, th.name) for th in threading.enumerate()])
        for threadId, stack in sys._current_frames().items():
            print '\n# Thread: %s(%d)' %(id2name[threadId], threadId)
            for filename, lineno, name, line in traceback.extract_stack(stack):
                print 'File: "%s", line %d, in %s' %(filename, lineno, name)
                if line:
                    print '  %s' % (line.strip())

    print "DEBUG: SIGUSR1 invokes the console.."
    print "       SIGUSR2 prints a backtrace.."
    signal.signal (signal.SIGUSR1, debug_callback)
    signal.signal (signal.SIGUSR2, trace_callback)


if __name__ == "__main__":
    # Read the arguments
    try:
        scgi_port = sys.argv[1]
        cfg_file  = sys.argv[2]
    except:
        print _("Incorrect parameters: PORT CONFIG_FILE")
        raise SystemExit

    # Debugging mode
    if '-x' in sys.argv:
        debug_set_up()

    # Init
    init (scgi_port, cfg_file)

    # Ancient config file
    def are_vsrvs_num():
        tmp = [True] + [x.isdigit() for x in CTK.cfg.keys('vserver')]
        return reduce (lambda x,y: x and y, tmp)

    if not are_vsrvs_num():
        import PageError
        CTK.publish (r'', PageError.AncientConfig, file=cfg_file)

        while not are_vsrvs_num():
            CTK.step()

        CTK.unpublish (r'')

    # Check config file and set up
    if not os.path.exists (cfg_file):
        import PageNewConfig
        CTK.publish (r'', PageNewConfig.Render)

        while not os.path.exists (cfg_file):
            CTK.step()

        CTK.unpublish (r'')
        CTK.cfg.file = cfg_file
        CTK.cfg.load()

    if not os.access (cfg_file, os.W_OK):
        import PageError
        CTK.publish (r'', PageError.NotWritable, file=cfg_file)

        while not os.access (cfg_file, os.W_OK):
            CTK.step()

        CTK.unpublish (r'')

    if not os.path.isdir (CHEROKEE_ICONSDIR):
        import PageError
        CTK.publish (r'', PageError.IconsMissing, path=CHEROKEE_ICONSDIR)

        while not os.path.isdir (CHEROKEE_ICONSDIR):
            CTK.step()

        CTK.unpublish (r'')

    # Set up the error page
    import PageException
    CTK.error.page = PageException.Page

    # Import the Pages
    import PageIndex
    import PageGeneral
    import PageVServers
    import PageVServer
    import PageRule
    import PageEntry
    import PageSources
    import PageSource
    import PageAdvanced
    import PageNewConfig
    import PageHelp
    import PageStatus

    # Let's get asyncronous..
    CTK.set_synchronous (False)

    # Run forever
    CTK.run()

