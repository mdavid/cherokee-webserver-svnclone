# -*- coding: utf-8 -*-
#
# Cherokee-admin
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

import re
import copy
import CTK
from consts import *

DEFAULT_RULE_WARNING = N_('The default match ought not to be changed.')

URL_APPLY_LOGIC = "/rule/logic/apply"


class RulePlugin (CTK.Plugin):
    def __init__ (self, key):
        CTK.Plugin.__init__ (self, key)

    def GetName (self):
        raise NotImplementedError

def RuleButtons_apply():
    def move (old, new):
        val = CTK.cfg.get_val(old)
        tmp = copy.deepcopy (CTK.cfg[old])
        del (CTK.cfg[old])

        CTK.cfg[new] = val
        CTK.cfg.set_sub_node (new, tmp)

    # Data collection
    key = CTK.post['key']
    op  = CTK.post['op']

    # Apply
    if op == 'NOT':
        if CTK.cfg.get_val(key) == 'not':
            move ("%s!right"%(key), key)
        else:
            move (key, "%s!right"%(key))
            CTK.cfg[key] = 'not'

    elif op == 'AND':
        move (key, "%s!left"%(key))
        CTK.cfg[key] = 'and'

    elif op == 'OR':
        move (key, "%s!left"%(key))
        CTK.cfg[key] = 'or'

    elif op == 'DEL':
        rule = CTK.cfg[key]
        if rule == 'not':
            move ("%s!right"%(key), key)
        elif rule in ('and', 'or'):
            move ("%s!left"%(key), key)
        else:
            del(CTK.cfg[key])

    return {'ret': 'ok'}


class RuleButtons (CTK.Box):
    def __init__ (self, key, depth):
        CTK.Box.__init__ (self, {'class': 'rulebuttons'})

        if depth == 0:
            opts = ['OR', 'AND', 'NOT']
        else:
            opts = ['OR', 'AND', 'NOT', 'DEL']

        for op in opts:
            submit = CTK.Submitter (URL_APPLY_LOGIC)
            submit += CTK.Hidden ('key', key)
            submit += CTK.Hidden ('op', op)
            submit += CTK.SubmitterButton (op)

            self += submit


class Rule (CTK.Box):
    def __init__ (self, key, refresh=None, depth=0):
        assert type(key) == str
        assert not refresh or isinstance(refresh, CTK.Refreshable)
        assert type(depth) == int

        CTK.Box.__init__ (self)
        self.key     = key
        self.depth   = depth
        self.refresh = refresh

    def GetName (self):
        # Default Rule
        value = CTK.cfg.get_val (self.key)
        if value == 'default':
            return _('Default')

        # Logic ops
        elif value == "and":
            rule1 = Rule ('%s!left' %(self.key), self.refresh, self.depth+1)
            rule2 = Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            return '(%s AND %s)' %(rule1.GetName(), rule2.GetName())

        elif value == "or":
            rule1 = Rule ('%s!left' %(self.key), self.refresh, self.depth+1)
            rule2 = Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            return '(%s OR %s)' %(rule1.GetName(), rule2.GetName())

        elif value == "not":
            rule = Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            return '(NOT %s)' %(rule.GetName())

        # No rule (yet)
        if not value:
            return ''

        # Regular rules
        plugin = CTK.instance_plugin (value, self.key)
        return plugin.GetName()

    def Render (self):
        # Default Rule
        value = CTK.cfg.get_val (self.key)
        if value == 'default':
            self += CTK.Notice ('important-information', CTK.RawHTML (DEFAULT_RULE_WARNING))
            return CTK.Box.Render (self)

        # Special rule types
        elif value == "and":
            self.props['class'] = 'rule-and'
            self += Rule ('%s!left' %(self.key), self.refresh, self.depth+1)
            self += Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            self += RuleButtons (self.key, self.depth)
            return CTK.Box.Render (self)

        elif value == "or":
            self.props['class'] = 'rule-or'
            self += Rule ('%s!left' %(self.key), self.refresh, self.depth+1)
            self += Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            self += RuleButtons (self.key, self.depth)
            return CTK.Box.Render (self)

        elif value == "not":
            self.props['class'] = 'rule-not'
            self += Rule ('%s!right'%(self.key), self.refresh, self.depth+1)
            self += RuleButtons (self.key, self.depth)
            return CTK.Box.Render (self)

        # Regular rules
        vsrv_num = self.key.split('!')[1]

        table = CTK.PropsTable()
        modul = CTK.PluginSelector (self.key, RULES, vsrv_num=vsrv_num)
        table.Add (_('Rule Type'), modul.selector_widget, '')

        if self.refresh:
            modul.selector_widget.bind ('changed', self.refresh.JS_to_refresh());

        self += table
        self += modul
        self += RuleButtons (self.key, self.depth)

        # Render
        return CTK.Box.Render (self)


CTK.publish (r"^%s$"%(URL_APPLY_LOGIC), RuleButtons_apply, method="POST")
