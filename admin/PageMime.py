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

from consts import *
from configured import *

URL_BASE  = '/mime'
URL_APPLY = '/mime/apply'

HELPS = [('config_mime_types', N_("MIME types"))]

NOTE_NEW_MIME       = N_('New MIME type to be added.')
NOTE_NEW_EXTENSIONS = N_('Comma separated list of file extensions associated with the MIME type.')
NOTE_NEW_MAXAGE     = N_('Maximum time that this sort of content can be cached (in seconds).')

def apply():
    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


class Render():
    def __call__ (self):
        page = Page.Base(_('Icons'), helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('MIME Types')))
        return page.Render()


CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
