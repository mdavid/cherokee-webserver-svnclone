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

import os
import time

from configured import *

BETA_TESTER_NOTICE = N_("""
<h3>Beta testing</h3> <p>Individuals like yourself who download and
test the latest developer snapshots of Cherokee Web Server help us to
create the highest quality product. For that, we thank you.</p>
""")


def Launch():
    if not Cherokee.server.is_alive():
        Cherokee.server.launch()
    return CTK.HTTP_Redir('/')

def Stop():
    Cherokee.pid.refresh()
    Cherokee.server.stop()
    return CTK.HTTP_Redir('/')

class ServerInfo (CTK.Table):
    def __init__ (self):
        CTK.Table.__init__ (self)
        self.id = "server_info_table"
        self.set_header (column=True, num=1)

        self.add (_('Status'),       ['Stopped', 'Running'][Cherokee.server.is_alive()])
        self.add (_('PID'),          Cherokee.pid.pid or _("Not running"))
        self.add (_('Version'),      VERSION)
        self.add (_("Default WWW"),  self._get_droot())
        self.add (_("Prefix"),       PREFIX)
        self.add (_("Config File"),  CTK.cfg.file or _("Not found"))
        self.add (_("Modified"),     self._get_cfg_ctime())

    def add (self, title, string):
        self += [CTK.RawHTML(title), CTK.RawHTML(str(string))]

    def _get_droot (self):
        tmp = [int(x) for x in CTK.cfg.keys('vserver')]
        tmp.sort()

        if not tmp:
            return WWWROOT

        return CTK.cfg.get_val ('vserver!%d!document_root'%(tmp[0]), WWWROOT)

    def _get_cfg_ctime (self):
        info = os.stat(CTK.cfg.file)
        return time.ctime(info.st_ctime)


class Render():
    def __call__ (self):
        Cherokee.pid.refresh()

        self.page = Page.Base(_('Welcome to Cherokee Admin'))
        self.page += CTK.RawHTML ("<h1>%s</h1>"% _('Welcome to Cherokee Admin'))

        if 'b' in VERSION:
            notice  = CTK.Notice()
            notice += CTK.RawHTML(_(BETA_TESTER_NOTICE))
            self.page += notice

        self.page += ServerInfo()

        return self.page.Render()


CTK.publish ('^/$',       Render)
CTK.publish ('^/launch$', Launch)
CTK.publish ('^/stop$',   Stop)
