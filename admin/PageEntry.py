import validations

from Page import *
from Form import *
from Table import *
from Entry import *
from VirtualServer import *
from Module import *
from consts import *

DATA_VALIDATION = [
    ("vserver!.*?!(directory|extensions|request)!.*?!document_root", validations.is_local_dir_exists)
]

class PageEntry (PageMenu, FormHelper):
    def __init__ (self, cfg):
        PageMenu.__init__ (self, 'entry', cfg)
        FormHelper.__init__ (self, 'entry', cfg)
        self._priorities = None

    def _op_render (self):
        raise "no"

    def _op_handler (self, uri, post):
        assert (len(uri) > 1)
        assert ("prio" in uri)
        
        # Parse the URL
        temp = uri.split('/')

        self._host        = temp[1]
        self._prio        = temp[3]        
        self._priorities  = VServerEntries (self._host, self._cfg)
        self._entry       = self._priorities[self._prio]

        # Entry not found
        if not self._entry:
            return "/vserver/%s" % (self._host)

        self._conf_prefix = 'vserver!%s!%s!%s' % (self._host, self._entry[0], self._entry[1])

        # Check what to do..
        if uri.endswith('/update'):
            self._op_apply_changes (uri, post)
            if self.has_errors():
                return self._op_default (uri)

            parts = uri.split('/')
            prio_url = '/vserver/' + reduce (lambda x,y: '%s/%s'%(x,y), parts[:-1])
            return prio_url

        return self._op_default (uri)
    
    def _op_apply_changes (self, uri, post):
        # Handler properties
        pre  = "%s!handler" % (self._conf_prefix)
        name = self._cfg[pre].value

        props = module_obj_factory (name, self._cfg, pre)
        props._op_apply_changes (uri, post)

        # Apply changes
        self.ApplyChanges (["%s!only_secure"%(self._conf_prefix)], post, DATA_VALIDATION)

        # Clean old properties
        self.conf_props_clean_up ()

    def conf_props_clean_up (self):
        pre     = "%s!handler" % (self._conf_prefix)
        handler = self._cfg[pre].value
        module  = module_obj_factory (handler, self._cfg, pre)
        props   = module.__class__.PROPERTIES

        to_be_deleted = []
        for entry in self._cfg[pre]:
            prop_cfg = "%s!%s" % (pre, entry)
            if entry not in props:
                to_be_deleted.append (prop_cfg)

        for entry in to_be_deleted:
            del(self._cfg[entry])

    def _op_default (self, uri):
        # Render page
        title   = self._get_title()
        content = self._render_guts()

        self.AddMacroContent ('title',   title)
        self.AddMacroContent ('content', content)
        return Page.Render(self)

    def _get_title (self, html=False):
        if html:
            txt = '<a href="/vserver/%s">%s</a> - ' % (self._host, self._host)
        else:
            txt = '%s - ' % (self._host)

        for n in range(len(ENTRY_TYPES)):
            if ENTRY_TYPES[n][0] == self._entry[0]:
                txt += "%s: %s" % (ENTRY_TYPES[n][1], self._entry[1])
                return txt

    def _render_guts (self):
        pre = self._conf_prefix
        txt = '<h1>%s</h1>' % (self._get_title (html=True))

        table = Table(2)
        e = self.AddTableOptions_w_ModuleProperties (table, 'Handler', '%s!handler'%(pre), HANDLERS)
        self.AddTableEntry (table, 'Document Root', '%s!document_root'%(pre))
        self.AddTableCheckbox (table, 'Only Secure', '%s!only_secure'%(pre), False)

        txt += str(table)

        txt += '<h2>Handler properties</h2>'
        txt += e

        txt += '<h2>Authentication</h2>'
        txt += self._render_auth ()

        form = Form ('/vserver/%s/prio/%s/update' % (self._host, self._prio))
        return form.Render(txt)

    def _render_handler_properties (self):
        pre  = "%s!handler" % (self._conf_prefix)
        name = self._cfg[pre].value

        try:
            props = module_obj_factory (name, self._cfg, pre)
        except IOError:
            return "Couldn't load the properties module"
        return props._op_render()

    def _render_auth (self):
        pre = self._conf_prefix

        txt   = ""
        table = Table(2)
        e = self.AddTableOptions_w_ModuleProperties (table, 'Validator', '%s!auth'%(pre), VALIDATORS)
        txt += str(table) + e

        return txt

