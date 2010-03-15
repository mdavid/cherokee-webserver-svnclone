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

def reorder (arg):
    print "reorder", CTK.post[arg]
    return {'ret': 'ok'}


class Render_Source():
    def __call__ (self):
        num = re.findall(r'^%s/([\d]+)$'%(URL_BASE), CTK.request.url)[0]

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h1>Source: %s</h1>'%(CTK.cfg.get_val('source!%s!nick'%(num))))

        render = cont.Render()
        return render.toStr()


class Render():
    def __call__ (self):
        # Placeholders
        box   = CTK.Box({'id': 'source_content'})
        panel = SelectionPanel.SelectionPanel (reorder, 'source_content')

        # Build the panel list
        for k in CTK.cfg.keys('source'):
            host = CTK.cfg.get_val('source!%s!host'%(k))
            nick = CTK.cfg.get_val('source!%s!nick'%(k))
            tipe = CTK.cfg.get_val('source!%s!type'%(k))

            panel.Add ('/source/%s'%(k), [CTK.RawHTML('<b>%s</b>'%(nick)), CTK.RawHTML(tipe)])

        page = Page.Base (_("Information Sources"), body_id='source', helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('Information Sources Settings')))
        page += panel
        page += box

        return page.Render()

CTK.publish ('^%s$'%(URL_BASE), Render)
CTK.publish ('^%s/[\d]+$'%(URL_BASE), Render_Source)
CTK.publish ('^%s$'%(URL_APPLY), apply, validation=VALIDATIONS, method="POST")
