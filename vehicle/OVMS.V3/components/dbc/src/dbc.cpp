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
#include <math.h>
#include "dbc.h"
#include "dbc_tokeniser.hpp"
#include "dbc_parser.hpp"
#ifdef CONFIG_OVMS
#include "ovms_config.h"
#endif // #ifdef CONFIG_OVMS

// N.B. The conditions on CONFIG_OVMS are to allow this module to be
//      compiled and tested outside the OVMS subsystem.

////////////////////////////////////////////////////////////////////////
// Helper functions

static inline uint64_t
dbc_extract_bits(uint8_t *candata, unsigned int bpos, unsigned int align, unsigned int shifter, unsigned int pos)
  {
  uint64_t val = (uint64_t)candata[bpos/8];
  unsigned int mask = (1 << shifter) - 1;
  return ((val >> align) & mask) << pos;
  }

static uint64_t
dbc_extract_bits_little_endian(uint8_t *candata, unsigned int bpos, unsigned int bits)
  {
  unsigned int pos, aligner, shifter;
  uint64_t val = 0;

  pos = 0;
  while (bits > 0)
    {
    aligner = bpos % 8;
    shifter = 8 - aligner;
    shifter = MIN(shifter, bits);

    val |= dbc_extract_bits(candata, bpos, aligner, shifter, pos);
    pos += shifter;

    bpos += shifter;
    bits -= shifter;
    }

  return val;
  }

static uint64_t
dbc_extract_bits_big_endian(uint8_t *candata, unsigned int bpos, unsigned int bits)
  {
  unsigned int pos, aligner, slicer;
  uint64_t val = 0;

  pos = bits;
  while (bits > 0)
    {
    slicer = (bpos % 8) + 1;
    slicer = MIN(slicer, bits);
    aligner = ((bpos % 8) + 1) - slicer;

    pos -= slicer;
    val |= dbc_extract_bits(candata, bpos, aligner, slicer, pos);

    bpos = ((bpos / 8) + 1) * 8 + 7;
    bits -= slicer;
    }

  return val;
  }

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

int dbcNewSymbolTable::GetCount()
  {
  return m_entrymap.size();
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

const std::string& dbcNode::GetName()
  {
  return m_name;
  }

void dbcNode::SetName(const std::string& name)
  {
  m_name = name;
  }

void dbcNode::SetName(const char* name)
  {
  m_name = std::string(name);
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
  m_entrymap[node->GetName()] = node;
  }

void dbcNodeTable::RemoveNode(dbcNode* node, bool free)
  {
  m_entrymap.erase(node->GetName());
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

int dbcNodeTable::GetCount()
  {
  return m_entrymap.size();
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
    callback(param, it->second->GetName().c_str());
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
    prefix.append(it->second->GetName());
    prefix.append(" \"");
    it->second->m_comments.WriteFile(callback, param, prefix);
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcBitTiming...

dbcBitTiming::dbcBitTiming()
  {
  m_baudrate = 0;
  m_btr1 = 0;
  m_btr2 = 0;
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

uint32_t dbcBitTiming::GetBaudRate()
  {
  return m_baudrate;
  }

uint32_t dbcBitTiming::GetBTR1()
  {
  return m_btr1;
  }

uint32_t dbcBitTiming::GetBTR2()
  {
  return m_btr2;
  }

void dbcBitTiming::SetBaud(const uint32_t baudrate, const uint32_t btr1, const uint32_t btr2)
  {
  m_baudrate = baudrate;
  m_btr1 = btr1;
  m_btr2 = btr2;
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

const std::string& dbcValueTable::GetName()
  {
  return m_name;
  }

void dbcValueTable::SetName(const std::string& name)
  {
  m_name = name;
  }

void dbcValueTable::SetName(const char* name)
  {
  m_name = std::string(name);
  }

int dbcValueTable::GetCount()
  {
  return m_entrymap.size();
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
  if (m_entrymap.size() > 0)
    {
    for (dbcValueTableTableEntry_t::iterator itt = m_entrymap.begin();
         itt != m_entrymap.end();
         itt++)
      itt->second->WriteFile(callback, param, NULL);
    callback(param, "\n");
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcSignal

dbcSignal::dbcSignal()
  {
  m_start_bit = 0;
  m_signal_size = 0;
  m_metric = NULL;
  }

dbcSignal::dbcSignal(std::string name)
  {
  m_start_bit = 0;
  m_signal_size = 0;
  m_name = name;
  m_metric = MyMetrics.Find(name.c_str());
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

const std::string& dbcSignal::GetName()
  {
  return m_name;
  }

void dbcSignal::SetName(const std::string& name)
  {
  m_name = name;

  std::string mappedname(name);
  std::replace( mappedname.begin(), mappedname.end(), '_', '.');
  m_metric = MyMetrics.Find(mappedname.c_str());
  }

void dbcSignal::SetName(const char* name)
  {
  SetName(std::string(name));
  }

bool dbcSignal::IsMultiplexor()
  {
  return (m_mux.multiplexed == DBC_MUX_MULTIPLEXOR);
  }

bool dbcSignal::IsMultiplexSwitch()
  {
  return (m_mux.multiplexed == DBC_MUX_MULTIPLEXED);
  }

void dbcSignal::SetMultiplexor()
  {
  m_mux.multiplexed = DBC_MUX_MULTIPLEXOR;
  }

uint32_t dbcSignal::GetMultiplexSwitchvalue()
  {
  return m_mux.switchvalue;
  }

bool dbcSignal::SetMultiplexed(const uint32_t switchvalue)
  {
  if (m_mux.multiplexed == DBC_MUX_MULTIPLEXOR)
    {
    return false;
    }
  else
    {
    m_mux.multiplexed = DBC_MUX_MULTIPLEXED;
    m_mux.switchvalue = switchvalue;
    return true;
    }
  }

bool dbcSignal::ClearMultiplexed()
  {
  if (m_mux.multiplexed == DBC_MUX_MULTIPLEXOR)
    {
    return false;
    }
  else
    {
    m_mux.multiplexed = DBC_MUX_NONE;
    m_mux.switchvalue = 0;
    return true;
    }
  }

int dbcSignal::GetStartBit()
  {
  return m_start_bit;
  }

int dbcSignal::GetSignalSize()
  {
  return m_signal_size;
  }

dbcByteOrder_t dbcSignal::GetByteOrder()
  {
  return m_byte_order;
  }

dbcValueType_t dbcSignal::GetValueType()
  {
  return m_value_type;
  }

dbcNumber dbcSignal::GetFactor()
  {
  return m_factor;
  }

dbcNumber dbcSignal::GetOffset()
  {
  return m_offset;
  }

dbcNumber dbcSignal::GetMinimum()
  {
  return m_minimum;
  }

dbcNumber dbcSignal::GetMaximum()
  {
  return m_maximum;
  }

void dbcSignal::SetStartSize(const int startbit, const int size)
  {
  m_start_bit = startbit;
  m_signal_size = size;
  }

void dbcSignal::SetByteOrder(const dbcByteOrder_t order)
  {
  m_byte_order = order;
  }

void dbcSignal::SetValueType(const dbcValueType_t type)
  {
  m_value_type = type;
  }

void dbcSignal::SetFactorOffset(const dbcNumber factor, const dbcNumber offset)
  {
  m_factor = factor;
  m_offset = offset;
  }

void dbcSignal::SetFactorOffset(const double factor, const double offset)
  {
  m_factor = factor;
  m_offset = offset;
  }

void dbcSignal::SetMinMax(const dbcNumber minimum, const dbcNumber maximum)
  {
  m_minimum = minimum;
  m_maximum = maximum;
  }

void dbcSignal::SetMinMax(const double minimum, const double maximum)
  {
  m_minimum = minimum;
  m_maximum = maximum;
  }

const std::string& dbcSignal::GetUnit()
  {
  return m_unit;
  }

void dbcSignal::SetUnit(const std::string& unit)
  {
  m_unit = unit;
  }

void dbcSignal::SetUnit(const char* unit)
  {
  m_unit = std::string(unit);
  }

void dbcSignal::Encode(dbcNumber* source, CAN_frame_t* msg)
  {
  // TODO: An efficient encoding of the signal
  }

dbcNumber dbcSignal::Decode(CAN_frame_t* msg)
  {
  uint64_t val;
  dbcNumber result;

  if (m_byte_order == DBC_BYTEORDER_BIG_ENDIAN)
    val = dbc_extract_bits_big_endian(msg->data.u8,m_start_bit,m_signal_size);
  else
    val = dbc_extract_bits_little_endian(msg->data.u8,m_start_bit,m_signal_size);

  if (m_value_type == DBC_VALUETYPE_UNSIGNED)
    result.Cast((uint32_t)val, DBC_NUMBER_INTEGER_UNSIGNED);
  else
    result.Cast((uint32_t)val, DBC_NUMBER_INTEGER_SIGNED);

  // Apply factor and offset
  if (!(m_factor == 1))
    {
    result = (result * m_factor);
    }
  if (!(m_offset == 0))
    {
    result = (result + m_offset);
    }

  return result;
  }

void dbcSignal::AssignMetric(OvmsMetric* metric)
  {
  m_metric = metric;
  }

OvmsMetric* dbcSignal::GetMetric()
  {
  return m_metric;
  }

void dbcSignal::WriteFile(dbcOutputCallback callback, void* param)
  {
  std::ostringstream ss;

  ss << "  SG_ ";
  ss << m_name;
  switch (m_mux.multiplexed)
    {
    case DBC_MUX_MULTIPLEXOR:
      ss << " M";
      break;
    case DBC_MUX_MULTIPLEXED:
      {
      ss << " m";
      ss << m_mux.switchvalue;
      }
      break;
    default:
      break;
    }
  ss << " : ";
  ss << m_start_bit;
  ss << '|';
  ss << m_signal_size;
  ss << '@';
  ss << ((m_byte_order == DBC_BYTEORDER_BIG_ENDIAN)?"1":"0");
  ss << ((m_value_type == DBC_VALUETYPE_SIGNED)?"- ":"+ ");
  ss << '(';
  ss << m_factor;
  ss << ',';
  ss << m_offset;
  ss << ") [";
  ss << m_minimum;
  ss << '|';
  ss << m_maximum;
  ss << "] \"";
  ss << m_unit;
  ss << "\" ";

  bool first=true;
  for (std::string receiver : m_receivers)
    {
    if (!first) { ss << ","; }
    ss << receiver;
    first=false;
    }

  ss << "\n";
  callback(param, ss.str().c_str());
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
  if (m_values.GetCount()>0)
    {
    std::ostringstream ss;
    ss << "VAL_ ";
    ss << messageid;
    ss << " ";
    ss << m_name;
    std::string prefix = ss.str();
    m_values.WriteFile(callback, param, prefix.c_str());
    }
  }

////////////////////////////////////////////////////////////////////////
// dbcMessage...

dbcMessage::dbcMessage()
  {
  m_id = 0;
  m_size = 0;
  m_multiplexor = NULL;
  }

dbcMessage::dbcMessage(uint32_t id)
  {
  m_size = 0;
  m_multiplexor = NULL;
  m_id = id;
  }

dbcMessage::~dbcMessage()
  {
  }

void dbcMessage::AddComment(const std::string& comment)
  {
  m_comments.AddComment(comment);
  }

void dbcMessage::AddComment(const char* comment)
  {
  m_comments.AddComment(comment);
  }

void dbcMessage::RemoveComment(const std::string& comment)
  {
  m_comments.RemoveComment(comment);
  }

bool dbcMessage::HasComment(const std::string& comment)
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
    if (signal->GetName().compare(name)==0) return signal;
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
    *covered += signal->GetSignalSize();
    }
  }

uint32_t dbcMessage::GetID()
  {
  return m_id;
  }

CAN_frame_format_t dbcMessage::GetFormat()
  {
  return ((m_id & 0x80000000) == 0)?CAN_frame_std:CAN_frame_ext;
  }

bool dbcMessage::IsExtended()
  {
  return ((m_id & 0x80000000) != 0);
  }

bool dbcMessage::IsStandard()
  {
  return ((m_id & 0x80000000) == 0);
  }

void dbcMessage::SetID(const uint32_t id)
  {
  m_id = id;
  }

int dbcMessage::GetSize()
  {
  return m_size;
  }

void dbcMessage::SetSize(const int size)
  {
  m_size = size;
  }

const std::string& dbcMessage::GetName()
  {
  return m_name;
  }

void dbcMessage::SetName(const std::string& name)
  {
  m_name = name;
  }

void dbcMessage::SetName(const char* name)
  {
  m_name = std::string(name);
  }

const std::string& dbcMessage::GetTransmitterNode()
  {
  return m_transmitter_node;
  }

void dbcMessage::SetTransmitterNode(std::string node)
  {
  m_transmitter_node = node;
  }

void dbcMessage::SetTransmitterNode(const char* node)
  {
  m_transmitter_node = std::string(node);
  }

bool dbcMessage::IsMultiplexor()
  {
  return (m_multiplexor != NULL);
  }

dbcSignal* dbcMessage::GetMultiplexorSignal()
  {
  return m_multiplexor;
  }

void dbcMessage::SetMultiplexorSignal(dbcSignal* signal)
  {
  m_multiplexor = signal;
  if (signal != NULL)
    {
    signal->SetMultiplexor();
    }
  }

void dbcMessage::WriteFile(dbcOutputCallback callback, void* param)
  {
  std::ostringstream ss;
  ss << "BO_ ";
  ss << m_id;
  ss << " ";
  ss << m_name;
  ss << ": ";
  ss << m_size;
  ss << " ";
  ss << m_transmitter_node;
  ss << "\n";
  callback(param, ss.str().c_str());

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

dbcMessage* dbcMessageTable::FindMessage(CAN_frame_format_t format, uint32_t id)
  {
  if (format == CAN_frame_ext)
    id |= 0x80000000;
  else
    id &= 0x7FFFFFFF;

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

void dbcMessageTable::WriteSummary(dbcOutputCallback callback, void* param)
  {
  for (dbcMessageEntry_t::iterator itt = m_entrymap.begin();
       itt != m_entrymap.end();
       itt++)
    itt->second->WriteFile(callback, param);
  }

////////////////////////////////////////////////////////////////////////
// dbcfile

dbcfile::dbcfile()
  {
  m_locks = 0;
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

bool dbcfile::LoadFile(const char* name, const char* path, FILE* fd)
  {
  FreeAllocations();
  m_name = std::string(name);

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

bool dbcfile::LoadString(const char* name, const char* source, size_t length)
  {
  FreeAllocations();
  m_name = std::string(name);

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

void dbcfile::WriteSummary(dbcOutputCallback callback, void* param)
  {
  callback(param,"Path:    ");
  callback(param,m_path.c_str());
  callback(param,"\n");

  callback(param,"Version: ");
  callback(param,m_version.c_str());
  callback(param,"\n\n");

  m_messages.WriteSummary(callback, param);
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
  ss << ", ";
  ss << m_locks;
  ss << " lock(s)";

  return ss.str();
  }

std::string dbcfile::GetName()
  {
  return m_name;
  }

std::string dbcfile::GetPath()
  {
  return m_path;
  }

std::string dbcfile::GetVersion()
  {
  return m_version;
  }

void dbcfile::LockFile()
  {
  m_locks++;
  }

void dbcfile::UnlockFile()
  {
  m_locks--;
  }

bool dbcfile::IsLocked()
  {
  return (m_locks > 0);
  }
