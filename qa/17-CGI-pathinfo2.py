from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "CGI with pathinfo II"

        self.request          = "GET /cgi-bin3/inside/test/test_parameter HTTP/1.0\r\n"
        self.conf             = "Directory /cgi-bin3 { Handler cgi }"
        self.expected_error   = 200

    def Prepare (self, www):
        self.Mkdir (www, "cgi-bin3")
        self.Mkdir (www, "cgi-bin3/inside")
        self.WriteFile (www, "cgi-bin3/inside/test", 0755,
                        """#!/bin/sh

                        echo "Content-Type: text/plain"
                        echo
                        """)
