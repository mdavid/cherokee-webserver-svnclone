import random
import string
from base import *

MAGIC=''
for i in xrange(100*1024):
    MAGIC += random.choice(string.letters)

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Big POST, 100k"

        self.request          = "POST /Post100k.php HTTP/1.0\r\n" +\
                                "Content-type: application/x-www-form-urlencoded\r\n" +\
                                "Content-length: %d\r\n" % (4+len(MAGIC))
        self.post             = "var="+MAGIC
        self.expected_error   = 200
        self.expected_content = MAGIC

    def Prepare (self, www):
        self.WriteFile (www, "Post100k.php", 0444,
                        "<?php echo $_POST['var']; ?>")

    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)


