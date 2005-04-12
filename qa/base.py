import os
import imp
import sys
import types
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
        self.name              = None    # Test 01: Basic functionality
        self.conf              = None    # Directory /test { .. }
        self.request           = ""      # GET / HTTP/1.0
        self.post              = None
        self.expected_error    = None
        self.expected_content  = None
        self.forbidden_content = None

        self._initialize()
        
    def _initialize (self):
        self.ssl               = None
        self.reply             = ""      # "200 OK"..
        self.version           = None    # HTTP/x.y: 9, 0 or 1
        self.reply_err         = None    # 200

    def _do_request (self, port, ssl):
        for res in socket.getaddrinfo(HOST, port, socket.AF_UNSPEC, socket.SOCK_STREAM):
            af, socktype, proto, canonname, sa = res

            try:
                s = socket.socket(af, socktype, proto)
            except socket.error, msg:
                continue

            try:
                s.connect(sa)
            except socket.error, msg:
                s.close()
                s = None
                continue
            break    

        if s is None:
            raise Exception("Couldn't connect to the server")

        if ssl:
            try:
                self.ssl = socket.ssl (s)
            except:
                raise Exception("Couldn't handshake SSL")

        request = self.request + "\r\n"
        if self.post is not None:
            request += self.post

        if self.ssl:
            self.ssl.write (request)
        else:
            s.send (request)
        
        while 1:
            if self.ssl:
                try:
                    d = self.ssl.read(8192)
                except:
                    d = ''
            else:
                d = s.recv(8192)

            if len(d) == 0: break
            self.reply += d

        s.close()

    def _parse_output (self):
        if (len(self.reply) == 0):
            raise Exception("Empty header")
            
        lines = string.split(self.reply, "\n")
        reply = lines[0]        

        if reply[:8] == "HTTP/0.9":
            self.version = 9
        elif reply[:8] == "HTTP/1.0":
            self.version = 0
        elif reply[:8] == "HTTP/1.1":
            self.version = 1
        else:
            raise Exception("Invalid header, len=%d: '%s'" % (len(reply), reply))

        reply = reply[9:]

        try:
            self.reply_err = int (reply[:3])
        except:
            raise Exception("Invalid header, version=%d len=%d: '%s'" % (self.version, len(reply), reply))

        return 0

    def _check_result (self):
        if self.reply_err != self.expected_error:
            return -1

        if self.expected_content != None:
            if type(self.expected_content) == types.StringType:
                if not self.expected_content in self.reply:
                    return -1
            elif type(self.expected_content) == types.ListType:
                for entry in self.expected_content:
                    if not entry in self.reply:
                        return -1
            else:
                raise Exception("Syntax error")

        if self.forbidden_content != None:
            if type(self.forbidden_content) == types.StringType:
                if self.forbidden_content in self.reply:
                    return -1
            elif type(self.forbidden_content) == types.ListType:
                for entry in self.forbidden_content:
                    if entry in self.reply:
                        return -1
            else:
                raise Exception("Syntax error")
                
        return 0

    def Clean (self):
        self._initialize()

    def Precondition (self):
        return True

    def Prepare (self, www):
        None

    def JustBefore (self, www):
        None

    def JustAfter (self, www):
        None

    def Run (self, port, ssl):
        self._do_request(port, ssl)
        self._parse_output()
        return self._check_result()

    def __str__ (self):
        src = "\tName     = %s\n" % (self.name)

        if self.version == 9:
            src += "\tVersion  = HTTP/0.9\n"
        elif self.version == 0:
            src += "\tVersion  = HTTP/1.0\n"
        elif self.version == 1:
            src += "\tVersion  = HTTP/1.1\n"

        if self.conf is not None:
            src += "\tConfig   = %s\n" % (self.conf)

        header_full = string.split (self.reply,  "\r\n\r\n")[0]
        headers     = string.split (header_full, "\r\n")
        requests    = string.split (self.request, "\r\n")

        src += "\tRequest  = %s\n" % (requests[0])
        for request in requests[1:]:
            if len(request) > 1:
                src += "\t\t%s\n" %(request)

        if self.post is not None:
            src += "\tPost     = %s\n" % (self.post)

        if self.expected_error is not None:
            src += "\tExpected = Code: %d\n" % (self.expected_error)
        else:
            src += "\tExpected = Code: UNSET!\n"

        if self.expected_content is not None:
            src += "\tExpected = Content: %s\n" % (self.expected_content)

        if self.forbidden_content is not None:
            src += "\tForbidden= Content: %s\n" % (self.forbidden_content)

        src += "\tReply    = %s\n" % (headers[0])
        for header in headers[1:]:
            src += "\t\t%s\n" %(header)

        src += "\tBody     = %s\n" % (self.reply[len(header_full)+4:])

        return src

    def Mkdir (self, www, dir):
        fulldir = os.path.join (www, dir)
        os.makedirs(fulldir)
        return fulldir

    def WriteFile (self, www, filename, mode=0444, content=''):
        fullpath = os.path.join (www, filename)
        f = open (fullpath, 'w')
        f.write (content)
        f.close()
        os.chmod(fullpath, mode)

    def Remove (self, www, filename):
        fullpath = os.path.join (www, filename)
        if os.path.isfile(fullpath):
            os.unlink (fullpath)
        else:
            os.removedirs (fullpath)
            
