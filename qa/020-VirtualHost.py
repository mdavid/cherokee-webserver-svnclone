from base import *

MAGIC = "First virtual Host test magic string"

CONF = """
vserver!cherokee.test!document_root = %s
vserver!cherokee.test!directory!/!handler = common
vserver!cherokee.test!directory!/!priority = 10
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Virtual Host"

        self.request          = "GET /file HTTP/1.1\r\n" +\
                                "Host: cherokee.test\r\n"

        self.expected_error   = 200
        self.expected_content = MAGIC

    def Prepare (self, www):
        d = self.Mkdir (www, "vhost1")
        self.WriteFile (d, "file", 0444, MAGIC)
        
        self.conf = CONF % (d)
