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
from CTK.Submitter import HEADER as Submit_HEADER
from CTK.TextField import HEADER as TextField_HEADER

URL_BASE  = '/source'
URL_APPLY = '/source/apply'
HELPS     = [('config_info_sources', N_("Information Sources"))]

NOTE_SOURCE      = N_('The source can be either a local interpreter or a remote host acting as an information source.')
NOTE_NICK        = N_('Source nick. It will be referenced by this name in the rest of the server.')
NOTE_TYPE        = N_('It allows to choose whether it runs the local host or a remote server.')
NOTE_HOST        = N_('Where the information source can be accessed. The host:port pair, or the Unix socket path.')
NOTE_INTERPRETER = N_('Command to spawn a new source in case it were not accessible.')
NOTE_TIMEOUT     = N_('How long should the server wait when spawning an interpreter before giving up (in seconds). Default: 3.')
NOTE_USAGE       = N_('Sources currently in use. Note that the last source of any rule cannot be deleted until the rule has been manually edited.')
NOTE_USER        = N_('Execute the interpreter under a different user. Default: Same UID as the server.')
NOTE_GROUP       = N_('Execute the interpreter under a different group. Default: Default GID of the new process UID.')
NOTE_ENV_INHETIR = N_('Whether the new child process should inherit the environment variables from the server process. Default: yes.')

VALIDATIONS = [
    ('source!.+?!host',        validations.is_information_source),
    ('source!.+?!timeout',     validations.is_positive_int),
    ('tmp!new_source_host',    validations.is_information_source),
    ('tmp!new_source_timeout', validations.is_positive_int),
    ("source_clone_trg",       validations.is_safe_id),
]

ENTRY_HOST = """
<div class="nick">%(nick)s</div>
<div class="type">%(type)s</div>
<div class="host">%(host)s</div>
"""

ENTRY_INTER = """
<div class="nick">%(nick)s</div>
<div class="type">%(type)s</div>
<div class="host">%(host)s</div>
<div class="inter">%(inter)s</div>
"""


def reorder (arg):
    print "reorder", CTK.post[arg]
    return {'ret': 'ok'}


class Render_Source():
    def __call__ (self):
        num = re.findall(r'^%s/([\d]+)$'%(URL_BASE), CTK.request.url)[0]

        tipe = CTK.cfg.get_val('source!%s!type'%(num))
        nick = CTK.cfg.get_val('source!%s!nick'%(num))

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h1>Source: %s</h1>'%(nick))

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
        return render.toStr()


class Render():
    class Content (CTK.Container):
        def __init__ (self, refresh, box_id):
            CTK.Container.__init__ (self)

            panel = SelectionPanel.SelectionPanel (reorder, box_id)

            # Build the panel list
            for k in CTK.cfg.keys('source'):
                props = {}
                props['host']  = CTK.cfg.get_val('source!%s!host'%(k))
                props['nick']  = CTK.cfg.get_val('source!%s!nick'%(k))
                props['type']  = tipe = CTK.cfg.get_val('source!%s!type'%(k))
                props['inter'] = CTK.cfg.get_val('source!%s!interpreter'%(k))

                if tipe == 'host':
                    panel.Add ('/source/%s'%(k), [CTK.RawHTML(ENTRY_HOST%(props))])
                elif tipe == 'interpreter':
                    panel.Add ('/source/%s'%(k), [CTK.RawHTML(ENTRY_INTER%(props))])

            self += panel

    def __call__ (self):
        # Content
        box = CTK.Box({'id': 'source_content'})

        # Sources List
        refresh = CTK.Refreshable ({'id': 'source_panel'})
        refresh.register (lambda: self.Content(refresh, box.id).Render())

        # Refresh the list whenever the content change
        box.bind ('submit_success', refresh.JS_to_refresh());

        # Build the page
        headers = Submit_HEADER + TextField_HEADER
        page = Page.Base (_("Information Sources"), body_id='source', helps=HELPS, headers=headers)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('Information Sources Settings')))
        page += refresh
        page += box

        #page += CTK.TextCfg('')
        #page += CTK.Submitter('')

        return page.Render()

CTK.publish ('^%s$'%(URL_BASE), Render)
CTK.publish ('^%s/[\d]+$'%(URL_BASE), Render_Source)
CTK.publish ('^%s$'%(URL_APPLY), CTK.cfg_apply_post, validation=VALIDATIONS, method="POST")
