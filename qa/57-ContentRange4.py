import random
import string
from base import *

LENGTH = 100*1024
OFFSET = 15

MAGIC = ""
for i in xrange(LENGTH):
    MAGIC += random.choice(string.letters)

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Content Range 100k, end"

        self.request           = "GET /Range100k2 HTTP/1.0\r\n" +\
                                 "Content-Range: bytes=-%d\r\n" % (OFFSET)
        self.expected_error    = 206
        self.expected_content  = [MAGIC[:OFFSET], "Content-length: %d" % (OFFSET)]
        self.forbidden_content = MAGIC[OFFSET:]

    def Prepare (self, www):
        self.WriteFile (www, "Range100k2", 0444, MAGIC)
