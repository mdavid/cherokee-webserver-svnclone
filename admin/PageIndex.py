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
import gettext
import xmlrpclib
import XMLServerDigest
import Graph
import PageError

import os
import time

from consts import *
from configured import *

OWS_PROUD = 'http://www.octality.com/api/proud/open/'


BETA_TESTER_NOTICE = N_("""
<h3>Beta testing</h3> <p>Individuals like yourself who download and
test the latest developer snapshots of Cherokee Web Server help us to
create the highest quality product. For that, we thank you.</p>
""")

PROUD_USERS_WEB = "http://www.cherokee-project.com/cherokee-domain-list.html"

PROUD_USERS_NOTICE = N_("""
We would love to know that you are using Cherokee. Submit your domain
name and it will be <a target="_blank" href="%s">listed on the
Cherokee Project web site</a>.
""" %(PROUD_USERS_WEB))

PROUD_DIALOG_OK     = N_("The information has been successfully sent. Thank you!")
PROUS_DIALOG_ERROR1 = N_("Unfortunatelly something went wrong, and the information could not be submitted:")
PROUS_DIALOG_ERROR2 = N_("Please, try again. Do not hesitate to report the problem if it persists.")

HELPS = [('config_status',     N_("Status"))]

def Launch():
    if not Cherokee.server.is_alive():
        error = Cherokee.server.launch()
        if error:
            page_error = PageError.PageErrorLaunch (error)
            return page_error.Render()

    return CTK.HTTP_Redir('/')

def Stop():
    Cherokee.pid.refresh()
    Cherokee.server.stop()
    return CTK.HTTP_Redir('/')


class ServerInfo (CTK.Table):
    def __init__ (self):
        CTK.Table.__init__ (self)
        self.id = "server_info_table"
        self.set_header (column=True, num=1)

        self.add (_('Status'),       ['Stopped', 'Running'][Cherokee.server.is_alive()])
        self.add (_('PID'),          Cherokee.pid.pid or _("Not running"))
        self.add (_('Version'),      VERSION)
        self.add (_("Default WWW"),  self._get_droot())
        self.add (_("Prefix"),       PREFIX)
        self.add (_("Config File"),  CTK.cfg.file or _("Not found"))
        self.add (_("Modified"),     self._get_cfg_ctime())

    def add (self, title, string):
        self += [CTK.RawHTML(title), CTK.RawHTML(str(string))]

    def _get_droot (self):
        tmp = [int(x) for x in CTK.cfg.keys('vserver')]
        tmp.sort()

        if not tmp:
            return WWWROOT

        return CTK.cfg.get_val ('vserver!%d!document_root'%(tmp[0]), WWWROOT)

    def _get_cfg_ctime (self):
        info = os.stat(CTK.cfg.file)
        return time.ctime(info.st_ctime)


def Lang_Apply():
    # Sanity check
    langs = CTK.post.get_val('lang')
    if not langs:
        return {'ret': 'error', 'errors': {'lang': 'Cannot be empty'}}

    # Install the new language
    languages = [l for s in langs.split(',') for l in s.split(';') if not '=' in l]
    try:
        gettext.translation('cherokee', LOCALEDIR, languages).install()
    except:
        pass

    return {'ret': 'ok', 'redirect': '/'}

class LanguageSelector (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'lenguage-selector'})
        languages = [('', _('Choose'))] + AVAILABLE_LANGUAGES

        submit = CTK.Submitter('/lang/apply')
        submit += CTK.Combobox ({'name': 'lang'}, languages)

        self += CTK.RawHTML('<h3>%s</h3>' %(_('Lenguaje')))
        self += submit


def ProudUsers_Apply():
    # Collect domains
    domains = []

    for v in CTK.cfg.keys('vserver'):
        domains.append (CTK.cfg.get_val('vserver!%s!nick'%(v)))

        for d in CTK.cfg.keys('vserver!%s!match!domain'%(v)):
            domains.append (CTK.cfg.get_val('vserver!%s!match!domain!%s'%(v, d)))

        for d in CTK.cfg.keys('vserver!%s!match!regex'%(v)):
            domains.append (CTK.cfg.get_val('vserver!%s!match!regex!%s'%(v, d)))

    # Send the list
    try:
        xmlrpc = XMLServerDigest.XmlRpcServer (OWS_PROUD)
        xmlrpc.add_domains_to_review(domains)

    except xmlrpclib.ProtocolError, err:
        details  = "Error code:    %d\n" % err.errcode
        details += "Error message: %s\n" % err.errmsg
        details += "Headers: %s\n"       % err.headers

        return '<p>%s</p>'            %(PROUS_DIALOG_ERROR1)      + \
               '<p><pre>%s</pre></p>' %(CTK.escape_html(details)) + \
               '<p>%s</p>'            %(PROUS_DIALOG_ERROR2)

    except Exception, e:
        return '<p>%s</p>'              %(PROUS_DIALOG_ERROR1)     + \
               '<p><pre>%s\n</pre></p>' %(CTK.escape_html(str(e))) + \
               '<p>%s</p>'              %(PROUS_DIALOG_ERROR2)

    return "<p>%s</p>" %(_(PROUD_DIALOG_OK))


class ProudUsers (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'proud-users'})

        # Dialog
        dialog = CTK.DialogProxyLazy ('/proud/apply', {'title': _('Proud Cherokee User List Submittion'), 'width': 500})
        dialog.AddButton (_('Close'), "close")

        button = CTK.Button (_('Send domains'))
        button.bind ('click', dialog.JS_to_show())

        self += CTK.RawHTML('<h3>%s</h3>' %(_('Proud Cherokee Users')))
        self += CTK.Box ({'id': 'notice'}, CTK.RawHTML (PROUD_USERS_NOTICE))
        self += button
        self += dialog


class Render():
    def __call__ (self):
        Cherokee.pid.refresh()

        self.page = Page.Base(_('Welcome to Cherokee Admin'), body_id='index', helps=HELPS)
        self.page += CTK.RawHTML ("<h1>%s</h1>"% _('Welcome to Cherokee Admin'))

        if 'b' in VERSION:
            notice  = CTK.Notice()
            notice += CTK.RawHTML(_(BETA_TESTER_NOTICE))
            self.page += notice

        self.page += ServerInfo()
        self.page += CTK.RawHTML('<a href="/launch">Launch</a> | <a href="/stop">Stop</a>')
        self.page += LanguageSelector()
        self.page += ProudUsers()

        if CTK.cfg.get_val('server!collector') == 'rrd':
            self.page += Graph.GraphServer_Instancer()

        return self.page.Render()


CTK.publish (r'^/$',            Render)
CTK.publish (r'^/launch$',      Launch)
CTK.publish (r'^/stop$',        Stop)
CTK.publish (r'^/lang/apply$',  Lang_Apply, method="POST")
CTK.publish (r'^/proud/apply$', ProudUsers_Apply, method="POST")
