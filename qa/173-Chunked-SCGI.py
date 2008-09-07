from base import *

DIR            = "chunked_scgi_1"
MAGIC          = letters_random (300)

CHUNKED_BEGIN  = hex(len(MAGIC))[2:] + '\r\n'
CHUNKED_FINISH = "0\r\n\r\n"

PORT           = get_free_port()
PYTHON         = look_for_python()
source         = get_next_source()

SCRIPT = """
from pyscgi import *

class TestHandler (SCGIHandler):
    def handle_request (self):
        self.handle_post()
        self.output.write('Content-Length: %d\\r\\n')
        self.output.write('Content-Type: text/plain\\r\\n\\r\\n')
        self.output.write('%s')

SCGIServer(TestHandler, port=%d).serve_forever()
""" % (len(MAGIC), MAGIC, PORT)

CONF = """
vserver!1!rule!1730!match = directory
vserver!1!rule!1730!match!directory = /%(DIR)s
vserver!1!rule!1730!handler = scgi
vserver!1!rule!1730!handler!balancer = round_robin
vserver!1!rule!1730!handler!balancer!source!1 = %(source)d

source!%(source)d!type = interpreter
source!%(source)d!host = localhost:%(PORT)d
source!%(source)d!interpreter = %(PYTHON)s %(scgi_file)s
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Chunked encoding: scgi"

        self.request           = "GET /%s/test HTTP/1.1\r\n" % (DIR) + \
                                 "Host: localhost\r\n"               + \
                                 "Connection: Keep-Alive\r\n\r\n"    + \
                                 "OPTIONS / HTTP/1.0\r\n"

        self.expected_error    = 200
        self.expected_content  = ["Transfer-Encoding: chunked",
                                  CHUNKED_BEGIN, MAGIC, CHUNKED_FINISH]

    def Prepare (self, www):
        scgi_file = self.WriteFile (www, "scgi_chunked1.scgi", 0444, SCRIPT)

        pyscgi = os.path.join (www, 'pyscgi.py')
        if not os.path.exists (pyscgi):
            self.CopyFile ('pyscgi.py', pyscgi)

        vars = globals()
        vars['scgi_file'] = scgi_file
        self.conf = CONF % (vars)
