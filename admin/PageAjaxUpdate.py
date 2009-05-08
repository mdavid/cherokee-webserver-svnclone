from Form import *

class PageAjaxUpdate (WebComponent):
    def __init__ (self, cfg):
        WebComponent.__init__ (self, 'page', cfg)

    def _op_render (self):
        return "ok"

    def _op_handler (self, uri, post):

        if post.get_val('save_rules'): 
            return self._save_rules(post)

        for confkey in post:
            if not '!' in confkey:
                continue

            value = post[confkey][0]
            if not value:
                del (self._cfg[confkey])
            else:
                self._cfg[confkey] = value

            return "ok"
    
    def _save_rules (self, post):
        g = 0
        pre = post['pre']
        print "Save Rules"
        print post
        return "ok"
        gpath = "%s!%s"%(pre,g)
        gmatch = self._cfg.get_val(gpath)
        while gmatch != None:
            r = 0
            print "Group Match %s = %s"%(gpath,gmatch)
            txt += "addGroup(%s, '%s');"%(g, gmatch)
            rpath = "%s!%s!%s"%(pre,g,r)
            rmatch = self._cfg.get_val(rpath)
            while rmatch != None:
                print "Rule Match %s = %s"%(rpath,rmatch)
                print "    Condition = %s"%(self._cfg.get_val("%s!%s"%(rpath,'cond')))
                print "        Value = %s"%(self._cfg.get_val("%s!%s"%(rpath,'val')))
                rule_module = module_obj_factory (rmatch, self._cfg, rpath, self.submit_url)
                txt += rule_module._op_render()
                r += 1
                rpath = "%s!%s!%s"%(pre,g,r)
                rmatch = self._cfg.get_val(rpath)
            g += 1
            gpath = "%s!%s"%(pre,g)
            gmatch = self._cfg.get_val(gpath)

    def _get_post_val(self, post, key, default=None):
        # Maybe this needs to be included in Post module
        try:
            val = post[key]
        except:
            return default
        if not val:
            return default
        return val


       
