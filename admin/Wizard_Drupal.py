from config import *
from util import *
from Page import *
from Wizard import *
from Wizard_PHP import wizard_php_get_info
from Wizard_PHP import wizard_php_get_source_info

NOTE_SOURCES  = _("Path to the directory where the Drupal source code is located. (Example: /usr/share/drupal)")
NOTE_WEB_DIR  = _("Web directory where you want Drupal to be accessible. (Example: /blog)")
NOTE_HOST     = _("Host name of the virtual host that is about to be created.")
ERROR_NO_SRC  = _("Does not look like a Drupal source directory.")
ERROR_NO_WEB  = _("A web directory must be provided.")
ERROR_NO_HOST = _("A host name must be provided.")

CONFIG_DIR = """
%(pre_rule_plus2)s!handler = redir
%(pre_rule_plus2)s!handler!rewrite!1!show = 0
%(pre_rule_plus2)s!handler!rewrite!1!substring = %(web_dir)s/index.php
%(pre_rule_plus2)s!match = request
%(pre_rule_plus2)s!match!request = ^%(web_dir)s/$

%(pre_rule_plus1)s!document_root = %(local_src_dir)s
%(pre_rule_plus1)s!match = directory
%(pre_rule_plus1)s!match!directory = %(web_dir)s
%(pre_rule_plus1)s!match!final = 0

# IMPORTANT: The PHP rule comes here

%(pre_rule_minus1)s!match = and
%(pre_rule_minus1)s!match!left = directory
%(pre_rule_minus1)s!match!left!directory = %(web_dir)s
%(pre_rule_minus1)s!match!right = exists
%(pre_rule_minus1)s!match!right!iocache = 1
%(pre_rule_minus1)s!match!right!match_any = 1
%(pre_rule_minus1)s!handler = file

%(pre_rule_minus2)s!handler = redir
%(pre_rule_minus2)s!handler!rewrite!1!show = 0
%(pre_rule_minus2)s!handler!rewrite!1!regex = /(.*)\?(.*)$
%(pre_rule_minus2)s!handler!rewrite!1!substring = %(web_dir)s/index.php?q=/$1&$2
%(pre_rule_minus2)s!handler!rewrite!2!show = 0
%(pre_rule_minus2)s!handler!rewrite!2!regex = /(.*)$
%(pre_rule_minus2)s!handler!rewrite!2!substring = %(web_dir)s/index.php?q=/$1
%(pre_rule_minus2)s!match = directory
%(pre_rule_minus2)s!match!directory = %(web_dir)s
"""

SRC_PATHS = [
    "/usr/share/drupal6",       # Debian, Fedora
    "/usr/share/drupal5",
    "/usr/share/drupal",
    "/var/www/*/htdocs/drupal", # Gentoo
    "/srv/www/htdocs/drupal"    # SuSE
]

class Wizard_Rules_Drupal (WizardPage):
    ICON = "drupal.png"
    DESC = "Configures Drupal inside a public web directory."

    def __init__ (self, cfg, pre):
        WizardPage.__init__ (self, cfg, pre, 
                             submit = '/vserver/%s/wizard/Drupal'%(pre.split('!')[1]),
                             id     = "Drupal_Page1",
                             title  = _("Drupal Wizard"),
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
        self.AddPropEntry (table, _('Source Directory'),'tmp!wizard_drupal!sources', NOTE_SOURCES, value=guessed_src)
        self.AddPropEntry (table, _('Web Directory'),   'tmp!wizard_drupal!web_dir', NOTE_WEB_DIR, value="/blog")

        txt  = '<h1>%s</h1>' % (self.title)
        txt += self.Indent(table)
        form = Form (url_pre, add_submit=True, auto=False)
        return form.Render(txt, DEFAULT_SUBMIT_VALUE)

    def _op_apply (self, post):
        # Validation
        self.Validate_NotEmpty (post, 'tmp!wizard_drupal!sources', ERROR_NO_SRC)
        self.Validate_NotEmpty (post, 'tmp!wizard_drupal!web_dir', ERROR_NO_WEB)

        local_src_dir = post.get_val('tmp!wizard_drupal!sources', '')
        if not os.path.exists (os.path.join (local_src_dir, "includes/module.inc")):
            self.errors['tmp!wizard_drupal!sources'] = (ERROR_NO_SRC, local_src_dir)

        if self.has_errors(): return

        # Incoming info
        local_src_dir = post.pop('tmp!wizard_drupal!sources')
        web_dir       = post.pop('tmp!wizard_drupal!web_dir')

        # Replacement
        php_info = wizard_php_get_info (self._cfg, self._pre)
        php_rule = int (php_info['rule'].split('!')[-1])

        pre_rule_plus2  = "%s!rule!%d" % (self._pre, php_rule + 1)
        pre_rule_plus1  = "%s!rule!%d" % (self._pre, php_rule + 2)
        pre_rule_minus1 = "%s!rule!%d" % (self._pre, php_rule - 1)
        pre_rule_minus2 = "%s!rule!%d" % (self._pre, php_rule - 2)

        # Add the new rules
        config = CONFIG_DIR % (locals())
        self._apply_cfg_chunk (config)
