# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#      Taher Shihadeh <taher@unixwars.com>
#
# Copyright (C) 2001-2010 Alvaro Lopez Ortega
#
# This program is free software; you can redistribute it and/or
# modify it under the terms of version 2 of the GNU General Public
# License as published by the Free Software Foundation.
#
# This program is distributed in the hope that it will be useful,
# but WITHOUT ANY WARRANTY; without even the implied warranty of
# MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
# GNU General Public License for more details.
#
# You should have received a copy of the GNU General Public License
# along with this program; if not, write to the Free Software
# Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
# 02110-1301, USA.
#

import CTK

GRAPH_VSERVER   = '/graphs/%(prefix)s_%(type)s_%(vserver)s_%(interval)s.png'
GRAPH_SERVER    = '/graphs/%(prefix)s_%(type)s_%(interval)s.png'

GRAPH_TYPES     = [N_('Server Traffic',         'traffic'),
                   N_('Connections / Requests', 'accepts'),
                   N_('Connection Timeouts',    'timeouts'),]

GRAPH_INTERVALS = [N_('1 hour',  '1h'),
                   N_('6 hours', '6h'),
                   N_('1 day',   '1d'),
                   N_('1 week',  '1w'),
                   N_('1 month', '1m')]


class Graph (CTK.Box):
    def __init__ (self, refreshable, **kwargs):
        CTK.Box.__init__ (self)
        self.graph = {}

    def build_graph (self):
        tabs = CTK.Tab ()
        for x in GRAPH_intervals:
            self.graph['interval'] = x[1]
            props = {'src': self.template % self.graph,
                     'alt': '%s: %s' %(graph_type[0], x[0])}
            image = CTK.Image(props)
            tabs.Add (_(x[0]), image)
        self += tabs


class GraphVServer (Graph):
    def __init__ (self, vserver, **kwargs):
        Graphs.__init__ (self, **kwargs)
        self.template         = GRAPH_VSERVER
        self.graph['prefix']  = 'vserver'
        self.graph['type']    = GRAPH_TYPES[0]
        self.graph['vserver'] = vserver
        self.build_graph ()


class GraphServer (Graph):
    def __init__ (self, refreshable, **kwargs):
        Graphs.__init__ (self, **kwargs)
        self.template        = GRAPH_SERVER
        self.graph['prefix'] = 'server'

        combo = CTK.Combobox ({'name': 'type', 'selected': '1h'}, GRAPH_TYPES)
        combo.bind('change', 'alert($(this).val(););')
        self += combo

        self.build_graph ()


class GraphServer_Instancer (CTK.Container):
    def __init__ (self):
        CTK.Container.__init__ (self)

        # Refresher
        refresh = CTK.Refreshable ({'id': 'grapharea'})
        refresh.register (lambda: GraphServer(refresh).Render())
        self += refresh


"""

graphs/server_accepts_1d.png
graphs/server_accepts_1h.png
graphs/server_accepts_1m.png
graphs/server_accepts_1w.png
graphs/server_accepts_6h.png

graphs/server_timeouts_1d.png
graphs/server_timeouts_1h.png
graphs/server_timeouts_1m.png
graphs/server_timeouts_1w.png
graphs/server_timeouts_6h.png

graphs/server_traffic_1d.png
graphs/server_traffic_1h.png
graphs/server_traffic_1m.png
graphs/server_traffic_1w.png
graphs/server_traffic_6h.png

graphs/server_traffic_1h.png
graphs/server_accepts_1h.png
graphs/server_timeouts_1h.png
graphs/vserver_traffic_example.com_1h.png
"""
