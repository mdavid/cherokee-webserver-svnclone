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

from util import *

URL_BASE  = r'/vserver'
URL_APPLY = r'/vserver/apply'

HELPS = [('config_virtual_servers', N_("Virtual Servers"))]

def apply():
    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]

    return {'ret': 'ok'}


class VServerListWidget (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'vserver-list'})

        # Sanity check
        if not CTK.cfg.keys('vserver'):
            CTK.cfg['vserver!1!nick']           = 'default'
            CTK.cfg['vserver!1!document_root']  = '/tmp'
            CTK.cfg['vserver!1!rule!1!match']   = 'default'
            CTK.cfg['vserver!1!rule!1!handler'] = 'common'

        # Build the Virtual Server list
        vservers = CTK.cfg.keys('vserver')
        vservers.sort (lambda x,y: cmp(int(x), int(y)))

        submit = CTK.Submitter (URL_APPLY)
        self += submit

        for k in vservers:
            nick    = CTK.cfg.get_val('vserver!%s!nick'%(k), _('Unknown'))
            droot   = CTK.cfg.get_val('vserver!%s!document_root'%(k), '')
            logging = bool (CTK.cfg.get_val('vserver!%s!logger'%(k)))


            vsrv_box = CTK.Box ({'id': 'vserver-list-entry'})
            vsrv_box += CTK.Box ({'class': 'name'},     CTK.Link ('/vserver/%s'%(k), CTK.RawHTML(nick)))
            vsrv_box += CTK.Box ({'class': 'disabled'}, CTK.iPhoneCfg ('vserver!%s!disabled'%(k), False))
            vsrv_box += CTK.Box ({'class': 'droot'},    CTK.RawHTML ('<b>Document Root</b>: %s'%(droot)))
            vsrv_box += CTK.Box ({'class': 'logging'},  CTK.RawHTML ('<b>Access Logging</b>: %s'%(bool_to_active (logging))))

            submit += vsrv_box


class Render():
    def __call__ (self):
        title = _('Virtual Servers')

        page = Page.Base(title, body_id='vservers', helps=HELPS)
        page += CTK.RawHTML ("<h1>%s</h1>" % (title))
        page += VServerListWidget()
        return page.Render()


CTK.publish (URL_BASE, Render)
CTK.publish (URL_APPLY, apply, method="POST")
