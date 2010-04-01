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
    gettext.install('cherokee')

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

    # Init CTK in advance
    # Params could be passed to CTK.run() later
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


if __name__ == "__main__":
    # Read the arguments
    try:
        scgi_port = sys.argv[1]
        cfg_file  = sys.argv[2]
    except:
        print _("Incorrect parameters: PORT CONFIG_FILE")
        raise SystemExit

    # Init
    init (scgi_port, cfg_file)

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

    # Run
    CTK.run()
