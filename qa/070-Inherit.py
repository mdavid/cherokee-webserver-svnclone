from base import *

CONF = """
vserver!default!rule!directory!/inherit1!auth = plain
vserver!default!rule!directory!/inherit1!auth!methods = basic
vserver!default!rule!directory!/inherit1!auth!realm = Test
vserver!default!rule!directory!/inherit1!auth!passwdfile = %s
vserver!default!rule!directory!/inherit1!priority = 700

vserver!default!rule!directory!/inherit1/dir1/dir2/dir3!handler = file
vserver!default!rule!directory!/inherit1/dir1/dir2/dir3!priority = 701
vserver!default!rule!directory!/inherit1/dir1/dir2/dir3!final = 0
"""

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name           = "Inherit dir config"
        self.request        = "GET /inherit1/dir1/dir2/dir3/test HTTP/1.0\r\n"
        self.expected_error = 401

    def Prepare (self, www):
        self.Mkdir (www, "inherit1/dir1/dir2/dir3")
        fn = self.WriteFile (www, "inherit1/dir1/dir2/dir3/test", 0555, "content")

        self.conf = CONF % (fn)
