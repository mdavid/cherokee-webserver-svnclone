from Form import *
from Table import *
from Module import *
import validations

class ModuleRequest (Module, FormHelper):
    validation = [('tmp!new_rule!value', validations.is_regex)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'request', cfg)
        Module.__init__ (self, 'request', cfg, prefix, submit_url)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["request", "%s", "%s"]);'%(self.get_group(), 
                                                          self.get_condition(), 
                                                          self.get_name())
        return txt

    def _rule_def (self):
        _desc = N_('Request')
        _contains = _("contains")
        _contains_hint = _("Request contains the given string.")
        _doesnotcontains = _("does not contains")
        _doesnotcontains_hint = _("Request does not contains the given string.")
        _begins = _("begins with")
        _begins_hint = _("Request begins with the given string.")
        _ends = _("ends with")
        _ends_hint = _("Request ends with the given string.")
        _is = _("is equals to")
        _is_hint = _("Request is the given string.")
        _regexp = _("match regular expression")
        _regexp_hint = _("Request match the given regular expression.")
        txt = """
        cherokeeRules["request"] = {
            "desc": "%(_desc)s",
            "conditions": {
                "contains": {
                    "d": "%(_contains)s",
                    "h": "%(_contains_hint)s"
                },
                "doesnotcontains": {
                    "d": "%(_doesnotcontains)s",
                    "h": "%(_doesnotcontains_hint)s"
                },
                "begins": {
                    "d": "%(_begins)s",
                    "h": "%(_begins_hint)s"
                },
                "ends": {
                    "d": "%(_ends)s",
                    "h": "%(_ends_hint)s"
                },
                "is": {
                    "d": "%(_is)s",
                    "h": "%(_is_hint)s"
                },
                "regexp": {
                    "d": "%(_regexp)s",
                    "h": "%(_regexp_hint)s"
                }
            },
            "field": {
                "type": "entry",
                "value": ""
            }
        }
        """ % (locals())
        return txt

    def _op_apply_changes (self, uri, post):
        self.ApplyChangesPrefix (self._prefix, None, post)

    def apply_cfg (self, values):
        if not values.has_key('value'):
            print _("ERROR, a 'value' entry is needed!")

        exts = values['value']
        self._cfg['%s!request'%(self._prefix)] = exts

    def get_group (self):
        return self._prefix.split('!')[-2]

    def get_rule_pos (self):
        return int(self._prefix.split('!')[-1])

    def get_condition (self):
        return self._cfg.get_val ('%s!cond'%(self._prefix))

    def get_name (self):
        return self._cfg.get_val ('%s!val'%(self._prefix))

    def get_type_name (self):
        return _('Request')

    def get_rule_condition_name (self):
        cond = self.get_condition()


        if cond == 'contains':
            condTxt = _("contains")
        elif cond == 'doesnotcontains':
            condTxt = _("does not contains")
        elif cond == 'begins':
            condTxt = _("begins with")
        elif cond == 'ends':
            condTxt = _("ends with")
        elif cond == 'is':
            condTxt = _("is equals to")
        elif cond == 'regexp':
            condTxt = _("match regular expression")
        else:
            condTxt = _("Undefined..")

        txt = "%s %s %s" % (self.get_type_name(), condTxt, self.get_name())
        return txt

