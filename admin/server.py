#!/usr/bin/env python

import os
import re
import sys
import cgi
import stat
import time
import pyscgi
import thread
import signal

# Application modules
#
from config import *
from PageMain import *
from PageGeneral import *
from PageIcon import *
from PageMime import *
from PageVServer import *
from PageVServers import *
from PageEntry import *
from PageAdvanced import *
from PageError import *
from CherokeeManagement import *

# Constants
#
SCGI_PORT             = 4000
MODIFIED_CHECK_ELAPSE = 1

# Globals
#
cfg = None

# Request handler
#
class Handler(pyscgi.SCGIHandler):
    def __init__ (self, *args, **kwords):
        pyscgi.SCGIHandler.__init__ (self, *args)

    def handle_request (self):
        global cfg

        page    = None
        headers = ""
        body    = ""
        status  = "200 OK"
        uri     = self.env['REQUEST_URI']

        # Ensure that the configuration file is writable
        if not cfg.has_tree():
            page = PageError (cfg, PageError.CONFIG_NOT_FOUND)
        elif not cfg.is_writable():
            page = PageError (cfg, PageError.CONFIG_NOT_WRITABLE)
        elif not os.path.isdir(CHEROKEE_ICONSDIR):
            page = PageError (cfg, PageError.ICONS_DIR_MISSING)

        if page:
            self.wfile.write ('Status: 200 OK\r\n\r\n' +
                              page.HandleRequest (uri, None))
            return

        # Check the URL        
        if uri.startswith('/general'):
            page = PageGeneral(cfg)
        elif uri.startswith('/icon'):
            page = PageIcon(cfg)
        elif uri.startswith('/mime'):
            page = PageMime(cfg)
        elif uri.startswith('/advanced'):
            page = PageAdvanced(cfg)
        elif uri == '/vserver' or \
             uri == '/vserver/' or \
             uri == '/vserver/add_vserver':
            page = PageVServers(cfg)
        elif uri.startswith('/vserver/'):
            if "/prio/" in uri:
                page = PageEntry(cfg)
            else:
                page = PageVServer(cfg)
        elif uri.startswith('/apply'):
            manager = cherokee_management_get (cfg)
            manager.save()
            cherokee_management_reset()
            body = "/"
        elif uri.startswith('/launch'):
            manager = cherokee_management_get (cfg)
            manager.launch()
            cherokee_management_reset()
            body = "/"
        elif uri.startswith('/stop'):
            manager = cherokee_management_get (cfg)
            manager.stop()
            cherokee_management_reset()
            body = "/"
        elif uri == '/':
            page = PageMain(cfg)
        else:
            body = "/"

        # Handle post
        self.handle_post()
        post = cgi.parse_qs(self.post, keep_blank_values=1)
        
        # Execute page
        if page:
            body = page.HandleRequest(uri, post)

        # Is it a redirection?
        if body[0] == '/':
            status   = "302 Moved Temporarily"
            headers += "Location: %s\r\n" % (body)

        # Send result
        self.wfile.write('Status: %s\r\n' % (status) +
                         headers + '\r\n' + body)



# Server
#
def main():
    # Try to avoid zombie processes 
    signal.signal(signal.SIGCHLD, signal.SIG_IGN)

    # Move to the server directory
    pathname, scriptname = os.path.split(sys.argv[0])
    os.chdir(os.path.abspath(pathname))

    # SCGI server
    srv = pyscgi.ServerFactory (True, handler_class=Handler, port=SCGI_PORT)
    srv.socket.settimeout (MODIFIED_CHECK_ELAPSE)

    # Read configuration file
    global cfg
    cfg_file = sys.argv[1]
    cfg = Config(cfg_file)
    
    print ("Server running.. PID=%d" % (os.getpid()))
    while True:
        # Do it
        srv.handle_request()

    srv.server_close()


if __name__ == '__main__':
    main()
