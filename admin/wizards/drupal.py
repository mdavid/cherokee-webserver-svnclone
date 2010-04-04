# -*- coding: utf-8 -*-
#
# CTK: Cherokee Toolkit
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

import os
import CTK
import Wizard
import validations
from util import *

NOTE_WELCOME_H1 = N_("Welcome to the Drupal wizard")
NOTE_WELCOME_P1 = N_("Drupal is " + "bla, "*50)
NOTE_WELCOME_P2 = N_("It also "   + "bla, "*50)
NOTE_LOCAL_H1   = N_("Application Source Code")
NOTE_LOCAL_DIR  = N_("Local directory where the Drupal source code is located. Example: /usr/share/drupal.")
NOTE_HOST_H1    = N_("New Virtual Server Details")
NOTE_HOST       = N_("Host name of the virtual host that is about to be created.")

ERROR_NO_SRC    = N_("Does not look like a Drupal source directory.")

PREFIX    = 'tmp!wizard!drupal'
URL_APPLY = '/wizard/vserver/drupal/apply'

SRC_PATHS = [
    "/usr/share/drupal7",         # Debian, Fedora
    "/usr/share/drupal",
    "/usr/share/drupal6",
    "/usr/share/drupal5",
    "/var/www/*/htdocs/drupal",   # Gentoo
    "/srv/www/htdocs/drupal",     # SuSE
    "/usr/local/www/data/drupal*" # BSD
]


def commit():
    if CTK.post.pop('final'):
        ret = CTK.cfg_apply_post()

        next = CTK.cfg.get_next_entry_prefix('vserver')
        CTK.cfg['%s!nick'%(next)] = CTK.cfg.get_val('%s!host'%(PREFIX))

        del(CTK.cfg[PREFIX])
        print "ret", ret
        return ret

    return CTK.cfg_apply_post()


class Host:
    def __call__ (self):
        table = CTK.PropsTable()
        table.Add (_('New Host Name'),    CTK.TextCfg ('%s!host'%(PREFIX), False, {'value': 'www.example.com'}), NOTE_HOST)
        table.Add (_('Use Same Logs as'), Wizard.CloneLogsCfg('%s!logs_as_vsrv'%(PREFIX)), _(Wizard.CloneLogsCfg.NOTE))

        submit = CTK.Submitter (URL_APPLY)
        submit += CTK.Hidden('final', '1')
        submit += table

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(NOTE_HOST_H1))
        cont += submit
        cont += CTK.DruidButtonsPanel_PrevCreate_Auto()
        return cont.Render().toStr()


class LocalSource:
    def __call__ (self):
        guessed_src = path_find_w_default (SRC_PATHS)

        table = CTK.PropsTable()
        table.Add (_('Drupal Local Directory'), CTK.TextCfg ('%s!local_dir'%(PREFIX), False, {'value': guessed_src}), NOTE_LOCAL_DIR)

        submit = CTK.Submitter (URL_APPLY)
        submit += table

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(NOTE_LOCAL_H1))
        cont += submit
        cont += CTK.DruidButtonsPanel_PrevNext_Auto()
        return cont.Render().toStr()


class Welcome:
    def __call__ (self):
        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s</h2>' %(NOTE_WELCOME_H1))
        cont += CTK.RawHTML ('<p>%s</p>' %(NOTE_WELCOME_P1))
        cont += CTK.RawHTML ('<p>%s</p>' %(NOTE_WELCOME_P2))
        cont += CTK.DruidButtonsPanel_Next_Auto()
        return cont.Render().toStr()


def is_drupal_dir (path):
    path = validations.is_local_dir_exists (path)
    module_inc = os.path.join (path, 'includes/module.inc')
    if not os.path.exists (module_inc):
        raise ValueError, _(ERROR_NO_SRC)
    return path


VALS = [
    ('%s!local_dir'%(PREFIX), is_drupal_dir),
    ('%s!host'     %(PREFIX), validations.is_new_vserver_nick)
]

CTK.publish ('^/wizard/vserver/drupal$',   Welcome)
CTK.publish ('^/wizard/vserver/drupal/2$', LocalSource)
CTK.publish ('^/wizard/vserver/drupal/3$', Host)
CTK.publish (r'^%s$'%(URL_APPLY), commit, method="POST", validation=VALS)
