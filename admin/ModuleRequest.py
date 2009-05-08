from Form import *
from Table import *
from Module import *
import validations

# For gettext
N_ = lambda x: x

NOTE_REQUEST = N_("Regular expression against which the request will be executed.")

class ModuleRequest (Module, FormHelper):
    validation = [('tmp!new_rule!value', validations.is_regex)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'request', cfg)
        Module.__init__ (self, 'request', cfg, prefix, submit_url)

    def _op_render (self):
        table = TableProps()
        if self._prefix.startswith('tmp!'):
            self.AddPropEntry (table, _('Regular Expression'), '%s!value'%(self._prefix), _(NOTE_REQUEST))
        else:
            self.AddPropEntry (table, _('Regular Expression'), '%s!request'%(self._prefix), _(NOTE_REQUEST))
        return str(table)

    def _op_apply_changes (self, uri, post):
        self.ApplyChangesPrefix (self._prefix, None, post)

    def apply_cfg (self, values):
        if not values.has_key('value'):
            print _("ERROR, a 'value' entry is needed!")

        exts = values['value']
        self._cfg['%s!request'%(self._prefix)] = exts

    def get_name (self):
        return self._cfg.get_val ('%s!request'%(self._prefix))

    def get_type_name (self):
        return _('Regular Expression')
