from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Redir to URL"

        self.request          = "GET /redir46/ HTTP/1.0\r\n"
        self.conf             = "Directory /redir46 { Handler redir { URL http://www.cherokee-project.com } }"
        self.expected_error   = 301
