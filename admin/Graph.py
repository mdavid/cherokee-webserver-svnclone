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

GRAPH_TYPES     = [('traffic',  N_('Server Traffic')),
                   ('accepts',  N_('Connections / Requests')),
                   ('timeouts', N_('Connection Timeouts')),]

GRAPH_INTERVALS = [('1h', N_('1 hour'),  '1h'),
                   ('6h', N_('6 hours'), '6h'),
                   ('1d', N_('1 day'),   '1d'),
                   ('1w', N_('1 week'),  '1w'),
                   ('1m', N_('1 month'), '1m')]


class Graph (CTK.Box):
    def __init__ (self,  **kwargs):
        CTK.Box.__init__ (self)
        self.graph = {}
        self.graph['type']    = GRAPH_TYPES[0][0]
        self.graph['type_txt']= GRAPH_TYPES[0][1]

    def build_graph (self):
        tabs = CTK.Tab ()
        for x in GRAPH_INTERVALS:
            self.graph['interval'] = x[0]
            props = {'src': self.template % self.graph,
                     'alt': '%s: %s' %(self.graph['type_txt'], x[1])}
            image = CTK.Image(props)
            tabs.Add (_(x[0]), image)
        self += tabs


class GraphVServer (Graph):
    def __init__ (self, refreshable, vserver, **kwargs):
        Graph.__init__ (self, **kwargs)
        self.template         = GRAPH_VSERVER
        self.graph['prefix']  = 'vserver'
        self.graph['vserver'] = vserver
        self.__call__()

    def __call__ (self):
        self.build_graph ()


class GraphServer (Graph):
    def __init__ (self, refreshable, **kwargs):
        Graph.__init__ (self, **kwargs)
        self.template        = GRAPH_SERVER
        self.graph['prefix'] = 'server'
        self.__call__()

    def __call__ (self):
        props = {'name': 'type', 'selected': self.graph['type']}
        combo = CTK.Combobox (props, GRAPH_TYPES)
        combo.bind('change', 'alert($(this).val());')
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
graphs/server_traffic_1h.png
graphs/server_accepts_1h.png
graphs/server_timeouts_1h.png
graphs/vserver_traffic_example.com_1h.png
"""
