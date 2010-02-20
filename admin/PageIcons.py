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

from consts import *
from configured import *

URL_BASE  = '/icons'
URL_APPLY = '/icons/apply'

HELPS = [('config_icons', N_('Icons'))]

def apply():
    # New extension
    new_exts = CTK.post.pop('new_exts')
    if new_exts:
        icon = CTK.post.get_val('new_exts_icon')
        CTK.cfg['icons!suffix!%s'%(icon)] = new_exts
        return {'ret': 'ok'}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


ICON_COMBO_JS = """
$("#%(id_combo)s").change(function() {
   $("#%(id_img)s").attr('src', "/icons_local/" + $("#%(id_combo)s option:selected").text());
});
"""

class IconCombo (CTK.Widget):
    def __init__ (self, key):
        CTK.Widget.__init__ (self)

        # Check the icon files
        file_options = []
        for file in os.listdir (CHEROKEE_ICONSDIR):
            f = file.lower()
            if (f.endswith(".jpg") or
                f.endswith(".png") or
                f.endswith(".gif") or
                f.endswith(".svg")):
                file_options.append((file, file))

        # Icon
        self.image    = CTK.Image()
        self.image.id = "image_%s" %(key)

        # Check the selected option
        selected = CTK.cfg.get_val(key)
        if not selected:
            file_options.insert (0, ('', _('Choose..')))
        else:
            image.props['src'] = os.path.join('/icons_local', k)

        # Combo
        self.combo = CTK.ComboCfg (key, file_options, {'class': "required"})

    def Render(self):
        render = self.combo.Render()
        render.js += ICON_COMBO_JS %({'id_img':   self.image.id,
                                      'id_combo': self.combo.id})
        return render

class ExtensionsTable (CTK.Container):
    def __init__ (self, refreshable, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        # List
        icons = CTK.cfg.keys('icons!suffix')
        if icons:
            table = CTK.Table()
            table.id = "icon_extensions"
            table += [None, CTK.RawHTML(_('Icon File')), CTK.RawHTML(_('Extensions'))]
            table.set_header(1)

            for k in icons:
                image  = CTK.Image ({'src': os.path.join('/icons_local', k)})
                submit = CTK.Submitter (URL_APPLY)
                submit += CTK.TextCfg ('icons!suffix!%s'%(k), props={'size': '46'})
                table += [image, CTK.RawHTML(k), submit]

            self += table

        # Nex entry
        exts   = CTK.TextField({'name': "new_exts", 'class': "noauto"})
        icombo = IconCombo ("new_exts_icon")
        button = CTK.SubmitterButton (_('Add'))

        table = CTK.Table()
        table.set_header(1)
        table += [None, CTK.RawHTML(_('Icon')), CTK.RawHTML(_('Extensions'))]
        table += [icombo.image, icombo, exts, button]

        submit = CTK.Submitter(URL_APPLY)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit.bind ('submit_success', exts.JS_to_focus())
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Add new extension'))
        self += CTK.Indenter (submit)

class ExtensionsWidget_Instancer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # Refresher
        refresh = CTK.Refreshable()
        refresh.register (lambda: ExtensionsTable(refresh).Render())
        self += refresh

class Render():
    def __call__ (self):
        tabs = CTK.Tab()
        tabs.Add (_('Extensions'), ExtensionsWidget_Instancer())

        page = Page.Base(_('Icons'), helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('Icon configuration')))
        page += tabs

        return page.Render()


CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
