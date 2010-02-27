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

import CTK
from Rule import RulePlugin

URL_APPLY = '/plugin/directory/apply'

NOTE_DIRECTORY = N_("Public Web Directory to which content the configuration will be applied.")

def apply():
    # New
    new_dir = CTK.post.pop('tmp!directory')
    if new_dir:
        vsrv_num = re.findall (r'^%s/([\d]+)'%(URL_APPLY), CTK.request.url)[0]
        print "NEW DIR", new_dir, "vsrv", vsrv_num
        return {'ret': 'ok'}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


class Plugin_directory (RulePlugin):
    def __init__ (self, key, **kwargs):
        RulePlugin.__init__ (self, key, **kwargs)

        table = CTK.PropsTable()
        table.Add (_('Web Directory'), CTK.TextCfg('%s!directory'%(key)), _(NOTE_DIRECTORY))
        self += table

    def GetName (self):
        directory = CTK.cfg.get_val ('%s!directory' %(self.key), '')
        return "Directory %s" %(directory)
