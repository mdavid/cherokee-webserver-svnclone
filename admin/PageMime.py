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
import util
import Mime


URL_BASE  = '/mime'

HELPS = [('config_mime_types', N_("MIME types"))]

class Render():
    # WIP: To be removed when PageMime becomes a Tab
    def __call__ (self):
        page = Page.Base(_('MIME types configuration'), body_id='mime', helps=HELPS)
        page += Mime.MIME_Widget()
        return page.Render()


CTK.publish ('^%s'%(URL_BASE), Render)
