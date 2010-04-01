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
from consts import *

URL_APPLY = '/plugin/error_redir/apply'

REDIRECTION_TYPE = [
    ('0', _('Internal')),
    ('1', _('External'))
]

def apply():
    # New entry
    key       = CTK.post.pop('key')
    new_error = CTK.post.pop('new_error')
    new_redir = CTK.post.pop('new_redir')
    new_type  = CTK.post.pop('new_type')

    if key and new_error and new_redir and new_type:
        CTK.cfg['%s!%s!url' %(key, new_error)] = new_redir
        CTK.cfg['%s!%s!show'%(key, new_error)] = new_type
        return {'ret':'ok'}

    # Apply
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]
    return {'ret':'ok'}


class Content (CTK.Container):
    def __init__ (self, refreshable, key, url_apply, **kwargs):
        CTK.Container.__init__ (self, **kwargs)

        # List
        entries = CTK.cfg.keys(key)
        entries.sort (lambda x,y: cmp(int(x),int(y)))

        if entries:
            table = CTK.Table()
            table.set_header(1)
            table += [CTK.RawHTML(x) for x in ('Error', 'Redirection', 'Type')]

            for i in entries:
                show  = CTK.ComboCfg ('%s!%s!show'%(key,i), REDIRECTION_TYPE)
                redir = CTK.RawHTML (CTK.cfg.get_val('%s!%s!url'%(key,i)))
                rm    = CTK.ImageStock('del')
                table += [CTK.RawHTML(i), redir, show, rm]

                rm.bind ('click', CTK.JS.Ajax (url_apply,
                                               data = {"%s!%s"%(key,i): ''},
                                               complete = refreshable.JS_to_refresh()))
            submit = CTK.Submitter (url_apply)
            submit += table
            self += submit

        # Add new
        redir_codes = filter (lambda x: not x[0] in entries, ERROR_CODES)

        table = CTK.PropsTable()
        table.Add (_('Error'),       CTK.ComboCfg('new_error', redir_codes, {'class':'noauto'}), _('TODO'))
        table.Add (_('Redirection'), CTK.TextCfg ('new_redir', False, {'class':'noauto'}), _('TODO'))
        table.Add (_('Type'),        CTK.ComboCfg('new_type', REDIRECTION_TYPE, {'class':'noauto'}), _('TODO'))

        dialog = CTK.Dialog({'title': _('Add New Custom Error'), 'width': 540})

        submit = CTK.Submitter(url_apply)
        submit += table
        submit += CTK.HiddenField ({'name': 'key', 'value': key})
        submit += CTK.SubmitterButton(_('Add'))
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit.bind ('submit_success', dialog.JS_to_close())

        dialog += submit
        self += dialog

        add_new = CTK.Button(_('Add New…'))
        add_new.bind ('click', dialog.JS_to_show())
        self += add_new


class Plugin_error_redir (CTK.Plugin):
    def __init__ (self, key, vsrv_num):
        CTK.Plugin.__init__ (self, key)

        url_apply = '%s/%s' %(URL_APPLY, vsrv_num)

        # Content
        refresh = CTK.Refreshable ({'id': 'plugin_error'})
        refresh.register (lambda: Content(refresh, key, url_apply).Render())
        self += CTK.Indenter (refresh)

        # Validation, and Public URLs
        CTK.publish ('^%s/[\d]+$'%(URL_APPLY), apply, method="POST")
