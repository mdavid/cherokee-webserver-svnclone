from Form import *
from Table import *
from Module import *
import validations

class ModuleExists (Module, FormHelper):
    validation = [('tmp!new_rule!value', validations.is_safe_id_list)]

    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'exists', cfg)
        Module.__init__ (self, 'exists', cfg, prefix, submit_url)

        # Special case: there is a check in the rule
        self.checks = ['%s!iocache'%(self._prefix),
                       '%s!match_any'%(self._prefix)]

    def _op_render (self):
        table = TableProps()
        if self._prefix.startswith('tmp!'):
            self.AddPropEntry (table, _('Files'), '%s!value'%(self._prefix), NOTE_EXISTS)
        else:
            self.AddPropCheck (table, _('Match any file'), '%s!match_any'%(self._prefix), False, NOTE_ANY)
            if not int(self._cfg.get_val ('%s!match_any'%(self._prefix), '0')):
                self.AddPropEntry (table, _('Files'), '%s!exists'%(self._prefix), NOTE_EXISTS)
            self.AddPropCheck (table, _('Use I/O cache'), '%s!iocache'%(self._prefix), False, NOTE_IOCACHE)
        return str(table)

    def _op_render (self):
        txt = 'addRule(%s, 0, ["%s", "%s", "%s"]);'%(self.get_group(), 
                                                     self._cfg.get_val(self._prefix), 
                                                     self.get_condition(), 
                                                     self.get_name())
        return txt

    def _rule_def (self):
        _exists_any            = _("Any file exists")
        _exists_any_hint       = _("Match the request if any file exists")
        _exists_any_these      = _("Any of these files exists")
        _exists_any_hint       = _("Match the request if any of the given files exist")
        _exists_none           = _("None of these files exists")
        _exists_none_hint      = _("Match the request if none of the given files exist")
        _exists_none_dont      = _("None of these files doesn't exist")
        _exists_none_dont_hint = _("Match the request if none of the given files doesn't exist")
        _exists_all            = _("All of these files exist")
        _exists_all_hint       = _("Match the request if all the given files exist")

        txt = """
        cherokeeRules["exists_any"] = {
            "desc": "%(_exists_any)s",
            "conditions": false,
            "field": false,
            "hint": "%(_exists_any_hint)s"
        };

        cherokeeRules["exists_any_these"] = {
            "desc": "%(_exists_any_these)s",
            "conditions": false,
            "field": {
                "type": "entry",
                "value": ""
            },
            "hint": "%(_exists_any_these)s"
        };

        cherokeeRules["exists_none"] = {
            "desc": "%(_exists_none)s",
            "conditions": false,
            "field": {
                "type": "entry",
                "value": ""
            },
            "hint": "%(_exists_none)s"
        };

        cherokeeRules["exists_none_dont"] = {
            "desc": "%(_exists_none_dont)s",
            "conditions": false,
            "field": {
                "type": "entry",
                "value": ""
            },
            "hint": "%(_exists_none_dont)s"
        };

        cherokeeRules["exists_all"] = {
            "desc": "%(_exists_all)s",
            "conditions": false,
            "field": {
               "type": "entry",
               "value": ""
            },
            "hint": "%(_exists_all_hint)s"
        };
        """ % (locals())
        return txt

    def _op_apply_changes (self, uri, post):
        self.ApplyChangesPrefix (self._prefix, None, post)

    def apply_cfg (self, values):
        if not values.has_key('value'):
            print _("ERROR, a 'value' entry is needed!")

        exts = values['value']
        self._cfg['%s!exists'%(self._prefix)] = exts

    def get_group (self):
        return self._prefix.split('!')[-2]

    def get_rule_pos (self):
        return int(self._prefix.split('!')[-1])

    def get_condition (self):
        return self._cfg.get_val ('%s!cond'%(self._prefix))

    def get_name (self):
        return self._cfg.get_val ('%s!val'%(self._prefix))


    def get_type_name (self):
        val = self._cfg.get_val(self._prefix)
        if val == 'exists_any': 
            valTxt = _("Any file exists")
        elif val == exists_any_these:
            valTxt = _("Any of these files exists")
        elif val == exists_none:
            valTxt = _("None of these files exists")
        elif val == exists_none_dont:
            valTxt = _("None of these files doesn't exist")
        elif val == exists_all:
            valTxt = _("All of these files exist")
        else:
            valTxt = _("Undefined..")
        return valTxt

    def get_rule_condition_name (self):
        if self._cfg.get_val(self._prefix) == 'exists_any':
            txt = '%s' % (self.get_type_name())
        else:
            txt = '%s: "%s"' % (self.get_type_name(), self.get_name())
        return txt

