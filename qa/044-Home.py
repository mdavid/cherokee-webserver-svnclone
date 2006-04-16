from base import *

CONF = """
vserver!missing.host1!user_dir = public_html
vserver!missing.host1!user_dir!directory!/!handler = common
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Invalid home"

        self.request          = "GET /~missing/file HTTP/1.1\r\n" + \
                                "Host: missing.host1\r\n"
        self.conf             = CONF
        self.expected_error   = 404

# "UserDir public_html { Directory / { Handler common }}"
