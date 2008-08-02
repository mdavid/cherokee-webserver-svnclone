import os
from base import *

DIR   = "/SCGI5"
PORT  = get_free_port()

SCRIPT = """
from pyscgi import *

class TestHandler (SCGIHandler):
    def handle_request (self):
        self.handle_post()
        self.output.write('Content-Type: text/plain\\r\\n\\r\\n')

        for v in self.env:
            self.output.write('%%s: "%%s"\\n' %% (v, self.env[v]))

SCGIServer(TestHandler, port=%d).serve_forever()
""" % (PORT)

CONF = """
vserver!001!rule!1510!match = directory
vserver!001!rule!1510!match!directory = <dir>
vserver!001!rule!1510!handler = scgi
vserver!001!rule!1510!handler!check_file = 0
vserver!001!rule!1510!handler!balancer = round_robin
vserver!001!rule!1510!handler!balancer!type = interpreter
vserver!001!rule!1510!handler!balancer!1!host = localhost:%d
vserver!001!rule!1510!handler!balancer!1!interpreter = %s %s
"""

EXPECTED = [
    'PATH_INFO: "/"',
    'SCRIPT_NAME: "%s"' % (DIR)
]

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "SCGI V: PATH_INFO and SCRIPT_NAME"

        self.request           = "GET %s/ HTTP/1.0\r\n" %(DIR) 
        self.expected_error    = 200
        self.expected_content  = EXPECTED
        self.forbidden_content = ['pyscgi', 'SCGIServer', 'write']

    def Prepare (self, www):
        scgi_file = self.WriteFile (www, "scgi_test5.scgi", 0444, SCRIPT)

        pyscgi = os.path.join (www, 'pyscgi.py')
        if not os.path.exists (pyscgi):
            self.CopyFile ('pyscgi.py', pyscgi)

        self.conf = CONF % (PORT, look_for_python(), scgi_file)
        self.conf = self.conf.replace ('<dir>', DIR)
