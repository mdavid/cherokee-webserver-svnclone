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
import validations

from consts import *
from configured import *

URL_BASE  = '/general'
URL_APPLY = '/general/apply'

NOTE_ADD_PORT    = N_('Defines a port that the server will listen to')
NOTE_IPV6        = N_('Set to enable the IPv6 support. The OS must support IPv6 for this to work.')
NOTE_TOKENS      = N_('This option allows to choose how the server identifies itself.')
NOTE_TIMEOUT     = N_('Time interval until the server closes inactive connections.')
NOTE_TLS         = N_('Which, if any, should be the TLS/SSL backend.')
NOTE_COLLECTORS  = N_('How the usage graphics should be generated.')
NOTE_POST_TRACKS = N_('How to track uploads/posts so its progress can be reported to the user.')
NOTE_USER        = N_('Changes the effective user. User names and IDs are accepted.')
NOTE_GROUP       = N_('Changes the effective group. Group names and IDs are accepted.')
NOTE_CHROOT      = N_('Jail the server inside the directory. Don\'t use it as the only security measure.')

HELPS = [('config_general',    N_("General Configuration")),
         ('config_quickstart', N_("Configuration Quickstart"))]

VALIDATIONS = [
    ("server!ipv6",              validations.is_boolean),
    ("server!timeout",           validations.is_number),
    ("server!bind!.*!port",      validations.is_tcp_port),
    ("server!bind!.*!interface", validations.is_ip),
    ("server!bind!.*!tls",       validations.is_boolean),
    ("server!chroot",            validations.is_local_dir_exists),
    ("new_port",                 validations.is_tcp_port)
]


def apply():
    # Add a new port
    port = CTK.post.pop('new_port')
    if port:
        # Look for the next entry
        pre = CTK.cfg.get_next_entry_prefix ('server!bind')
        CTK.cfg['%s!port'%(pre)] = port

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]

    return {'ret': 'ok'}


class NetworkWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsAuto (URL_APPLY)
        table.Add (_('IPv6'),             CTK.CheckCfg('server!ipv6', True), _(NOTE_IPV6))
        table.Add (_('SSL/TLS back-end'), CTK.ComboCfg('server!tls', Cherokee.modules_available(CRYPTORS)), _(NOTE_TLS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Support')))
        self += table

        table = CTK.PropsAuto (URL_APPLY)
        table.Add (_('Timeout (<i>secs</i>)'), CTK.TextCfg('server!timeout'), _(NOTE_TIMEOUT))
        table.Add (_('Server Tokens'),         CTK.ComboCfg('server!server_tokens', PRODUCT_TOKENS), _(NOTE_TOKENS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Network behavior')))
        self += table

        table = CTK.PropsAuto (URL_APPLY)
        modul = CTK.PluginSelector('server!collector', Cherokee.modules_available(COLLECTORS))
        table.Add (_('Graphs Type'), modul.selector_widget, _(NOTE_COLLECTORS), False)
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Information Collector')))
        self += table
        self += modul

        table = CTK.PropsAuto (URL_APPLY)
        modul = CTK.PluginSelector('server!post_track', Cherokee.modules_available(POST_TRACKERS))
        table.Add (_('Upload Tracking'), modul.selector_widget, _(NOTE_POST_TRACKS), False)
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Upload Tracking')))
        self += table
        self += modul


class PortsTable (CTK.Table):
    def __init__ (self, refreshable, **kwargs):
        CTK.Table.__init__(self)

        has_tls = CTK.cfg.get_val('server!tls')

        # Header
        self[(1,1)] = [CTK.RawHTML(x) for x in (_('Port'), _('Bind to'), _('TLS'), '')]
        self.set_header (row=True, num=1)

        # Entries
        n = 2
        for k in CTK.cfg.keys('server!bind'):
            pre = 'server!bind!%s'%(k)

            port   = CTK.TextCfg ('%s!port'%(pre),      False, {'size': 8})
            listen = CTK.TextCfg ('%s!interface'%(pre), True,  {'size': 45})
            tls    = CTK.CheckCfg('%s!tls'%(pre),       False, {'disabled': not has_tls})
            delete = CTK.Image ({'src': '/CTK/images/del.png', 'alt': 'Del'})

            from CTK.Refreshable import REFRESHABLE_UPDATE_JS as update_js

            delete.bind('click', CTK.JS.Ajax (URL_APPLY,
                                              data     = {pre: ''},
                                              complete = update_js %({'id': refreshable.id,
                                                                      'url': refreshable.url})))
            self[(n,1)] = [port, listen, tls, delete]
            n += 1

class PortsWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # List ports
        refresh = CTK.Refreshable()
        refresh.register (lambda: PortsTable(refresh).Render())

        self += CTK.RawHTML ("<h2>%s</h2>" % (_('Listening to ports')))
        self += refresh

        # Add new entry
        new_field  = CTK.TextCfg('new_port')
        new_submit = CTK.Submitter (URL_APPLY)
        new_submit += new_field

        new_submit.bind ('submit_success', refresh.JS_to_refresh())
        new_submit.bind ('submit_success', new_field.JS_to_clean())

        table = CTK.PropsAuto (URL_APPLY)
        table.Add (_('Add new port'), new_submit, _(NOTE_ADD_PORT), use_submitter=False)
        self += table

class PermsWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsAuto (URL_APPLY)
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Execution Permissions')))
        table.Add (_('User'),   CTK.TextCfg('server!user',   True),  _(NOTE_USER))
        table.Add (_('Group'),  CTK.TextCfg('server!group',  True), _(NOTE_GROUP))
        table.Add (_('Chroot'), CTK.TextCfg('server!chroot', True), _(NOTE_CHROOT))
        self += table

class Render():
    def __call__ (self):
        tabs = CTK.Tab()
        tabs.Add (_('Network'),            NetworkWidget())
        tabs.Add (_('Ports to listen'),    PortsWidget())
        tabs.Add (_('Permissions'), PermsWidget())

        page = Page.Base (_("General"), helps=HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('General Settings')))
        page += tabs

        return page.Render()

CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, validation=VALIDATIONS, method="POST")
