import os
from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Directory indexer, /index, vbles"

        self.request           = "GET /inside/ HTTP/1.0\r\n" +\
                                 "Host: directoryindex3\r\n"
        self.expected_error    = 200

    def Prepare (self, www):
        self.dr = self.Mkdir (www, "directoryindex3")
        self.Mkdir (www, "directoryindex3/inside/foo")

        self.expected_content  = ["DocumentRoot %s" % (self.dr),
                                  "ScriptName /super_test_index.php",
                                  "RequestUri /inside/"]

        self.conf              = """
        Server directoryindex3 {
           Directory / { Handler common }
           DirectoryIndex index.php, /super_test_index.php
           DocumentRoot %s
           Extension php { Handler phpcgi }
        }""" % (self.dr)

    def JustBefore (self, www):
        self.WriteFile (self.dr, "super_test_index.php", 0666, """<?php
                        echo "DocumentRoot ".$_SERVER[DOCUMENT_ROOT]."\n";
                        echo "ScriptName "  .$_SERVER[SCRIPT_NAME]."\n";
                        echo "RequestUri "  .$_SERVER[REQUEST_URI]."\n";
                        ?>""")

    def Precondition (self):
        if os.path.exists (PHPCGI_PATH) is False:
            return False

        f = os.popen("ps -p %d -L | wc -l" % (os.getpid()))
        threads = int(f.read()[:-1])
        f.close()

        return (threads <= 2)
        
