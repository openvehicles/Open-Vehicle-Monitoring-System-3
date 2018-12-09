/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th November 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;
; With credit due to candbc-lexer.l, for idea and structure
; https://github.com/Polyconseil/libcanardbc
; Copyright (C) 2007-2009,2014 Andreas Heitmann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software.
;
; THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
; IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
; FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
; AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
; LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
; OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
; THE SOFTWARE.
*/

%{
#include <stdio.h>
#include <string.h>
#include "dbc_parser.hpp"

#define YY_NO_INPUT
#define YY_NO_UNPUT
%}

%option yylineno
%option nounput

whitespace          [ \t]+
newline             [\n\r]

identifier          [a-zA-Z_]([_\.a-zA-Z0-9]*)?

string              \"([^"\\]|(\\.))*\"
decimal             [-+]?[0-9]+
hexadecimal         0x[0-9A-Fa-f]+
floatingpoint       [-+]?[0-9]+(\.[0-9]+)?([eE][-+]?[0-9]+)?

%%

"//"[^\n]*          ;

"VERSION"           { return T_VERSION; }
"BO_"               { return T_BO; }
"BS_"               { return T_BS; }
"BU_"               { return T_BU; }
"SG_"               { return T_SG; }
"EV_"               { return T_EV; }
"SIG_VALTYPE_"      { return T_SIG_VALTYPE; }
"NS_"               { return T_NS; }

"INT"               { return T_INT; }
"FLOAT"             { return T_FLOAT; }
"NAN"               { return T_NAN; }
"STRING"            { return T_STRING; }
"ENUM"              { return T_ENUM; }
"HEX"               { return T_HEX; }

"NS_DESC_"          { return T_NS_DESC; }
"CM_"               { return T_CM; }
"BA_DEF_"           { return T_BA_DEF; }
"BA_"               { return T_BA; }
"VAL_"              { return T_VAL; }
"CAT_DEF_"          { return T_CAT_DEF; }
"CAT_"              { return T_CAT; }
"FILTER"            { return T_FILTER; }
"BA_DEF_DEF_"       { return T_BA_DEF_DEF; }
"EV_DATA_"          { return T_EV_DATA; }
"ENVVAR_DATA_"      { return T_ENVVAR_DATA; }
"SGTYPE_"           { return T_SGTYPE; }
"SGTYPE_VAL_"       { return T_SGTYPE_VAL; }
"BA_DEF_SGTYPE_"    { return T_BA_DEF_SGTYPE; }
"BA_SGTYPE_"        { return T_BA_SGTYPE; }
"SIG_TYPE_REF_"     { return T_SIG_TYPE_REF; }
"VAL_TABLE_"        { return T_VAL_TABLE; }
"SIG_GROUP_"        { return T_SIG_GROUP; }
"SIGTYPE_VALTYPE_"  { return T_SIGTYPE_VALTYPE; }
"BO_TX_BU_"         { return T_BO_TX_BU; }
"BA_DEF_REL_"       { return T_BA_DEF_REL; }
"BA_REL_"           { return T_BA_REL; }
"BA_DEF_DEF_REL_"   { return T_BA_DEF_DEF_REL; }
"BU_SG_REL_"        { return T_BU_SG_REL; }
"BU_EV_REL_"        { return T_BU_EV_REL; }
"BU_BO_REL_"        { return T_BU_BO_REL; }
"SG_MUL_VAL_"       { return T_SG_MUL_VAL; }
"DUMMY_NODE_VECTOR"[0-3] {
  yylval.number = yytext[17]-'0';
  return T_DUMMY_NODE_VECTOR;
  }

{newline}           ;
{whitespace}        ;

{identifier}        {
  yylval.string = strdup(yytext);
  return T_ID;
  }

{string}            {
  int len = strlen(yytext);
  if(len>=2)
    {
    yylval.string = (char *) malloc (len-1);
    memcpy (yylval.string, yytext+1, len-2);
    yylval.string[len-2]='\0';
    }
   else
     {
     yylval.string = NULL;
     }
  return T_STRING_VAL;
  }

{decimal}           {
  yylval.number = atoll(yytext);
  return T_INT_VAL;
  }

{hexadecimal}       {
  yylval.number = strtol(yytext,NULL,16);
  return T_INT_VAL;
  }

{floatingpoint}     {
  yylval.double_val = strtod(yytext, NULL);
  return T_DOUBLE_VAL;
  }

":"                 { return T_COLON; }
";"                 { return T_SEMICOLON; }
"|"                 { return T_SEP; }
","                 { return T_COMMA; }
"@"                 { return T_AT; }
"+"                 { return T_PLUS; }
"-"                 { return T_MINUS; }
"["                 { return T_BOX_OPEN; }
"]"                 { return T_BOX_CLOSE; }
"("                 { return T_PAR_OPEN; }
")"                 { return T_PAR_CLOSE; }

<<EOF>>             { yyterminate(); }
.                   { return yytext[0]; }

%%

int
yywrap()
  {
  return 1;
  }
