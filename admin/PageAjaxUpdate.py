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
        pre = "%s!match" % (post.get_val('pre'))

        # FIXME: I guess is better work first on a tmp array with 
        # the rules validated instead removing the rules in the first place...
        del (self._cfg[pre])

        # Global condition
        self._cfg[pre] = post.get_val(pre)

        gpath = "%s!%s"%(pre,g)
        gmatch = post.get_val(gpath)

        while gmatch != None:
            r = 0

            # Group condition
            self._cfg[gpath] = gmatch

            rpath = "%s!%s!%s"%(pre,g,r)
            rmatch = post.get_val(rpath)
            while rmatch != None:
                # Rule type
                self._cfg[rpath] = rmatch

                # Rule condition
                self._cfg["%s!cond"%(rpath)] = post.get_val("%s!cond"%(rpath))

                # Rule value
                self._cfg["%s!val"%(rpath)] = post.get_val("%s!val"%(rpath))

                # Extra fields
                extras = (extra for extra in post if extra.startswith("%s!"%(rpath)))

                for extra in extras: 
                    if not extra.endswith(('cond', 'val')):
                        self._cfg[extra] = post.get_val(extra)

                r += 1
                rpath = "%s!%s!%s"%(pre,g,r)
                rmatch = post.get_val(rpath)
            g += 1
            gpath = "%s!%s"%(pre,g)
            gmatch = post.get_val(gpath)

        return "ok"
