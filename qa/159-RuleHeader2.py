from base import *

DIR       = "header_test2_referer_match"
REFERER   = "example.com"

CONF = """
vserver!default!rule!1590!match = header
vserver!default!rule!1590!match!header = Referer
vserver!default!rule!1590!match!match = .+\.com
vserver!default!rule!1590!handler = file

vserver!default!rule!1591!match = header
vserver!default!rule!1591!match!header = Referer
vserver!default!rule!1591!match!match = .+\.net
vserver!default!rule!1591!handler = cgi
"""

CGI = """#!/bin/sh

echo "just a test"
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Rule header: match II"

        self.request           = "GET /%s/test HTTP/1.0\r\n" % (DIR) + \
                                 "Referer: %s\r\n" % (REFERER)
        self.conf              = CONF
        self.expected_error    = 200
        self.required_content  = ["/bin/sh", "echo"]

    def Prepare (self, www):
        d = self.Mkdir (www, DIR)
        f = self.WriteFile (d, 'test', 755, CGI)
