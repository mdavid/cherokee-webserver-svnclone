from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Double dot"

        self.expected_error = 301
        self.request        = "GET /../ HTTP/1.0\r\n"

