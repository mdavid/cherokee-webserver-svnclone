from base import *


class TestEntry (TestBase):
    def __init__ (self):
        TestBase.__init__ (self)
        self.expected_error = 405
        

class Test (TestCollection):
    def __init__ (self):
        TestCollection.__init__ (self)

        self.name = "HTTP Methods"
        self.conf = "Directory /methods_group { Handler common } "

    def Prepare (self, www):

        obj = self.Add (TestEntry())
        obj.request = "DELETE /methods_group/folder1 HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" 

        obj = self.Add (TestEntry())
        obj.request = "COPY /methods_group/index.html HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" +\
                      "Destination:  http://www.example.com/folder/index.html\r\n" +\
                      "Overwrite: T\r\n"

        obj = self.Add (TestEntry())
        obj.request = "MOVE /methods_group/folder1/ HTTP/1.1\r\n" +\
                      "Destination: http://www.example.com/pub2/folder2/\r\n" +\
                      "Host: www.example.com\r\n"

        obj = self.Add (TestEntry())
        obj.request = "POLL /public/subtest/ HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" +\
                      "Subscription-ID: 8,9,10,11,12\r\n" +\
                      "Content-Length: 0\r\n"

        obj = self.Add (TestEntry())
        obj.request = "SUBSCRIBE /public/subtest HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" +\
                      "Notification-type: Update\r\n" +\
                      "Call-Back: httpu://www.example.com:8080/510\r\n"

        obj = self.Add (TestEntry())
        obj.request = "UNSUBSCRIBE /public/subtest HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" +\
                      "Subscription-id: 16\r\n"

        obj = self.Add (TestEntry())
        obj.request = "LOCK /public/docs/myfile.doc HTTP/1.1\r\n" +\
                      "Host: www.example.com\r\n" +\
                      "Timeout: Infinite, Second-4100000000\r\n" +\
                      "If: (<opaquelocktoken:e71df4fae-5dec-22d6-fea5-00a0c91e6be4>)\r\n"

        obj = self.Add (TestEntry())
        obj.request = "UNLOCK /docs/myfile.doc HTTP/1.1\r\n" +\
                      "Host: www.contoso.com\r\n" +\
                      "Lock-Token: <opaquelocktoken:e71d4fae-5dec-22df-fea5-00a0c93bd5eb1>\r\n"


