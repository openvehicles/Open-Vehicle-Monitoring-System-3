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
#include <functional>

#define DBC_MAX_LINELENGTH 2048

typedef std::function<void(void*, const char*)> dbcOutputCallback;

typedef enum
  {
  DBC_MUX_NONE=0,
  DBC_MUX_MULTIPLEXOR=1,
  DBC_MUX_MULTIPLEXED=2
  } dbcMultiplex_t;

struct dbcMultiplexor_t
  {
  dbcMultiplex_t multiplexed;
  uint32_t switchvalue;
  };

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
    void AddComment(std::string comment);
    void AddComment(const char* comment);
    void RemoveComment(std::string comment);
    bool HasComment(std::string comment);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param, std::string prefix);

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
    void AddSymbol(std::string symbol);
    void AddSymbol(const char* symbol);
    void RemoveSymbol(std::string symbol);
    bool HasSymbol(std::string symbol);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);

  public:
    dbcNewSymbolList_t m_entrymap;
  };

class dbcNode
  {
  public:
    dbcNode();
    dbcNode(std::string name);
    dbcNode(const char* name);
    ~dbcNode();

  public:
    void AddComment(std::string comment);
    void AddComment(const char* comment);
    void RemoveComment(std::string comment);
    bool HasComment(std::string comment);

  public:
    std::string m_name;
    dbcCommentTable m_comments;
  };

typedef std::map<std::string, dbcNode*> dbcNodeEntry_t;
class dbcNodeTable
  {
  public:
    dbcNodeTable();
    ~dbcNodeTable();

  public:
    void AddNode(dbcNode* node);
    void RemoveNode(dbcNode* node, bool free=false);
    dbcNode* FindNode(std::string name);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param);

  public:
    dbcNodeEntry_t m_entrymap;
  };

class dbcBitTiming
  {
  public:
    dbcBitTiming();
    ~dbcBitTiming();

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);

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
    dbcValueTable(const char* name);
    ~dbcValueTable();

  public:
    void AddValue(uint32_t id, std::string value);
    void AddValue(uint32_t id, const char* value);
    void RemoveValue(uint32_t id);
    bool HasValue(uint32_t id);
    std::string GetValue(uint32_t id);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param, const char* prefix);

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
    void AddValueTable(std::string name, dbcValueTable* vt);
    void AddValueTable(const char* name, dbcValueTable* vt);
    void RemoveValueTable(std::string name, bool free=false);
    dbcValueTable* FindValueTable(std::string name);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);

  public:
    dbcValueTableTableEntry_t m_entrymap;
  };

typedef std::list<std::string> dbcReceiverList_t;
class dbcSignal
  {
  public:
    dbcSignal();
    dbcSignal(std::string name);
    ~dbcSignal();

  public:
    void AddReceiver(std::string receiver);
    void RemoveReceiver(std::string receiver);
    bool HasReceiver(std::string receiver);
    void AddComment(std::string comment);
    void AddComment(const char* comment);
    void RemoveComment(std::string comment);
    bool HasComment(std::string comment);
    void AddValue(uint32_t id, std::string value);
    void RemoveValue(uint32_t id);
    bool HasValue(uint32_t id);
    std::string GetValue(uint32_t id);

  public:
    bool SetBitsDBC(std::string dbcval);
    bool SetFactorOffsetDBC(std::string dbcval);
    bool SetMinMaxDBC(std::string dbcval);

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param, std::string messageid);
    void WriteFileValues(dbcOutputCallback callback, void* param, std::string messageid);

  public:
    std::string m_name;
    dbcMultiplexor_t m_mux;
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
    dbcCommentTable m_comments;
    dbcValueTable m_values;
  };

typedef std::list<dbcSignal*> dbcSignalList_t;
class dbcMessage
  {
  public:
    dbcMessage();
    dbcMessage(uint32_t id);
    ~dbcMessage();

  public:
    void AddComment(std::string comment);
    void AddComment(const char* comment);
    void RemoveComment(std::string comment);
    bool HasComment(std::string comment);
    void AddSignal(dbcSignal* signal);
    void RemoveSignal(dbcSignal* signal, bool free=false);
    dbcSignal* FindSignal(std::string name);
    void Count(int* signals, int* bits, int* covered);

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param);
    void WriteFileValues(dbcOutputCallback callback, void* param);

  public:
    uint32_t m_id;
    std::string m_name;
    uint8_t m_size;
    std::string m_transmitter_node;
    dbcSignalList_t m_signals;
    dbcCommentTable m_comments;
    std::string m_multiplexor;
  };

typedef std::map<uint32_t, dbcMessage*> dbcMessageEntry_t;
class dbcMessageTable
  {
  public:
    dbcMessageTable();
    ~dbcMessageTable();

  public:
    void AddMessage(uint32_t id, dbcMessage* message);
    void RemoveMessage(uint32_t id, bool free=false);
    dbcMessage* FindMessage(uint32_t id);
    void Count(int* messages, int* signals, int* bits, int* covered);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param);
    void WriteFileValues(dbcOutputCallback callback, void* param);

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

  public:
    bool LoadFile(const char* path, FILE *fd=NULL);
    bool LoadString(const char* source, size_t length);
    void WriteFile(dbcOutputCallback callback, void* param);
    std::string Status();

  public:
    std::string m_path;
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

#endif //#ifndef __DBC_H__
