from Form import *
from Table import *
from Module import *

import validations

class ModuleBind (Module, FormHelper):
    validation = []

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'bind', cfg)
        Module.__init__ (self, 'bind', cfg, prefix, submit_url)

    def _build_option_bind_num (self, num):
        port = self._cfg.get_val ("server!bind!%s!port"%(num))
        inte = self._cfg.get_val ("server!bind!%s!interface"%(num))

        if inte:
            return (num, _("Port") + " %s (%s)"%(inte, port))
        return (num, _("Port") + " %s"%(port))

    def _op_render (self):
        txt = 'addRule(%s, 0, ["bind", "%s", "%s"]);'%(self.get_group(), 
                                                            self.get_condition(), 
                                                            self.get_name())
        return txt

    def _rule_def (self):
        cfg_key = '%s!bind'%(self._prefix)

        _desc       = N_('Incoming port')
        _is         = _("is")
        _is_hint    = _("Incoming port to which that match this rule")
        _isnot      = _("is not")
        _isnot_hint = _("Incoming port to which that not match this rule")

        ports = self._cfg.keys("server!bind")
        _choices = "{"
        if ports:
            for b in ports:
                tmp = self._build_option_bind_num (b)
                _choices += '"%s": "%s," ' %(tmp[0], tmp[1])
        _choices = _choices[:-1]
        _choices += "}"

        txt = """
        cherokeeRules["bind"] = {
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
                "type": "dropdown",
                "choices": %(_choices)s,
                "value": ""
            }
        };
        """ % (locals())
        return txt

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

