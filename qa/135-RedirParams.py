from base import *

SERVER  = "hidden_params"
REQUEST = "function"
PARAMS  = "one=001&two=002"

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name             = "Hidden redir with params"
        self.request          = "GET /%s?%s HTTP/1.1\r\n" % (REQUEST, PARAMS) + \
                                "Host: %s\r\n" % (SERVER)
        self.expected_error   = 301
        self.expected_content = "Location: /index.php?q=%s&%s" % (REQUEST, PARAMS)

    def Prepare (self, www):
        d  = self.Mkdir (www, "hidde_w_params_server")
        d2 = self.Mkdir (d, "hidde_w_params")

        self.conf = """Server %s {
                         DocumentRoot %s

                         Request "^/([^\?]*)$" {
                           Handler redir {
                             Show Rewrite "/index.php?q=$1"
                           }
                         }

                         Request "^/([^\?]*)\?(.*)$" {
                           Handler redir {
                             Show Rewrite "/index.php?q=$1&$2"
                           }
                         }
                    }""" % (SERVER, d)

    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)
