import random
import string
from base import *

MAGIC=""
for i in xrange(50*1024):
    MAGIC += random.choice(string.letters)

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "50k random"

        self.request          = "GET /50k HTTP/1.0\r\n"
        self.expected_error   = 200
        self.expected_content = MAGIC

    def Prepare (self, www):
        self.WriteFile (www, "50k", 0444, MAGIC)

