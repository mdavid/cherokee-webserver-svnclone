import os
import imp
import sys
import socket
import string

from conf import *

def importfile(path):
    filename = os.path.basename(path)
    name, ext = os.path.splitext(filename)

    file = open(path, 'r')
    module = imp.load_module(name, file, path, (ext, 'r', imp.PY_SOURCE))
    file.close()

    return module

class TestBase:
    def __init__ (self):
        self.name           = None    # Test 01: Basic functionality
        self.conf           = None    # Directory /test { .. }
        self.request        = ""      # GET / HTTP/1.0
        self.reply          = ""      # "200 OK"..
        self.version        = None    # HTTP/x.y: 9, 0 or 1
        self.reply_err      = None    # 200
        self.expected_error = None

    def _do_request (self):
        for res in socket.getaddrinfo(HOST, PORT, socket.AF_UNSPEC, socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res

            try:
                s = socket.socket(af, socktype, proto)
            except socket.error, msg:
                s = None
                continue

            try:
                s.connect(sa)
            except socket.error, msg:
                s.close()
                s = None
                continue
            break    

        if s is None:
            raise "Couldn't connect to the server"
        
        s.send (self.request+"\r\n")
        
        while 1:
            d = s.recv(8192)
            if len(d) == 0: break
            self.reply += d

        s.close()

    def _parse_output (self):
        lines = string.split(self.reply, "\n")
        reply = lines[0]        

        if reply[:8] == "HTTP/0.9":
            self.version = 9
        elif reply[:8] == "HTTP/1.0":
            self.version = 0
        elif reply[:8] == "HTTP/1.1":
            self.version = 1
        else:
            raise "Invalid header: %s" % (reply)

        reply = reply[9:]

        try:
            self.reply_err = int (reply[:3])
        except:
            raise "Invalid header: %s" % (reply)

        return 0

    def _check_result (self):
        if self.reply_err != self.expected_error:
            return -1

        return 0

    def Prepare (self, www):
        None

    def Run (self):
        self._do_request()
        self._parse_output()
        return self._check_result()

    def __str__ (self):
        src = "\tName    = %s\n" % (self.name)

        if self.version == 9:
            src += "\tVersion = HTTP/0.9\n"
        elif self.version == 0:
            src += "\tVersion = HTTP/1.0\n"
        elif self.version == 1:
            src += "\tVersion = HTTP/1.1\n"

        if self.conf is not None:
            src += "\tConfig  = %s\n" % (self.conf)

        src += "\tRequest = %s" % (self.request)

        header_full = string.split (self.reply,  "\r\n\r\n")[0]
        headers     = string.split (header_full, "\r\n")

        if self.expected_error is not None:
            src += "\tExpected = %d\n" % (self.expected_error)
        else:
            src += "\tExpected = UNSET!\n"

        src += "\tReply    = %s\n" % (headers[0])
        for header in headers[1:]:
            src += "\t\t%s\n" %(header)
            
        return src

    def Mkdir (self, www, dir):
        fulldir = os.path.join (www, dir)
        os.mkdir(fulldir)

    def WriteFile (self, www, filename, mode, content):
        fullpath = os.path.join (www, filename)
        f = open (fullpath, 'w')
        f.write (content)
        f.close()
        os.chmod(fullpath, mode)
        print fullpath
