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
  DBC_MUX_MULTIPLEXSRC=1,
  DBC_MUX_MULTIPLEXED=2,
  DBC_MUX_MULTIPLEXED_MULTIPLEXSRC=3
  } dbcMultiplex_t;

struct dbcSwitchRange_t
  {
  uint32_t min_val, max_val;
  };

class dbcSignal;
struct dbcMultiplexor_t
  {
  dbcMultiplexor_t();
  dbcMultiplex_t multiplexed;
  uint32_t switchvalue;
  std::list<dbcSwitchRange_t> switchvalues;
  dbcSignal *source;
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
    bool HasComment(std::string comment) const;

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param, std::string prefix) const;

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
    void WriteFile(dbcOutputCallback callback, void* param) const;

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
    void WriteFile(dbcOutputCallback callback, void* param) const;
    void WriteFileComments(dbcOutputCallback callback, void* param) const;

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
    void WriteFile(dbcOutputCallback callback, void* param) const;

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
    bool HasValue(uint32_t id) const;
    std::string GetValue(uint32_t id) const;
    const std::string& GetName() const;
    void SetName(const std::string& name);
    void SetName(const char* name);
    int GetCount() const;
    bool IsEmpty() const;

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param, const char* prefix) const;

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
    void WriteFile(dbcOutputCallback callback, void* param) const;

  public:
    dbcValueTableTableEntry_t m_entrymap;
  };


class dbcMetric
  {
  public:
    virtual ~dbcMetric() {};
    virtual void SetValue(dbcNumber value, metric_unit_t metric) = 0;
    virtual void SetValue(const std::string &value) = 0;
    virtual metric_unit_t DefaultUnit() const = 0;
  };
class dbcOvmsMetric : public dbcMetric
  {
  private:
    OvmsMetric *m_metric;
  public:
    dbcOvmsMetric(OvmsMetric *metric);
    void SetValue(dbcNumber value, metric_unit_t unit) override;
    void SetValue(const std::string &value) override;
    metric_unit_t DefaultUnit() const override;
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
    bool HasValues() const;
    bool HasValue(uint32_t id) const;
    std::string GetValue(uint32_t id) const;

  public:
    const std::string& GetName() const;
    void SetName(const std::string& name);
    void SetName(const char* name);
    bool IsMainMultiplexSource() const;
    bool IsMultiplexor() const;
    bool IsMultiplexSwitch() const;
    void SetMultiplexSource();
    bool IsMultiplexSwitchvalue(uint32_t value) const;
    bool SetMultiplexed(const uint32_t switchvalue);
    void SetMultiplexSource(dbcSignal* source);
    dbcSignal* GetMultiplexSource() const { return m_mux.source;}
    void AddMultiplexRange(const dbcSwitchRange_t &range);
    bool ClearMultiplexed();
    int GetStartBit() const;
    int GetSignalSize() const;
    dbcByteOrder_t GetByteOrder() const;
    dbcValueType_t GetValueType() const;
    dbcNumber GetFactor() const;
    dbcNumber GetOffset() const;
    dbcNumber GetMinimum() const;
    dbcNumber GetMaximum() const;
    void SetStartSize(const int startbit, const int size);
    void SetByteOrder(const dbcByteOrder_t order);
    void SetValueType(const dbcValueType_t type);
    void SetFactorOffset(const dbcNumber factor, const dbcNumber offset);
    void SetFactorOffset(const double factor, const double offset);
    void SetMinMax(const dbcNumber minimum, const dbcNumber maximum);
    void SetMinMax(const double minimum, const double maximum);
    const std::string& GetUnit() const;
    void SetUnit(const std::string& unit);
    void SetUnit(const char* unit);
    metric_unit_t GetMetricUnit() const
      {
      return m_metric_unit;
      }

  public:
    void Encode(dbcNumber* source, CAN_frame_t* msg);

    dbcNumber Decode(const uint8_t* msg, uint8_t size) const;

  public:
    void AssignMetric(OvmsMetric* metric);
    void AttachDbcMetric(dbcMetric* metric);
    dbcMetric* GetMetric() const;

  public:
    void WriteFile(dbcOutputCallback callback, void* param) const;
    void WriteFileComments(dbcOutputCallback callback, void* param, std::string messageid) const;
    void WriteFileValues(dbcOutputCallback callback, void* param, std::string messageid) const;
    void WriteFileExtended(dbcOutputCallback callback, void* param, std::string messageid) const;

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
    metric_unit_t m_metric_unit;

    dbcMetric* m_metric;
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
    void Count(int* signals, int* bits, int* covered) const;

    void DecodeSignal(const uint8_t* msg, uint8_t size, bool assignMetrics = true, OvmsWriter* writer = nullptr) const;

  public:
    void AddComment(const std::string& comment);
    void AddComment(const char* comment);
    void RemoveComment(const std::string& comment);
    bool HasComment(const std::string& comment) const;
    uint32_t GetID() const;
    CAN_frame_format_t GetFormat() const;
    bool IsExtended() const;
    bool IsStandard() const;
    void SetID(const uint32_t id);
    int GetSize() const;
    void SetSize(const int size);
    const std::string& GetName() const;
    void SetName(const std::string& name);
    void SetName(const char* name);
    const std::string& GetTransmitterNode() const;
    void SetTransmitterNode(std::string node);
    void SetTransmitterNode(const char* node);
    bool IsMultiplexor() const;
    dbcSignal* GetMultiplexorSignal() const;
    void SetMultiplexorSignal(dbcSignal* signal);

  public:
    void WriteFile(dbcOutputCallback callback, void* param) const;
    void WriteFileComments(dbcOutputCallback callback, void* param) const;
    void WriteFileValues(dbcOutputCallback callback, void* param) const;
    void WriteFileExtended(dbcOutputCallback callback, void* param) const;

  public:
    dbcSignalList_t m_signals;
    dbcCommentTable m_comments;

  protected:
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
    dbcMessage* FindMessage(uint32_t id) const;
    dbcMessage* FindMessage(CAN_frame_format_t format, uint32_t id) const;
    void Count(int* messages, int* signals, int* bits, int* covered) const;

  public:
    void EmptyContent();

  public:
    void WriteFile(dbcOutputCallback callback, void* param) const;
    void WriteFileComments(dbcOutputCallback callback, void* param) const;
    void WriteFileValues(dbcOutputCallback callback, void* param) const;
    void WriteSummary(dbcOutputCallback callback, void* param) const;

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
    void WriteFile(dbcOutputCallback callback, void* param) const;
    void WriteSummary(dbcOutputCallback callback, void* param) const;
    std::string Status() const;
    std::string GetName() const;
    std::string GetPath() const;
    std::string GetVersion() const;

  public:
    void LockFile();
    void UnlockFile();
    bool IsLocked() const;

    void DecodeSignal(CAN_frame_format_t format, uint32_t msg_id, const uint8_t* msg, uint8_t size, bool assignMetrics = true, OvmsWriter* writer = nullptr) const;
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
