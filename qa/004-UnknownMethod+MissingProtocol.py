from base import *

class Test (TestBase):
    def __init__ (self):
        TestBase.__init__ (self, __file__)
        self.name = "Unknown Method + Missing Protocol"

        self.expected_error = 501
        self.request        = "XYZ / \r\n" +\
                              "Connection: Keep-alive\r\n"

