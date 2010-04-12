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
import CTK

URL_CAT_LIST   = '/wizard/category'
URL_CAT_LIST_R = r'^/wizard/category/(.*)$'
URL_CAT_APPLY  = '/wizard/new/apply'

WIZARDS_CMS = [
    {'wizard': 'drupal',    'title': 'Drupal',    'descr': 'Drupal is bla, bla, bla, bla..'},
    {'wizard': 'wordpress', 'title': 'WordPress', 'descr': 'WordPress is bla, bla, bla, bla..'},
    {'wizard': 'trac',      'title': 'Trac',      'descr': 'Track is also bla, bla, bla, bla..'},
]

USUAL_STATIC_FILES = ['/favicon.ico', '/robots.txt', '/crossdomain.xml']

class Wizard (CTK.Box):
    def __init__ (self, _props={}):
        props = _props.copy()

        if 'class' in self.props:
            props['class'] += ' wizard'
        else:
            props['class'] = 'wizard'

        CTK.Box.__init__ (self, props)

    def show (self):
        raise NotImplementedError, 'Wizard.show must be implemented'


class Wizard_Rule (CTK.Box):
    def __init__ (self, key):
        Wizard.__init__ (self)
        self.key = key


class WizardList (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'class': 'wizard-list'})


class Categories:
    def __init__ (self):
        self.list = [
            {'title':   N_('CMS'),
             'descr':   N_('Content Management Systems'),
             'url_pre': '%s/cms' %(URL_CAT_LIST)},

            {'title':   N_('Wiki'),
             'descr':   N_('Wiki wika wiki wika wika wiki wika..'),
             'url_pre': '%s/wiki' %(URL_CAT_LIST)},

            {'title':   N_('Other'),
             'descr':   N_('Whatever, whatever, whatever, whatever..'),
             'url_pre': '%s/other' %(URL_CAT_LIST)}]

    def __iter__ (self):
        return iter(self.list)

class Icon (CTK.Image):
    def __init__ (self, name, _props={}):
        props = _props.copy()
        props['src'] = '/static/images/wizards/%s.png'%(name)
        props['alt'] = "%s logo" %(name.capitalize())

        if 'class' in props:
            props['class'] += ' wizard-icon'
        else:
            props['class'] = 'wizard-icon'

        CTK.Image.__init__ (self, props)


# Handle 'click' events inside the Wizard Categories list.
#
JS_WIZARD_LIST = """
$('#%(list_id)s').each(function() {
    var box    = $(this);
    var hidden = box.find('input:hidden');

    box.find('li').each(function() {
        var li = $(this);

        li.bind ('click', function(event) {
            box.find('li').removeClass('wizard-list-selected');
            $(this).addClass ('wizard-list-selected');
            hidden.val (li.attr('wizard'));
        });
    });
});
"""

# Generates a 'open_wizard' event whenever the dialog is submitted.
#
JS_WIZARD_LIST_SUBMIT = """
var selected = $('#%(hidden)s').val();
if (selected) {
    $(this).trigger ({type: 'open_wizard', 'wizard': selected});
}

return false;
"""

class CategoryList_Widget (CTK.Box):
    def __init__ (self, category):
        CTK.Box.__init__ (self)
        self.category = category

        # Build the list (*TEMPORALLY* HARDCODED!)
        #
        wlist = CTK.List({'class': 'wizard-list'})

        if category == 'cms':
            for w in WIZARDS_CMS:
                wlist.Add ([CTK.Box({'class': 'logo'},  Icon(w['wizard'])),
                            CTK.Box({'class': 'title'}, CTK.RawHTML(w['title'])),
                            CTK.Box({'class': 'descr'}, CTK.RawHTML(w['descr']))],
                           {'wizard': w['wizard']})

        hidden = CTK.Hidden ('wizard')

        # Assembling
        submit = CTK.Submitter (URL_CAT_APPLY)
        submit += wlist
        submit += hidden
        submit += CTK.RawHTML (js = JS_WIZARD_LIST %({'list_id': self.id}))
        submit.bind('submit_success', JS_WIZARD_LIST_SUBMIT %({'hidden': hidden.id}))
        self += submit


def CategoryList():
    # Figure the category
    category = re.findall (URL_CAT_LIST_R, CTK.request.url)[0]

    # Instance and Render
    content = CategoryList_Widget (category)
    return content.Render().toJSON()


def CategoryList_Apply():
    print CTK.post
    return {'ret': 'ok'}

#
# Widgets
#
class CloneLogsCfg (CTK.ComboCfg):
    NOTE = N_("Use the same logging configuration as one of the other virtual servers.")

    def __init__ (self, cfg):
        # Build the options list
        options = [('', 'Do not configure')]

        for v in CTK.cfg.keys('vserver'):
            pre  = 'vserver!%s!logger' %(v)
            log  = CTK.cfg.get_val(pre)
            nick = CTK.cfg.get_val('vserver!%s!nick'%(v))
            if not log:
                continue
            options.append (('vserver!%s'%(v), '%s (%s)'%(nick, log)))

        # Init
        CTK.ComboCfg.__init__ (self, cfg, options, {'class': 'noauto'})

def CloneLogsCfg_Apply (key_tmp, vserver):
    # Check the source
    logging_as = CTK.cfg.get_val (key_tmp)
    if not logging_as:
        return

    # Clone
    CTK.cfg.clone ('%s!logger'%(logging_as),       '%s!logger'%(vserver))
    CTK.cfg.clone ('%s!error_writer'%(logging_as), '%s!error_writer'%(vserver))


def AddUsualStaticFiles (rule_pre):
    CTK.cfg['%s!match'%(rule_pre)]   = 'fullpath'
    CTK.cfg['%s!handler'%(rule_pre)] = 'file'

    n = 1
    for file in USUAL_STATIC_FILES:
        CTK.cfg['%s!match!fullpath!%d'%(rule_pre,n)] = file
        n += 1

#
# Init (TEMPORARY)
#
def init():
    global _is_init
    _is_init = True

    CTK.load_module ('drupal',    'wizards')
    CTK.load_module ('wordpress', 'wizards')
    CTK.load_module ('trac',      'wizards')

_is_init = False
if not _is_init:
    init()


CTK.publish (URL_CAT_LIST_R, CategoryList)
CTK.publish (URL_CAT_APPLY,  CategoryList_Apply, method="POST")
