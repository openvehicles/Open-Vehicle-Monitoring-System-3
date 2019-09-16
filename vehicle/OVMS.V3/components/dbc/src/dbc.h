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
#include <iostream>
#include "dbc_number.h"
#include "can.h"
#include "ovms_metrics.h"

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
  DBC_BYTEORDER_BIG_ENDIAN=0,
  DBC_BYTEORDER_LITTLE_ENDIAN=1
  } dbcByteOrder_t;

typedef enum
  {
  DBC_VALUETYPE_UNSIGNED = '+',
  DBC_VALUETYPE_SIGNED = '-'
  } dbcValueType_t;

uint32_t dbcMessageIdFromString(const char* id);

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
    int GetCount();

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
    const std::string& GetName();
    void SetName(const std::string& name);
    void SetName(const char* name);

  public:
    dbcCommentTable m_comments;

  protected:
    std::string m_name;
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
    int GetCount();

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
    uint32_t GetBaudRate();
    uint32_t GetBTR1();
    uint32_t GetBTR2();
    void SetBaud(const uint32_t baudrate, const uint32_t btr1, const uint32_t btr2);

  public:
    void WriteFile(dbcOutputCallback callback, void* param);

  protected:
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
    const std::string& GetName();
    void SetName(const std::string& name);
    void SetName(const char* name);
    int GetCount();

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param, const char* prefix);

  public:
    dbcValueTableEntry_t m_entrymap;

  protected:
    std::string m_name;
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
    const std::string& GetName();
    void SetName(const std::string& name);
    void SetName(const char* name);
    bool IsMultiplexor();
    bool IsMultiplexSwitch();
    void SetMultiplexor();
    uint32_t GetMultiplexSwitchvalue();
    bool SetMultiplexed(const uint32_t switchvalue);
    bool ClearMultiplexed();
    int GetStartBit();
    int GetSignalSize();
    dbcByteOrder_t GetByteOrder();
    dbcValueType_t GetValueType();
    dbcNumber GetFactor();
    dbcNumber GetOffset();
    dbcNumber GetMinimum();
    dbcNumber GetMaximum();
    void SetStartSize(const int startbit, const int size);
    void SetByteOrder(const dbcByteOrder_t order);
    void SetValueType(const dbcValueType_t type);
    void SetFactorOffset(const dbcNumber factor, const dbcNumber offset);
    void SetFactorOffset(const double factor, const double offset);
    void SetMinMax(const dbcNumber minimum, const dbcNumber maximum);
    void SetMinMax(const double minimum, const double maximum);
    const std::string& GetUnit();
    void SetUnit(const std::string& unit);
    void SetUnit(const char* unit);

  public:
    void Encode(dbcNumber* source, CAN_frame_t* msg);
    dbcNumber Decode(CAN_frame_t* msg);

  public:
    void AssignMetric(OvmsMetric* metric);
    OvmsMetric* GetMetric();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param, std::string messageid);
    void WriteFileValues(dbcOutputCallback callback, void* param, std::string messageid);

  public:
    dbcReceiverList_t m_receivers;
    dbcCommentTable m_comments;
    dbcValueTable m_values;

  protected:
    std::string m_name;
    dbcMultiplexor_t m_mux;
    int m_start_bit;
    int m_signal_size;
    dbcByteOrder_t m_byte_order;
    dbcValueType_t m_value_type;
    dbcNumber m_factor;
    dbcNumber m_offset;
    dbcNumber m_minimum;
    dbcNumber m_maximum;
    std::string m_unit;
    OvmsMetric* m_metric;
  };

typedef std::list<dbcSignal*> dbcSignalList_t;
class dbcMessage
  {
  public:
    dbcMessage();
    dbcMessage(uint32_t id);
    ~dbcMessage();

  public:
    void AddSignal(dbcSignal* signal);
    void RemoveSignal(dbcSignal* signal, bool free=false);
    void RemoveAllSignals(bool free=false);
    dbcSignal* FindSignal(std::string name);
    void Count(int* signals, int* bits, int* covered);

  public:
    void AddComment(const std::string& comment);
    void AddComment(const char* comment);
    void RemoveComment(const std::string& comment);
    bool HasComment(const std::string& comment);
    uint32_t GetID();
    CAN_frame_format_t GetFormat();
    bool IsExtended();
    bool IsStandard();
    void SetID(const uint32_t id);
    int GetSize();
    void SetSize(const int size);
    const std::string& GetName();
    void SetName(const std::string& name);
    void SetName(const char* name);
    const std::string& GetTransmitterNode();
    void SetTransmitterNode(std::string node);
    void SetTransmitterNode(const char* node);
    bool IsMultiplexor();
    dbcSignal* GetMultiplexorSignal();
    void SetMultiplexorSignal(dbcSignal* signal);

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param);
    void WriteFileValues(dbcOutputCallback callback, void* param);

  public:
    dbcSignalList_t m_signals;
    dbcCommentTable m_comments;

  protected:
    dbcSignal* m_multiplexor;
    uint32_t m_id;
    std::string m_name;
    int m_size;
    std::string m_transmitter_node;
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
    dbcMessage* FindMessage(CAN_frame_format_t format, uint32_t id);
    void Count(int* messages, int* signals, int* bits, int* covered);

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteFileComments(dbcOutputCallback callback, void* param);
    void WriteFileValues(dbcOutputCallback callback, void* param);
    void WriteSummary(dbcOutputCallback callback, void* param);

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
    bool LoadFile(const char* name, const char* path, FILE *fd=NULL);
    bool LoadString(const char* name, const char* source, size_t length);
    void WriteFile(dbcOutputCallback callback, void* param);
    void WriteSummary(dbcOutputCallback callback, void* param);
    std::string Status();
    std::string GetName();
    std::string GetPath();
    std::string GetVersion();

  public:
    void LockFile();
    void UnlockFile();
    bool IsLocked();

  public:
    std::string m_name;
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
    int m_locks;
  };

#endif //#ifndef __DBC_H__
