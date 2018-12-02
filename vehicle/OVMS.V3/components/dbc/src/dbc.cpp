/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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

#include "ovms_log.h"
static const char *TAG = "dbc";

#include <algorithm>
#include <list>
#include <vector>
#include <sstream>
#include <string.h>
#include <stdlib.h>
#include "dbc.h"
#include "dbc_tokeniser.hpp"
#include "dbc_parser.hpp"
#ifdef CONFIG_OVMS
#include "ovms_config.h"
#endif // #ifdef CONFIG_OVMS

// N.B. The conditions on CONFIG_OVMS are to allow this module to be
//      compiled and tested outside the OVMS subsystem.

////////////////////////////////////////////////////////////////////////
// dbcComment...

dbcCommentTable::dbcCommentTable()
  {
  }

dbcCommentTable::~dbcCommentTable()
  {
  EmptyContent();
  }

void dbcCommentTable::AddComment(std::string comment)
  {
  m_entrymap.push_back(comment);
  }

void dbcCommentTable::AddComment(const char* comment)
  {
  m_entrymap.push_back(std::string(comment));
  }

void dbcCommentTable::RemoveComment(std::string comment)
  {
  m_entrymap.remove(comment);
  }

bool dbcCommentTable::HasComment(std::string comment)
  {
  auto it = std::find(m_entrymap.begin(), m_entrymap.end(), comment);
  return (it != m_entrymap.end());
  }

void dbcCommentTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcCommentTable::WriteFile(dbcOutputCallback callback,
                                void* param,
                                std::string prefix)
  {
  for (dbcCommentList_t::iterator it=m_entrymap.begin();
       it != m_entrymap.end();
       ++it)
    {
    callback(param,prefix.c_str());
    callback(param,it->c_str());
    callback(param,"\"\n");
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcNewSymbol...

dbcNewSymbolTable::dbcNewSymbolTable()
  {
  }

dbcNewSymbolTable::~dbcNewSymbolTable()
  {
  EmptyContent();
  }

void dbcNewSymbolTable::AddSymbol(std::string symbol)
  {
  m_entrymap.push_back(symbol);
  }

void dbcNewSymbolTable::AddSymbol(const char* symbol)
  {
  m_entrymap.push_back(std::string(symbol));
  }

void dbcNewSymbolTable::RemoveSymbol(std::string symbol)
  {
  m_entrymap.remove(symbol);
  }

bool dbcNewSymbolTable::HasSymbol(std::string symbol)
  {
  auto it = std::find(m_entrymap.begin(), m_entrymap.end(), symbol);
  return (it != m_entrymap.end());
  }

void dbcNewSymbolTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcNewSymbolTable::WriteFile(dbcOutputCallback callback, void* param)
  {
  callback(param,"NS_ :");
  for (dbcNewSymbolList_t::iterator it=m_entrymap.begin();
       it != m_entrymap.end();
       ++it)
    {
    callback(param," ");
    callback(param,it->c_str());
    }
  callback(param,"\n\n");
  }

////////////////////////////////////////////////////////////////////////
// dbcNode...

dbcNode::dbcNode()
  {
  }

dbcNode::dbcNode(std::string name)
  {
  m_name = name;
  }

dbcNode::dbcNode(const char* name)
  {
  m_name = std::string(name);
  }

dbcNode::~dbcNode()
  {
  }

void dbcNode::AddComment(std::string comment)
  {
  m_comments.AddComment(comment);
  }

void dbcNode::AddComment(const char* comment)
  {
  m_comments.AddComment(std::string(comment));
  }

void dbcNode::RemoveComment(std::string comment)
  {
  m_comments.RemoveComment(comment);
  }

bool dbcNode::HasComment(std::string comment)
  {
  return m_comments.HasComment(comment);
  }

dbcNodeTable::dbcNodeTable()
  {
  }

dbcNodeTable::~dbcNodeTable()
  {
  EmptyContent();
  }

void dbcNodeTable::AddNode(dbcNode* node)
  {
  m_entrymap[node->m_name] = node;
  }

void dbcNodeTable::RemoveNode(dbcNode* node, bool free)
  {
  m_entrymap.erase(node->m_name);
  if (free) delete node;
  }

dbcNode* dbcNodeTable::FindNode(std::string name)
  {
  auto search = m_entrymap.find(name);
  if (search != m_entrymap.end())
    return search->second;
  else
    return NULL;
  }

void dbcNodeTable::EmptyContent()
  {
  for (auto it = m_entrymap.begin(); it != m_entrymap.end(); it++)
    {
    delete it->second;
    }
  m_entrymap.clear();
  }

void dbcNodeTable::WriteFile(dbcOutputCallback callback, void* param)
  {
  callback(param, "BU_ :");
  for (dbcNodeEntry_t::iterator it = m_entrymap.begin();
       it != m_entrymap.end();
       it++)
    {
    callback(param," ");
    callback(param, it->second->m_name.c_str());
    }
  callback(param,"\n\n");
  }

void dbcNodeTable::WriteFileComments(dbcOutputCallback callback, void* param)
  {
  std::string prefix;
  for (dbcNodeEntry_t::iterator it=m_entrymap.begin();
       it != m_entrymap.end();
       ++it)
    {
    prefix = std::string("CM_ BU_ ");
    prefix.append(it->second->m_name);
    prefix.append(" \"");
    it->second->m_comments.WriteFile(callback, param, prefix);
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcBitTiming...

dbcBitTiming::dbcBitTiming()
  {
  }

dbcBitTiming::~dbcBitTiming()
  {
  m_baudrate = 0;
  m_btr1 = 0;
  m_btr2 = 0;
  }

void dbcBitTiming::EmptyContent()
  {
  }

void dbcBitTiming::WriteFile(dbcOutputCallback callback, void* param)
  {
  callback(param, "BS_ : ");
  if (m_baudrate != 0)
    {
    char buf[64];
    sprintf(buf,"%u:%u,%u",m_baudrate,m_btr1,m_btr2);
    callback(param, buf);
    }
  callback(param, "\n\n");
  }

////////////////////////////////////////////////////////////////////////
// dbcValueTable...

dbcValueTable::dbcValueTable()
  {
  }

dbcValueTable::dbcValueTable(std::string name)
  {
  m_name = name;
  }

dbcValueTable::dbcValueTable(const char* name)
  {
  m_name = std::string(name);
  }

void dbcValueTable::AddValue(uint32_t id, std::string value)
  {
  m_entrymap[id] = value;
  }

void dbcValueTable::AddValue(uint32_t id, const char* value)
  {
  m_entrymap[id] = std::string(value);
  }

void dbcValueTable::RemoveValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    m_entrymap.erase(search);
  }

bool dbcValueTable::HasValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  return (search != m_entrymap.end());
  }

std::string dbcValueTable::GetValue(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    return search->second;
  else
    return std::string("");
  }

dbcValueTable::~dbcValueTable()
  {
  EmptyContent();
  }

void dbcValueTable::EmptyContent()
  {
  m_entrymap.clear();
  }

void dbcValueTable::WriteFile(dbcOutputCallback callback, void* param, const char* prefix)
  {
  if (prefix != NULL)
    {
    callback(param,prefix);
    }
  else
    {
    callback(param, "VAL_TABLE_ ");
    callback(param, m_name.c_str());
    }
  for (dbcValueTableEntry_t::iterator it = m_entrymap.begin();
       it != m_entrymap.end();
       it++)
    {
    char buf[40];
    sprintf(buf," %d \"",it->first);
    callback(param,buf);
    callback(param,it->second.c_str());
    callback(param,"\"");
    }
  callback(param,";\n");
  }

dbcValueTableTable::dbcValueTableTable()
  {
  }

dbcValueTableTable::~dbcValueTableTable()
  {
  EmptyContent();
  }

void dbcValueTableTable::AddValueTable(std::string name, dbcValueTable* vt)
  {
  m_entrymap[name] = vt;
  }

void dbcValueTableTable::AddValueTable(const char* name, dbcValueTable* vt)
  {
  m_entrymap[std::string(name)] = vt;
  }

void dbcValueTableTable::RemoveValueTable(std::string name, bool free)
  {
  auto search = m_entrymap.find(name);
  if (search != m_entrymap.end())
    {
    if (free) delete search->second;
    m_entrymap.erase(search);
    }
  }

dbcValueTable* dbcValueTableTable::FindValueTable(std::string name)
  {
  auto search = m_entrymap.find(name);
  if (search != m_entrymap.end())
    return search->second;
  else
    return NULL;
  }

void dbcValueTableTable::EmptyContent()
  {
  dbcValueTableTableEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    delete it->second;
    ++it;
    }
  m_entrymap.clear();
  }

void dbcValueTableTable::WriteFile(dbcOutputCallback callback, void* param)
  {
  for (dbcValueTableTableEntry_t::iterator itt = m_entrymap.begin();
       itt != m_entrymap.end();
       itt++)
    itt->second->WriteFile(callback, param, NULL);
  callback(param, "\n");
  }

////////////////////////////////////////////////////////////////////////
// dbcSignal

dbcSignal::dbcSignal()
  {
  }

dbcSignal::dbcSignal(std::string name)
  {
  m_name = name;
  }

dbcSignal::~dbcSignal()
  {
  }

void dbcSignal::AddReceiver(std::string receiver)
  {
  m_receivers.push_back(receiver);
  }

void dbcSignal::RemoveReceiver(std::string receiver)
  {
  m_receivers.remove(receiver);
  }

bool dbcSignal::HasReceiver(std::string receiver)
  {
  auto it = std::find(m_receivers.begin(), m_receivers.end(), receiver);
  return (it != m_receivers.end());
  }

void dbcSignal::AddComment(std::string comment)
  {
  m_comments.AddComment(comment);
  }

void dbcSignal::AddComment(const char* comment)
  {
  m_comments.AddComment(comment);
  }

void dbcSignal::RemoveComment(std::string comment)
  {
  m_comments.RemoveComment(comment);
  }

bool dbcSignal::HasComment(std::string comment)
  {
  return m_comments.HasComment(comment);
  }

void dbcSignal::AddValue(uint32_t id, std::string value)
  {
  m_values.AddValue(id,value);
  }

void dbcSignal::RemoveValue(uint32_t id)
  {
  m_values.RemoveValue(id);
  }

bool dbcSignal::HasValue(uint32_t id)
  {
  return m_values.HasValue(id);
  }

std::string dbcSignal::GetValue(uint32_t id)
  {
  return m_values.GetValue(id);
  }

bool dbcSignal::SetBitsDBC(std::string dbcval)
  {
  // Set bits from a DBC style specification like 23|2@0+
  const char *p = dbcval.c_str();

  m_start_bit = atoi(p);

  p = strchr(p,'|');
  if (p==NULL) return false;

  m_signal_size = atoi(p+1);

  p = strchr(p,'@');
  if (p==NULL) return false;

  if (strlen(p)<3) return false;
  m_byte_order = (p[1]=='0')?DBC_BYTEORDER_LITTLE_ENDIAN:DBC_BYTEORDER_BIG_ENDIAN;
  m_value_type = (p[2]=='-')?DBC_VALUETYPE_SIGNED:DBC_VALUETYPE_UNSIGNED;

  return true;
  }

bool dbcSignal::SetFactorOffsetDBC(std::string dbcval)
  {
  // Set factor and offset from a DBC style specification like (1,0)
  const char *p = dbcval.c_str();

  if (*p != '(') return false;
  m_factor = atof(p+1);

  p = strchr(p,',');
  if (p==NULL) return false;

  m_offset = atof(p+1);

  return true;
  }

bool dbcSignal::SetMinMaxDBC(std::string dbcval)
  {
  // Set min and max from a DBC style specification like [0,0]
  const char *p = dbcval.c_str();

  if (*p != '[') return false;
  m_minimum = atof(p+1);

  p = strchr(p,'|');
  if (p==NULL) return false;

  m_maximum = atof(p+1);

  return true;
  }

void dbcSignal::WriteFile(dbcOutputCallback callback, void* param)
  {
  char buf[64];
  callback(param, "  SG_ ");
  callback(param, m_name.c_str());
  switch (m_mux.multiplexed)
    {
    case DBC_MUX_MULTIPLEXOR:
      callback(param, " M");
      break;
    case DBC_MUX_MULTIPLEXED:
      sprintf(buf," m%u",m_mux.switchvalue);
      break;
    default:
      break;
    }
  callback(param, " : ");
  sprintf(buf,"%u|%u@",m_start_bit,m_signal_size);
  callback(param, buf);
  switch(m_byte_order)
    {
    case DBC_BYTEORDER_BIG_ENDIAN:
      callback(param, "1");
      break;
    default:
      callback(param, "0");
      break;
    }
  switch(m_value_type)
    {
    case DBC_VALUETYPE_SIGNED:
      callback(param, "- ");
      break;
    default:
      callback(param, "+ ");
      break;
    }
  sprintf(buf,"(%f,%f) ",m_factor,m_offset);
  callback(param, buf);
  sprintf(buf,"[%f|%f] \"",m_minimum,m_maximum);
  callback(param, buf);
  callback(param, m_unit.c_str());
  callback(param, "\" ");

  bool first=true;
  for (std::string receiver : m_receivers)
    {
    if (!first)
      callback(param, ",");
    callback(param, receiver.c_str());
    first=false;
    }

  callback(param, "\n");
  }

void dbcSignal::WriteFileComments(dbcOutputCallback callback,
                                  void* param,
                                  std::string messageid)
  {
  std::string prefix("CM_ SG_ ");
  prefix.append(messageid);
  prefix.append(" ");
  prefix.append(m_name);
  prefix.append(" \"");
  m_comments.WriteFile(callback, param, prefix);
  }

void dbcSignal::WriteFileValues(dbcOutputCallback callback,
                                  void* param,
                                  std::string messageid)
  {
  std::ostringstream ss;
  ss << "VAL_ ";
  ss << messageid;
  ss << " ";
  ss << m_name;
  std::string prefix = ss.str();
  m_values.WriteFile(callback, param, prefix.c_str());
  }

////////////////////////////////////////////////////////////////////////
// dbcMessage...

dbcMessage::dbcMessage()
  {
  }

dbcMessage::dbcMessage(uint32_t id)
  {
  m_id = id;
  }

dbcMessage::~dbcMessage()
  {
  }

void dbcMessage::AddComment(std::string comment)
  {
  m_comments.AddComment(comment);
  }

void dbcMessage::AddComment(const char* comment)
  {
  m_comments.AddComment(comment);
  }

void dbcMessage::RemoveComment(std::string comment)
  {
  m_comments.RemoveComment(comment);
  }

bool dbcMessage::HasComment(std::string comment)
  {
  return m_comments.HasComment(comment);
  }

void dbcMessage::AddSignal(dbcSignal* signal)
  {
  m_signals.push_back(signal);
  }

void dbcMessage::RemoveSignal(dbcSignal* signal, bool free)
  {
  m_signals.remove(signal);
  if (free) delete signal;
  }

dbcSignal* dbcMessage::FindSignal(std::string name)
  {
  for (dbcSignal* signal : m_signals)
    {
    if (signal->m_name.compare(name)==0) return signal;
    }
  return NULL;
    }

void dbcMessage::Count(int* signals, int* bits, int* covered)
  {
  *bits = m_size*8;
  *signals = 0;
  *covered = 0;
  for (dbcSignal* signal : m_signals)
    {
    *signals += 1;
    *covered += signal->m_signal_size;
    }
  }

void dbcMessage::WriteFile(dbcOutputCallback callback, void* param)
  {
  char buf[40];
  callback(param, "BO_ ");
  sprintf(buf,"%u ",m_id);
  callback(param, buf);
  callback(param, m_name.c_str());
  sprintf(buf,": %u ",m_size);
  callback(param, buf);
  callback(param, m_transmitter_node.c_str());
  callback(param, "\n");

  for (dbcSignal* signal : m_signals)
    {
    signal->WriteFile(callback, param);
    }

  callback(param, "\n");
  }

void dbcMessage::WriteFileComments(dbcOutputCallback callback, void* param)
  {
  std::ostringstream ss;
  ss << m_id;
  std::string id(ss.str());
  std::string prefix("CM_ BO_ ");
  prefix.append(id);
  prefix.append(" \"");
  m_comments.WriteFile(callback, param, prefix);

  for (dbcSignal* s : m_signals)
    {
    s->WriteFileComments(callback, param, id);
    }
  }

void dbcMessage::WriteFileValues(dbcOutputCallback callback, void* param)
  {
  std::ostringstream ss;
  ss << m_id;
  std::string id(ss.str());

  for (dbcSignal* s : m_signals)
    {
    s->WriteFileValues(callback, param, id);
    }
  }

dbcMessageTable::dbcMessageTable()
  {
  }

dbcMessageTable::~dbcMessageTable()
  {
  EmptyContent();
  }

void dbcMessageTable::AddMessage(uint32_t id, dbcMessage* message)
  {
  m_entrymap[id] = message;
  }

void dbcMessageTable::RemoveMessage(uint32_t id, bool free)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    {
    if (free) delete search->second;
    m_entrymap.erase(search);
    }
  }

dbcMessage* dbcMessageTable::FindMessage(uint32_t id)
  {
  auto search = m_entrymap.find(id);
  if (search != m_entrymap.end())
    return search->second;
  else
    return NULL;
  }

void dbcMessageTable::Count(int* messages, int* signals, int* bits, int* covered)
  {
  *messages = 0;
  *signals = 0;
  *bits = 0;
  *covered = 0;
  for (dbcMessageEntry_t::iterator itt = m_entrymap.begin();
       itt != m_entrymap.end();
       itt++)
    {
    *messages += 1;
    int ms,mb,mc;
    itt->second->Count(&ms, &mb, &mc);
    *signals += ms;
    *bits += mb;
    *covered += mc;
    }
  }

void dbcMessageTable::EmptyContent()
  {
  dbcMessageEntry_t::iterator it=m_entrymap.begin();
  while (it!=m_entrymap.end())
    {
    delete it->second;
    ++it;
    }
  m_entrymap.clear();
  }

void dbcMessageTable::WriteFile(dbcOutputCallback callback, void* param)
  {
  for (dbcMessageEntry_t::iterator itt = m_entrymap.begin();
       itt != m_entrymap.end();
       itt++)
    itt->second->WriteFile(callback, param);
  }

void dbcMessageTable::WriteFileComments(dbcOutputCallback callback, void* param)
  {
  for (dbcMessageEntry_t::iterator it=m_entrymap.begin();
       it != m_entrymap.end();
       ++it)
    {
    it->second->WriteFileComments(callback, param);
    }
  for (dbcMessageEntry_t::iterator it=m_entrymap.begin();
       it != m_entrymap.end();
       ++it)
    {
    it->second->WriteFileValues(callback, param);
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcfile

dbcfile::dbcfile()
  {
  }

dbcfile::~dbcfile()
  {
  FreeAllocations();
  }

void dbcfile::FreeAllocations()
  {
  m_version.clear();
  m_newsymbols.EmptyContent();
  m_bittiming.EmptyContent();
  m_nodes.EmptyContent();
  m_values.EmptyContent();
  m_messages.EmptyContent();
  m_comments.EmptyContent();
  }

bool dbcfile::LoadFile(const char* path, FILE* fd)
  {
  FreeAllocations();

#ifdef CONFIG_OVMS
  if (MyConfig.ProtectedPath(path))
    {
    ESP_LOGW(TAG,"Path %s is protected",path);
    return false;
    }
#endif // #ifdef CONFIG_OVMS

  void yyrestart(FILE *input_file);
  int yyparse (void *YYPARSE_PARAM);
  bool result;
  m_path = path;

  if (fd == NULL)
    {
    fd = fopen(path, "r");
    if (!fd)
      {
      ESP_LOGW(TAG,"Could not open %s for reading",path);
      return false;
      }
    FILE *yyin = fd;
    yyrestart(yyin);
    result = (yyparse ((void *)this) == 0);
    fclose(fd);
    }
  else
    {
    fseek(fd,0,SEEK_SET);
    FILE *yyin = fd;
    yyrestart(yyin);
    result = (yyparse ((void *)this) == 0);
    fseek(fd,0,SEEK_SET);
    }

  return result;
  }

bool dbcfile::LoadString(const char* source, size_t length)
  {
  FreeAllocations();

  void yyrestart(FILE *input_file);
  int yyparse (void *YYPARSE_PARAM);

  YY_BUFFER_STATE buffer = yy_scan_bytes(source, length);
  bool result = (yyparse (this) == 0);
  yy_delete_buffer(buffer);

  return result;
  }

void dbcfile::WriteFile(dbcOutputCallback callback, void* param)
  {
  callback(param,"VERSION \"");
  callback(param,m_version.c_str());
  callback(param,"\"\n\n");

  m_newsymbols.WriteFile(callback, param);
  m_bittiming.WriteFile(callback, param);
  m_nodes.WriteFile(callback, param);
  m_values.WriteFile(callback, param);
  m_messages.WriteFile(callback, param);
  m_comments.WriteFile(callback, param, std::string("CM_ \""));
  m_nodes.WriteFileComments(callback, param);
  m_messages.WriteFileComments(callback, param);
  }

std::string dbcfile::Status()
  {
  std::ostringstream ss;
  int messages, signals, bits, covered;
  m_messages.Count(&messages, &signals, &bits, &covered);
  if (m_version.length() > 0)
    {
    ss << m_version;
    ss << ": ";
    }
  ss << messages;
  ss << " message(s), ";
  ss << signals;
  ss << " signal(s)";
  if (covered>0)
    {
    ss << ", ";
    ss << (int)(covered*100)/bits;
    ss << "% coverage";
    }
  return ss.str();
  }
