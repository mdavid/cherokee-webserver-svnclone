import validations

from Page import *
from Form import *
from Table import *
from Entry import *
from RuleList import *
from Module import *
from consts import *
from Rule import *

# For gettext
N_ = lambda x: x

NOTE_DOCUMENT_ROOT   = N_('Allows to specify an alternative document root path.')
NOTE_HANDLER         = N_('How the connection will be handled.')
NOTE_HTTPS_ONLY      = N_('Enable to allow access to the resource only by https.')
NOTE_ALLOW_FROM      = N_('List of IPs and subnets allowed to access the resource.')
NOTE_VALIDATOR       = N_('Which, if any, will be the authentication method.')
NOTE_EXPIRATION      = N_('Points how long the files should be cached')
NOTE_RATE            = N_("Set an outbound traffic limit. It must be specified in Bytes per second.")
NOTE_EXPIRATION_TIME = N_("""How long from the object can be cached.<br />
The <b>m</b>, <b>h</b>, <b>d</b> and <b>w</b> suffixes are allowed for minutes, hours, days, and weeks. Eg: 2d.
""")

DATA_VALIDATION = [
    ("vserver!.*?!rule!(\d+)!document_root",  (validations.is_dev_null_or_local_dir_exists, 'cfg')),
    ("vserver!.*?!rule!(\d+)!allow_from",      validations.is_ip_or_netmask_list),
    ("vserver!.*?!rule!(\d+)!rate",            validations.is_number_gt_0),
    ("vserver!.*?!rule!(\d+)!expiration!time", validations.is_time) 
]

HELPS = [
    ('config_virtual_servers_rule', N_("Behavior rules"))
]

class PageEntry (PageMenu, FormHelper):
    def __init__ (self, cfg):
        PageMenu.__init__ (self, 'entry', cfg, HELPS)
        FormHelper.__init__ (self, 'entry', cfg)
        self._priorities = None
        self._is_userdir = False

    def _op_render (self):
        raise "no"

    def _parse_uri (self, uri):
        assert (len(uri) > 1)
        assert ("rule" in uri)

        # Parse the URL
        temp = uri.split('/')
        self._is_userdir = (temp[2] == 'userdir')

        if self._is_userdir:
            self._host        = temp[1]
            self._prio        = temp[4]
            self._priorities  = RuleList (self._cfg, 'vserver!%s!user_dir!rule'%(self._host))
            self._entry       = self._priorities[int(self._prio)]
            url = '/vserver/%s/userdir/rule/%s' % (self._host, self._prio)
        else:
            self._host        = temp[1]
            self._prio        = temp[3]
            self._priorities  = RuleList (self._cfg, 'vserver!%s!rule'%(self._host))
            self._entry       = self._priorities[int(self._prio)]
            url = '/vserver/%s/rule/%s' % (self._host, self._prio)

        # Set the submit URL
        self.set_submit_url (url)

    def _op_handler (self, uri, post):
        # Parse the URI
        self._parse_uri (uri)

        # Entry not found
        if not self._entry:
            return "/vserver/%s" % (self._host)

        if not self._is_userdir:
            self._conf_prefix = 'vserver!%s!rule!%s' % (self._host, self._prio)
        else:
            self._conf_prefix = 'vserver!%s!user_dir!rule!%s' % (self._host, self._prio)

        # Check what to do..
        if post.get_val('is_submit'):
            self._op_apply_changes (uri, post)

        return self._op_default (uri)

    def _op_apply_changes (self, uri, post):
        # Handler properties
        pre = "%s!handler" % (self._conf_prefix)
        self.ApplyChanges_OptionModule (pre, uri, post)

        # Validator properties
        pre = "%s!auth" % (self._conf_prefix)
        self.ApplyChanges_OptionModule (pre, uri, post)

        # Check boxes
        checks = ["%s!only_secure"%(self._conf_prefix)]
        for e,e_name in modules_available(ENCODERS):
            checks.append ('%s!encoder!%s' % (self._conf_prefix, e))

        pre = '%s!match'%(self._conf_prefix)
        rule = Rule (self._cfg, pre, self.submit_url, 0)
        checks += rule.get_checks()

        # Apply changes
        self.ApplyChanges (checks, post, DATA_VALIDATION)

    def _op_default (self, uri):
        # Render page
        title   = self._get_title()
        content = self._render_guts()

        self.AddMacroContent ('title',   title)
        self.AddMacroContent ('content', content)
        return Page.Render(self)

    def _get_title (self, html=False):
        nick = self._cfg.get_val ('vserver!%s!nick'%(self._host))

        if html:
            txt = '<a href="/vserver/%s">%s</a> - ' % (self._host, nick)
        else:
            txt = '%s - ' % (nick)

        # Load the rule plugin
        pre = "%s!match"%(self._conf_prefix)
        rule = Rule (self._cfg, pre, self.submit_url)

        txt += rule.get_title()
        return txt

    def _render_guts (self):
        pre  = self._conf_prefix
        tabs = []

        # Rule Properties
        tabs += [(_('Rule'), self._render_rule())]

        # Handler
        table = TableProps()
        e = self.AddPropOptions_Reload (table, _('Handler'), '%s!handler'%(pre),
                                        modules_available(HANDLERS), NOTE_HANDLER)

        props = self._get_handler_properties()
        if props and props.show_document_root:
            self.AddPropEntry (table, _('Document Root'), '%s!document_root'%(pre), NOTE_DOCUMENT_ROOT)

        if e:
            tabs += [(_('Handler'), str(table) + e)]
        else:
            tabs += [(_('Handler'), str(table))]

        self.AddHelps (module_get_help (self._cfg.get_val('%s!handler'%(pre))))

        # Encoding
        tabs += [(_('Encoding'), self._render_encoding())]

        # Expiration
        tabs += [(_('Expiration'), self._render_expiration())]

        # Security
        tabs += [(_('Security'), self._render_security())]

        # Trafic Shaping
        tabs += [(_('Traffic Shaping'), self._render_traffic_shaping())]

        txt  = '<h1>%s</h1>' % (self._get_title (html=True))
        txt += self.InstanceTab (tabs)
        form = Form (self.submit_url, add_submit=False)
        return form.Render(txt)

    def _get_handler_properties (self):
        pre  = "%s!handler" % (self._conf_prefix)
        name = self._cfg.get_val(pre)
        if not name:
            return None

        try:
            props = module_obj_factory (name, self._cfg, pre, self.submit_url)
        except IOError:
            return None
        return props

    def _render_rule (self):
        txt  = "<h2>%s</h2>" % (_('Matching Rule'))

        txt += '<script type="text/javascript">'
        txt += 'var cherokeeRules = new Array();'
        txt += 'var cherokeePre = "%s";' % (self._conf_prefix)
        
        for r in modules_available(RULES):
            module_name, label = r
            rule_module = module_obj_factory (module_name, self._cfg, '', self.submit_url)
            if '_rule_def' in dir(rule_module):
                txt += rule_module._rule_def()
     
        txt += '</script>'

        txt += '<script src="/static/js/rules.js" type="text/javascript"></script>'
        txt += '<div id="rule_all"></div>'
        txt += '<div id="rules_area"></div>'

        txt += '<script> $(document).ready(function () {'
        txt += 'disableSaveRules();'

        pre  = "%s!match"%(self._conf_prefix)
        rule = Rule (self._cfg, pre, self.submit_url)
        txt += rule._op_render()

        txt += 'enableSaveRules();'
        txt += '});';
        txt += '</script>'

        return txt

    def _render_traffic_shaping (self):
        txt = ''

        table = TableProps()
        self.AddPropEntry (table, _('Limit traffic to'), '%s!rate'%(self._conf_prefix), NOTE_RATE)

        txt += "<h2>%s</h2>" % (_('Traffic Shaping'))
        txt += self.Indent(table)
        return txt


    def _render_expiration (self):
        txt = ''
        pre = "%s!expiration"%(self._conf_prefix)

        table = TableProps()
        self.AddPropOptions_Ajax (table, _("Expiration"), pre, EXPIRATION_TYPE, NOTE_EXPIRATION)

        exp = self._cfg.get_val(pre)
        if exp == 'time':
            self.AddPropEntry (table, _('Time to expire'), '%s!time'%(pre), NOTE_EXPIRATION_TIME)

        txt += "<h2>%s</h2>" % (_('Content Expiration'))
        txt += self.Indent(table)
        return txt

    def _render_security (self):
        pre = self._conf_prefix

        self.AddHelp (('cookbook_authentication', _('Authentication')))

        txt   = "<h2>%s</h2>" % (_('Access Restrictions'))
        table = TableProps()
        self.AddPropCheck (table, _('Only https'), '%s!only_secure'%(pre), False, NOTE_HTTPS_ONLY)
        self.AddPropEntry (table, _('Allow From'),  '%s!allow_from' %(pre), NOTE_ALLOW_FROM)
        txt += self.Indent(table)

        txt += "<h2>%s</h2>" % (_('Authentication'))
        table = TableProps()
        e = self.AddPropOptions_Reload (table, _('Validation Mechanism'), '%s!auth'%(pre),
                                        modules_available(VALIDATORS), NOTE_VALIDATOR)
        txt += self.Indent (table)
        txt += e

        self.AddHelps (module_get_help (self._cfg.get_val('%s!auth'%(pre))))

        return txt

    def _render_encoding (self):
        txt = ''
        pre = "%s!encoder"%(self._conf_prefix)
        encoders = modules_available(ENCODERS)

        for e in encoders:
            self.AddHelp (('modules_encoders_%s'%(e[0]),
                           '%s encoder' % (_(e[1]))))

        txt += "<h2>%s</h2>" % (_('Information Encoders'))
        table = TableProps()
        for e,e_name in encoders:
            note = _("Use the %s encoder whenever the client requests it.")
            note = note % (_(e_name))
            self.AddPropCheck (table, _("Allow") + " " + (_(e_name)), "%s!%s"%(pre,e), False, note)

        txt += self.Indent(table)
        return txt
