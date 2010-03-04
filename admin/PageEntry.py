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

import re

from Rule import Rule
from consts import *
from configured import *

URL_BASE  = r'^/vserver/([\d]+)/rule/([\d]+)/?$'
URL_APPLY = r'^/vserver/([\d]+)/rule/([\d]+)/apply$'

NOTE_DOCUMENT_ROOT   = N_('Allows to specify an alternative document root path.')
NOTE_TIMEOUT         = N_('Apply a custom timeout to the connections matching this rule.')
NOTE_HANDLER         = N_('How the connection will be handled.')
NOTE_HTTPS_ONLY      = N_('Enable to allow access to the resource only by https.')
NOTE_ALLOW_FROM      = N_('List of IPs and subnets allowed to access the resource.')
NOTE_VALIDATOR       = N_('Which, if any, will be the authentication method.')
NOTE_EXPIRATION      = N_('Points how long the files should be cached')
NOTE_RATE            = N_("Set an outbound traffic limit. It must be specified in Bytes per second.")
NOTE_NO_LOG          = N_("Do not log requests matching this rule.")
NOTE_EXPIRATION_TIME = N_("""How long from the object can be cached.<br />
The <b>m</b>, <b>h</b>, <b>d</b> and <b>w</b> suffixes are allowed for minutes, hours, days, and weeks. Eg: 2d.
""")

VALIDATIONS = [
    ("vserver![\d]+!rule![\d]+!document_root",   validations.is_dev_null_or_local_dir_exists),
    ("vserver![\d]+!rule![\d]+!allow_from",      validations.is_ip_or_netmask_list),
    ("vserver![\d]+!rule![\d]+!rate",            validations.is_number_gt_0),
    ("vserver![\d]+!rule![\d]+!timeout",         validations.is_number_gt_0),
    ("vserver![\d]+!rule![\d]+!expiration!time", validations.is_time)
]

HELPS = [
    ('config_virtual_servers_rule', N_("Behavior rules")),
    ('modules_encoders_gzip',       N_("GZip encoder")),
    ('modules_encoders_deflate',    N_("Deflate encoder"))
]

ENCODE_OPTIONS = [
    ('',       N_('Leave unset')),
    ('allow',  N_('Allow')),
    ('forbid', N_('Forbid'))
]



def apply():
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


class EncodingWidget (CTK.Container):
    def __init__ (self, vsrv, rule, apply):
        CTK.Container.__init__ (self)
        pre = 'vserver!%s!rule!%s!encoder' %(vsrv, rule)
        encoders = Cherokee.support.filter_available (ENCODERS)

        table = CTK.PropsTable()
        for e,e_name in encoders:
            note  = _("Use the %s encoder whenever the client requests it.") %(_(e_name))
            table.Add (_("%s Support") %(_(e_name)), CTK.ComboCfg('%s!%s'%(pre,e), ENCODE_OPTIONS), note)

        submit = CTK.Submitter (apply)
        submit += table

        self += CTK.RawHTML ("<h2>%s</h2>" % (_('Information Encoders')))
        self += CTK.Indenter (submit)


class RuleWidget (CTK.Container):
    def __init__ (self, vsrv, rule, apply, refresh):
        CTK.Container.__init__ (self)
        pre = 'vserver!%s!rule!%s!match' %(vsrv, rule)

        rule = Rule (pre)
        rule.bind ('submit_success', refresh.JS_to_refresh())

        self += CTK.RawHTML ("<h2>%s</h2>" % (_('Matching Rule')))
        self += rule


class Header (CTK.Container):
    def __init__ (self, refreshable, vsrv_num, rule_num, vsrv_nam):
        CTK.Container.__init__(self)

        rule = Rule ('vserver!%s!rule!%s!match' %(vsrv_num, rule_num))
        rule_nam = rule.GetName()

        self += CTK.RawHTML ('<h1><a href="/vserver">%s</a>: <a href="/vserver/%s">%s</a>: %s</h1>' %(_('Virtual Server'), vsrv_num, vsrv_nam, rule_nam))


class Render():
    def __call__ (self):
        # Parse request
        vsrv_num, rule_num = re.findall (URL_BASE, CTK.request.url)[0]
        vsrv_nam = CTK.cfg.get_val ("vserver!%s!nick" %(vsrv_num), _("Unknown"))

        url_apply = '/vserver/%s/rule/%s/apply' %(vsrv_num, rule_num)

        # Ensure the rule exists
        if not CTK.cfg.keys('vserver!%s!rule!%s'%(vsrv_num, rule_num)):
            return CTK.HTTP_Redir ('/vserver/%s' %(vsrv_num))

        # Header refresh
        refresh = CTK.Refreshable()
        refresh.register (lambda: Header(refresh, vsrv_num, rule_num, vsrv_nam).Render())

        # Tabs
        tabs = CTK.Tab()
        tabs.Add (_('Rule'),     RuleWidget (vsrv_num, rule_num, url_apply, refresh))
        tabs.Add (_('Encoding'), EncodingWidget (vsrv_num, rule_num, url_apply))

        # Page
        page = Page.Base ('%s: %s: %s' %(_('Virtual Server'), vsrv_nam, rule_num), helps=HELPS)
        page += refresh
        page += tabs

        return page.Render()


CTK.publish (URL_BASE, Render)
CTK.publish (URL_APPLY, apply, validation=VALIDATIONS, method="POST")
