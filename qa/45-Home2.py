import os
import string
from base import *

PUBLIC_HTML = "public_html"

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Valid home"

        # Get the current username
        self.user = os.getlogin()

        self.request          = "GET /~%s/ HTTP/1.0\r\n" % (self.user)
        self.conf             = "UserDir %s { Directory / { Handler common }}" % (PUBLIC_HTML)
        self.expected_error   = 200


    def Precondition (self):
        # Read the /etc/passwd file
        f = open ("/etc/passwd", "r")
        pwuser = filter(lambda x: x.find(self.user) == 0, f.readlines())
        f.close

        # Sanity check
        if len(pwuser) is not 1:
            return False

        # Get the home directory
        home = string.split (pwuser[0], ":")[5]
        public_html = os.path.join (home, PUBLIC_HTML)

        # Look for the public_html directory
        if not os.path.exists(public_html):
            return False

        return True

