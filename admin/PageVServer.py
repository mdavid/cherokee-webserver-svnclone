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
from Rule import Rule
from configured import *
from CTK.util import find_copy_name

URL_BASE  = '/vserver/content'
URL_APPLY = '/vserver/content/apply'

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


def validation_ca_list (text):
    "Empty or local_file_exists"
    if not text:
        return text
    text = validations.is_local_file_exists (text)
    return text

VALIDATIONS = [
    ("vserver![\d]+!user_dir",                   validations.is_safe_id),
    ("vserver![\d]+!document_root",              validations.is_dev_null_or_local_dir_exists),
    ("vserver![\d]+!post_max_len",               validations.is_positive_int),
    ("vserver![\d]+!ssl_certificate_file",       validations.is_local_file_exists),
    ("vserver![\d]+!ssl_certificate_key_file",   validations.is_local_file_exists),
    ("vserver![\d]+!ssl_ca_list_file",           validation_ca_list),
    ("vserver![\d]+!ssl_verify_depth",           validations.is_positive_int),
    ("vserver![\d]+!logger!.+?!filename",        validations.parent_is_dir),
    ("vserver![\d]+!logger!.+?!command",         validations.is_local_file_exists),
    ("vserver![\d]+!logger!x_real_ip_access$",   validations.is_ip_or_netmask_list),
]

HELPS = [
    ('config_virtual_servers', N_("Virtual Servers")),
    ('modules_loggers',        N_("Loggers")),
    ('cookbook_ssl',           N_("SSL cookbook"))
]


def commit_clone():
    num = re.findall(r'^%s/([\d]+)/clone$'%(URL_BASE), CTK.request.url)[0]
    next = CTK.cfg.get_next_entry_prefix ('vserver')

    orig  = CTK.cfg.get_val ('vserver!%s!nick'%(num))
    names = [CTK.cfg.get_val('vserver!%s!nick'%(x)) for x in CTK.cfg.keys('vserver')]
    new_nick = find_copy_name (orig, names)

    CTK.cfg.clone ('vserver!%s'%(num), next)
    CTK.cfg['%s!nick' %(next)] = new_nick
    return {'ret': 'ok'}


class HostMatchWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)
        is_default = CTK.cfg.get_lowest_entry("vserver") == int(vsrv_num)

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Host Names')))

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


class BehaviorWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        pre       = "vserver!%s" %(vsrv_num)
        url_apply = "%s/%s" %(URL_APPLY, vsrv_num)

        # List
        table = CTK.Table()
        table.set_header(1)
        table += [CTK.RawHTML(x) for x in (_('Match'), _('Handler'), _('Auth'), _('Final'))]

        rules = CTK.cfg.keys('vserver!%s!rule'%(vsrv_num))
        rules.sort (lambda x,y: cmp(int(x), int(y)))
        rules.reverse()

        for r in rules:
            rule = Rule ('vserver!%s!rule!%s!match'%(vsrv_num, r))
            rule_name = rule.GetName()
            link = CTK.Link ('/vserver/%s/rule/%s'%(vsrv_num, r), CTK.RawHTML (rule_name))

            handler = None
            tmp     = CTK.cfg.get_val ('vserver!%s!rule!%s!handler'%(vsrv_num, r), '')
            if tmp:
                handler = CTK.RawHTML (filter (lambda x: x[0] == tmp, HANDLERS)[0][1])

            auth = None
            tmp  = CTK.cfg.get_val ('vserver!%s!rule!%s!auth'%(vsrv_num, r), '')
            if tmp:
                auth = CTK.RawHTML (filter (lambda x: x[0] == tmp, VALIDATORS)[0][1])

            final    = CTK.CheckCfg  ('vserver!%s!rule!%s!final'%(vsrv_num, r), True)
            disabled = CTK.iPhoneCfg ('vserver!%s!rule!%s!disabled'%(vsrv_num, r), False)

            table += [link, handler, auth, final, disabled]

        # Submit
        submit = CTK.Submitter (url_apply)
        submit += table

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Behavior Rules')))
        self += CTK.Indenter (submit)



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
        table.Add (_('Keep-alive'),      CTK.CheckCfgText('%s!keepalive'%(pre), True, _('Allow')), _(NOTE_KEEPALIVE))
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


class ErrorHandlerWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)

        table = CTK.PropsAuto (url_apply)
        modul = CTK.PluginSelector ('%s!error_handler'%(pre), Cherokee.support.filter_available(ERROR_HANDLERS), vsrv_num=vsrv_num)
        table.Add (_('Method'), modul.selector_widget, _(NOTE_ERROR_HANDLER), False)

        self += CTK.RawHTML ('<h2>%s</h2>' %(_('Error Handling hook')))
        self += CTK.Indenter (table)
        self += modul


class LogginWidgetContent (CTK.Container):
    def __init__ (self, vsrv_num, refreshable):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)
        writers    = [('', N_('Disabled'))] + LOGGER_WRITERS

        # Error writer
        table = CTK.PropsTable()
        table.Add (_('Write errors to'), CTK.ComboCfg('%s!error_writer!type'%(pre), writers), _(NOTE_ERRORS))

        writer = CTK.cfg.get_val ('%s!error_writer!type'%(pre))
        if writer == 'file':
            table.Add (_('Filename'), CTK.TextCfg('%s!error_writer!filename'%(pre)), _(NOTE_WRT_FILE))
        elif writer == 'exec':
            table.Add (_('Command'), CTK.TextCfg('%s!error_writer!command'%(pre)), _(NOTE_WRT_EXEC))

        submit = CTK.Submitter(url_apply)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit += table

        self += CTK.RawHTML ('<h2>%s</h2>' % (_('Error Logging')))
        self += CTK.Indenter (submit)

        # Access logger
        pre = 'vserver!%s!logger' %(vsrv_num)

        submit = CTK.Submitter(url_apply)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit += CTK.ComboCfg(pre, Cherokee.support.filter_available(LOGGERS))

        table = CTK.PropsTable()
        table.Add (_('Format'), submit, _(NOTE_LOGGERS))

        format = CTK.cfg.get_val(pre)
        if format:
            submit = CTK.Submitter(url_apply)
            submit.bind ('submit_success', refreshable.JS_to_refresh())
            submit += CTK.ComboCfg('%s!access!type'%(pre), LOGGER_WRITERS)
            table.Add (_('Write accesses to'), submit, _(NOTE_ACCESSES))

            submit = CTK.Submitter(url_apply)
            writer = CTK.cfg.get_val ('%s!access!type'%(pre))
            if writer == 'file' or not writer:
                submit += CTK.TextCfg('%s!access!filename'%(pre))
                table.Add (_('Filename'), submit, _(NOTE_WRT_FILE))
            elif writer == 'exec':
                submit += CTK.TextCfg('%s!access!command'%(pre))
                table.Add (_('Command'), submit, _(NOTE_WRT_EXEC))

            if format == 'custom':
                submit = CTK.Submitter(url_apply)
                submit += CTK.TextCfg('%s!access!access_template'%(pre))
                table.Add (_('Template: '), submit, _(NOTE_LOGGER_TEMPLATE))

        self += CTK.RawHTML ('<h2>%s</h2>' % (_('Access Logging')))
        self += CTK.Indenter (table)

        # Properties
        if CTK.cfg.get_val (pre):
            table = CTK.PropsTable()
            table.Add (_('Time standard'), CTK.ComboCfg ('%s!utc_time'%(pre), UTC_TIME), _(NOTE_UTC_TIME))
            table.Add (_('Accept Forwarded IPs'), CTK.CheckCfgText ('%s!x_real_ip_enabled'%(pre), False, _('Accept')), _(NOTE_X_REAL_IP))

            if int (CTK.cfg.get_val('%s!x_real_ip_enabled'%(pre), "0")):
                table.Add (_('Don\'t check origin'), CTK.CheckCfgText ('%s!x_real_ip_access_all'%(pre), False, _('Do not check')), _(NOTE_X_REAL_IP_ALL))

                if not int (CTK.cfg.get_val ('%s!x_real_ip_access_all'%(pre), "0")):
                    table.Add (_('Accept from Hosts'), CTK.TextCfg ('%s!x_real_ip_access'%(pre)), _(NOTE_X_REAL_IP_ACCESS))

            submit = CTK.Submitter(url_apply)
            submit.bind ('submit_success', refreshable.JS_to_refresh())
            submit += table

            self += CTK.RawHTML ('<h3>%s</h3>' % (_('Logging Option')))
            self += CTK.Indenter (submit)



class LogginWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        # Content
        refresh = CTK.Refreshable({'id': 'vserver_login'})
        refresh.register (lambda: LogginWidgetContent(vsrv_num, refresh).Render())
        self += refresh


class SecutiryWidgetContent (CTK.Container):
    def __init__ (self, vsrv_num, refreshable):
        CTK.Container.__init__ (self)

        pre        = "vserver!%s" %(vsrv_num)
        url_apply  = "%s/%s" %(URL_APPLY, vsrv_num)

        # Required SSL/TLS values
        table = CTK.PropsTable()
        table.Add (_('Certificate'),     CTK.TextCfg ('%s!ssl_certificate_file'%(pre), True), _(NOTE_CERT))
        table.Add (_('Certificate key'), CTK.TextCfg ('%s!ssl_certificate_key_file'%(pre), True), _(NOTE_CERT_KEY))

        submit = CTK.Submitter (url_apply)
        submit += table

        self += CTK.RawHTML ('<h2>%s</h2>' % (_('Required SSL/TLS Values')))
        self += CTK.Indenter (submit)

        # Advanced options
        table = CTK.PropsTable()
        table.Add (_('Ciphers'),               CTK.TextCfg ('%s!ssl_ciphers' %(pre), True), _(NOTE_CIPHERS))
        table.Add (_('Client Certs. Request'), CTK.ComboCfg('%s!ssl_client_certs' %(pre), CLIENT_CERTS), _(NOTE_CLIENT_CERTS))

        if CTK.cfg.get_val('%s!ssl_client_certs' %(pre)):
            table.Add (_('CA List'), CTK.TextCfg ('%s!ssl_ca_list_file' %(pre), False), _(NOTE_CA_LIST))

            if CTK.cfg.get_val('%s!ssl_ca_list_file' %(pre)):
                table.Add (_('Verify Depth'), CTK.TextCfg ('%s!ssl_verify_depth' %(pre), False, {'size': 4}), _(NOTE_VERIFY_DEPTH))

        submit = CTK.Submitter (url_apply)
        submit.bind ('submit_success', refreshable.JS_to_refresh())
        submit += table

        self += CTK.RawHTML ('<h2>%s</h2>' % (_('Advanced Options')))
        self += CTK.Indenter (submit)


class SecurityWidget (CTK.Container):
    def __init__ (self, vsrv_num):
        CTK.Container.__init__ (self)

        # Content
        refresh = CTK.Refreshable ({'id': 'vserver_security'})
        refresh.register (lambda: SecutiryWidgetContent(vsrv_num, refresh).Render())
        self += refresh


class Render():
    def __call__ (self):
        # Parse request
        vsrv_num = re.findall (r'^%s/([\d]+)'%(URL_BASE), CTK.request.url)[0]
        vsrv_nam = CTK.cfg.get_val ("vserver!%s!nick" %(vsrv_num), _("Unknown"))

        # Ensure the vserver exists
        if not CTK.cfg.keys ("vserver!%s"%(vsrv_num)):
            return CTK.HTTP_Redir("/vserver")

        # Tabs
        tabs = CTK.Tab()
        tabs.Add (_('Basics'),        BasicsWidget (vsrv_num))
        tabs.Add (_('Host Match'),    HostMatchWidget (vsrv_num))
        tabs.Add (_('Behavior'),      BehaviorWidget (vsrv_num))
        tabs.Add (_('Error Handler'), ErrorHandlerWidget (vsrv_num))
        tabs.Add (_('Logging'),       LogginWidget (vsrv_num))
        tabs.Add (_('Security'),      SecurityWidget (vsrv_num))

        cont = CTK.Container()
        cont += CTK.RawHTML ('<h2>%s: %s</h2>' %(_('Virtual Server'), vsrv_nam))
        cont += tabs

        return cont.Render().toStr()


CTK.publish (r'^%s/[\d]+$'      %(URL_BASE), Render)
CTK.publish (r'^%s/[\d]+$'      %(URL_APPLY), CTK.cfg_apply_post, validation=VALIDATIONS, method="POST")
CTK.publish ('^%s/[\d]+/clone$' %(URL_BASE), commit_clone)

