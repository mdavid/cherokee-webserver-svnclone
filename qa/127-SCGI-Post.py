import os
from base import *

DIR   = "/SCGI2/"
MAGIC = "Cherokee and SCGI rocks!"
PORT  = 5002

SCRIPT = """
from scgi.scgi_server import *

class TestHandler (SCGIHandler):
    def handle_connection (self, socket):
        r = socket.makefile ('r')
        s = socket.makefile ('w')

        env = read_env(r)
        post = r.read (int(env['CONTENT_LENGTH']))
        s.write('Content-Type: text/plain\\r\\n\\r\\n')
        s.write('Post: %%s\\n' %% (post))
        socket.close()

SCGIServer(TestHandler, port=%d).serve()
""" % (PORT)

CONF = """
vserver!default!directory!%s!handler = scgi
vserver!default!directory!%s!handler!balancer = round_robin
vserver!default!directory!%s!handler!balancer!type = interpreter
vserver!default!directory!%s!handler!balancer!local_scgi2!host = localhost:%d
vserver!default!directory!%s!handler!balancer!local_scgi2!interpreter = %s %s
vserver!default!directory!%s!priority = 1270
"""


class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "SCGI II: Post"

        self.request           = "POST %s HTTP/1.0\r\n" %(DIR) +\
                                 "Content-type: application/x-www-form-urlencoded\r\n" +\
                                 "Content-length: %d\r\n" % (len(MAGIC))
        self.post              = MAGIC
        self.expected_error    = 200
        self.expected_content  = "Post: "+MAGIC
        self.forbidden_content = ["scgi.scgi_server", "SCGIServer", "write"]

    def Prepare (self, www):
        scgi_file = self.WriteFile (www, "scgi_test2.scgi", 0444, SCRIPT)

        self.conf = CONF % (DIR, DIR, DIR, DIR, PORT, DIR, PYTHON_PATH, scgi_file, DIR)

    def Precondition (self):
        re = os.system ("%s -c 'import scgi.scgi_server'" % (PYTHON_PATH)) 
        return (re == 0)
