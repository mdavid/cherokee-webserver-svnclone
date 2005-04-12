from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.name = "Directory indexer, /index, vbles"

        self.request           = "GET /directoryindex3/inside/ HTTP/1.0\r\n"
        self.conf              = "Directory /directoryindex3 { Handler common }"
        self.expected_error    = 200

    def Prepare (self, www):
        self.Mkdir (www, "directoryindex3/inside/foo")

        self.expected_content  = ["DocumentRoot %s" % (www),
                                  "ScriptName /super_test_index.php",
                                  "RequestUri /directoryindex3/inside/"]

    def Precondition (self):
        return os.path.exists (PHPCGI_PATH)

    def JustBefore (self, www):
        self.WriteFile (www, "super_test_index.php", 0444, """<?php
                        echo "DocumentRoot ".$_SERVER[DOCUMENT_ROOT]."\n";
                        echo "ScriptName "  .$_SERVER[SCRIPT_NAME]."\n";
                        echo "RequestUri "  .$_SERVER[REQUEST_URI]."\n";
                        ?>""")

    def JustAfter (self, www):
        self.Remove (www, "super_test_index.php")

