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

import re
import CTK
import Page
import Cherokee
import SelectionPanel
import validations

from consts import *
from CTK.util import find_copy_name
from CTK.Submitter import HEADER as Submit_HEADER
from CTK.TextField import HEADER as TextField_HEADER


URL_BASE  = '/source'
URL_APPLY = '/source/apply'
HELPS     = [('config_info_sources', N_("Information Sources"))]

NOTE_SOURCE        = N_('The source can be either a local interpreter or a remote host acting as an information source.')
NOTE_NICK          = N_('Source nick. It will be referenced by this name in the rest of the server.')
NOTE_TYPE          = N_('It allows to choose whether it runs the local host or a remote server.')
NOTE_HOST          = N_('Where the information source can be accessed. The host:port pair, or the Unix socket path.')
NOTE_INTERPRETER   = N_('Command to spawn a new source in case it were not accessible.')
NOTE_TIMEOUT       = N_('How long should the server wait when spawning an interpreter before giving up (in seconds). Default: 3.')
NOTE_USAGE         = N_('Sources currently in use. Note that the last source of any rule cannot be deleted until the rule has been manually edited.')
NOTE_USER          = N_('Execute the interpreter under a different user. Default: Same UID as the server.')
NOTE_GROUP         = N_('Execute the interpreter under a different group. Default: Default GID of the new process UID.')
NOTE_ENV_INHETIR   = N_('Whether the new child process should inherit the environment variables from the server process. Default: yes.')
NOTE_DELETE_DIALOG = N_('You are about to delete an Information Source. Are you sure you want to proceed?')
NOTE_NO_ENTRIES    = N_('The Information Source list is currently empty.')

VALIDATIONS = [
    ('source!.+?!host',        validations.is_information_source),
    ('source!.+?!timeout',     validations.is_positive_int),
    ('tmp!new_host',           validations.is_safe_information_source),
    ("source_clone_trg",       validations.is_safe_id),
]

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


def commit_clone():
    num = re.findall(r'^%s/([\d]+)/clone$'%(URL_BASE), CTK.request.url)[0]
    next = CTK.cfg.get_next_entry_prefix ('source')

    orig  = CTK.cfg.get_val ('source!%s!nick'%(num))
    names = [CTK.cfg.get_val('source!%s!nick'%(x)) for x in CTK.cfg.keys('source')]
    new_nick = find_copy_name (orig, names)

    CTK.cfg.clone ('source!%s'%(num), next)
    CTK.cfg['%s!nick' %(next)] = new_nick
    return {'ret': 'ok'}


def commit():
    new_nick = CTK.post.pop('tmp!new_nick')
    new_host = CTK.post.pop('tmp!new_host')

    # New
    if new_nick and new_host:
        next = CTK.cfg.get_next_entry_prefix ('source')
        CTK.cfg['%s!nick'%(next)] = new_nick
        CTK.cfg['%s!host'%(next)] = new_host
        CTK.cfg['%s!type'%(next)] = 'host'
        return {'ret': 'ok'}

    # Modification
    return CTK.cfg_apply_post()


class Render_Source():
    def __call__ (self):
        # /source/empty
        if CTK.request.url.endswith('/empty'):
            notice = CTK.Notice ('information', CTK.RawHTML (NOTE_NO_ENTRIES))
            return notice.Render().toJSON()

        # /source/\d+
        num = re.findall(r'^%s/([\d]+)$'%(URL_BASE), CTK.request.url)[0]

        tipe = CTK.cfg.get_val('source!%s!type'%(num))
        nick = CTK.cfg.get_val('source!%s!nick'%(num))

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>Source: %s</h2>'%(nick))

        table = CTK.PropsTable()
        table.Add (_('Type'),       CTK.ComboCfg ('source!%s!type'%(num), SOURCE_TYPES), _(NOTE_TYPE))
        table.Add (_('Nick'),       CTK.TextCfg ('source!%s!nick'%(num), False), _(NOTE_NICK))
        table.Add (_('Connection'), CTK.TextCfg ('source!%s!host'%(num), False), _(NOTE_HOST))
        if tipe == 'interpreter':
            table.Add (_('Interpreter'),         CTK.TextCfg ('source!%s!interpreter'%(num),   False), _(NOTE_INTERPRETER))
            table.Add (_('Spawning timeout'),    CTK.TextCfg ('source!%s!timeout'%(num),       True),  _(NOTE_TIMEOUT))
            table.Add (_('Execute as User'),     CTK.TextCfg ('source!%s!user'%(num),          True),  _(NOTE_USER))
            table.Add (_('Execute as Group'),    CTK.TextCfg ('source!%s!group'%(num),         True),  _(NOTE_GROUP))
            table.Add (_('Inherit Environment'), CTK.TextCfg ('source!%s!env_inherited'%(num), False), _(NOTE_ENV_INHETIR))

        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden ('source', num)
        submit += table
        cont += submit

        render = cont.Render()
        return render.toJSON()


class CloneSource (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)
        self += CTK.RawHTML ('Information Source Cloning dialog.')


class AddSource (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsTable()
        table.Add (_('Nick'),       CTK.TextCfg ('tmp!new_nick', False, {'class': 'noauto'}), _(NOTE_NICK))
        table.Add (_('Connection'), CTK.TextCfg ('tmp!new_host', False, {'class': 'noauto'}), _(NOTE_HOST))

        submit = CTK.Submitter (URL_APPLY)
        submit += table
        self += submit


class Render():
    class PanelList (CTK.Container):
        def __init__ (self, refresh, right_box):
            CTK.Container.__init__ (self)

            # Helper
            entry = lambda klass, key: CTK.Box ({'class': klass}, CTK.RawHTML (CTK.cfg.get_val(key, '')))

            # Build the panel list
            panel = SelectionPanel.SelectionPanel (None, right_box.id, URL_BASE, '%s/empty'%(URL_BASE), draggable=False)
            self += panel

            sources = CTK.cfg.keys('source')
            sources.sort (lambda x,y: cmp (int(x), int(y)))

            for k in sources:
                tipe = CTK.cfg.get_val('source!%s!type'%(k))

                dialog = CTK.Dialog ({'title': _('Do you really want to remove it?'), 'width': 480})
                dialog.AddButton (_('Remove'), CTK.JS.Ajax (URL_APPLY, async=False,
                                                            data    = {'source!%s'%(k):''},
                                                            success = dialog.JS_to_close() + \
                                                                      refresh.JS_to_refresh()))
                dialog.AddButton (_('Cancel'), "close")
                dialog += CTK.RawHTML (NOTE_DELETE_DIALOG)
                self += dialog

                remove = CTK.ImageStock('del', {'class': 'del'})
                remove.bind ('click', dialog.JS_to_show() + "return false;")

                group = CTK.Box ({'class': 'sel-actions'}, [remove])

                if tipe == 'host':
                    panel.Add (k, '/source/%s'%(k), [group, 
                                                     entry('nick',  'source!%s!nick'%(k)),
                                                     entry('type',  'source!%s!type'%(k)),
                                                     entry('host',  'source!%s!host'%(k))])
                elif tipe == 'interpreter':
                    panel.Add (k, '/source/%s'%(k), [group,
                                                     entry('nick',  'source!%s!nick'%(k)),
                                                     entry('type',  'source!%s!type'%(k)),
                                                     entry('host',  'source!%s!host'%(k)),
                                                     entry('inter', 'source!%s!interpreter'%(k))])


    class PanelButtons (CTK.Box):
        def __init__ (self):
            CTK.Box.__init__ (self, {'class': 'panel-buttons'})

            # Add New
            dialog = CTK.Dialog ({'title': _('Add New Information Source'), 'width': 380})
            dialog.AddButton (_('Add'), dialog.JS_to_trigger('submit'))
            dialog.AddButton (_('Cancel'), "close")
            dialog += AddSource()

            button = CTK.Button(_('New…'), {'id': 'source-new-button', 'class': 'panel-button', 'title': _('Add New Information Source')})
            button.bind ('click', dialog.JS_to_show())
            dialog.bind ('submit_success', dialog.JS_to_close())
            dialog.bind ('submit_success', self.JS_to_trigger('submit_success'));

            self += button
            self += dialog

            # Clone
            dialog = CTK.Dialog ({'title': _('Clone Information Source'), 'width': 480})
            dialog.AddButton (_('Clone'), JS_CLONE + dialog.JS_to_close())
            dialog.AddButton (_('Cancel'), "close")
            dialog += CloneSource()

            button = CTK.Button(_('Clone…'), {'id': 'source-clone-button', 'class': 'panel-button', 'title': _('Clone Selected Information Source')})
            button.bind ('click', dialog.JS_to_show())

            self += dialog
            self += button

    def __call__ (self):
        # Content
        left  = CTK.Box({'class': 'panel'})
        left += CTK.RawHTML('<h2>%s</h2>'%(_('Information Sources')))

        # Sources List
        refresh = CTK.Refreshable ({'id': 'source_panel'})
        refresh.register (lambda: self.PanelList(refresh, right).Render())

        # Refresh on 'New' or 'Clone'
        buttons = self.PanelButtons()
        buttons.bind ('submit_success', refresh.JS_to_refresh (on_success=JS_ACTIVATE_LAST))
        left += buttons

        left += CTK.Box({'class': 'filterbox'}, CTK.TextField({'class':'filter', 'optional_string': _('Sources Filtering'), 'optional': True}))

        right = CTK.Box({'class': 'source_content'})

        left += refresh

        # Refresh the list whenever the content change
        right.bind ('submit_success', refresh.JS_to_refresh());

        # Build the page
        headers = Submit_HEADER + TextField_HEADER
        page = Page.Base (_("Information Sources"), body_id='source', helps=HELPS, headers=headers)
        page += left
        page += right

        return page.Render()

CTK.publish ('^%s$'              %(URL_BASE), Render)
CTK.publish ('^%s/([\d]+|empty)$'%(URL_BASE), Render_Source)
CTK.publish ('^%s$'              %(URL_APPLY), commit, validation=VALIDATIONS, method="POST")
CTK.publish ('^%s/[\d]+/clone$'  %(URL_BASE), commit_clone)
