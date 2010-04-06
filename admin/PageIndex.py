# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#      Taher Shihadeh <taher@unixwars.com>
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
import urllib
import urllib2
import re

from consts import *
from configured import *

# URLs
LINK_OCTALITY   = 'http://www.octality.com/'
LINK_SUPPORT    = '%sengineering.html' % LINK_OCTALITY
PROUD_USERS_WEB = "http://www.cherokee-project.com/cherokee-domain-list.html"
OWS_PROUD       = 'http://www.octality.com/api/proud/open/'

# Links
LINK_BUGTRACKER = 'http://bugs.cherokee-project.com/'
LINK_TWITTER    = 'http://twitter.com/webserver'
LINK_FACEBOOK   = 'http://www.facebook.com/cherokee.project'
LINK_DOWNLOAD   = 'http://www.cherokee-project.com/download/'
LINK_LIST       = 'http://lists.octality.com/listinfo/cherokee'
LINK_IRC        = 'irc://irc.freenode.net/cherokee'

# Subscription
SUBSCRIBE_URL   = 'http://lists.octality.com/subscribe/cherokee-dev'
SUBSCRIBE_CHECK = 'Your subscription request has been received'
SUBSCRIBE_APPLY = '/index/subscribe/apply'

NOTE_EMAIL      = N_("You will be sent an email requesting confirmation")
NOTE_NAME       = N_("Optionally provide your name")

# Notices
SUPPORT_NOTICE      = N_('Commercial support for Cherokee is provided by <a target="_blank" href="%s">Octality</a>. They provide top notch <a target="_blank" href="%s">Consulting, Custom Engineering, and Enterprise Support Services.</a>' %(LINK_OCTALITY, LINK_SUPPORT))
LIST_NOTICE         = N_('The Cherokee-Project Community Mailing List is the place to go for help and sharing experiences about Cherokee. Subscribe now!')
IRC_NOTICE          = N_('Join us at the <a target="_blank" href="%s">#cherokee</a> IRC Channel on freenode.net.'%(LINK_IRC))
BUG_TRACKER_NOTICE  = N_('Your feedback is important! Log Bug Reports and Requests for Enhancements in our <a target="_blank" href="%s">bug tracker</a> to help us improve Cherokee.' %(LINK_BUGTRACKER))
SOCIAL_MEDIA_NOTICE = N_("Find out what's going on with Cherokee on your favorite Social Media!")
TWITTER_NOTICE      = N_('<a target="_blank" href="%s">Cherokee at Twitter</a>: Microblogging about Cherokee' %(LINK_TWITTER))
FACEBOOK_NOTICE     = N_('<a target="_blank" href="%s">Cherokee at Facebook</a>: Cherokee\'s day to day news' %(LINK_FACEBOOK))

BETA_TESTER_NOTICE = N_("""\
<h3>Beta testing</h3> <p>Individuals like yourself who download and
test the latest developer snapshots of Cherokee Web Server help us to
create the highest quality product. For that, we thank you.</p>
""")

PROUD_USERS_NOTICE = N_("""\
We would love to know that you are using Cherokee. Submit your domain
name and it will be <a target="_blank" href="%s">listed on the
Cherokee Project web site</a>.
""" %(PROUD_USERS_WEB))

# Dialogs
PROUD_DIALOG_OK     = N_("The information has been successfully sent. Thank you!")
PROUS_DIALOG_ERROR1 = N_("Unfortunatelly something went wrong, and the information could not be submitted:")
PROUS_DIALOG_ERROR2 = N_("Please, try again. Do not hesitate to report the problem if it persists.")

# "Latest release" regex
LATEST_REGEX = r'LATEST_is_(.+?)<'
LATEST_URL   = '/index/release'

# Help entries
HELPS = [('config_status', N_("Status"))]


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
        dialog = CTK.DialogProxyLazy ('/proud/apply', {'title': _('Proud Cherokee User List Submission'), 'width': 500})
        dialog.AddButton (_('Close'), "close")

        button = CTK.Button (_('Send domains'))
        button.bind ('click', dialog.JS_to_show())

        self += CTK.RawHTML('<h3>%s</h3>' %(_('Proud Cherokee Users')))
        self += CTK.Box ({'id': 'notice'}, CTK.RawHTML (PROUD_USERS_NOTICE))
        self += button
        self += dialog


class LatestRelease (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        self += CTK.Proxy (None, LATEST_URL)


class LatestReleaseBox (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'latest-release'})

        self += CTK.RawHTML('<h3>%s</h3>' % _('Latest Release'))
        latest = self._find_latest_cherokee_release()

        if not latest:
            self += CTK.RawHTML(_('Latest version could not be determined at the moment.'))
        elif VERSION.startswith(latest['version']):
            self += CTK.RawHTML(_('Cherokee is up to date.'))
        else:
            txt = '%s v%s. %s v%s.' % (
                _('You are running Cherokee'),
                VERSION,
                _('Latest release is '),
                latest['version'])
            self += CTK.RawHTML(txt)

    def __call__ (self):
        render = self.Render()
        return render.toStr()

    def _find_latest_cherokee_release (self):
        """Find out latest Cherokee release"""
        try:
            txt    = urllib.urlopen(LINK_DOWNLOAD).read()
            latest = re.findall (LATEST_REGEX, txt, re.DOTALL)[0]
            return { 'version': latest }
        except (IOError, IndexError):
            pass


def Subscribe_Apply ():
    values = {}
    for k in CTK.post:
        values[k] = CTK.post[k]

    data = urllib.urlencode(values)
    req  = urllib2.Request(SUBSCRIBE_URL, data)
    response = urllib2.urlopen(req)
    results_page = response.read()
    if SUBSCRIBE_CHECK in results_page:
        return {'ret':'ok'}

    return {'ret':'error'}


class MailingListSubscription (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        table = CTK.PropsTable()
        table.Add (_('Your email address'), CTK.TextField({'name': 'email', 'class': 'noauto'}), _(NOTE_EMAIL))
        table.Add (_('Your name'),          CTK.TextField({'name': 'fullname', 'class': 'noauto', 'optional':True}), _(NOTE_NAME))

        submit = CTK.Submitter(SUBSCRIBE_APPLY)
        submit += table
        self += submit


class ContactChannels (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'contact-channels'})

        self += CTK.RawHTML('<h3>%s</h3>' % _('Contact Channels'))

        self += CTK.RawHTML('<h4>%s</h4>' % _('IRC'))
        self += CTK.RawHTML(_(IRC_NOTICE))

        self += CTK.RawHTML('<h4>%s</h4>' % _('Mailing List'))
        self += CTK.RawHTML(_(LIST_NOTICE))

        # Subscribe Button
        dialog = CTK.Dialog ({'title': _('Mailing list subscription'), 'width': 560})
        dialog.AddButton (_('Subscribe'), dialog.JS_to_trigger('submit'))
        dialog.AddButton (_('Cancel'), "close")
        dialog += MailingListSubscription()

        button = CTK.Button(_('Subscribe'))
        button.bind ('click', dialog.JS_to_show())
        dialog.bind ('submit_success', dialog.JS_to_close())

        self += button
        self += dialog


class BugTracker (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'bug-tracker'})

        self += CTK.RawHTML('<h3>%s</h3>' % _('Bug Tracker'))
        self += CTK.RawHTML(_(BUG_TRACKER_NOTICE))


class SocialMedia (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'social-media'})

        self += CTK.RawHTML('<h3>%s</h3>' % _('Social Media'))
        self += CTK.RawHTML(_(SOCIAL_MEDIA_NOTICE))

        # @ION: These icons have been dropped as they were. Change, remove or do with them whatever you see fit.
        self += CTK.Image ({'alt': 'Twitter button',  'title': 'Twitter',  'src': '/static/images/other/twitter_button.png', 'height':"32", 'width':"32"})
        self += CTK.RawHTML(_(TWITTER_NOTICE))

        self += CTK.Image ({'alt': 'Facebook button', 'title': 'Facebook', 'src': '/static/images/other/facebook_button.png', 'height':"32", 'width':"32"})
        self += CTK.RawHTML(_(FACEBOOK_NOTICE))


class CommunityBox (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'community-box'})

        self += CTK.RawHTML('<h2>%s</h2>' % _('Community Support'))

        self += ProudUsers()
        self += LatestRelease()
        self += ContactChannels()
        self += BugTracker()
        self += SocialMedia()


class EnterpriseBox (CTK.Box):
    def __init__ (self):
        CTK.Box.__init__ (self, {'id': 'enterprise-box'})

        self += CTK.RawHTML('<h2>%s</h2>' % _('Commercial Support'))
        self += CTK.RawHTML(_(SUPPORT_NOTICE))


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

        self.page += EnterpriseBox()
        self.page += CommunityBox()

        return self.page.Render()


CTK.publish (r'^/$',            Render)
CTK.publish (r'^/launch$',      Launch)
CTK.publish (r'^/stop$',        Stop)
CTK.publish (r'^/lang/apply$',  Lang_Apply, method="POST")
CTK.publish (r'^/proud/apply$', ProudUsers_Apply, method="POST")
CTK.publish (r'^%s$'%(SUBSCRIBE_APPLY), Subscribe_Apply, method="POST")
CTK.publish (r'^%s$'%(LATEST_URL),      LatestReleaseBox, method="POST")
