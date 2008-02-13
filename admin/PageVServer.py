import validations

from Page import *
from Form import *
from Table import *
from Entry import *
from consts import *
from VirtualServer import *

DATA_VALIDATION = [
    ("vserver!.*?!document_root", validations.is_local_dir_exists),
    ("vserver!.*?!(directory|extensions|request)!.*?!priority", validations.is_positive_int)
]

class PageVServer (PageMenu, FormHelper):
    def __init__ (self, cfg):
        PageMenu.__init__ (self, 'vserver', cfg)
        FormHelper.__init__ (self, 'vserver', cfg)
        self._priorities = None

    def _op_handler (self, uri, post):
        assert (len(uri) > 1)

        host = uri.split('/')[1]
        default_render = False

        if uri.endswith('/update'):
            self._op_apply_changes (post)
            if not self.has_errors():
                return "/%s/%s" % (self._id, host)
            default_render = True
        elif uri.endswith('/add_new_entry'):
            self._op_add_new_entry(host, post)
            return "/%s/%s" % (self._id, host)            
        elif uri.endswith('/remove'):
            if host != "default":
                del(self._cfg["vserver!%s" % (host)])
            return '/vserver'
        else:
            default_render = True

        if default_render:
            self._priorities = VServerEntries (host, self._cfg)
            return self._op_render_vserver_details (host, uri[len(host)+1:])

    def _op_add_new_entry (self, host, post):
        entry    = post['add_new_entry'][0]
        type     = post['add_new_type'][0]
        handler  = post['add_new_handler'][0]
        priority = post['add_new_priority'][0]

        pre = "vserver!%s!%s!%s" % (host, type, entry)
        self._cfg["%s!handler"%(pre)]  = handler
        self._cfg["%s!priority"%(pre)] = priority

    def _op_render_vserver_details (self, host, uri):
        content = self._render_vserver_guts (host)

        self.AddMacroContent ('title', 'Virtual Server: %s' %(host))
        self.AddMacroContent ('content', content)

        return Page.Render(self)

    def _render_vserver_guts (self, host):
        pre = "vserver!%s" % (host)
        cfg = self._cfg[pre]
        
        tabs = []
        txt = "<h1>Virtual Server: %s</h1>" % (host)

        # Basics
        table = Table(2)
        self.AddTableEntry (table, 'Document Root',   '%s!document_root' % (pre))
        self.AddTableEntry (table, 'Domains',         '%s!domain' % (pre))
        self.AddTableEntry (table, 'Directory Index', '%s!directory_index' % (pre))
        tabs += [('Basics', str(table))]

        # Logging
        tmp   = ''
        table = Table(2)
        props = {}

        t1 = Table(2)
        self.AddTableEntry (t1, 'Filename', '%s!logger!access!filename' % (pre))
        t2 = Table(2)
        self.AddTableEntry (t2, 'Command', '%s!logger!access!command' % (pre))
        props['stderr'] = ''
        props['syslog'] = ''
        props['file']   = str(t1)
        props['exec']   = str(t2)
        e = self.AddTableOptions_w_Properties (table, "Access", '%s!logger!access!type' % (pre), 
                                               LOGGER_WRITERS, props, 'log1')
        tmp += str(table) + e

        table = Table(2)
        props = {}
        t1 = Table(2)
        self.AddTableEntry (t1, 'Filename', '%s!logger!error!filename' % (pre))
        t2 = Table(2)
        self.AddTableEntry (t2, 'Command', '%s!logger!error!command' % (pre))
        props['stderr'] = ''
        props['syslog'] = ''
        props['file']   = str(t1)
        props['exec']   = str(t2)
        e = self.AddTableOptions_w_Properties (table, "Error", '%s!logger!error!type' % (pre), 
                                               LOGGER_WRITERS, props, 'log2')
        tmp += str(table) + e        
        tabs += [('Logging', tmp)]

        # Security
        table = Table(2)
        self.AddTableEntry (table, 'Certificate',     '%s!ssl_certificate_file' % (pre))
        self.AddTableEntry (table, 'Certificate key', '%s!ssl_certificate_key_file' % (pre))
        self.AddTableEntry (table, 'CA List',         '%s!ssl_ca_list_file' % (pre))
        tabs += [('Security', str(table))]

        # Behaviour
        tmp  = self._render_rules (host, cfg['directory'], cfg['extensions'], cfg['request'])
        tmp += self._render_add_rule(host)
        tabs += [('Behaviour', tmp)]

        txt += self.InstanceTab (tabs)

        form = Form ("/%s/%s/update" % (self._id, host))
        return form.Render(txt)

    def _render_add_rule (self, host):
        # Add new rule
        txt      = ''
        entry    = self.InstanceEntry ('add_new_entry', 'text')
        type     = EntryOptions ('add_new_type',      ENTRY_TYPES, selected='directory')
        handler  = EntryOptions ('add_new_handler',   HANDLERS,    selected='common')
        priority = self.InstanceEntry ('add_new_priority', 'text')
        
        table  = Table(5,1)
        table += ('Entry', 'Type', 'Handler', 'Priority')
        table += (entry, type, handler, priority, SUBMIT_ADD)

        form1  = Form("/%s/%s/add_new_entry" % (self._id, host), add_submit=False)

        txt += "<h3>Add new rule</h3>"
        txt += form1.Render(str(table))
        return txt

    def _render_rules (self, host, dirs_cfg, exts_cfg, reqs_cfg):
        txt    = ''
        table  = Table(4,1)
        table += ('', 'Type', 'Handler', 'Priority')
        
        # Rule list
        for rule in self._priorities:
            type, name, prio, conf = rule

            pre  = 'vserver!%s!%s!%s' % (host, type, name)
            link = '<a href="/vserver/%s/prio/%s">%s</a>' % (host, prio, name)
            e1   = EntryOptions ('%s!handler' % (pre), HANDLERS, selected=conf['handler'].value)
            e2   = self.InstanceEntry ('%s!priority' % (pre), 'text', value=prio)
            table += (link, type, e1, e2)
        txt += str(table)
        return txt

    def _op_apply_changes (self, post):
        self.ApplyChanges ([], post, DATA_VALIDATION)
