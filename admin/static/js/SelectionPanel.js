/* CTK: Cherokee Toolkit
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2010 Alvaro Lopez Ortega
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of version 2 of the GNU General Public
 * License as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

;(function($) {
    var SelectionPanel = function (element, table_id, content_id, cookie_name, cookie_domain, url_empty) {
	   var obj  = this;       //  Object {}
	   var self = $(element); // .submitter

	   function deselect_row (row) {
            $('#'+row.attr('pid')).removeClass('panel-selected');
	   }

	   function select_row (row) {
		  var url      = '';
		  var activity = $("#activity");
		  var content  = $('#'+content_id);

		  // Ensure there's a row to be selected
		  if (row.length == 0) {
			 url = url_empty;

		  } else {
			 url = row.attr('url');

			 // Highlight
			 $('#'+row.attr('pid')).addClass('panel-selected');

			 // Cookie
			 $.cookie (cookie_name, row.attr('id'), {path: cookie_domain});
		  }

		  // Update box
            activity.show();
		  $.ajax ({type:     'GET',
				 dataType: 'json',
				 async:    true,
				 url:      url,
				 success: function(data) {
					// New content: HTML + JS
					txt = data.html;
					txt += '<script type="text/javascript"> $(document).ready(function(){'
					txt += data.js
					txt += '}); </script>'

					// Update the Help
					Help_add_entries (data.helps);

					// Transition
					content.hide();
					content.html (txt);
   					resize_cherokee_panels();
					content.show();
                         activity.fadeOut('fast');
				 }
				});
	   }

	   function auto_select_row (row) {
		  var row_id     = row.attr('id');
		  var did_select = false;

		  self.find('.row_content').each (function() {
			 if ($(this).attr('id') == row_id) {
				select_row ($(this));
				did_select = true;
			 } else {
				deselect_row ($(this));
			 }
		  });

		  return did_select;
	   }

	   function filter_entries (text) {
		  self.find('.row_content').each (function() {
			 if ($(this).text().search(text) == -1) {
                                $('#'+$(this).attr('pid')).hide();
			 } else {
                                $('#'+$(this).attr('pid')).show();
			 }
		  });
	   }

	   this.get_selected = function() {
		  var selected = self.find('.panel-selected:first');
		  return $(selected);
	   }

	   this.set_selected_cookie = function (pid) {
		  var row = self.find ('.row_content[pid='+ pid +']');
		  $.cookie (cookie_name, row.attr('id'), {path: cookie_domain});
	   }

	   this.select_last = function() {
		  auto_select_row (self.find('.row_content:last'));
	   }

	   this.init = function (self) {
		  var cookie_selected = $.cookie(cookie_name);

		  /* Initial Selection */
		  if (cookie_selected == undefined) {
			 var first_row = self.find('.row_content:first');
			 select_row (first_row);

		  } else {
			 var did_select = auto_select_row ($('#'+cookie_selected));

			 if (! did_select) {
				var first_row = self.find('.row_content:first');
				select_row (first_row);
			 }
		  }

		  /* Events */
		  self.find('.row_content').bind('click', function() {
			 auto_select_row ($(this));
		  });

		  /* Filtering */
		  var filter_input = self.parents('.panel').find('input:text.filter');
		  filter_input.bind ("keyup", function(event) {
			 filter_entries ($(this).val());
		  });

		  return obj;
	   }
    };

    $.fn.SelectionPanel = function (table_id, content_id, cookie, cookie_domain, url_empty) {
	   var self = this;
	   return this.each(function() {
		  if ($(this).data('selectionpanel')) return;
		  var submitter = new SelectionPanel(this, table_id, content_id, cookie, cookie_domain, url_empty);
		  $(this).data('selectionpanel', submitter);
		  submitter.init(self);
	   });
    };

})(jQuery);



function resize_cherokee_panels() {
   var tdsize = 0;
   if ($('#topdialog').is(":visible")) {
       tdsize = $('#topdialog').height();
   }

   if ($("#vservers_panel").length > 0) {
       if ($('.selection-panel table').height() > $('#vservers_panel').height()) {
           $(".selection-panel table").addClass('hasScroll');
       } else {
           $(".selection-panel table").removeClass('hasScroll');
       }
       $('#vservers_panel').height($(window).height() - 190 - tdsize);
       $('.vserver_content .ui-tabs .ui-tabs-panel').height($(window).height() - 140 - tdsize);
   }

   if ($("#rules_panel").length > 0) {
       $('#rules_panel').height($(window).height() - 190 - tdsize);
       $('.rules_content .ui-tabs .ui-tabs-panel').height($(window).height() - 140 - tdsize);
   }

   if ($("#source_panel").length > 0) {
       $('#source_panel').height($(window).height() - 190 - tdsize);
       $('.source_content .submitter').height($(window).height() - 92 - tdsize);
   }
}

$(document).ready(function() {
   resize_cherokee_panels();
   $(window).resize(function(){
       resize_cherokee_panels();
   });
});
