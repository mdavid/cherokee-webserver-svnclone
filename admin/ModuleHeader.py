from Form import *
from Table import *
from Module import *
import validations

LENGHT_LIMIT = 10

class ModuleHeader (Module, FormHelper):
    validation = [('tmp!new_rule!match', validations.is_regex)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'header', cfg)
        Module.__init__ (self, 'header', cfg, prefix, submit_url)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["header", "%s", "%s"]);'%(self.get_group(), 
                                                         self.get_condition(), 
                                                         self.get_name())
        return txt

    def _rule_def (self):
        _desc = N_('Header')

        _accept_encoding = _("Accept-Encoding match regular expression")
        _accept_charset  = _("Accept-Charset match regular expression")
        _accept_language = _("Accept-Encoding match regular expression")
        _referer         = _("Referer match regular expression")
        _user_agent      = _("User-Agent match regular expression")
        _cookie          = _("Cookie match regular expression")

        _hint = _("Header match the given regular expression")
        txt = """
        cherokeeRules["header"] = {
            "desc": "%(_desc)s",
            "conditions": {
                "accept-encoding": {
                    "d": "%(_accept_encoding)s"
                    },
                "accept-charset": {
                    "d": "%(_accept_charset)s"
                    },
                "accept-language": {
                    "d": "%(_accept_language)s"
                    },
                "referer": {
                    "d": "%(_referer)s"
                    },
                "user-agent": {
                    "d": "%(_user_agent)s"
                    },
                "cookie": {
                    "d": "%(_cookie)s"
                    }
            },
            "field": {
                "type": "entry",
                "value": ""
            },
            "hint": "%(_hint)s"
        };
        """ % (locals())
        return txt
        
    def apply_cfg (self, values):
        if values.has_key('value'):
            header = values['value']
            self._cfg['%s!header'%(self._prefix)] = header

        if values.has_key('match'):
            match = values['match']
            self._cfg['%s!match'%(self._prefix)] = match

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

        if cond == 'accept-encoding':
            condTxt = _("Accept-Encoding match regular expression")
        elif cond == 'accept_charset':
            condTxt = _("Accept-Charset match regular expression")
        elif cond == 'accept-language': 
            condTxt = _("Accept-Encoding match regular expression")
        elif cond == 'referer':         
            condTxt = _("Referer match regular expression")
        elif cond == 'user-agent':      
            condTxt = _("User-Agent match regular expression")
        elif cond == 'cookie':          
            condTxt = _("Cookie match regular expression")
        else:
            condTxt = _("Undefined..")

        txt = '%s %s: "%s"' % (self.get_type_name(), condTxt, self.get_name())
        return txt

