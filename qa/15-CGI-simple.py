from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "CGI Execution"

        self.expected_error = 200
        self.request        = "GET /cgi-bin/test1 HTTP/1.0\r\n"
        self.conf           = "Directory /cgi-bin { Handler cgi }"

    def Prepare (self, www):
        self.Mkdir (www, "cgi-bin")
        self.WriteFile (www, "cgi-bin/test1", 0755,
                        """#!/bin/sh

                        echo "Content-Type: text/plain"
                        echo
                        echo "Test1"
                        """)
