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

import os
import CTK

class Base (CTK.Page):
    def __init__ (self, helps=None, headers=None):
        # Look for the theme file
        srcdir = os.path.dirname (os.path.realpath (__file__))
        theme_file = os.path.join (srcdir, 'theme.html')

        # Parent's constructor
        template = CTK.Template (filename = theme_file)
        CTK.Page.__init__ (self, template, headers)

        # Help
        self.helps = helps
        template['helps'] = self._render_helps()

    def _render_helps (self):
        txt = ''
        for ref, name in self.helps:
            txt += '<div class="help_entry"><a href="/help/%s.html">%s</a></div>' %(ref, name)
        return txt
