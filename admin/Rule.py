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

import re
import CTK

DEFAULT_RULE_WARNING = N_('The default match ought not to be changed.')


class RulePlugin (CTK.Plugin):
    def __init__ (self, key):
        CTK.Plugin.__init__ (self, key)

    def GetName (self):
        raise NotImplementedError


class Rule (CTK.Box):
    def __init__ (self, key):
        CTK.Box.__init__ (self)
        self.key = key

    def GetName (self):
        # Default Rule
        value = CTK.cfg.get_val (self.key)
        if value == 'default':
            return _('Default')

        # Logic ops
        elif value == "and":
            rule1 = Rule ('%s!left' %(self.key))
            rule2 = Rule ('%s!right'%(self.key))
            return '(%s AND %s)' %(rule1.GetName(), rule2.GetName())

        elif value == "or":
            rule1 = Rule ('%s!left' %(self.key))
            rule2 = Rule ('%s!right'%(self.key))
            return '(%s OR %s)' %(rule1.GetName(), rule2.GetName())

        elif value == "not":
            rule = Rule ('%s!right'%(self.key))
            return '(NOT %s)' %(rule.GetName())

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
            self += Rule ('%s!left' %(self.key))
            self += Rule ('%s!right'%(self.key))
            return CTK.Box.Render (self)

        elif value == "or":
            self.props['class'] = 'rule-or'
            self += Rule ('%s!left' %(self.key))
            self += Rule ('%s!right'%(self.key))
            return CTK.Box.Render (self)

        elif value == "not":
            self.props['class'] = 'rule-not'
            self += Rule ('%s!right'%(self.key))
            return CTK.Box.Render (self)

        # Regular rules
        plugin = CTK.instance_plugin (value, self.key)
        self += plugin

        # Render
        return CTK.Box.Render (self)
