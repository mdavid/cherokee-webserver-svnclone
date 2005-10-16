from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name             = "Auth basic II"
        self.request          = "GET /auth2/ HTTP/1.0\r\n" + \
                                "Authorization: Basic QWxhZGRpbjpvcGVuIHNlc2FtZQ==\r\n"
        self.expected_error   = 200

    def Prepare (self, www):
        dir = self.Mkdir (www, "auth2")
        self.WriteFile (www, "auth2/passwd", 0444, 'Aladdin:open sesame\n')

        self.conf             = """Directory /auth2 {
                                     Handler common
                                     Auth Basic {
                                          Name "Test"
                                          Method plain { PasswdFile %s }
                                     }
                                }""" % (dir+"/passwd")
