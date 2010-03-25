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
        if not icon:
            return {'ret': 'ok'}

        CTK.cfg['icons!suffix!%s'%(icon)] = new_exts
        return {'ret': 'ok'}

    # New file
    new_file = CTK.post.pop('new_file')
    if new_file:
        icon = CTK.post.get_val('new_file_icon')
        if not icon:
            return {'ret': 'ok'}

        CTK.cfg['icons!file!%s'%(icon)] = new_file
        return {'ret': 'ok'}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


ICON_COMBO_SET_JS = """
$("#%(id_combo)s").change(function() {
   $("#%(id_img)s").attr('src', "/icons_local/" + $("#%(id_combo)s option:selected").val());
});
"""
def prettyfier (filen):
    # Remove extension
    if '.' in filen:
        filen = filen[:filen.rindex('.')]

    # Remove underscores
    filen = filen.replace('_',' ').replace('.',' ')

    # Capitalize
    return filen.capitalize()

class IconComboSet (CTK.Widget):
    def __init__ (self, key, add_choose=False, _props={}):
        CTK.Widget.__init__ (self)
        props = _props.copy()

        # Check the icon files
        file_options = ([],[('',_('Choose..'))])[add_choose]

        for file in self.get_unused_files():
            ext = file.lower().split('.')[-1]
            if ext in ('jpg', 'png', 'gif', 'svg'):
                file_options.append((file, prettyfier(file)))

        # Public widgets
        selected = CTK.cfg.get_val (key, 'blank.png')
        self.image = CTK.Image({'src': '/icons_local/%s'%(selected)})
        self.combo = CTK.ComboCfg (key, file_options, props)

    def get_unused_files (self):
        icons   = CTK.cfg.keys('icons!suffix')
        listdir = os.listdir (CHEROKEE_ICONSDIR)
        unused  = list(set(listdir) - set(icons))
        unused.sort()
        return unused

    def Render(self):
        render = CTK.Widget.Render (self)
        render.js += ICON_COMBO_SET_JS %({'id_img': self.image.id, 'id_combo': self.combo.id})
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

            icons.sort()
            for k in icons:
                pre    = 'icons!suffix!%s'%(k)
                image  = CTK.Image ({'src': os.path.join('/icons_local', k)})
                delete = CTK.ImageStock('del')
                submit = CTK.Submitter (URL_APPLY)
                submit += CTK.TextCfg (pre, props={'size': '46'})
                table += [image, CTK.RawHTML(prettyfier(k)), submit, delete]

                delete.bind('click', CTK.JS.Ajax (URL_APPLY, data = {pre: ''},
                                                  complete = refreshable.JS_to_refresh()))

            self += CTK.RawHTML ("<h2>%s</h2>" %_('Extension List'))
            self += CTK.Indenter (table)

        # Nex entry
        exts   = CTK.TextField({'name': "new_exts", 'class': "noauto"})
        icombo = IconComboSet ("new_exts_icon", True, {'class': "required"})
        button = CTK.SubmitterButton (_('Add'))

        table = CTK.Table()
        table.set_header(1)
        table += [None, CTK.RawHTML(_('Icon')), CTK.RawHTML(_('Extensions'))]
        table += [icombo.image, icombo.combo, exts, button]

        submit = CTK.Submitter(URL_APPLY)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit.bind ('submit_success', exts.JS_to_focus())
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Add new extension'))
        self += CTK.Indenter (submit)
        self += icombo


class FilesTable (CTK.Container):
    def __init__ (self, refreshable, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        # List
        icons = CTK.cfg.keys('icons!file')
        if icons:
            table = CTK.Table()
            table.id = "icon_files"
            table += [None, CTK.RawHTML(_('Match')), CTK.RawHTML(_('Files'))]
            table.set_header(1)

            for k in icons:
                pre     = 'icons!file!%s'%(k)
                image   = CTK.Image ({'src': os.path.join ('/icons_local', k)})
                submit  = CTK.Submitter (URL_APPLY)
                submit += CTK.TextCfg (pre, props={'size': '46'})
                delete  = CTK.ImageStock('del')
                table  += [image, CTK.RawHTML(prettyfier(k)), submit, delete]

                delete.bind('click', CTK.JS.Ajax (URL_APPLY, data = {pre: ''},
                                                  complete = refreshable.JS_to_refresh()))

            self += CTK.RawHTML ("<h2>%s</h2>" %_('File Matches'))
            self += CTK.Indenter (table)

        # Nex file
        nfile  = CTK.TextField({'name': "new_file", 'class': "noauto"})
        icombo = IconComboSet ("new_file_icon", True, {'class': "required"})
        button = CTK.SubmitterButton (_('Add'))

        table = CTK.Table()
        table.set_header(1)
        table += [None, CTK.RawHTML(_('Icon')), CTK.RawHTML(_('File'))]
        table += [icombo.image, icombo.combo, nfile, button]

        submit = CTK.Submitter(URL_APPLY)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Add New File Match'))
        self += CTK.Indenter (submit)
        self += icombo


class SpecialWidget (CTK.Container):
    def __init__ (self, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        table = CTK.Table()

        for k, desc in [('default',          _('Default')),
                        ('directory',        _('Directory')),
                        ('parent_directory', _('Go to Parent'))]:

            icon = CTK.cfg.get_val('icons!%s'%(k))
            iset = IconComboSet ('icons!%s'%(k))
            self += iset

            table += [iset.image, CTK.RawHTML(desc), iset.combo]

        submit  = CTK.Submitter(URL_APPLY)
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Special Files'))
        self += CTK.Indenter (submit)


class ExtensionsWidget_Instancer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # Refresher
        refresh = CTK.Refreshable ({'id': 'icons_extensions'})
        refresh.register (lambda: ExtensionsTable(refresh).Render())
        self += refresh

class FilesWidget_Instancer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # Refresher
        refresh = CTK.Refreshable ({'id': 'icons_files'})
        refresh.register (lambda: FilesTable(refresh).Render())
        self += refresh


class Render():
    def __call__ (self):
        tabs = CTK.Tab()
        tabs.Add (_('Extensions'),    ExtensionsWidget_Instancer())
        tabs.Add (_('Files'),         FilesWidget_Instancer())
        tabs.Add (_('Special Icons'), SpecialWidget())

        page = Page.Base(_('Icons'), body_id='icons', helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('Icon Configuration')))
        page += tabs

        return page.Render()


CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
