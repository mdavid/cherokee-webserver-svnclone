from Form import *
from Table import *
from Module import *
import validations

NOTE_REHOST = "Regular Expression against which the hosts be Host name will be compared."

class ModuleRehost (Module, FormHelper):
    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'rehost', cfg)
        Module.__init__ (self, 'rehost', cfg, prefix, submit_url)

    def _op_render (self):
        txt = ''
        pre = '%s!regex'%(self._prefix)
        cfg_domains = self._cfg[pre]

        available = "1"

        if cfg_domains and \
           cfg_domains.has_child():
            table = Table(2,1)
            table += ('Regular Expressions', '')

            # Build list
            for i in cfg_domains:
                domain = cfg_domains[i].value
                cfg_key = "%s!%s" % (pre, i)
                en = self.InstanceEntry (cfg_key, 'text')
                js = "post_del_key('/ajax/update','%s');" % (cfg_key)
                link_del = self.InstanceImage ("bin.png", "Delete", border="0", onClick=js)
                table += (en, link_del)
                
            txt += "<h2>Regular Expressions</h2>"
            txt += self.Indent(table)
            txt += "<br />"

        # Look for firs available
        i = 1
        while cfg_domains:
            if not cfg_domains[str(i)]:
                available = str(i)
                break
            i += 1

        table = TableProps()
        self.AddPropEntry (table, 'New Regular Expression', '%s!%s'%(pre, available), NOTE_REHOST)
        txt += "<h3>Add new</h3>"
        txt += self.Indent(table)

        return txt
