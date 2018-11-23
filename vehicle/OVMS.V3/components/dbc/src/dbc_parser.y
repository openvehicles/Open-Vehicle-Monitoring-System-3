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
; With due credit due to candbc-parser.y
; https://github.com/Polyconseil/libcanardbc
; Copyright (C) 2007-2009 Andreas Heitmann
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
#include <sys/types.h>
#include <string.h>
#include <stdlib.h>
#include <errno.h>
#include <ctype.h>
#include <math.h>
#include <string>
#include "dbc.h"
#include "ovms_log.h"

/* Tell Bison how much stack space is needed. */
#define YYMAXDEPTH 20000
%}

%parse-param {void* dbcptr}

%union {
  long long                   number;
  double                      double_val;
  char*                       string;
  }

%{
extern int yylex (void);
extern char *yytext;
extern int yylineno;

static const char *TAG = "dbc-parser";

extern "C"
  {
  void yyerror(void* dbcptr, const char *msg)
    {
    ESP_LOGE(TAG,"Error in line %d '%s', symbol '%s'",
      yylineno, msg, yytext);
    }
  }
%}

%token T_COLON
%token T_SEMICOLON
%token T_SEP
%token T_AT
%token T_PLUS
%token T_MINUS
%token T_BOX_OPEN
%token T_BOX_CLOSE
%token T_PAR_OPEN
%token T_PAR_CLOSE
%token T_COMMA
%token T_ID
%token T_STRING_VAL
%token T_INT_VAL
%token T_DOUBLE_VAL

%token T_VERSION
%token T_INT
%token T_FLOAT
%token T_STRING
%token T_ENUM
%token T_HEX
%token T_BO         /* Botschaft */
%token T_BS
%token T_BU         /* Steuergerät */
%token T_SG         /* Signal */
%token T_EV         /* Environment */
%token T_NS
%token T_NS_DESC
%token T_CM         /* Comment */
%token T_BA_DEF     /* Attribut-Definition */
%token T_BA         /* Attribut */
%token T_VAL
%token T_CAT_DEF
%token T_CAT
%token T_FILTE
%token T_BA_DEF_DEF
%token T_EV_DATA
%token T_ENVVAR_DATA
%token T_SGTYPE
%token T_SGTYPE_VAL
%token T_BA_DEF_SGTYPE
%token T_BA_SGTYPE
%token T_SIG_TYPE_REF
%token T_VAL_TABLE
%token T_SIG_GROUP
%token T_SIG_VALTYPE
%token T_SIGTYPE_VALTYPE
%token T_BO_TX_BU
%token T_BA_DEF_REL
%token T_BA_REL
%token T_BA_DEF_DEF_REL
%token T_BU_SG_REL
%token T_BU_EV_REL
%token T_BU_BO_REL
%token T_SG_MUL_VAL
%token T_DUMMY_NODE_VECTOR
%token T_NAN

%type <string>                    T_ID T_STRING_VAL version

%%

dbc:
version                   /* 2 */
    ;

version: T_VERSION T_STRING_VAL { $$ = $2; };
