from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "CGI Execution"

        self.expected_error = 200
        self.request        = "GET /cgi-bin1/test HTTP/1.0\r\n"
        self.conf           = "Directory /cgi-bin1 { Handler cgi }"

    def Prepare (self, www):
        self.Mkdir (www, "cgi-bin1")
        self.WriteFile (www, "cgi-bin1/test", 0755,
                        """#!/bin/sh

                        echo "Content-Type: text/plain"
                        echo
                        echo "Test1"
                        """)
