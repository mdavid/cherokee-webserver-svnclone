from Form import *
from Table import *
from Module import *
import validations

# For gettext
N_ = lambda x: x

NOTE_HEADER = N_("Header against which the regular expression will be evaluated.")
NOTE_MATCH  = N_("Regular expression.")

LENGHT_LIMIT = 10

HEADERS = [
    ('Accept-Encoding', 'Accept-Encoding'),
    ('Accept-Charset',  'Accept-Charset'),
    ('Accept-Language', 'Accept-Language'),
    ('Referer',         'Referer'),
    ('User-Agent',      'User-Agent'),
    ('Cookie',          'Cookie'),
    ('Host',            'Host')
]

class ModuleHeader (Module, FormHelper):
    def __init__ (self, cfg, prefix, submit_url):
        FormHelper.__init__ (self, 'header', cfg)
        Module.__init__ (self, 'header', cfg, prefix, submit_url)

        self.validation = [('tmp!new_rule!match',      validations.is_regex),
                           ('%s!match'%(self._prefix), validations.is_regex)]

    def _op_render (self):
        table = TableProps()
        if self._prefix.startswith('tmp!'):
            self.AddPropOptions_Reload_Plain (table, _('Header'), '%s!value'%(self._prefix), HEADERS, _(NOTE_HEADER))
        else:
            self.AddPropOptions_Reload_Plain (table, _('Header'), '%s!header'%(self._prefix), HEADERS, _(NOTE_HEADER))
        self.AddPropEntry (table, _('Regular Expression'), '%s!match'%(self._prefix), _(NOTE_MATCH))
        return str(table)

    def apply_cfg (self, values):
        if values.has_key('value'):
            header = values['value']
            self._cfg['%s!header'%(self._prefix)] = header

        if values.has_key('match'):
            match = values['match']
            self._cfg['%s!match'%(self._prefix)] = match

    def get_name (self):
        header = self._cfg.get_val ('%s!header'%(self._prefix))
        if not header:
            return ''

        tmp  = self._cfg.get_val ('%s!match'%(self._prefix), '')
        if len(tmp) > LENGHT_LIMIT:
            return "%s (%s..)" % (header, tmp[:5])

        return "%s (%s)" % (header, tmp)

    def get_type_name (self):
        return self._id.capitalize()