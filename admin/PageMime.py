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
    # New entry
    mime = CTK.post.pop('new_mime')
    exts = CTK.post.pop('new_exts')
    mage = CTK.post.pop('new_mage')

    if mime:
        if mage:
            CTK.cfg['mime!%s!max-age'%(mime)] = mage
        if exts:
            CTK.cfg['mime!%s!extensions'%(mime)] = exts

        return {'ret': 'ok'}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]

    return {'ret': 'ok'}


class MIME_Table (CTK.Container):
    def __init__ (self, refreshable, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        # List
        table = CTK.Table ({'id': "mimetable"})
        table.set_header(1)
        table += [CTK.RawHTML(x) for x in (_('Mime type'), _('Extensions'), _('MaxAge<br/>(<i>secs</i>)'))]

        mimes = CTK.cfg.keys('mime')
        mimes.sort()

        for mime in mimes:
            pre = "mime!%s"%(mime)
            e1 = CTK.TextCfgAuto ('%s!extensions'%(pre), URL_APPLY, False, {'size': 35})
            e2 = CTK.TextCfgAuto ('%s!max-age'%(pre),    URL_APPLY, True,  {'size': 6, 'maxlength': 6})
            rm = CTK.ImageStock('del')
            table += [CTK.RawHTML(mime), e1, e2, rm]

        self += CTK.Indenter (table)

        # Add new MIME
        table = CTK.PropsTable()
        table.Add (_('Mime Type'),  CTK.TextField({'name': 'new_mime', 'class': 'noauto'}), _(NOTE_NEW_MIME))
        table.Add (_('Extensions'), CTK.TextField({'name': 'new_exts', 'class': 'noauto optional'}), _(NOTE_NEW_EXTENSIONS))
        table.Add (_('Max Age'),    CTK.TextField({'name': 'new_mage', 'class': 'noauto optional'}), _(NOTE_NEW_MAXAGE))

        submit = CTK.Submitter(URL_APPLY)
        submit += table
        submit += CTK.SubmitterButton(_("Add"))
        submit.bind ('submit_success', refreshable.JS_to_refresh())

        self += CTK.RawHTML ('<h2>%s</h2>\n' %(_('Add new MIME')))
        self += CTK.Indenter(submit)


class MIME_Table_Instancer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # Refresher
        refresh = CTK.Refreshable()
        refresh.register (lambda: MIME_Table(refresh).Render())
        self += refresh

class Render():
    def __call__ (self):
        page = Page.Base(_('Icons'), body_id='mime', helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('MIME Types')))
        page += MIME_Table_Instancer()
        return page.Render()


CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
