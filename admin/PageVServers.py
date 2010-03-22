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
import SelectionPanel
import validations

from util import *
from consts import *
from CTK.Tab       import HEADER as Tab_HEADER
from CTK.Submitter import HEADER as Submit_HEADER
from CTK.TextField import HEADER as TextField_HEADER
from CTK.SortableList import HEADER as SortableList_HEADER


URL_BASE  = r'/vserver'
URL_APPLY = r'/vserver/apply'

HELPS = [('config_virtual_servers', N_("Virtual Servers"))]

NOTE_DELETE_DIALOG = N_('You are about to delete a Virtual Server. Are you sure you want to proceed?')


JS_ACTIVATE_LAST = """
$('.selection-panel:first').data('selectionpanel').select_last();
"""

JS_CLONE = """
  var panel = $('.selection-panel:first').data('selectionpanel').get_selected();
  var url   = panel.find('.row_content').attr('url');
  $.ajax ({type: 'GET', async: false, url: url+'/clone', success: function(data) {
      // A transaction took place
      $('.panel-buttons').trigger ('submit_success');
  }});
"""


def commit():
    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]

    return {'ret': 'ok'}


def reorder (arg):
    print "reorder", CTK.post[arg]
    return {'ret': 'ok'}


class CloneVServer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)
        self += CTK.RawHTML ('About to clone a Virtual Server.')


class Render():
    class PanelList (CTK.Container):
        def __init__ (self, refresh, right_box):
            CTK.Container.__init__ (self)

            # Sanity check
            if not CTK.cfg.keys('vserver'):
                CTK.cfg['vserver!1!nick']           = 'default'
                CTK.cfg['vserver!1!document_root']  = '/tmp'
                CTK.cfg['vserver!1!rule!1!match']   = 'default'
                CTK.cfg['vserver!1!rule!1!handler'] = 'common'

            # Helper
            entry = lambda klass, key: CTK.Box ({'class': klass}, CTK.RawHTML (CTK.cfg.get_val(key, '')))

            # Build the panel list
            panel = SelectionPanel.SelectionPanel (reorder, right_box.id, URL_BASE, 'a')
            self += panel

            # Build the Virtual Server list
            vservers = CTK.cfg.keys('vserver')
            vservers.sort (lambda x,y: cmp(int(x), int(y)))
            vservers.reverse()

            for k in vservers:
                content = [entry('nick',  'vserver!%s!nick'%(k)),
                           entry('droot', 'vserver!%s!document_root'%(k))]

                if k != vservers[-1]:
                    # Remove
                    dialog = CTK.Dialog ({'title': _('Do you really want to remove it?'), 'width': 480})
                    dialog.AddButton (_('Remove'), CTK.JS.Ajax (URL_APPLY, async=False,
                                                                data    = {'vserver!%s'%(k):''},
                                                                success = dialog.JS_to_close() + \
                                                                    refresh.JS_to_refresh()))
                    dialog.AddButton (_('Cancel'), "close")
                    dialog += CTK.RawHTML (NOTE_DELETE_DIALOG)
                    self += dialog

                    remove = CTK.ImageStock('del', {'class': 'del'})
                    remove.bind ('click', dialog.JS_to_show() + "return false;")

                    # Disable
                    disabled = CTK.Box ({'class': 'disable'}, CTK.iPhoneCfg('vserver!%s!disabled'%(k), False))
                    content += [disabled, remove]

                # List entry
                panel.Add ('/vserver/content/%s'%(k), content)


    class PanelButtons (CTK.Box):
        def __init__ (self):
            CTK.Box.__init__ (self, {'class': 'panel-buttons'})

            # Clone
            dialog = CTK.Dialog ({'title': _('Clone Virtual Server'), 'width': 480})
            dialog.AddButton (_('Clone'), JS_CLONE + dialog.JS_to_close())
            dialog.AddButton (_('Cancel'), "close")
            dialog += CloneVServer()

            button = CTK.Button(_('Clone'))
            button.bind ('click', dialog.JS_to_show())

            self += dialog
            self += button


    def __call__ (self):
        title = _('Virtual Servers')

        # Content
        right = CTK.Box({'class': 'vserver_content'})
        left  = CTK.Box({'class': 'panel'}, CTK.RawHTML('<h2>%s</h2>'%(title) ))

        # Virtual Server List
        refresh = CTK.Refreshable ({'id': 'vservers_panel'})
        refresh.register (lambda: self.PanelList(refresh, right).Render())

        # Refresh on 'New' or 'Clone'
        buttons = self.PanelButtons()
        buttons.bind ('submit_success', refresh.JS_to_refresh (on_success=JS_ACTIVATE_LAST))

        left += refresh
        left += buttons

        # Refresh the list whenever the content change
        right.bind ('submit_success', refresh.JS_to_refresh());

        # Build the page
        headers = Tab_HEADER + Submit_HEADER + TextField_HEADER + SortableList_HEADER
        page = Page.Base(title, body_id='vservers', helps=HELPS, headers=headers)
        page += left
        page += right

        return page.Render()


CTK.publish (URL_BASE, Render)
CTK.publish (URL_APPLY, commit, method="POST")
