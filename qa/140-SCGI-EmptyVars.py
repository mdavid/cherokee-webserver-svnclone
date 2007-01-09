import os
from base import *

DIR   = "/SCGI3/"
MAGIC = "Cherokee and SCGI rocks!"
PORT  = 5003

SCRIPT = """
from pyscgi import *

class TestHandler (SCGIHandler):
    def handle_request (self):
        self.handle_post()
        self.output.write('Content-Type: text/plain\\r\\n\\r\\n')

        for v in self.env:
            self.output.write('%%s: %%s\\n' %% (v, self.env[v]))

SCGIServer(TestHandler, port=%d).serve_forever()
""" % (PORT)

CONF = """
vserver!default!directory!<dir>!handler = scgi
vserver!default!directory!<dir>!handler!balancer = round_robin
vserver!default!directory!<dir>!handler!balancer!type = interpreter
vserver!default!directory!<dir>!handler!balancer!local_scgi2!host = localhost:%d
vserver!default!directory!<dir>!handler!balancer!local_scgi2!interpreter = %s %s
vserver!default!directory!<dir>!priority = 1270
"""


class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "SCGI III: Variables"

        self.request           = "GET %s HTTP/1.0\r\n" %(DIR) 
        self.expected_error    = 200
        self.expected_content  = ['PATH_INFO:', 'QUERY_STRING:']
        self.forbidden_content = ['pyscgi', 'SCGIServer', 'write']

    def Prepare (self, www):
        scgi_file = self.WriteFile (www, "scgi_test3.scgi", 0444, SCRIPT)

        pyscgi = os.path.join (www, 'pyscgi.py')
        if not os.path.exists (pyscgi):
            self.CopyFile ('pyscgi.py', pyscgi)

        self.conf = CONF % (PORT, look_for_python(), scgi_file)
        self.conf = self.conf.replace ('<dir>', DIR)
