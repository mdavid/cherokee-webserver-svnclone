from Form import *
from Table import *
from Module import *
import validations

METHODS = [
    ('get',         'GET'),
    ('post',        'POST'),
    ('head',        'HEAD'),
    ('put',         'PUT'),
    ('options',     'OPTIONS'),
    ('delete',      'DELETE'),
    ('trace',       'TRACE'),
    ('connect',     'CONNECT'),
    ('copy',        'COPY'),
    ('lock',        'LOCK'),
    ('mkcol',       'MKCOL'),
    ('move',        'MOVE'),
    ('notify',      'NOTIFY'),
    ('poll',        'POLL'),
    ('propfind',    'PROPFIND'),
    ('proppatch',   'PROPPATCH'),
    ('search',      'SEARCH'),
    ('subscribe',   'SUBSCRIBE'),
    ('unlock',      'UNLOCK'),
    ('unsubscribe', 'UNSUBSCRIBE')
]



from Form import *
from Table import *
from Module import *
import validations

class ModuleMethod (Module, FormHelper):
    validation = []

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'method', cfg)
        Module.__init__ (self, 'method', cfg, prefix, submit_url)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["method", "%s", "%s"]);'%(self.get_group(), 
                                                            self.get_condition(), 
                                                            self.get_name())
        return txt

    def _rule_def (self):
        _desc       = N_('HTTP Method')
        _is         = _("is")
        _is_hint    = _("The HTTP method that should match this rule.")
        _isnot      = _("is not")
        _isnot_hint    = _("The HTTP method that should noth match this rule.")

        _choices = "{"
        for c in METHODS:
            _choices += '"%s": "%s",' %(c[0], c[1])
        _choices = _choices[:-1]
        _choices += "}"

        txt = """
        cherokeeRules["method"] = {
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
        return _("HTTP Method")

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
