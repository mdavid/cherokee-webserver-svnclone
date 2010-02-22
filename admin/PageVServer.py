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
import validations

import re

from consts import *
from configured import *

URL_BASE  = '/vserver'
URL_APPLY = '/vserver/apply'

NOTE_NICKNAME         = N_('Nickname for the virtual server.')
NOTE_CERT             = N_('This directive points to the PEM-encoded Certificate file for the server (Full path to the file)')
NOTE_CERT_KEY         = N_('PEM-encoded Private Key file for the server (Full path to the file)')
NOTE_CA_LIST          = N_('File containing the trusted CA certificates, utilized for checking the client certificates (Full path to the file)')
NOTE_CIPHERS          = N_('Ciphers that TLS/SSL is allowed to use. <a target="_blank" href="http://www.openssl.org/docs/apps/ciphers.html">Reference</a>. (Default: all ciphers supported by the OpenSSL version used).')
NOTE_CLIENT_CERTS     = N_('Skip, Accept or Require client certificates.')
NOTE_VERIFY_DEPTH     = N_('Limit up to which depth certificates in a chain are used during the verification procedure (Default: 1)')
NOTE_ERROR_HANDLER    = N_('Allows the selection of how to generate the error responses.')
NOTE_PERSONAL_WEB     = N_('Directory inside the user home directory to use as root web directory. Disabled if empty.')
NOTE_DISABLE_PW       = N_('The personal web support is currently turned on.')
NOTE_ADD_DOMAIN       = N_('Adds a new domain name. Wildcards are allowed in the domain name.')
NOTE_DOCUMENT_ROOT    = N_('Virtual Server root directory.')
NOTE_DIRECTORY_INDEX  = N_('List of name files that will be used as directory index. Eg: <em>index.html,index.php</em>.')
NOTE_MAX_UPLOAD_SIZE  = N_('The maximum size, in bytes, for POST uploads. (Default: unlimited)')
NOTE_KEEPALIVE        = N_('Whether this virtual server is allowed to use Keep-alive (Default: yes)')
NOTE_DISABLE_LOG      = N_('The Logging is currently enabled.')
NOTE_LOGGERS          = N_('Logging format. Apache compatible is highly recommended here.')
NOTE_ACCESSES         = N_('Back-end used to store the log accesses.')
NOTE_ERRORS           = N_('Back-end used to store the log errors.')
NOTE_ACCESSES_ERRORS  = N_('Back-end used to store the log accesses and errors.')
NOTE_WRT_FILE         = N_('Full path to the file where the information will be saved.')
NOTE_WRT_EXEC         = N_('Path to the executable that will be invoked on each log entry.')
NOTE_X_REAL_IP        = N_('Whether the logger should read and use the X-Real-IP and X-Forwarded-For headers (send by reverse proxies).')
NOTE_X_REAL_IP_ALL    = N_('Accept all the X-Real-IP and X-Forwarded-For headers. WARNING: Turn it on only if you are centain of what you are doing.')
NOTE_X_REAL_IP_ACCESS = N_('List of IP addresses and subnets that are allowed to send X-Real-IP and X-Forwarded-For headers.')
NOTE_EVHOST           = N_('How to support the "Advanced Virtual Hosting" mechanism. (Default: off)')
NOTE_LOGGER_TEMPLATE  = N_('The following variables are accepted: <br/>${ip_remote}, ${ip_local}, ${protocol}, ${transport}, ${port_server}, ${query_string}, ${request_first_line}, ${status}, ${now}, ${time_secs}, ${time_nsecs}, ${user_remote}, ${request}, ${request_original}, ${vserver_name}, ${response_size}')
NOTE_MATCHING_METHOD  = N_('Allows the selection of domain matching method.')
NOTE_COLLECTOR        = N_('Whether or not it should collected statistics about the traffic of this virtual server.')
NOTE_UTC_TIME         = N_('Time standard to use in the log file entries.')

DEFAULT_HOST_NOTE     = N_("<p>The 'default' virtual server matches all the domain names.</p>")

VALIDATIONS = [
    ("vserver![\d]+!user_dir",                   validations.is_safe_id),
    ("vserver![\d]+!document_root",              validations.is_dev_null_or_local_dir_exists),
    ("vserver![\d]+!post_max_len",               validations.is_positive_int),
    ("vserver![\d]+!ssl_certificate_file",       validations.is_local_file_exists),
    ("vserver![\d]+!ssl_certificate_key_file",   validations.is_local_file_exists),
    ("vserver![\d]+!ssl_ca_list_file",           validations.is_local_file_exists),
    ("vserver![\d]+!ssl_verify_depth",           validations.is_positive_int),
    ("vserver![\d]+!logger![\d]+!filename",      validations.parent_is_dir),
    ("vserver![\d]+!logger![\d]+!command",       validations.is_local_file_exists),
    ("vserver![\d]+!logger!x_real_ip_access$",   validations.is_ip_or_netmask_list),
]

HELPS = [
    ('config_virtual_servers', N_("Virtual Servers")),
    ('modules_loggers',        N_("Loggers")),
    ('cookbook_ssl',           N_("SSL cookbook"))
]

def apply():
    # Modifications
    for k in CTK.post:
        CTK.cfg[k] = CTK.post[k]

    return {'ret': 'ok'}

class HostMatchWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)
        is_default = CTK.cfg.get_lowest_entry("vserver") == int(vsrv_num)

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Host names')))

        if is_default:
            notice  = CTK.Notice()
            notice += CTK.RawHTML(DEFAULT_HOST_NOTE)
            self += notice
            return

        table = CTK.PropsAuto (url_apply)
        modul = CTK.PluginSelector ('%s!match'%(pre), Cherokee.support.filter_available(VRULES), vsrv_num=vsrv_num)
        table.Add (_('Method'), modul.selector_widget, _(NOTE_MATCHING_METHOD), False)
        self += CTK.Indenter (table)
        self += modul


class BasicsWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)
        vsrv_nick  = CTK.cfg.get_val("%s!nick"%(pre), '')
        is_default = CTK.cfg.get_lowest_entry("vserver") == int(vsrv_num)

        # Server ID
        if not is_default:
            table = CTK.PropsAuto (url_apply)
            table.Add (_('Virtual Server nickname'), CTK.TextCfg('%s!nick'%(pre)), _(NOTE_NICKNAME))

            self += CTK.RawHTML ('<h2>%s</h2>' %(_('Server ID')))
            self += CTK.Indenter (table)

        # Paths
        table = CTK.PropsAuto (url_apply)
        table.Add (_('Document Root'),     CTK.TextCfg('%s!document_root'%(pre)),   _(NOTE_DOCUMENT_ROOT))
        table.Add (_('Directory Indexes'), CTK.TextCfg('%s!directory_index'%(pre)), _(NOTE_DIRECTORY_INDEX))

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Paths')))
        self += CTK.Indenter (table)

        # Network
        table = CTK.PropsAuto (url_apply)
        table.Add (_('Keep-alive'),      CTK.CheckCfg('%s!keepalive'%(pre), True), _(NOTE_KEEPALIVE))
        table.Add (_('Max Upload Size'), CTK.TextCfg('%s!post_max_len'%(pre), True), _(NOTE_MAX_UPLOAD_SIZE))

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Network')))
        self += CTK.Indenter (table)

        # Advanced Virtual Hosting
        table = CTK.PropsAuto (url_apply)
        modul = CTK.PluginSelector('%s!evhost'%(pre), Cherokee.support.filter_available(EVHOSTS))
        table.Add (_('Method'), modul.selector_widget, _(NOTE_EVHOST), False)

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Advanced Virtual Hosting')))
        self += CTK.Indenter (table)
        self += CTK.Indenter (modul)


class Render():
    def __call__ (self):
        # Parse request
        vsrv_num = re.findall (r'^%s/([\d]+)'%(URL_BASE), CTK.request.url)[0]
        vsrv_nam = CTK.cfg.get_val ("vserver!%s!nick" %(vsrv_num), _("Unknown"))

        # Tabs
        tabs = CTK.Tab()
        tabs.Add (_('Basics'),     BasicsWidget (vsrv_num))
        tabs.Add (_('Host Match'), HostMatchWidget (vsrv_num))

        # Instance Page
        title = '%s: %s'%(_('Virtual Server'), vsrv_nam)

        page = Page.Base (title, helps=HELPS)
        page += CTK.RawHTML ("<h1>%s</h1>" % (title))
        page += tabs

        return page.Render()


CTK.publish (r'^%s/[\d]+'%(URL_BASE), Render)
CTK.publish (r'^%s/[\d]+'%(URL_APPLY), apply, validations=VALIDATIONS, method="POST")
