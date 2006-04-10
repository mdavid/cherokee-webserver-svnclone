from base import *

MAGIC = "The one and only.. Cherokee! :-)"
HOST  = "request_mini"

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Request tiny"

        self.request           = "GET / HTTP/1.1\r\n" +\
                                 "Host: %s\r\n" %(HOST)
        self.expected_error    = 200
        self.expected_content  = MAGIC
        self.forbidden_content = ["<?php", "index.php"]

    def Prepare (self, www):
        host_dir = self.Mkdir (www, "tmp_host_request_mini")

        self.conf              = """Server %s {
                                    DocumentRoot %s

                                    Directory / { Handler file }
                                    
                                    Request "^/$" {
                                       Handler redir {
                                          Rewrite "^.*$" "/index.php"
                                       }
                                    }
                                    
                                    %s
                                 }""" % (HOST, host_dir, self.php_conf)

        self.WriteFile (host_dir, "index.php", 0444, '<?php echo "%s" ?>' %(MAGIC))
        
    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)

