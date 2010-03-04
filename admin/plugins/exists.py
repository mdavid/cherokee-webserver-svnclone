# Cheroke Admin: File Exists rule plug-in
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
from util import *
from copy import copy

URL_APPLY = '/plugin/exists/apply'

NOTE_EXISTS      = N_("Comma separated list of files. Rule applies if one exists.")
NOTE_IOCACHE     = N_("Uses cache during file detection. Disable if directory contents change frequently. Enable otherwise.")
NOTE_ANY         = N_("Match the request if any file exists.")
NOTE_ONLY_FILES  = N_("Only match regular files. If unset directories will be matched as well.")
NOTE_INDEX_FILES = N_("If a directory is request, check the index files inside it.")

OPTIONS = [('0', _('Match a specific list of files')),
           ('1', _('Match any file'))]

def apply():
    # POST info
    key         = CTK.post.pop ('key', None)
    vsrv_num    = CTK.post.pop ('vsrv_num', None)
    new_any     = CTK.post.pop ('tmp!match_any', '0')
    new_exists  = CTK.post.pop ('tmp!exists', '')

    # New
    if int(new_any) or new_exists:
        next_rule, next_pre = cfg_vsrv_rule_get_next ('vserver!%s'%(vsrv_num))

        CTK.cfg['%s!match'%(next_pre)]                   = 'exists'
        CTK.cfg['%s!match!match_any'%(next_pre)]         = new_any
        CTK.cfg['%s!match!iocache'%(next_pre)]           = CTK.post.pop ('tmp!iocache')
        CTK.cfg['%s!match!match_only_file'%(next_pre)]   = CTK.post.pop ('tmp!match_only_files')
        CTK.cfg['%s!match!match_index_files'%(next_pre)] = CTK.post.pop ('tmp!match_index_files')
        if new_exists:
            CTK.cfg['%s!match!exists'%(next_pre)]        = new_exists

        return {'ret': 'ok', 'redirect': '/vserver/%s/rule/%s' %(vsrv_num, next_rule)}

    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret': 'ok'}


class Modify (CTK.Submitter):
    def __init__ (self, key, vsrv_num, refresh):
        CTK.Submitter.__init__ (self, URL_APPLY)

        # Main table
        table = CTK.PropsTable()
        table.Add (_('Match any file'), CTK.CheckCfg('%s!match_any'%(key), False), _(NOTE_ANY))

        if not int (CTK.cfg.get_val('%s!match_any'%(key), '0')):
            table.Add (_('Files'),                 CTK.TextCfg ('%s!exists'%(key),           False), _(NOTE_EXISTS))

        table.Add (_('Use I/O cache'),             CTK.CheckCfg('%s!iocache'%(key),           True), _(NOTE_IOCACHE))
        table.Add (_('Only match files'),          CTK.CheckCfg('%s!match_only_files'%(key),  True), _(NOTE_ONLY_FILES))
        table.Add (_('If dir, check index files'), CTK.CheckCfg('%s!match_index_files'%(key), True), _(NOTE_INDEX_FILES))

        # Submit
        self += table
        self += CTK.Hidden ('key', key)
        self += CTK.Hidden ('vsrv_num', vsrv_num)
        self.bind ('submit_success', refresh.JS_to_refresh())


class Plugin_exists (RulePlugin):
    def __init__ (self, key, **kwargs):
        RulePlugin.__init__ (self, key)
        vsrv_num  = kwargs.pop('vsrv_num', '')
        is_new    = key.startswith('tmp')

        # Build the GUI
        if is_new:
            self._GUI_new (key, vsrv_num)
        else:
            refresh = CTK.Refreshable()
            refresh.register (lambda: Modify(key, vsrv_num, refresh).Render())
            self += refresh

        # Validation, and Public URLs
        CTK.publish (URL_APPLY, apply, method="POST")

    def _GUI_new (self, key, vsrv_num):
        any = CTK.CheckCfg('%s!match_any'%(key), False, {'class': 'noauto'})

        # Main table
        table = CTK.PropsTable()
        table.Add (_('Match any file'), any, _(NOTE_ANY))
        table.Add (_('Files'),                     CTK.TextCfg ('%s!exists'%(key),           False, {'class': 'noauto'}), _(NOTE_EXISTS))
        table.Add (_('Use I/O cache'),             CTK.CheckCfg('%s!iocache'%(key),           True, {'class': 'noauto'}), _(NOTE_IOCACHE))
        table.Add (_('Only match files'),          CTK.CheckCfg('%s!match_only_files'%(key),  True, {'class': 'noauto'}), _(NOTE_ONLY_FILES))
        table.Add (_('If dir, check index files'), CTK.CheckCfg('%s!match_index_files'%(key), True, {'class': 'noauto'}), _(NOTE_INDEX_FILES))

        # Special events
        any.bind ('change',
                  """if (! $('#%(any)s')[0].checked) {
                           $('#%(row)s').show();
                     } else {
                           $('#%(row)s').hide();
                  }""" %({'any': any.id, 'row': table[1].id}))

        # Submit
        submit = CTK.Submitter (URL_APPLY)
        submit += table
        submit += CTK.Hidden ('key', key)
        submit += CTK.Hidden ('vsrv_num', vsrv_num)
        submit += CTK.SubmitterButton(_("Add"))
        self += submit

    def GetName (self):
        if int (CTK.cfg.get_val ('%s!match_any' %(self.key), '0')):
            return _("File exists")

        files = CTK.cfg.get_val ('%s!exists' %(self.key), '').split(',')
        if len(files) == 1:
            return _("File %s exists") %(files[0])
        elif len(files) == 0:
            return _("File exists (never matched)")

        return _("File %s or %s exists") %(', '.join (files[:-1]), files[-1])
