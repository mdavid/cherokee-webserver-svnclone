# -*- coding: utf-8 -*-
#
# phpBB Wizard
#
# Authors:
#      Taher Shihadeh <taher@octality.com>
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

#
# Tested:
# 2009/10/xx: phpBB 3.0.5 / Cherokee 0.99.25
# 2010/04/19: phpBB 3.0.5 / Cherokee 0.99.45b
#

import os
import re
import CTK
import Wizard
import validations
from util import *

NOTE_WELCOME_H1 = N_("Welcome to the phpBB Wizard")
NOTE_WELCOME_P1 = N_('<a target="_blank" href="http://www.phpbb.com/">phpBB</a> has become the most widely used Open Source forum solution.')
NOTE_WELCOME_P2 = N_('With a large and highly customisable set of key features coupled with a standards compliant interface, phpBB will grow with, and complement your website. With millions of installations worldwide, it is no wonder phpBB™ is one of the most popular ways to create and support a community.')

NOTE_LOCAL_H1   = N_("Application Source Code")
NOTE_LOCAL_DIR  = N_("Local directory where the phpBB source code is located. Example: /usr/share/phpbb.")
ERROR_NO_SRC    = N_("Does not look like a phpBB source directory.")

NOTE_HOST_H1    = N_("New Virtual Server Details")
NOTE_HOST       = N_("Host name of the virtual server that is about to be created.")

NOTE_WEBDIR     = N_("Web directory where you want phpBB to be accessible. (Example: /phpbb)")
NOTE_WEBDIR_H1  = N_("Public Web Directory")

PREFIX          = 'tmp!wizard!phpbb'
URL_APPLY       = r'/wizard/vserver/phpbb/apply'

CONFIG_DIR = """
%(pre_rule_plus1)s!handler = custom_error
%(pre_rule_plus1)s!handler!error = 403
%(pre_rule_plus1)s!match = request
%(pre_rule_plus1)s!match!request = ^%(web_dir)s/(config.php|common.php|cache/.*|files/.*|images/avatars/upload/.*|store/.*)

# IMPORTANT: The PHP rule comes here
"""

CONFIG_VSERVER = """
%(pre_vsrv)s!nick = %(host)s
%(pre_vsrv)s!document_root = %(local_src_dir)s
%(pre_vsrv)s!directory_index = index.php,index.html

%(pre_rule_plus1)s!handler = custom_error
%(pre_rule_plus1)s!handler!error = 403
%(pre_rule_plus1)s!match = request
%(pre_rule_plus1)s!match!request = /(config.php|common.php|cache/.*|files/.*|images/avatars/upload/.*|store/.*)

# The PHP rule comes here

%(pre_rule_minus1)s!handler = common
%(pre_rule_minus1)s!handler!iocache = 0
%(pre_rule_minus1)s!match = default
"""

SRC_PATHS = [
    "/usr/share/phpBB3",         # Debian, Fedora
    "/usr/share/phpbb",
    "/var/www/*/htdocs/phpbb",   # Gentoo
    "/srv/www/htdocs/phpbb",     # SuSE
    "/usr/local/www/data/phpbb*" # BSD
]


class Commit:
    def Commit_VServer (self):
        # Create the new Virtual Server
        next = CTK.cfg.get_next_entry_prefix('vserver')
        CTK.cfg['%s!nick'%(next)] = CTK.cfg.get_val('%s!host'%(PREFIX))
        Wizard.CloneLogsCfg_Apply ('%s!logs_as_vsrv'%(PREFIX), next)

        # PHP
        php = CTK.load_module ('php', 'wizards')

        error = php.wizard_php_add (next)
        if error:
            return {'ret': 'error', 'errors': {'msg': error}}

        php_info = php.get_info (next)

        # phpBB
        props = cfg_get_surrounding_repls ('pre_rule', php_info['rule'])
        props['pre_vsrv']      = next
        props['host']          = CTK.cfg.get_val('%s!host'    %(PREFIX))
        props['local_src_dir'] = CTK.cfg.get_val('%s!sources' %(PREFIX))

        config = CONFIG_VSERVER %(props)
        CTK.cfg.apply_chunk (config)

        # Common static
        Wizard.AddUsualStaticFiles(props['pre_rule_plus9'])

        # Clean up
        CTK.cfg.normalize ('%s!rule'%(next))
        CTK.cfg.normalize ('vserver')

        del (CTK.cfg[PREFIX])
        return CTK.cfg_reply_ajax_ok()

    def Commit_Rule (self):
        vsrv_num = CTK.cfg.get_val ('%s!vsrv_num'%(PREFIX))
        next = 'vserver!%s' %(vsrv_num)

        # PHP
        php = CTK.load_module ('php', 'wizards')

        error = php.wizard_php_add (next)
        if error:
            return {'ret': 'error', 'errors': {'msg': error}}

        php_info = php.get_info (next)

        # phpBB
        props = cfg_get_surrounding_repls ('pre_rule', php_info['rule'])
        props['pre_vsrv']      = next
        props['web_dir']       = CTK.cfg.get_val('%s!web_dir'   %(PREFIX))
        props['local_src_dir'] = CTK.cfg.get_val('%s!sources' %(PREFIX))

        config = CONFIG_DIR %(props)
        CTK.cfg.apply_chunk (config)

        # Clean up
        CTK.cfg.normalize ('%s!rule'%(next))

        del (CTK.cfg[PREFIX])
        return CTK.cfg_reply_ajax_ok()

    def __call__ (self):
        if CTK.post.pop('final'):
            # Apply POST
            CTK.cfg_apply_post()

            # VServer or Rule?
            if CTK.cfg.get_val ('%s!vsrv_num'%(PREFIX)):
                return self.Commit_Rule()
            return self.Commit_VServer()

        return CTK.cfg_apply_post()


class WebDirectory:
    def __call__ (self):
        table = CTK.PropsTable()
        table.Add (_('Web Directory'), CTK.TextCfg ('%s!web_dir'%(PREFIX), False, {'value': '/phpbb', 'class': 'noauto'}), _(NOTE_WEBDIR))

        vsrv_num = CTK.cfg.get_val ('%s!vsrv_num'%(PREFIX))
        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden('final', '1')
        submit += table

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(_(NOTE_WEBDIR_H1)))
        cont += submit
        cont += CTK.DruidButtonsPanel_PrevCreate_Auto()
        return cont.Render().toStr()


class Host:
    def __call__ (self):
        table = CTK.PropsTable()
        table.Add (_('New Host Name'),    CTK.TextCfg ('%s!host'%(PREFIX), False, {'value': 'phpbb.example.com', 'class': 'noauto'}), _(NOTE_HOST))
        table.Add (_('Use Same Logs as'), Wizard.CloneLogsCfg('%s!logs_as_vsrv'%(PREFIX)), _(Wizard.CloneLogsCfg.NOTE))

        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden('final', '1')
        submit += table

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(_(NOTE_HOST_H1)))
        cont += submit
        cont += CTK.DruidButtonsPanel_PrevCreate_Auto()
        return cont.Render().toStr()


class LocalSource:
    def __call__ (self):
        guessed_src = path_find_w_default (SRC_PATHS)

        table = CTK.PropsTable()
        table.Add (_('Source Directory'), CTK.TextCfg ('%s!sources'%(PREFIX), False, {'value': guessed_src}), _(NOTE_LOCAL_DIR))

        submit = CTK.Submitter (URL_APPLY)
        submit += table

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(_(NOTE_LOCAL_H1)))
        cont += submit
        cont += CTK.DruidButtonsPanel_PrevNext_Auto()
        return cont.Render().toStr()


class PHP:
    def __call__ (self):
        php = CTK.load_module ('php', 'wizards')
        return php.External_FindPHP()


class Welcome:
    def __call__ (self):
        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(_(NOTE_WELCOME_H1)))
        cont += Wizard.Icon ('phpbb', {'class': 'wizard-descr'})
        box = CTK.Box ({'class': 'wizard-welcome'})
        box += CTK.RawHTML ('<p>%s</p>' %(_(NOTE_WELCOME_P1)))
        box += CTK.RawHTML ('<p>%s</p>' %(_(NOTE_WELCOME_P2)))
        box += Wizard.CookBookBox ('cookbook_phpbb')
        cont += box

        # Send the VServer num if it's a Rule
        tmp = re.findall (r'^/wizard/vserver/(\d+)/', CTK.request.url)
        if tmp:
            submit = CTK.Submitter (URL_APPLY)
            submit += CTK.Hidden('%s!vsrv_num'%(PREFIX), tmp[0])
            cont += submit

        cont += CTK.DruidButtonsPanel_Next_Auto()
        return cont.Render().toStr()


def is_phpbb_dir (path):
    path = validations.is_local_dir_exists (path)
    module_inc = os.path.join (path, 'includes/bbcode.php')
    try:
        validations.is_local_file_exists (module_inc)
    except:
        raise ValueError, _(ERROR_NO_SRC)
    return path

VALS = [
    ('%s!sources' %(PREFIX), validations.is_not_empty),
    ('%s!host'    %(PREFIX), validations.is_not_empty),
    ('%s!web_dir' %(PREFIX), validations.is_not_empty),

    ('%s!sources' %(PREFIX), is_phpbb_dir),
    ('%s!host'    %(PREFIX), validations.is_new_vserver_nick),
    ('%s!web_dir' %(PREFIX), validations.is_dir_formatted),
]

# VServer
CTK.publish ('^/wizard/vserver/phpbb$',   Welcome)
CTK.publish ('^/wizard/vserver/phpbb/2$', PHP)
CTK.publish ('^/wizard/vserver/phpbb/3$', LocalSource)
CTK.publish ('^/wizard/vserver/phpbb/4$', Host)

# Rule
CTK.publish ('^/wizard/vserver/(\d+)/phpbb$',   Welcome)
CTK.publish ('^/wizard/vserver/(\d+)/phpbb/2$', PHP)
CTK.publish ('^/wizard/vserver/(\d+)/phpbb/3$', LocalSource)
CTK.publish ('^/wizard/vserver/(\d+)/phpbb/4$', WebDirectory)

# Common
CTK.publish (r'^%s$'%(URL_APPLY), Commit, method="POST", validation=VALS)
