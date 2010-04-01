# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2010 Alvaro Lopez Ortega
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
from urllib import quote

ERROR_LAUNCH_URL_ADMIN = N_('The server suggests to check <a href="%s">this page</a>. Most probably the problem can he solved in there.')


class PageErrorLaunch (CTK.Page):
    def __init__ (self, error_str, **kwargs):
        srcdir = os.path.dirname (os.path.realpath (__file__))
        theme_file = os.path.join (srcdir, 'exception.html')

        # Set up the template
        template = CTK.Template (filename = theme_file)
        template['body_props'] = ' id="body-launch-error"'

        # Parent's constructor
        CTK.Page.__init__ (self, template, **kwargs)

        # Error parse
        self._error     = None
        self._error_raw = error_str

        for line in error_str.split("\n"):
            if not line:
                continue

            if line.startswith("{'type': "):
                src = "self._error = " + line
                exec(src)
                break

        # Build the page content
        if self._error:
            template['title'] = '%s' %(self._error['title'])
            self._build_error_py()
        else:
            template['title'] = _('Server Launch Error')
            self._build_error_raw()

    def _build_error_py (self):
        self += CTK.RawHTML ('<h1>%s</h1>' %(_('Server Launch Error')))
        self += CTK.Box ({'class': 'description'}, CTK.RawHTML(self._error.get('description','')))

        admin_url = self._error.get('admin_url')
        if admin_url:
            self += CTK.Box ({'class': 'admin-url'}, CTK.RawHTML(_(ERROR_LAUNCH_URL_ADMIN)%(admin_url)))

        debug = self._error.get('debug')
        if debug:
            self += CTK.Box ({'class': 'debug'}, CTK.RawHTML(debug))

        backtrace = self._error.get('backtrace')
        if backtrace:
            self += CTK.Box ({'class': 'backtrace'}, CTK.RawHTML(backtrace))

        self += CTK.Box ({'class': 'time'}, CTK.RawHTML(self._error.get('time','')))

    def _build_error_raw (self):
        self += CTK.RawHTML ('<h1>%s</h1>' %(_('Server Launch Error')))
        self += CTK.Box ({'class': 'description'}, CTK.RawHTML(_('Something unexpected just happened.')))
        self += CTK.Box ({'class': 'backtrace'},   CTK.RawHTML(self._error_raw))
