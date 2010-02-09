import validations

from Table import *
from ModuleHandler import *

# For gettext
N_ = lambda x: x

NOTE_SCRIPT_ALIAS     = N_('Path to an executable that will be run with the CGI as parameter.')
NOTE_CHANGE_USER      = N_('Execute the CGI under its file owner user ID.')
NOTE_ERROR_HANDLER    = N_('Send errors exactly as they are generated.')
NOTE_CHECK_FILE       = N_('Check whether the file is in place.')
NOTE_PASS_REQ         = N_('Forward all the client headers to the CGI encoded as HTTP_*. headers.')
NOTE_XSENDFILE        = N_('Allow the use of the non-standard X-Sendfile header.')
NOTE_X_REAL_IP        = N_('Whether the handler should read and use the X-Real-IP header and use it in REMOTE_ADDR.')
NOTE_X_REAL_IP_ALL    = N_('Accept all the X-Real-IP headers. WARNING: Turn it on only if you are centain of what you are doing.')
NOTE_X_REAL_IP_ACCESS = N_('List of IP addresses and subnets that are allowed to send the X-Real-IP header.')


DATA_VALIDATION = [
    ('vserver!.+?!rule!.+?!handler!script_alias', validations.is_path),
]

HELPS = [
    ('modules_handlers_cgi', "CGIs")
]

class ModuleCgiBase (ModuleHandler):
    PROPERTIES = [
        'script_alias',
        'change_user',
        'error_handler',
        'check_file',
        'pass_req_headers',
        'xsendfile',
        'env',
        'x_real_ip_enabled',
        'x_real_ip_access_all',
        'x_real_ip_access'
    ]

    def __init__ (self, cfg, prefix, name, submit_url):
        ModuleHandler.__init__ (self, name, cfg, prefix, submit_url)

        self.show_script_alias = True
        self.show_change_uid   = True

    def _op_render (self):
        txt   = "<h2>%s</h2>" % (_('Common CGI options'))

        table = TableProps()
        if self.show_script_alias:
            self.AddPropEntry (table, _("Script Alias"),  "%s!script_alias" % (self._prefix), _(NOTE_SCRIPT_ALIAS), optional=True)
        if self.show_change_uid:
            self.AddPropCheck (table, _("Change UID"), "%s!change_user"%(self._prefix), False, _(NOTE_CHANGE_USER))

        self.AddPropCheck (table, _("Error handler"),        "%s!error_handler"% (self._prefix),     True,  _(NOTE_ERROR_HANDLER))
        self.AddPropCheck (table, _("Check file"),           "%s!check_file"   % (self._prefix),     True,  _(NOTE_CHECK_FILE))
        self.AddPropCheck (table, _("Pass Request Headers"), "%s!pass_req_headers" % (self._prefix), True,  _(NOTE_PASS_REQ))
        self.AddPropCheck (table, _("Allow X-Sendfile"),     "%s!xsendfile" % (self._prefix),        False, _(NOTE_XSENDFILE))

        # X-Real-IP
        x_real_ip     = int(self._cfg.get_val('%s!x_real_ip_enabled' %(self._prefix), "0"))
        x_real_ip_all = int(self._cfg.get_val('%s!x_real_ip_access_all' %(self._prefix), "0"))

        self.AddPropCheck (table, _('Read X-Real-IP'), '%s!x_real_ip_enabled'%(self._prefix), False, _(NOTE_X_REAL_IP))
        if x_real_ip:
            self.AddPropCheck (table, _('Don\'t check origin'), '%s!x_real_ip_access_all'%(self._prefix), False, _(NOTE_X_REAL_IP_ALL))
            if not x_real_ip_all:
                self.AddPropEntry (table, _('Accept from Hosts'), '%s!x_real_ip_access'%(self._prefix), _(NOTE_X_REAL_IP_ACCESS))

        txt += self.Indent(table)

        txt1 = '<h2>%s</h2>' % (_('Custom environment variables'))
        envs = self._cfg.keys('%s!env'%(self._prefix))
        if envs:
            table = Table(3, title_left=1, style='width="90%"')

            for env in envs:
                pre = '%s!env!%s'%(self._prefix,env)
                val = self.InstanceEntry(pre, 'text', size=25)
                link_del = self.AddDeleteLink ('/ajax/update', pre)
                table += (env, val, link_del)

            txt1 += self.Indent(table)

        txt1 += '<h3>%s</h3>' % (_('Add new custom environment variable'));
        name  = self.InstanceEntry('new_custom_env_name',  'text', size=25, noautosubmit=True)
        value = self.InstanceEntry('new_custom_env_value', 'text', size=25, noautosubmit=True)

        table = Table(3, 1, style='width="90%"')
        table += (_('Name'), _('Value'), '')
        table += (name, value, SUBMIT_ADD)
        txt1 += self.Indent(table)
        txt += txt1

        return txt

    def _op_apply_changes (self, uri, post):
        new_name = post.pop('new_custom_env_name')
        new_value = post.pop('new_custom_env_value')

        if new_name and new_value:
            self._cfg['%s!env!%s'%(self._prefix, new_name)] = new_value

        checkboxes = ['error_handler', 'pass_req_headers', 'xsendfile',
                      'change_user', 'check_file', 'x_real_ip_enabled',
                      'x_real_ip_access_all']

        self.ApplyChangesPrefix (self._prefix, checkboxes, post, DATA_VALIDATION)

class ModuleCgi (ModuleCgiBase):
    def __init__ (self, cfg, prefix, submit_url):
        ModuleCgiBase.__init__ (self, cfg, prefix, 'cgi', submit_url)

    def _op_render (self):
        return ModuleCgiBase._op_render (self)

    def _op_apply_changes (self, uri, post):
        return ModuleCgiBase._op_apply_changes (self, uri, post)