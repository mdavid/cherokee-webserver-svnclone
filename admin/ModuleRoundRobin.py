from Form import *
from Table import *
from Module import *

from consts import *

RR_COMMENT = """
The <i>Round Robin</i> balancer can either access remote servers or
launch local application servers and connect to them.
"""

class ModuleRoundRobin (Module, FormHelper):
    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'round_robin', cfg)
        Module.__init__ (self, 'round_robin', cfg, prefix, submit_url)

    def _op_render (self):
        txt = ''

        # Options
        table = Table(2)
        self.AddTableOptions_Reload (table, "Information sources", 
                                     "%s!type" % (self._prefix), 
                                     BALANCER_TYPES)
        txt += str(table)

        # Get the host/interpreter list
        try:
            cfg = self._cfg[self._prefix]
            hosts = filter (lambda x: x != 'type', cfg)
        except:
            hosts = []

        # Read the type
        type_ = self._cfg.get_val('%s!type'%(self._prefix))
        if not type_: type_ = 'interpreter'

        if type_ == 'host':
            # Host list
            if hosts:
                t1 = Table(2,1)
                t1 += ('Host', '')
                for host in hosts:
                    pre = '%s!%s' % (self._prefix, host)
                    e_host = self.InstanceEntry('%s!host'%(pre), 'text')
                    js = "post_del_key('/ajax/update', '%s');" % (pre)
                    button = self.InstanceButton ('Del', onClick=js)
                    t1 += (e_host, button)
                txt += str(t1)

            # New host
            t1  = Table(2,1)
            t1 += ('New host', '')
            en1 = self.InstanceEntry('new_host', 'text')
            t1 += (en1, SUBMIT_ADD)
            txt += str(t1)

        elif type_ == 'interpreter':
            # Interpreter list
            if hosts:
                for host in hosts:
                    pre = '%s!%s' % (self._prefix, host)
                    e_host = self.InstanceEntry('%s!host'%(pre), 'text')
                    e_inte = self.InstanceEntry('%s!interpreter'%(pre), 'text')
                    e_envs = self._render_envs('%s!env'%(pre))

                    t2 = Table(2)
                    t2 += ('Host', e_host)
                    t2 += ('Interpreter', e_inte)
                    t2 += ('Environment', e_envs)
                    txt += str(t2)
                    txt += "<hr />"

                if txt.endswith("<hr />"):
                    txt = txt[:-6]

            t2 = Table(3,1)
            t2 += ('Host', 'Interpreter', '')
            e_host = self.InstanceEntry('new_host', 'text')
            e_inte = self.InstanceEntry('new_interpreter', 'text')
            t2 += (e_host, e_inte, SUBMIT_ADD)
            txt += str(t2)
            
        else:
            txt = 'UNKNOWN type: ' + str(type_)

        return txt


    def __find_name (self):
        i = 1
        while True:
            key = "%s!%d" % (self._prefix, i)
            tmp = self._cfg[key]
            if not tmp: 
                return str(i)
            i += 1

    def _render_envs (self, cfg_key):
        txt = ''
        cfg = self._cfg[cfg_key]

        # Current environment
        if cfg and cfg.has_child():
            table = Table(4)
            for env in cfg:
                host        = cfg_key.split('!')[1]
                cfg_key_env = "%s!%s" % (cfg_key, env)

                js = "post_del_key('/ajax/update', '%s');" % (cfg_key_env)
                button = self.InstanceButton ('Del', onClick=js)
                table += (env, '=', self._cfg[cfg_key_env].value, button)
            txt += str(table)

        # Add new environment variable
        en_env = self.InstanceEntry('balancer_new_env',     'text')
        en_val = self.InstanceEntry('balancer_new_env_val', 'text')
        hidden = self.HiddenInput  ('balancer_new_env_key', cfg_key)

        table = Table(3,1)
        table += ('Variable', 'Value', '')
        table += (en_env, en_val, SUBMIT_ADD)
        txt += str(table) + hidden

        return txt

    def _op_apply_changes (self, uri, post):
        new_host        = post.pop('new_host')
        new_interpreter = post.pop('new_interpreter')

        # Addind new host/interpreter
        if new_host or new_interpreter:
            num = self.__find_name()

            if new_host:
                key = "%s!%s!host" % (self._prefix, num)
                self._cfg[key] = new_host
        
            if new_interpreter:
                key = "%s!%s!interpreter" % (self._prefix, num)
                self._cfg[key] = new_interpreter
        
        # New environment variable
        env = post.pop('balancer_new_env')
        val = post.pop('balancer_new_env_val')
        key = post.pop('balancer_new_env_key')
                    
        if env and val and key:
            self._cfg["%s!%s"%(key, env)] = val

        # Everything else
        self.ApplyChanges ([], post)

