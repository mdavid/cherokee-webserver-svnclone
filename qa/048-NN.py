from base import *

MAGIC = "Sesamo"
CONTENT = "This is the content of " + MAGIC

CONF = """
vserver!default!rule!480!match!type = directory
vserver!default!rule!480!match!directory = /nn1
vserver!default!rule!480!handler = nn
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "NN"

        self.request           = "GET /nn1/Xesano HTTP/1.0\r\n"
        self.conf              = CONF
        self.expected_error    = 200
        self.expected_content  = CONTENT
        self.forbidden_content = "Location: "

    def Prepare (self, www):
        self.Mkdir (www, "nn1")
        self.WriteFile (www, "nn1/name")
        self.WriteFile (www, "nn1/"+MAGIC, 0444, CONTENT)
        self.WriteFile (www, "nn1/ABCD")
        self.WriteFile (www, "nn1/alobbs")
