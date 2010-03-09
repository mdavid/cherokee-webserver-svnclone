# Cheroke Admin
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

import CTK

from Rule import RulePlugin
from Flags import ComboFlags
from util import *

URL_APPLY = '/plugin/geoip/apply'

ISO3166_URL      = "http://www.iso.org/iso/country_codes/iso_3166_code_lists/english_country_names_and_code_elements.htm"
NOTE_NEW_COUNTRY = N_("Add the initial country. It's possible to add more later on.")
NOTE_ADD_COUNTRY = N_("Pick an additional country to add to the country list.")
NOTE_COUNTRIES   = N_("""List of countries from the client IPs. It must use the
<a target=\"_blank\" href=\"%s\">ISO 3166</a> country notation.""") % (ISO3166_URL)

def apply():
    # POST info
    key         = CTK.post.pop ('key', None)
    vsrv_num    = CTK.post.pop ('vsrv_num', None)
    new_country = CTK.post.pop('tmp!country')
    add_country = CTK.post.pop('tmp!add_country')

    if new_country:
        next_rule, next_pre = cfg_vsrv_rule_get_next ('vserver!%s'%(vsrv_num))
        CTK.cfg['%s!match'%(next_pre)]           = 'geoip'
        CTK.cfg['%s!match!countries'%(next_pre)] = new_country
        return {'ret': 'ok', 'redirect': '/vserver/%s/rule/%s' %(vsrv_num, next_rule)}

    # Add an additional country code
    if add_country:
        countries = [x.strip() for x in CTK.cfg.get_val('%s!countries'%(key),'').split(',')]
        if add_country not in countries:
            country_list = ','.join (filter (lambda x: x, countries + [add_country]))
            CTK.cfg['%s!countries'%(key)] = country_list
            return {'ret': 'ok', 'updates': {'%s!countries'%(key): country_list}}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


class Plugin_geoip (RulePlugin):
    def __init__ (self, key, **kwargs):
        RulePlugin.__init__ (self, key)
        self.vsrv_num = kwargs.pop('vsrv_num', '')

        if key.startswith('tmp'):
            return self.GUI_new()
        return self.GUI_mod()

    def GUI_new (self):
        table = CTK.PropsTable()
        table.Add (_('Country'), ComboFlags ('%s!country'%(self.key), {'class': 'noauto'}), _(NOTE_NEW_COUNTRY))

        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden ('key', self.key)
        submit += CTK.Hidden ('vsrv_num', self.vsrv_num)
        submit += table
        self += submit

    def GUI_mod (self):
        table = CTK.PropsTable()
        table.Add (_('Countries'),   CTK.TextCfg('%s!countries'%(self.key)), _(NOTE_COUNTRIES))
        table.Add (_('Add Country'), ComboFlags ('tmp!add_country'), _(NOTE_ADD_COUNTRY))

        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden ('key', self.key)
        submit += CTK.Hidden ('vsrv_num', self.vsrv_num)
        submit += table
        self += submit

    def GetName (self):
        match = CTK.cfg.get_val ('%s!match'%(self.key), '')

        if int(CTK.cfg.get_val ('%s!match_any'%(self.key), "0")):
            return "Any arg. matches ~ %s" %(match)

        arg = CTK.cfg.get_val ('%s!arg'%(self.key), '')
        return "Arg %s matches %s" %(arg, match)

# Validation, and Public URLs
CTK.publish (URL_APPLY, apply, method="POST")
