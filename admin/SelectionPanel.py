# -*- coding: utf-8 -*-
#
# Cherokee-admin
#
# Authors:
#      Alvaro Lopez Ortega <alvaro@alobbs.com>
#
# Copyright (C) 2010 Alvaro Lopez Ortega
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
import string

JS_ROW_CLICK = """
  var row = $(this);
  var id  = $(this).attr('id');

  /* Highlight */
  $(this).parents('table').find('.row_content').each(function(){
     if (id == this.id) {
        $(this).addClass('panel-selected');
     } else {
        $(this).removeClass('panel-selected');
     }
  });

  /* Load content */
  $.ajax ({type: 'GET', url: '%(url)s', async: true,
           success: function(data) {
              $('#%(id_content)s').html(data);
           }
  });
"""

class SelectionPanel (CTK.Container):
    def __init__ (self, callback, id_content):
        CTK.Container.__init__ (self)

        self.table      = CTK.SortableList (callback)
        self.id_content = id_content

        box = CTK.Box({'class': 'selection-panel'})
        box += self.table
        self += box

    def Add (self, url, content):
        assert type(url) == str
        assert type(content) == list

        # Row Content
        row_content = CTK.Box({'class': 'row_content'})
        for w in content:
            row_content += w

        props = {'url':        url,
                 'id_content': self.id_content}
        row_content.bind ('click', JS_ROW_CLICK %(props))

        # Row ID
        row_id = ''.join([('_',x)[x in string.letters+string.digits] for x in url])

        # Add to the table
        self.table += [None, row_content]
        self.table[-1].props['id'] = row_id
        self.table[-1][1].props['class'] = "dragHandle"
        self.table[-1][2].props['class'] = "nodrag nodrop"

    def Render (self):
        render = CTK.Container.Render (self)

        props = {'id': self.id}
        #render.js += JS_CLICK_ROW %(props)

        return render
