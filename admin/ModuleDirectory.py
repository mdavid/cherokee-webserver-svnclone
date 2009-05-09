from Form import *
from Table import *
from Module import *
import validations

class ModuleDirectory (Module, FormHelper):
    validation = [('tmp!new_rule!value', validations.is_dir_formated)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'directory', cfg)
        Module.__init__ (self, 'directory', cfg, prefix, submit_url)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["directory", "%s", "%s"]);'%(self.get_group(), 
                                                            self.get_condition(), 
                                                            self.get_name())
        return txt

    def _rule_def (self):
        _desc       = N_('Directory')
        _is         = _("is")
        _is_hint    = _("Public Web Directory to which content the configuration will be applied.")
        _isnot      = _("is not")
        _isnot_hint = _("Public Web Directory to which content the configuration will not be applied.")
        txt = """
        cherokeeRules["directory"] = {
            "desc": "%(_desc)s",
            "conditions": {
                "is": {
                    "d": "%(_is)s",
                    "h": "%(_is_hint)s"
                    },
                "isnot": {
                    "d": "%(_isnot)s",
                    "h": "%(_isnot_hint)s"
                    }
            },
            "field": {
                "type": "entry",
                "value": ""
            }
        };
        """ % (locals())
        return txt

    def apply_cfg (self, values):
        if values.has_key('value'):
            dir_name = values['value']
            self._cfg['%s!directory'%(self._prefix)] = dir_name

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

    def get_rule_condition_name (self):
        cond = self.get_condition()

        if cond == 'is':
            condTxt = _("is")
        elif cond == 'isnot':
            condTxt = _("is not")
        else:
            condTxt = _("Undefined..")

        txt = "%s %s %s" % (self.get_type_name(), condTxt, self.get_name())
        return txt

