# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2001-2010 Alvaro Lopez Ortega
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of version 2 of the GNU General Public
# License as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import CTK
import Page
import Cherokee

from configured import *

class ServerInfo (CTK.Table):
    def __init__ (self):
        CTK.Table.__init__ (self)

        self.add (_('Status'),      ['Stopped', 'Running'][Cherokee.server.is_alive()])
        self.add (_('PID'),         Cherokee.pid.pid)
        self.add (_('Version'),     VERSION)
        self.add (_("Default WWW"), self._get_droot())
        self.add (_("Prefix"),      PREFIX)

    def add (self, title, string):
        self += [CTK.RawHTML(title), CTK.RawHTML(str(string))]

    def _get_droot (self):
        tmp = [int(x) for x in CTK.cfg.keys('vserver')]
        tmp.sort()

        if not tmp:
            return WWWROOT

        return CTK.cfg.get_val ('vserver!%d!document_root'%(tmp[0]), WWWROOT)


class Render():
    def __call__ (self):
        Cherokee.pid.refresh()

        self.page = Page.Base(_("Index"))
        self.page += CTK.RawHTML (_('<h1>Welcome to Cherokee-Admin</h1>'))
        self.page += ServerInfo()

        return self.page.Render()


CTK.publish ('/', Render)
