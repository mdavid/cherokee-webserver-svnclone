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

class PortsWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)
        self += CTK.RawHTML ('Ports Content')

class NetworkWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)
        self += CTK.RawHTML ('Network Content')


class Render():
    def __call__ (self):
        txt = "<h1>%s</h1>" % (_('General Settings'))

        tabs = CTK.Tab()
        tabs.Add (_('Network'),         NetworkWidget())
        tabs.Add (_('Ports to listen'), PortsWidget())

        page = Page.Base()
        page += CTK.RawHTML(txt)
        page += tabs

        return page.Render()


CTK.publish ('^/general', Render)
