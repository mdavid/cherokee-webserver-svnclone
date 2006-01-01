%{
/* Cherokee
 *
 * Authors:
 *      Alvaro Lopez Ortega <alvaro@alobbs.com>
 *
 * Copyright (C) 2001-2006 Alvaro Lopez Ortega
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307
 * USA
 */

#include "mime_grammar.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
%}

%option yylineno
%option nounput

ws              [ \t]+
nl              [\n\r]+

comment         #.*$

mime            [\+\.\-_a-zA-Z0-9\/\\]+
suffix          [a-zA-Z0-9]+
age             [0-9]+
%%

{ws}          ;
{nl}          ;
{comment}     ;

"max-age"     { return T_MAXAGE; }
"maxage"      { return T_MAXAGE; }
"suffixes"    { return T_SUFFIXES; }

{age}         { yy_mime_lval.number =   atoi (yy_mime_text); return T_AGE; }
{suffix}      { yy_mime_lval.string = strdup (yy_mime_text); return T_SUFFIX; }
{mime}        { yy_mime_lval.string = strdup (yy_mime_text); return T_MIME; }

<<EOF>>       { yyterminate(); }

.		    { return yy_mime_text[0]; }


%%

int 
yy_mime_wrap()
{
       return 1;
}

