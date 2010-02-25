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
import validations

URL_BASE  = '/vserver'
URL_APPLY = '/vserver/apply'

HELPS = [('config_virtual_servers', N_("Virtual Servers"))]


class Render():
    def __call__ (self):
        title = _('Virtual Servers')

        page = Page.Base (title, helps=HELPS)
        page += CTK.RawHTML ("<h1>%s</h1>" % (title))
        return page.Render()


CTK.publish (URL_BASE, Render)
