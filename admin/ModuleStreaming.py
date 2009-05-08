import validations 

from Form import *
from Table import *

from ModuleHandler import *
from ModuleFile import *

# For gettext
N_ = lambda x: x

NOTE_RATE        = N_('Figure the bit rate of the media file, and limit the bandwidth to it.')
NOTE_RATE_FACTOR = N_('Factor by which the bandwidth limit will be increased. Default: 0.1')
NOTE_RATE_BOOST  = N_('Number of seconds to stream before setting the bandwidth limit. Default: 5.')

DATA_VALIDATION = [
    ('vserver!.+?!rule!.+?!handler!rate_factor', validations.is_float),
    ('vserver!.+?!rule!.+?!handler!rate_boost',  validations.is_number_gt_0),
]

HELPS = [
    ('modules_handlers_streaming', N_("Audio/Video Streaming"))
]

class ModuleStreaming (ModuleHandler):
    PROPERTIES = ModuleFile.PROPERTIES + [
        'rate'
    ]

    def __init__ (self, cfg, prefix, submit_url):
        ModuleHandler.__init__ (self, 'streaming', cfg, prefix, submit_url)

        self._file = ModuleFile (cfg, prefix, submit_url)

    def _op_render (self):
        txt = ''

        # Streaming
        table = TableProps()
        self.AddPropCheck (table, _("Auto Rate"),       "%s!rate" % (self._prefix), True,  _(NOTE_RATE))
        if int(self._cfg.get_val ('%s!rate'%(self._prefix), "1")):
            self.AddPropEntry (table, _("Speedup Factor"), "%s!rate_factor" % (self._prefix), _(NOTE_RATE_FACTOR))
            self.AddPropEntry (table, _("Initial Boost"),  "%s!rate_boost" % (self._prefix),  _(NOTE_RATE_BOOST))

        txt += '<h2>%s</h2>' % (_('Audio/Video Streaming'))
        txt += self.Indent(table)

        # Copy errors to the modules, 
        # they may need to print them
        self._copy_errors (self, self._file)

        txt += self._file._op_render()
        return txt

    def _op_apply_changes (self, uri, post):
        self.ApplyChangesPrefix (self._prefix, ['rate'], post, DATA_VALIDATION)

        # Apply the changes
        self._file._op_apply_changes (uri, post)

        # Copy errors from the child modules
        self._copy_errors (self._file, self)

    def _copy_errors (self, _from, _to):
        for e in _from.errors:
            _to.errors[e] = _from.errors[e]
