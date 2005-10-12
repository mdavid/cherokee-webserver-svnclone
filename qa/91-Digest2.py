from conf import *
from base import *

MAGIC  = "Show this"
REALM  = "realm"
USER   = "username"
PASSWD = "itissecret"


class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Digest - Plain: Valid user/passwd pair"
 
        self.expected_error   = 200
        self.expected_content = MAGIC
        
    def JustBefore (self, www):
        # It will read a validad nonce value just before each test
        #
        nested = TestBase()
        nested.request = "GET /digest2/file HTTP/1.0\r\n"
        nested.Run(PORT, 0)

        # Parse the authentication information line
        #
        nested.digest = self.Digest()
        vals = nested.digest.ParseHeader (nested.reply)

        # Calculate the response value
        #
        response = nested.digest.CalculateResponse (USER, REALM, PASSWD, "GET", "/digest2/file", 
                                                    vals["nonce"], vals["qop"], vals["cnonce"], vals["nc"])

        # At this point, we got a valid nonce, so let's write the 
        # request..
        self.request = "GET /digest2/file HTTP/1.0\r\n" +\
                       "Authorization: Digest response=\"%s\", username=\"%s\", realm=\"%s\", uri=\"%s\", nonce=\"%s\", qop=\"%s\", algorithm=\"%s\"\r\n" % \
                       (response, USER, REALM, "/digest2/file", vals["nonce"], vals["qop"], vals["algorithm"])


    def Prepare (self, www):
        # Create the infrastructure
        #
        self.Mkdir (www, "digest2")
        self.WriteFile (www, "digest2/file", 0444, MAGIC)
        passfile = self.WriteFile (www, "digest2/.passwd", 0444, "%s:%s\n" % (USER, PASSWD))

        # Set the configuration
        #
        self.conf             = """
           Directory /digest2 {
               Handler file
               Auth Digest {
                  Name "%s"
                  Method plain {
                    PasswdFile %s
                  }
               }
           }""" % (REALM, passfile)
