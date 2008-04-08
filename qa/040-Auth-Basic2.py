from base import *
from base64 import encodestring

LOGIN="Aladdin"
PASSWD="open sesame"

CONF = """
vserver!default!rule!directory!/auth2!auth = plain
vserver!default!rule!directory!/auth2!auth!methods = basic
vserver!default!rule!directory!/auth2!auth!realm = Test
vserver!default!rule!directory!/auth2!auth!passwdfile = %s
vserver!default!rule!directory!/auth2!final = 0
vserver!default!rule!directory!/auth2!priority = 400
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name             = "Auth basic II"
        self.request          = "GET /auth2/ HTTP/1.0\r\n" + \
                                "Authorization: Basic %s\r\n" % (encodestring ("%s:%s"%(LOGIN,PASSWD))[:-1])
        self.expected_error   = 200

    def Prepare (self, www):
        d = self.Mkdir (www, "auth2")
        self.WriteFile (d, "passwd", 0444, '%s:%s\n' %(LOGIN,PASSWD))

        self.conf = CONF % (d+"/passwd")
