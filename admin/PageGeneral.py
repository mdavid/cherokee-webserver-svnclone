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

URL_BASE  = '/general'
URL_APPLY = '/general/apply'

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


def apply():
    return {'ret': 'ok'}


class NetworkWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsTableAuto (URL_APPLY)
        table.Add (_('IPv6'),             CTK.CheckCfg('server!ipv6', True), _(NOTE_IPV6))
        table.Add (_('SSL/TLS back-end'), CTK.ComboCfg('server!tls', Cherokee.modules_available(CRYPTORS)), _(NOTE_TLS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Support')))
        self += table

        table = CTK.PropsTableAuto (URL_APPLY)
        table.Add (_('Timeout (<i>secs</i>)'), CTK.TextCfg('server!timeout'), _(NOTE_TIMEOUT))
        table.Add (_('Server Tokens'),         CTK.ComboCfg('server!server_tokens', PRODUCT_TOKENS), _(NOTE_TOKENS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Network behavior')))
        self += table

        table = CTK.PropsTableAuto (URL_APPLY)
        table.Add (_('Graphs Type'), CTK.ComboCfg('server!collector', Cherokee.modules_available(COLLECTORS)), _(NOTE_COLLECTORS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Information Collector')))
        self += table

        table = CTK.PropsTableAuto (URL_APPLY)
        table.Add (_('Upload Tracking'), CTK.ComboCfg('server!post_track', Cherokee.modules_available(POST_TRACKERS)), _(NOTE_POST_TRACKS))
        self += CTK.RawHTML ("<h2>%s</h2>" %(_('Upload Tracking')))
        self += table

class PortsWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)
        self += CTK.RawHTML ('Ports Content')

class PermsWidget (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsTableAuto (URL_APPLY)
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
        tabs.Add (_('Server Permissions'), PermsWidget())

        page = Page.Base (HELPS)
        page += CTK.RawHTML("<h1>%s</h1>" %(_('General Settings')))
        page += tabs

        return page.Render()

CTK.publish ('^%s'%(URL_BASE), Render)
CTK.publish ('^%s'%(URL_APPLY), apply, method="POST")
