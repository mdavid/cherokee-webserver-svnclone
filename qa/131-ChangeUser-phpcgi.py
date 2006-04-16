import os
import pwd
from base import *

USER = "nobody"
UID  = pwd.getpwnam(USER)[2]

CONF = """
vserver!default!directory!/change_user1!handler = phpcgi
vserver!default!directory!/change_user1!handler!change_user = 1
vserver!default!directory!/change_user1!handler!interpreter = %s
vserver!default!directory!/change_user1!priority = 1310
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "ChangeUser: phpcgi"

        self.request           = "GET /change_user1/test.php HTTP/1.0\r\n"
        self.expected_error    = 200
        self.expected_content  = "I'm %s" % (USER)

        self.conf = CONF % (PHPCGI_PATH)

    def Prepare (self, www):
        d = self.Mkdir (www, "change_user1", 0777)
        f = self.WriteFile (d, "test.php", 0444, '<?php echo "I\'m ". get_current_user() ."\n"; ?>')
        if os.geteuid() == 0:
            os.chown (f, UID, os.getgid())

    def Precondition (self):
        # It will only work it the server runs as root
        if os.geteuid() != 0:
            return False

        return os.path.exists (PHPCGI_PATH)
