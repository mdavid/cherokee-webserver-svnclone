import copy

from Form import *
from Table import *
from Page import *
from Module import *
from consts import *
import validations

DEFAULT_RULE_WARNING = 'The default match ought not to be changed.'

class Rule (Module, FormHelper):
    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'rule', cfg)
        Module.__init__ (self, 'rule', cfg, prefix, submit_url)

    def get_checks (self):
        matcher = self._cfg.get_val(self._prefix)
        if matcher == 'not':
            r = Rule(self._cfg, '%s!right'%(self._prefix), self.submit_url, self.depth+1)
            return r.get_checks()
        elif matcher in ['or', 'and']:
            r1 = Rule(self._cfg, '%s!left'%(self._prefix), self.submit_url)
            r2 = Rule(self._cfg, '%s!right'%(self._prefix), self.submit_url)
            return r1.get_checks() + r2.get_checks()
        else:
            rule_module = module_obj_factory (matcher, self._cfg, self._prefix, self.submit_url)
            if 'checks' in dir(rule_module):
                return rule_module.checks
        return []

    def get_title (self):
        matcher = self._cfg.get_val(self._prefix)
        if not matcher:
            return _("Unknown")

        return 'TBD'

    def get_name (self):
        matcher = self._cfg.get_val(self._prefix)
        if matcher in ['not', 'and', 'or']:
            return self.get_title()
        rule_module = module_obj_factory (matcher, self._cfg, self._prefix, self.submit_url)
        name = rule_module.get_name()

        if not name:
            name = _("Undefined..")

        return name

    def get_type_name (self):
        matcher = self._cfg.get_val(self._prefix)
        if matcher in ['not', 'and', 'or']:
            return "Complex"
        rule_module = module_obj_factory (matcher, self._cfg, self._prefix, self.submit_url)
        return rule_module.get_type_name()

    def _op_render (self):
        txt = ""
        pre = self._prefix

        matcher = self._cfg.get_val(pre)

        # Special Case: Default rule
        if matcher == 'default':
            return self.Dialog (DEFAULT_RULE_WARNING, 'important-information')

        if matcher in ["or", "and"]:
            txt += "addGlobal('%s');"%(matcher)

        g = 0
        gpath = "%s!%s"%(pre,g)
        gmatch = self._cfg.get_val(gpath)
        while gmatch != None:
            r = 0
            txt += "addGroup(%s, '%s');"%(g, gmatch)
            rpath = "%s!%s!%s"%(pre,g,r)
            rmatch = self._cfg.get_val(rpath)
            while rmatch != None:
                if rmatch.startswith('exists_'):
                    rule_module = module_obj_factory ('exists', self._cfg, rpath, self.submit_url)
                else:
                    rule_module = module_obj_factory (rmatch, self._cfg, rpath, self.submit_url)
                txt += rule_module._op_render()
                r += 1
                rpath = "%s!%s!%s"%(pre,g,r)
                rmatch = self._cfg.get_val(rpath)
            g += 1
            gpath = "%s!%s"%(pre,g)
            gmatch = self._cfg.get_val(gpath)

        return txt


class RuleRender (Module, FormHelper):
    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'rule_render', cfg)
        Module.__init__ (self, 'rule_render', cfg, prefix, submit_url)

    def _op_render (self):
        txt = ""
        return txt


class RuleOp (PageMenu, FormHelper):
    def __init__ (self, cfg):
        FormHelper.__init__ (self, 'rule_op', cfg)
        PageMenu.__init__ (self, 'rule_op', cfg, [])

    def __move (self, old, new):
        val = self._cfg.get_val(old)
        tmp = copy.deepcopy (self._cfg[old])
        del (self._cfg[old])

        self._cfg[new] = val
        self._cfg.set_sub_node (new, tmp)

    def _op_handler (self, uri, post):
        prefix = post['prefix'][0]

        if uri == "/not":
            if self._cfg.get_val(prefix) == "not":
                # not(not(rule)) == rule
                self.__move ("%s!right"%(prefix), prefix)
            else:
                self.__move (prefix, "%s!right"%(prefix))
                self._cfg[prefix] = "not"

        elif uri == '/and':
            self.__move (prefix, "%s!left"%(prefix))
            self._cfg[prefix] = "and"
        elif uri == '/or':
            self.__move (prefix, "%s!left"%(prefix))
            self._cfg[prefix] = "or"

        elif uri == "/del":
            rule = self._cfg.get_val(prefix)
            if rule == "not":
                self.__move ("%s!right"%(prefix), prefix)
            elif rule in ['and', 'or']:
                self.__move ("%s!left"%(prefix), prefix)
            else:
                del(self._cfg[prefix])
            
        else:
            print "%s '%s'" % (_('ERROR: Unknown uri'), uri)

        return "ok"
