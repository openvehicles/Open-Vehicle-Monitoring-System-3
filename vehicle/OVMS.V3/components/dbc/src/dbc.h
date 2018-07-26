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

#ifndef __DBC_H__
#define __DBC_H__

#include <string>
#include <map>
#include <list>
#include "ovms_command.h"
#include "ovms_mutex.h"
#include "ovms_utils.h"

#define DBC_MAX_LINELENGTH 2048

typedef enum
  {
  DBC_BYTEORDER_LITTLE_ENDIAN=0,
  DBC_BYTEORDER_BIG_ENDIAN=1
  } dbcByteOrder_t;

typedef enum
  {
  DBC_VALUETYPE_UNSIGNED = '+',
  DBC_VALUETYPE_SIGNED = '-'
  } dbcValueType_t;

typedef std::list<std::string> dbcCommentList_t;
class dbcCommentTable
  {
  public:
    dbcCommentTable();
    ~dbcCommentTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcCommentTable* source);

  public:
    dbcCommentList_t m_entrymap;
  };

typedef std::list<std::string> dbcNewSymbolList_t;
class dbcNewSymbolTable
  {
  public:
    dbcNewSymbolTable();
    ~dbcNewSymbolTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcNewSymbolTable* source);

  public:
    dbcNewSymbolList_t m_entrymap;
  };

class dbcNode
  {
  public:
    dbcNode();
    dbcNode(std::string name);
    ~dbcNode();

  public:
    std::string m_name;
    dbcCommentList_t m_comments;
  };

typedef std::list<dbcNode*> dbcNodeList_t;
class dbcNodeTable
  {
  public:
    dbcNodeTable();
    ~dbcNodeTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcNodeTable* source);

  public:
    dbcNodeList_t m_entrymap;
  };

class dbcBitTiming
  {
  public:
    dbcBitTiming();
    ~dbcBitTiming();

  public:
    void EmptyContent();
    void ReplaceContent(dbcBitTiming* source);

  public:
    uint32_t m_baudrate;
    uint32_t m_btr1;
    uint32_t m_btr2;
  };

typedef std::map<uint32_t, std::string> dbcValueTableEntry_t;
class dbcValueTable
  {
  public:
    dbcValueTable();
    dbcValueTable(std::string name);
    ~dbcValueTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcValueTable* source);

  public:
    std::string m_name;
    dbcValueTableEntry_t m_entrymap;
  };

typedef std::map<std::string, dbcValueTable*> dbcValueTableTableEntry_t;
class dbcValueTableTable
  {
  public:
    dbcValueTableTable();
    ~dbcValueTableTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcValueTableTable* source);

  public:
    dbcValueTableTableEntry_t m_entrymap;
  };

typedef std::map<double, std::string> dbcSignalValueEntry_t;
class dbcSignalValue
  {
  public:
    dbcSignalValue();
    ~dbcSignalValue();

  public:
    dbcSignalValueEntry_t m_entrymap;
  };

typedef std::list<std::string> dbcReceiverList_t;
class dbcSignal
  {
  public:
    dbcSignal();
    ~dbcSignal();

  public:
    std::string m_name;
    uint32_t m_multiplexor_switch_value;
    bool m_multiplexed;
    uint8_t m_start_bit;
    uint8_t m_signal_size;
    dbcByteOrder_t m_byte_order;
    dbcValueType_t m_value_type;
    double m_factor;
    double m_offset;
    double m_minimum;
    double m_maximum;
    std::string m_unit;
    dbcReceiverList_t m_receivers;
    dbcSignalValue* m_signal_values;
    dbcCommentList_t m_comments;
  };

typedef std::list<dbcSignal*> dbcSignalList_t;
class dbcMessage
  {
  public:
    dbcMessage();
    dbcMessage(uint32_t id);
    ~dbcMessage();

  public:
    uint32_t m_id;
    std::string m_name;
    uint8_t m_size;
    std::string m_transmitter_node;
    dbcSignalList_t m_signals;
    dbcCommentList_t m_comments;
  };

typedef std::map<uint32_t, dbcMessage*> dbcMessageEntry_t;
class dbcMessageTable
  {
  public:
    dbcMessageTable();
    ~dbcMessageTable();

  public:
    void EmptyContent();
    void ReplaceContent(dbcMessageTable* source);

  public:
    dbcMessageEntry_t m_entrymap;
  };

class dbcfile
  {
  public:
    dbcfile();
    ~dbcfile();

  private:
    void FreeAllocations();
    bool LoadParseOneLine(int linenumber, std::string line);

  public:
    bool LoadFile(const char* path);
    bool LoadString(const char* source);
    void ReplaceContent(dbcfile* dbc);
    void ShowStatusLine(OvmsWriter* writer);

  public:
    std::string m_version;
    dbcNewSymbolTable m_newsymbols;
    dbcBitTiming m_bittiming;
    dbcNodeTable m_nodes;
    dbcValueTableTable m_values;
    dbcMessageTable m_messages;
    dbcCommentTable m_comments;

  private:
    dbcMessage* m_lastmsg;
  };

typedef std::map<std::string, dbcfile*> dbcLoadedFiles_t;

class dbc
  {
  public:
    dbc();
    ~dbc();

  public:
    bool LoadFile(const char* name, const char* path);
    bool LoadString(const char* name, const char* source);
    bool Unload(const char* name);

  public:
    dbcfile* Find(const char* name);

  public:
    OvmsMutex m_mutex;
    dbcLoadedFiles_t m_dbclist;
  };

extern dbc MyDBC;

#endif //#ifndef __DBC_H__
