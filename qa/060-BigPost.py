import random
import string
from base import *
from util import *

LENGTH = 100*1024


class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Big POST, 100k"

        self.request          = "POST /Post100k.php HTTP/1.0\r\n" +\
                                "Content-type: application/x-www-form-urlencoded\r\n" +\
                                "Content-length: %d\r\n" % (4+LENGTH)
        self.expected_error   = 200

    def Prepare (self, www):
        random  = letters_random (100*1024)
        tmpfile = self.WriteTemp (random)

        self.WriteFile (www, "Post100k.php", 0444,
                        "<?php echo $_POST['var']; ?>")

        self.post             = "var="+random
        self.expected_content = "file:%s" % (tmpfile)

    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)

