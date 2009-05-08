from Form import *
from Table import *
from Module import *
import validations

class ModuleExtensions (Module, FormHelper):
    validation = [('tmp!new_rule!value', validations.is_safe_id_list)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'extensions', cfg)
        Module.__init__ (self, 'extensions', cfg, prefix, submit_url)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["extensions", "%s", "%s"]);'%(self.get_group(), 
                                                             self.get_condition(), 
                                                             self.get_name())
        return txt

    def _rule_def (self):
        _desc         = N_('Extensions');
	_is           = _("is")
	_is_hint      = _("File extension to which content the configuration will be applied.")
        _isin         = _("is in")
        _isin_hint    = _("File extension list to which content the configuration will be applied.")
        _isnotin      = _("is not in")
        _isnotin_hint = _("File extension list to which content the configuration will not be applied.")

        txt = """
        cherokeeRules["extensions"] = {
            "desc": "%(_desc)s",
            "conditions": {
                "is": {
                    "d": "%(_is)s",
                    "h": "%(_is_hint)s"
                    },
                "isin": {
                    "d": "%(_isin)s",
                    "h": "%(_isin_hint)s"
                    },
                "isnotin": {
                    "d": "%(_isnotin)s",
                    "h": "%(_isnotin_hint)s"
                    }
            },
            "field": {
                "type": "entry",
                "value": ""
            }
        };
        """ % (locals())
        return txt

    def _op_apply_changes (self, uri, post):
        self.ApplyChangesPrefix (self._prefix, None, post)

    def apply_cfg (self, values):
        if not values.has_key('value'):
            print _("ERROR, a 'value' entry is needed!")

        exts = values['value']
        self._cfg['%s!extensions'%(self._prefix)] = exts

    def get_group (self):
        return self._prefix.split('!')[-2]

    def get_rule_pos (self):
        return int(self._prefix.split('!')[-1])

    def get_condition (self):
        return self._cfg.get_val ('%s!cond'%(self._prefix))

    def get_name (self):
        return self._cfg.get_val ('%s!val'%(self._prefix))

    def get_type_name (self):
        return self._id.capitalize()
