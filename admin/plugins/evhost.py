# -*- coding: utf-8 -*-
#
# Cheroke-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2009-2010 Alvaro Lopez Ortega
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

URL_APPLY = '/plugin/evhost/apply'

NOTE_CHECK_DROOT = N_("Check the dynamically generated Document Root, and use the general Document Root if it doesn't exist.")
NOTE_REHOST      = N_("The Document Root directory will be built dynamically. The following variables are accepted:<br/>") +\
                   "${domain}, ${tld}, ${domain_no_tld}, ${subdomain1}, ${subdomain2}."

HELPS = [('config_virtual_servers_evhost', _("Advanced Virtual Hosting"))]

class Plugin_evhost (CTK.Plugin):
    def __init__ (self, key):
        CTK.Plugin.__init__ (self, key)

        # GUI
        table = CTK.PropsAuto (URL_APPLY)
        table.Add (_("Dynamic Document Root"), CTK.TextCfg('%s!tpl_document_root'%(key)), _(NOTE_REHOST))
        table.Add (_("Check Document Root"),   CTK.CheckCfg('%s!check_document_root'%(key), True), _(NOTE_CHECK_DROOT))
        self += table

        # URL mapping
        CTK.publish ('^%s'%(URL_APPLY), CTK.cfg_apply_post, method="POST")
