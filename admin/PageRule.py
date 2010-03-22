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
import SelectionPanel
import validations

from Rule          import Rule
from CTK.Tab       import HEADER as Tab_HEADER
from CTK.Submitter import HEADER as Submit_HEADER
from CTK.TextField import HEADER as TextField_HEADER
from CTK.SortableList import HEADER as SortableList_HEADER

from util import *
from consts import *
from CTK.util import *
from CTK.consts import *
from configured import *

URL_BASE       = r'^/vserver/(\d+)/rule$'
URL_APPLY      = r'^/vserver/(\d+)/rule/apply$'
URL_PARTICULAR = r'^/vserver/(\d+)/rule/\d+$'

HELPS = []
VALIDATIONS = []

JS_ACTIVATE_LAST = """
$('.selection-panel:first').data('selectionpanel').select_last();
"""

def Commit():
    # Modifications
    return CTK.cfg_apply_post()


def reorder (arg):
    # Process new list
    order = CTK.post.pop(arg)

    tmp = order.split(',')
    vsrv = tmp[0].split('_')[1]

    tmp = [x.split('_')[0] for x in tmp]
    tmp.reverse()

    # Build and alternative tree
    num = 100
    for r in tmp:
        CTK.cfg.clone ('vserver!%s!rule!%s'%(vsrv, r), 'tmp!vserver!%s!rule!%d'%(vsrv, num))
        num += 100

    # Set the new list in place
    del (CTK.cfg['vserver!%s!rule'%(vsrv)])
    CTK.cfg.rename ('tmp!vserver!%s!rule'%(vsrv), 'vserver!%s!rule'%(vsrv))
    return {'ret': 'ok'}


class RenderBase():
    class PanelList (CTK.Container):
        def __init__ (self, refresh, right_box, vsrv_num):
            CTK.Container.__init__ (self)
            url_base = '/vserver/%s' %(vsrv_num)

            # Build the panel list
            panel = SelectionPanel.SelectionPanel (reorder, right_box.id, url_base, '')
            self += panel

            # Build the Rule list
            rules = CTK.cfg.keys('vserver!%s!rule'%(vsrv_num))
            rules.sort (lambda x,y: cmp(int(x), int(y)))
            rules.reverse()

            for r in rules:
                rule = Rule ('vserver!%s!rule!%s!match'%(vsrv_num, r))
                rule_name = rule.GetName()

                content = [CTK.RawHTML(rule_name)]

                # List entry
                row_id = '%s_%s' %(r, vsrv_num)
                panel.Add (row_id, '/vserver/%s/rule/content/%s'%(vsrv_num, r), content)


    class PanelButtons (CTK.Box):
        def __init__ (self):
            CTK.Box.__init__ (self, {'class': 'panel-buttons'})

    def __call__ (self):
        title = _('Behavior')
        vsrv_num = re.findall (URL_BASE, CTK.request.url)[0]

        # Content
        right = CTK.Box({'class': 'rules_content'})
        left  = CTK.Box({'class': 'panel'}, CTK.RawHTML('<h2>%s</h2>'%(title)))

        # Virtual Server List
        refresh = CTK.Refreshable ({'id': 'rules_panel'})
        refresh.register (lambda: self.PanelList(refresh, right, vsrv_num).Render())

        # Refresh on 'New' or 'Clone'
        buttons = self.PanelButtons()
        buttons.bind ('submit_success', refresh.JS_to_refresh (on_success=JS_ACTIVATE_LAST))

        left += refresh
        left += buttons

        # Refresh the list whenever the content change
        right.bind ('submit_success', refresh.JS_to_refresh());

        # Build the page
        headers = Tab_HEADER + Submit_HEADER + TextField_HEADER + SortableList_HEADER
        self.page = Page.Base(title, body_id='rules', helps=HELPS, headers=headers)
        self.page += left
        self.page += right


class Render (RenderBase):
    def __call__ (self):
        RenderBase.__call__(self)
        return self.page.Render()

class RenderParticular (RenderBase):
    def __call__ (self):
        RenderBase.__call__(self)
        self.page += CTK.RawHTML (js=JS_PARTICULAR %({'url_base': URL_BASE}))
        return self.page.Render()


CTK.publish (URL_BASE,       Render)
CTK.publish (URL_PARTICULAR, RenderParticular)
CTK.publish (URL_APPLY,      Commit, method="POST", validation=VALIDATIONS)
