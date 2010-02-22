# Cheroke Admin: RRD plug-in
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

import re
import CTK
import validations

URL_APPLY = '/plugin/rrd/apply'

NOTE_ADDRESS  = N_("IP or Subnet of the NIC that accepted the request. Example: ::1, or 10.0.0.0/8")
WARNING_EMPTY = N_("At least one IP or Subnet entry must be defined.")


class AddressesTable (CTK.Container):
    def __init__ (self, refreshable, key, url_apply, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        pre     = "%s!to"%(key)
        entries = CTK.cfg.keys(pre)

        # List
        if entries:
            table = CTK.Table()
            self += CTK.Indenter(table)

            table.set_header(1)
            table += [CTK.RawHTML(_('IP or Subnet'))]

            for i in entries:
                e1 = CTK.TextCfg ("%s!%s"%(pre,i))
                rm = None
                if len(entries) >= 2:
                    rm = CTK.Image ({'src':'/CTK/images/del.png', 'alt':'Del'})
                    rm.bind('click', CTK.JS.Ajax (url_apply, data = {"%s!%s"%(pre,i): ''},
                                                  complete = refreshable.JS_to_refresh()))
                table += [e1, rm]

        # Add new
        table = CTK.PropsTable()
        next  = CTK.cfg.get_next_entry_prefix(pre)
        table.Add (_('New IP/Subnet'), CTK.TextCfg(next, False, {'class':'noauto'}), _(NOTE_ADDRESS))

        submit = CTK.Submitter(url_apply)
        submit += table
        submit += CTK.SubmitterButton(_('Add'))
        submit.bind ('submit_success', refreshable.JS_to_refresh())

        self += CTK.RawHTML("<h3>%s</h3>" % (_('Add new')))
        self += CTK.Indenter(submit)


class Plugin_target_ip (CTK.Plugin):
    def __init__ (self, key, vsrv_num):
        CTK.Plugin.__init__ (self, key)

        pre       = '%s!to' %(key)
        url_apply = '%s/%s' %(URL_APPLY, vsrv_num)

        self += CTK.RawHTML ("<h2>%s</h2>" % (_('Accepted Server IP addresses and subnets')))

        # Content
        entries = CTK.cfg.keys(pre)
        if not entries:
            notice = CTK.Notice('warning')
            notice += CTK.RawHTML (WARNING_EMPTY)
            self += notice

        refresh = CTK.Refreshable()
        refresh.register (lambda: AddressesTable(refresh, key, url_apply).Render())
        self += refresh

        # Validation, and Public URLs
        VALS = [('%s!.+'%(pre), validations.is_ip_or_netmask)]
        CTK.publish ('^%s/[\d]+$'%(URL_APPLY), self.apply, validation=VALS, method="POST")
