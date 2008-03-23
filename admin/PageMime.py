import validations

from Page import *
from Table import *
from Entry import *
from Form import *

DATA_VALIDATION = [
    ("server!mime_files", validations.is_path_list),
]

class PageMime (PageMenu, FormHelper):
    def __init__ (self, cfg):
        PageMenu.__init__ (self, 'mime', cfg)
        FormHelper.__init__ (self, 'mime', cfg)

    def _op_render (self):
        content = self._render_content()

        self.AddMacroContent ('title', 'Mime types configuration')
        self.AddMacroContent ('content', content)

        return Page.Render(self)

    def _op_apply_changes (self, uri, post):
        if post.get_val('new_mime') and \
          (post.get_val('new_extensions') or
           post.get_val('new_maxage')):
            self._add_new_mime (post)
            if self.has_errors():
                return self._op_render()

        self.ApplyChanges ([], post, DATA_VALIDATION)
        return "/%s" % (self._id)

    def _add_new_mime (self, post):
        mime = post.pop('new_mime')
        exts = post.pop('new_extensions')
        mage = post.pop('new_maxage')

        if mage:
            self._cfg['mime!%s!max-age'%(mime)] = mage
        if exts:
            self._cfg['mime!%s!extensions'%(mime)] = exts

    def _render_content (self):
        content  = self._render_mime_list()
        content += self._render_add_mime()

        form = Form ('/%s' % (self._id))
        return form.Render (content, DEFAULT_SUBMIT_VALUE)

    def _render_mime_list (self):
        txt = '<h1>MIME types</h1>'        
        cfg = self._cfg['mime']
        if cfg:
            table = Table(4, 1)
            table += ('Mime type', 'Extensions', 'Max Age (<i>secs</i>)')
            for mime in cfg:
                cfg_key = 'mime!%s'%(mime)
                e1 = self.InstanceEntry('%s!extensions'%(cfg_key), 'text')
                e2 = self.InstanceEntry('%s!max-age'%(cfg_key), 'text', size=6, maxlength=6)
                js = "post_del_key('/icons/update', '%s');" % (cfg_key)
                link_del = self.InstanceImage ("bin.png", "Delete", border="0", onClick=js)
                table += (mime, e1, e2, link_del)
            txt += str(table)
        return txt
        form = Form ('/%s' % (self._id))
        return form.Render(txt,DEFAULT_SUBMIT_VALUE)

    def _render_add_mime (self):
        txt = '<h2>Add new MIME</h2>\n'
        e1 = self.InstanceEntry('new_mime', 'text')
        e2 = self.InstanceEntry('new_extensions', 'text')
        e3 = self.InstanceEntry('new_maxage', 'text', size=6, maxlength=6)
        
        table = Table(3,1)
        table += ('Mime Type', 'Extensions', 'Max Age')
        table += (e1, e2, e3)

        txt += str(table)
        return txt
