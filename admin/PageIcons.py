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

ICON_ROW_SIZE = 9

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
                desc   = prettyfier(k)
                image  = CTK.Image ({'alt'  : desc, 'title': desc, 'src': os.path.join('/icons_local', k)})
                delete = CTK.ImageStock('del')
                submit = CTK.Submitter (URL_APPLY)
                submit += CTK.TextCfg (pre, props={'size': '46'})
                table += [image, submit, delete]

                delete.bind('click', CTK.JS.Ajax (URL_APPLY, data = {pre: ''},
                                                  complete = refreshable.JS_to_refresh()))

            self += CTK.RawHTML ("<h2>%s</h2>" %_('Extension List'))
            self += CTK.Indenter (table)


        # New entry
        button = AddDialogButton ('new_exts','Extensions')
        button.bind ('submit_success', refreshable.JS_to_refresh())

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Add New Extensions'))
        self += CTK.Indenter (button)


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
                desc    = prettyfier(k)
                image   = CTK.Image ({'alt'  : desc, 'title': desc, 'src': os.path.join('/icons_local', k)})
                submit  = CTK.Submitter (URL_APPLY)
                submit += CTK.TextCfg (pre, props={'size': '46'})
                delete  = CTK.ImageStock('del')
                table  += [image, submit, delete]

                delete.bind('click', CTK.JS.Ajax (URL_APPLY, data = {pre: ''},
                                                  complete = refreshable.JS_to_refresh()))

            self += CTK.RawHTML ("<h2>%s</h2>" %_('File Matches'))
            self += CTK.Indenter (table)

        # New file
        button = AddDialogButton ('new_file','File')
        button.bind ('submit_success', refreshable.JS_to_refresh())

        self += CTK.RawHTML ("<h2>%s</h2>" %_('Add New File Match'))
        self += CTK.Indenter (button)


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

ICON_CHOOSER_JS = """
$("input[type=hidden]").val("%s");
$(".icon_chooser").removeClass("icon_chooser_selected");
$("#%s").addClass("icon_chooser_selected");
"""
class IconChooser (CTK.Container):
    def __init__ (self, field_name, prefix):
        CTK.Container.__init__ (self)
        files, unused = self.get_files (prefix)
        total = len(files)

        hidden = CTK.Hidden(field_name, '')
        table  = CTK.Table()
        row = []
        for x in range(total):
            filename = files[x]
            desc     = prettyfier (filename)
            props = {'alt'  : desc,
                     'title': desc,
                     'src'  : '/icons_local/%s'%(filename),
                     'class': 'icon_chooser icon_chooser_used'}
            if filename in unused:
                     props['class'] = 'icon_chooser icon_chooser_unused'

            image = CTK.Image(props)
            if filename in unused:
                js = ICON_CHOOSER_JS % (filename, image.id)
                image.bind('click', js)
            row.append (image)
            if (x+1) % ICON_ROW_SIZE == 0 or (x+1) == total:
                table += row
                row = []

        self += hidden
        self += table


    def get_files (self, prefix = None):
        listdir = os.listdir (CHEROKEE_ICONSDIR)
        files   = []
        for filename in listdir:
            ext = filename.lower().split('.')[-1]
            if ext in ('jpg', 'png', 'gif', 'svg'):
                files.append(filename)

        if not prefix:
            return (files, files)

        icons   = CTK.cfg.keys(prefix)
        unused  = list(set(files) - set(icons))
        unused.sort()
        return (files, unused)


class AddIcon (CTK.Container):
    def __init__ (self, key):
        CTK.Container.__init__ (self)
        descs = {'new_file':         _('File'),
                 'new_exts':         _('Extensions'),
                 'default':          _('Default'),
                 'directory':        _('Directory'),
                 'parent_directory': _('Go to Parent')}
        assert key in descs.keys()
        label = descs[key]

        table = CTK.Table()
        row = [CTK.Box ({'class': 'icon_label'}, CTK.RawHTML (label))]

        if key in ['new_file','new_exts']:
            field = CTK.TextField({'name': key, 'class': 'noauto'})
            row.append(CTK.Box ({'class': 'icon_field'}, field))
            hidden_name = '%s_icon' % key
            prefix = ['icons!suffix', 'icons!file'][key == 'new_file']
        else:
            hidden_name = '%s' % key
            prefix = None

        table  += row
        submit  = CTK.Submitter(URL_APPLY)
        submit += table
        submit += IconChooser(hidden_name, prefix)
        self += submit


class AddDialogButton (CTK.Box):
    def __init__ (self, key, name):
        CTK.Box.__init__ (self, {'class': '%s-button'%name})

        # Add New
        dialog = CTK.Dialog ({'title': _('Add new %s'%name), 'width': 480})
        dialog.AddButton (_('Add'), dialog.JS_to_trigger('submit'))
        dialog.AddButton (_('Cancel'), "close")
        dialog += AddIcon(key)

        button = CTK.Button(_('Add'))
        button.bind ('click', dialog.JS_to_show())
        dialog.bind ('submit_success', dialog.JS_to_close())
        dialog.bind ('submit_success', self.JS_to_trigger('submit_success'));

        self += button
        self += dialog


CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
