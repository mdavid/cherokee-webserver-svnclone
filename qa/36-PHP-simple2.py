from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name             = "PHP simple, common"
        self.request          = "GET /php2/simple.php HTTP/1.0\r\n"
        self.conf             = "Directory /php2 { Handler common }"
        self.expected_error   = 200
        self.expected_content = "This is PHP"

    def Prepare (self, www):
        self.Mkdir (www, "php2")
        self.WriteFile (www, "php2/simple.php", 0444,
                        "<?php echo 'This'.' is '.'PHP' ?>")

    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)
