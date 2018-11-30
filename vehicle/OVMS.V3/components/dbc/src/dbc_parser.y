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
; With credit due to candbc-parser.y, for idea and structure
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
#define YYERROR_VERBOSE 1
#define YYMAXDEPTH 1000
%}

%parse-param {void* dbcptr}

%union {
  long long                   number;
  double                      double_val;
  char*                       string;
  };

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

dbcfile* current_dbc = NULL;
dbcValueTable* current_value_table = NULL;
dbcMessage* current_message = NULL;
dbcSignal* current_signal = NULL;
char* current_attribute = NULL;
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
%token T_NAN
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
%token T_FILTER
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

%type <string>                    T_ID T_STRING_VAL version_section signal_mux
%type <number>                    T_INT_VAL signal_endian signal_sign signal_start signal_length
%type <double_val>                T_DOUBLE_VAL double_val signal_scale signal_offset signal_min signal_max
%%

dbc:
  {
  current_dbc = (dbcfile*)dbcptr;
  current_value_table = NULL;
  }
  version_section           /* 2 */
  symbol_section            /* 3 */
  bit_timing_section        /* 4 */
  node_list_section         /* 5 */
  value_table_section_list  /* 6 */
  message_section_list      /* 7 */
  value_section_list        /* 8 */
  comment_section_list      /* 9 */
  ;

/************************************************************************/
/* version_section (VERSION)                                            */
/************************************************************************/

version_section: T_VERSION T_STRING_VAL
  {
  current_dbc->m_version = std::string($2);
  if ($2) { free($2); $2=NULL; }
  };

/************************************************************************/
/* symbol_section (NS_)                                                 */
/************************************************************************/

symbol_section:
    T_NS T_COLON
  | T_NS T_COLON symbol_list
    ;

symbol_list:
    symbol
  | symbol_list symbol
    ;

symbol:
    T_NS_DESC          { current_dbc->m_newsymbols.AddSymbol("NS_DESC_"); }
  | T_CM               { current_dbc->m_newsymbols.AddSymbol("CM_"); }
  | T_BA_DEF           { current_dbc->m_newsymbols.AddSymbol("BA_DEF_"); }
  | T_BA               { current_dbc->m_newsymbols.AddSymbol("BA_"); }
  | T_VAL              { current_dbc->m_newsymbols.AddSymbol("VAL_"); }
  | T_CAT_DEF          { current_dbc->m_newsymbols.AddSymbol("CAT_DEF_"); }
  | T_CAT              { current_dbc->m_newsymbols.AddSymbol("CAT_"); }
  | T_FILTER           { current_dbc->m_newsymbols.AddSymbol("FILTER"); }
  | T_BA_DEF_DEF       { current_dbc->m_newsymbols.AddSymbol("BA_DEF_DEF_"); }
  | T_EV_DATA          { current_dbc->m_newsymbols.AddSymbol("EV_DATA_"); }
  | T_ENVVAR_DATA      { current_dbc->m_newsymbols.AddSymbol("ENVVAR_DATA_"); }
  | T_SGTYPE           { current_dbc->m_newsymbols.AddSymbol("SG_TYPE_"); }
  | T_SGTYPE_VAL       { current_dbc->m_newsymbols.AddSymbol("SG_TYPE_VAL_"); }
  | T_BA_DEF_SGTYPE    { current_dbc->m_newsymbols.AddSymbol("BA_DEF_SGTYPE_"); }
  | T_BA_SGTYPE        { current_dbc->m_newsymbols.AddSymbol("BA_SGTYPE_"); }
  | T_SIG_TYPE_REF     { current_dbc->m_newsymbols.AddSymbol("SIG_TYPE_REF_"); }
  | T_VAL_TABLE        { current_dbc->m_newsymbols.AddSymbol("VAL_TABLE_"); }
  | T_SIG_GROUP        { current_dbc->m_newsymbols.AddSymbol("SIG_GROUP_"); }
  | T_SIG_VALTYPE      { current_dbc->m_newsymbols.AddSymbol("SIG_VALTYPE_"); }
  | T_SIGTYPE_VALTYPE  { current_dbc->m_newsymbols.AddSymbol("SIGTYPE_VALTYPE_"); }
  | T_BO_TX_BU         { current_dbc->m_newsymbols.AddSymbol("BO_TX_BU_"); }
  | T_BA_DEF_REL       { current_dbc->m_newsymbols.AddSymbol("BA_DEF_REL_"); }
  | T_BA_REL           { current_dbc->m_newsymbols.AddSymbol("BA_REL_"); }
  | T_BA_DEF_DEF_REL   { current_dbc->m_newsymbols.AddSymbol("BA_DEF_DEF_REL_"); }
  | T_BU_SG_REL        { current_dbc->m_newsymbols.AddSymbol("BU_SG_REL_"); }
  | T_BU_EV_REL        { current_dbc->m_newsymbols.AddSymbol("BU_EV_REL_"); }
  | T_BU_BO_REL        { current_dbc->m_newsymbols.AddSymbol("BU_BO_REL_"); }
  | T_SG_MUL_VAL       { current_dbc->m_newsymbols.AddSymbol("SG_MUL_VAL_"); }
  ;

/************************************************************************/
/* bit_timing_section (BS_)                                             */
/************************************************************************/

bit_timing_section:
    T_BS T_COLON
  | T_BS T_COLON T_INT_VAL T_COLON T_INT_VAL T_COMMA T_INT_VAL
    {
    current_dbc->m_bittiming.m_baudrate = $3;
    current_dbc->m_bittiming.m_btr1 = $5;
    current_dbc->m_bittiming.m_btr2 = $7;
    }
    ;

/************************************************************************/
/* node_list_section (BU_)                                             */
/************************************************************************/

node_list_section:
    T_BU T_COLON
  | T_BU T_COLON node_list
    ;

node_list:
    T_ID
      {
      current_dbc->m_nodes.AddNode(new dbcNode($1));
      free($1);
      }
  | node_list T_ID
      {
      current_dbc->m_nodes.AddNode(new dbcNode($2));
      free($2);
      }
    ;

/************************************************************************/
/* value_table_section_list (VAL_TABLE_)                                */
/************************************************************************/

value_table_section_list:
  | value_table_section value_table_section_list;

value_table_section:
    T_VAL_TABLE value_table_list T_SEMICOLON
    ;

value_table_list:
    T_ID T_INT_VAL T_STRING_VAL
    {
    current_value_table = new dbcValueTable($1);
    current_dbc->m_values.AddValueTable($1, current_value_table);
    current_value_table->AddValue($2, $3);
    free($1); free($3);
    }
  |
    value_table_list T_INT_VAL T_STRING_VAL
    {
    current_value_table->AddValue($2, $3);
    free($3);
    }
    ;

/************************************************************************/
/* message_section_list (BO_)                                           */
/************************************************************************/

message_section_list:
  | message_section_list message_section;

/* BO_ 1160 DAS_steeringControl: 4 NEO */
message_section:
    T_BO T_INT_VAL T_ID T_COLON T_INT_VAL T_ID signal_list
    {
    current_message->m_id = (uint32_t)$2;
    current_message->m_name = std::string($3); free($3);
    current_message->m_size = $5;
    current_message->m_transmitter_node = std::string($6); free($6);
    current_dbc->m_messages.AddMessage($2,current_message);
    current_message = NULL;
    }
    ;

signal_list:
  | signal_list signal
  ;

/* SG_ DAS_steeringAngleRequest : 6|15@0+ (0.1,-1638.35) [-1638.35|1638.35] "deg" EPAS */
signal:
    T_SG T_ID signal_mux T_COLON
         signal_start T_SEP signal_length T_AT signal_endian signal_sign
         T_PAR_OPEN signal_scale T_COMMA signal_offset T_PAR_CLOSE
         T_BOX_OPEN signal_min T_SEP signal_max T_BOX_CLOSE
         T_STRING_VAL receiver_list
    {
    if (current_message == NULL) current_message = new dbcMessage();

    current_signal->m_name = std::string($2); free($2);

    if ($3 == NULL)
      {
      current_signal->m_mux.multiplexed = DBC_MUX_NONE;
      current_signal->m_mux.switchvalue = 0;
      }
   else
     {
     switch($3[0])
       {
       case 'M':
         current_signal->m_mux.multiplexed = DBC_MUX_MULTIPLEXOR;
         current_signal->m_mux.switchvalue = 0;
         break;
       case 'm':
         current_signal->m_mux.multiplexed = DBC_MUX_MULTIPLEXED;
         current_signal->m_mux.switchvalue = (uint32_t)strtoul($3+1, NULL, 10);
         break;
       default:
         /* error: unknown mux type */
         break;
       }
     free($3);
     }

    current_signal->m_start_bit = (uint8_t)$5;
    current_signal->m_signal_size = (uint8_t)$7;

    current_signal->m_byte_order = (dbcByteOrder_t)$9;
    current_signal->m_value_type = ($10 == 0)?DBC_VALUETYPE_UNSIGNED:DBC_VALUETYPE_UNSIGNED;

    current_signal->m_factor = $12;
    current_signal->m_offset = $14;
    current_signal->m_minimum = $17;
    current_signal->m_maximum = $19;

    current_signal->m_unit = $21; free($21);

    current_message->AddSignal(current_signal);
    current_signal = NULL;
    }
    ;

double_val:
      T_DOUBLE_VAL  { $$ = $1; }
    | T_NAN         { $$ = NAN; }
    | T_INT_VAL     { $$ = (double)$1; }
    ;

signal_mux:
      { $$ = NULL; }
    | T_ID { $$ = $1; }
    ;

signal_endian:
    T_INT_VAL { $$ = $1; }
    ;

signal_sign:
    T_PLUS  { $$ = 0; }
  | T_MINUS { $$ = 1; }
    ;

signal_start:  T_INT_VAL  { $$ = $1; };
signal_length: T_INT_VAL  { $$ = $1; };
signal_scale:  double_val { $$ = $1; };
signal_offset: double_val { $$ = $1; };
signal_min:    double_val { $$ = $1; };
signal_max:    double_val { $$ = $1; };

receiver_list:
    receiver
  | receiver T_COMMA receiver_list
    ;

receiver:
    T_ID
    {
    if (current_signal == NULL) current_signal = new dbcSignal();
    current_signal->AddReceiver(std::string($1)); free($1);
    }
    ;

/************************************************************************/
/* value_section_list (VAL_)                                            */
/************************************************************************/

value_section_list:
  | value_section_list value_section;

/* VAL_ 792 GTW_updateInProgress 1 "IN_PROGRESS" 2 "IN_PROGRESS_NOT_USED"; */
value_section:
    value_list T_SEMICOLON
    ;

value_list:
  | T_VAL T_INT_VAL T_ID T_INT_VAL T_STRING_VAL
    {
    dbcMessage* m = current_dbc->m_messages.FindMessage((uint32_t)$2);
    if (m == NULL)
      {
      yyerror(current_dbc, "VAL_ message not found");
      free($3); free($5);
      YYABORT;
      }
    current_signal = m->FindSignal(std::string($3));
    if (current_signal == NULL)
      {
      yyerror(current_dbc, "VAL_ signal not found (in message)");
      free($3); free($5);
      YYABORT;
      }
    current_signal->AddValue((uint32_t)$4, std::string($5));
    free($3); free($5);
    }
  |
   value_list T_INT_VAL T_STRING_VAL
    {
    current_signal->AddValue((uint32_t)$2, std::string($3));
    free($3);
    }
    ;

/************************************************************************/
/* comment_section_list (CM_)                                           */
/************************************************************************/

comment_section_list:
  | comment_section_list comment_section;

/* CM_ "This is my comment"; */
comment_section:
    T_CM                     T_STRING_VAL T_SEMICOLON
    {
    current_dbc->m_comments.AddComment($2); free($2);
    }
  | T_CM T_BU T_ID           T_STRING_VAL T_SEMICOLON
    {
    dbcNode* n = current_dbc->m_nodes.FindNode(std::string($3));
    if (n != NULL)
      {
      n->AddComment($4);
      }
    else
      {
      yyerror(current_dbc, "BU_ node not found");
      free($3); free($4);
      YYABORT;
      }
    free($3); free($4);
    }
  | T_CM T_BO T_INT_VAL      T_STRING_VAL T_SEMICOLON
    {
    dbcMessage* m = current_dbc->m_messages.FindMessage((uint32_t)$3);
    if (m != NULL)
      {
      m->AddComment($4);
      }
    else
      {
      yyerror(current_dbc, "BO_ message not found");
      free($4);
      YYABORT;
      }
    free($4);
    }
  | T_CM T_SG T_INT_VAL T_ID T_STRING_VAL T_SEMICOLON
    {
    dbcMessage* m = current_dbc->m_messages.FindMessage((uint32_t)$3);
    if (m != NULL)
      {
      dbcSignal* s = m->FindSignal(std::string($4));
      if (s != NULL)
        {
        s->AddComment($5);
        }
      else
        {
        yyerror(current_dbc, "BO_ signal not found (in message)");
        free($4); free($5);
        YYABORT;
        }
      }
    else
      {
      yyerror(current_dbc, "SG_ message not found");
      free($4); free($5);
      YYABORT;
      }
    free($4); free($5);
    }
  | T_CM T_EV T_ID           T_STRING_VAL T_SEMICOLON
    {
    yyerror(current_dbc, "EV_ not currently supported");
    free($3); free($4);
    YYABORT;
    }
    ;
