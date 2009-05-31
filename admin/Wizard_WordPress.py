from config import *
from util import *
from Page import *
from Wizard import *
from Wizard_PHP import wizard_php_get_info
from Wizard_PHP import wizard_php_get_source_info

NOTE_SOURCES  = _("Path to the directory where the Wordpress source code is located. (Example: /usr/share/wordpress)")
NOTE_WEB_DIR  = _("Web directory where you want Wordpress to be accessible. (Example: /blog)")
NOTE_HOST     = _("Host name of the virtual host that is about to be created.")
ERROR_NO_SRC  = _("Does not look like a WordPress source directory.")
ERROR_NO_WEB  = _("A web directory must be provided.")
ERROR_NO_HOST = _("A host name must be provided.")

CONFIG_DIR = """
%(pre_rule_0)s!document_root = %(local_src_dir)s
%(pre_rule_0)s!match = directory
%(pre_rule_0)s!match!directory = %(web_dir)s
%(pre_rule_0)s!match!final = 0

%(pre_rule_1)s!match = and
%(pre_rule_1)s!match!final = 1
%(pre_rule_1)s!match!left = directory
%(pre_rule_1)s!match!left!directory = %(web_dir)s
%(pre_rule_1)s!match!right = exists
%(pre_rule_1)s!match!right!iocache = 1
%(pre_rule_1)s!match!right!match_any = 1
%(pre_rule_1)s!handler = file
%(pre_rule_1)s!handler!iocache = 1
"""

CONFIG_VSRV = """
%(pre_vsrv)s!document_root = %(local_src_dir)s
%(pre_vsrv)s!nick = %(host)s
%(pre_vsrv)s!directory_index = index.php,index.html

%(pre_rule_0)s!match = request
%(pre_rule_0)s!match!request = ^/$
%(pre_rule_0)s!handler = redir
%(pre_rule_0)s!handler!rewrite!1!substring = /index.php
%(pre_rule_0)s!handler!rewrite!1!show = 0

%(pre_rule_1)s!match = exists
%(pre_rule_1)s!match!iocache = 1
%(pre_rule_1)s!match!match_any = 1
%(pre_rule_1)s!match!match_only_files = 1
%(pre_rule_1)s!handler = file
%(pre_rule_1)s!handler!iocache = 1

%(pre_vsrv)s!rule!1!match = default
%(pre_vsrv)s!rule!1!handler = common
"""

SRC_PATHS = [
    "/usr/share/wordpress",        # Debian, Fedora
    "/var/www/*/htdocs/wordpress", # Gentoo
    "/srv/www/htdocs/wordpress"    # SuSE
]

class Wizard_Rules_WordPress (WizardPage):
    ICON = "wordpress.jpg"
    DESC = "Configures Wordpress inside a public web directory."
    
    def __init__ (self, cfg, pre):
        WizardPage.__init__ (self, cfg, pre, 
                             submit = '/vserver/%s/wizard/WordPress'%(pre.split('!')[1]),
                             id     = "WordPress_Page1",
                             title  = _("Wordpress Wizard: Location"),
                             group  = WIZARD_GROUP_CMS)

    def show (self):
        # Check for PHP
        php_info = wizard_php_get_info (self._cfg, self._pre)
        if not php_info:
            self.no_show = "PHP support is required."
            return False
        return True

    def _render_content (self, url_pre):        
        guessed_src = path_find_w_default (SRC_PATHS)

        table = TableProps()
        self.AddPropEntry (table, _('Source Directory'),'tmp!wizard_wp!sources', NOTE_SOURCES, value=guessed_src)
        self.AddPropEntry (table, _('Web Directory'),   'tmp!wizard_wp!web_dir', NOTE_WEB_DIR, value="/blog")

        txt  = '<h1>%s</h1>' % (self.title)
        txt += self.Indent(table)
        form = Form (url_pre, add_submit=True, auto=False)
        return form.Render(txt, DEFAULT_SUBMIT_VALUE)

    def _op_apply (self, post):
        # Validation
        self.Validate_NotEmpty (post, 'tmp!wizard_wp!sources', ERROR_NO_SRC)
        self.Validate_NotEmpty (post, 'tmp!wizard_wp!web_dir', ERROR_NO_WEB)

        local_src_dir = post.get_val('tmp!wizard_wp!sources', '')
        if not os.path.exists (os.path.join (local_src_dir, "wp-login.php")):
            self.errors['tmp!wizard_wp!sources'] = (ERROR_NO_SRC, local_src_dir)

        if self.has_errors(): return

        # Incoming info
        local_src_dir = post.pop('tmp!wizard_wp!sources')
        web_dir       = post.pop('tmp!wizard_wp!web_dir')

        # Replacement
        php_info = wizard_php_get_info (self._cfg, self._pre)
        php_rule = int (php_info['rule'].split('!')[-1])

        pre_rule_0 = "%s!rule!%d" % (self._pre, php_rule - 1)
        pre_rule_1 = "%s!rule!%d" % (self._pre, php_rule - 2)

        # Add the new rules
        config = CONFIG_DIR % (locals())
        self._apply_cfg_chunk (config)


class Wizard_VServer_WordPress (WizardPage):
    ICON = "wordpress.jpg"
    DESC = "Configure a new virtual server using Wordpress."

    def __init__ (self, cfg, pre):
        WizardPage.__init__ (self, cfg, pre,
                             submit = '/vserver/wizard/WordPress',
                             id     = "WordPress_Page1",
                             title  = _("WordPress Wizard"),
                             group  = WIZARD_GROUP_CMS)

    def show (self):
        return True
        
    def _render_content (self, url_pre):        
        txt = '<h1>%s</h1>' % (self.title)
        guessed_src = path_find_w_default (SRC_PATHS)

        table = TableProps()
        self.AddPropEntry (table, _('Source Directory'), 'tmp!wizard_wp!sources', NOTE_SOURCES, value=guessed_src)
        self.AddPropEntry (table, _('New Host Name'),    'tmp!wizard_wp!host',    NOTE_HOST,    value="blog.example.com")
        txt += self.Indent(table)

        txt += '<h2>Logging</h2>'
        txt += self._common_add_logging()

        form = Form (url_pre, add_submit=True, auto=False)
        return form.Render(txt, DEFAULT_SUBMIT_VALUE)

    def _op_apply (self, post):
        # Validation
        self.Validate_NotEmpty (post, 'tmp!wizard_wp!sources', ERROR_NO_SRC)
        self.Validate_NotEmpty (post, 'tmp!wizard_wp!host',    ERROR_NO_HOST)

        local_src_dir = post.get_val('tmp!wizard_wp!sources', '')
        if not os.path.exists (os.path.join (local_src_dir, "wp-login.php")):
            self.errors['tmp!wizard_wp!sources'] = (ERROR_NO_SRC, local_src_dir)

        if self.has_errors(): return

        # Incoming info
        local_src_dir = post.pop('tmp!wizard_wp!sources')
        host          = post.pop('tmp!wizard_wp!host')
        pre_vsrv      = cfg_vsrv_get_next (self._cfg)
        
        # Add PHP Rule
        from Wizard_PHP import Wizard_Rules_PHP
        php_wizard = Wizard_Rules_PHP (self._cfg, pre_vsrv)
        php_wizard.show()
        php_wizard.run (pre_vsrv, None)

        # Replacement
        php_info = wizard_php_get_info (self._cfg, pre_vsrv)
        php_rule = int (php_info['rule'].split('!')[-1])

        pre_rule_0 = "%s!rule!%d" % (pre_vsrv, php_rule - 1)
        pre_rule_1 = "%s!rule!%d" % (pre_vsrv, php_rule - 2)

        # Add the new rules    
        config = CONFIG_VSRV % (locals())
        self._apply_cfg_chunk (config)
        self._common_apply_logging (post, pre_vsrv)
