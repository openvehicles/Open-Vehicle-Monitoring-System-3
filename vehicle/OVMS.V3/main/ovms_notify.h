/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

#ifndef __OVMS_NOTIFY_H__
#define __OVMS_NOTIFY_H__

#include <functional>
#include <map>
#include <list>
#include <string>
#include <bitset>
#include <atomic>
#include <stdint.h>
#include "ovms.h"
#include "ovms_utils.h"

#define NOTIFY_MAX_READERS 32
#define NOTIFY_ERROR_AUTOSUPPRESS 120 // Auto-suppress for 120 seconds

using namespace std;

class OvmsNotifyType;

class OvmsNotifyEntry : public ExternalRamAllocated
  {
  public:
    OvmsNotifyEntry(const char* subtype);
    virtual ~OvmsNotifyEntry();

  public:
    virtual const extram::string GetValue();
    virtual size_t GetValueSize() { return 0; }
    virtual bool IsRead(size_t reader);
    virtual int CountPending();
    virtual bool IsAllRead();
    virtual OvmsNotifyType* GetType() { return m_type; }
    virtual const char* GetSubType();

  public:
    std::atomic_ulong m_pendingreaders;
    uint32_t m_id;
    uint32_t m_created;
    OvmsNotifyType* m_type;
    char* m_subtype;
  };

class OvmsNotifyEntryString : public OvmsNotifyEntry
  {
  public:
    OvmsNotifyEntryString(const char* subtype, const char* value);
    virtual ~OvmsNotifyEntryString();

  public:
    virtual const extram::string GetValue();
    virtual size_t GetValueSize() { return m_value.size(); }

  public:
     extram::string m_value;
  };

class OvmsNotifyEntryCommand : public OvmsNotifyEntry
  {
  public:
    OvmsNotifyEntryCommand(const char* subtype, int verbosity, const char* cmd);
    virtual ~OvmsNotifyEntryCommand();

  public:
    virtual const extram::string GetValue();
    virtual size_t GetValueSize() { return m_value.size(); }

  public:
     char* m_cmd;
     extram::string m_value;
  };

typedef std::map<uint32_t, OvmsNotifyEntry*, std::less<uint32_t>,
  ExtRamAllocator<std::pair<const uint32_t, OvmsNotifyEntry*>>> NotifyEntryMap_t;

class OvmsNotifyType
  {
  public:
    OvmsNotifyType(const char* name);
    virtual ~OvmsNotifyType();

  public:
    uint32_t QueueEntry(OvmsNotifyEntry* entry);
    uint32_t AllocateNextID();
    void ClearReader(size_t reader);
    OvmsNotifyEntry* FirstUnreadEntry(size_t reader, uint32_t floor);
    OvmsNotifyEntry* FindEntry(uint32_t id);
    void MarkRead(size_t reader, OvmsNotifyEntry* entry);

  protected:
    void Cleanup(OvmsNotifyEntry* entry);

  public:
    const char* m_name;
    uint32_t m_nextid;
    NotifyEntryMap_t m_entries;
  };

typedef std::function<bool(OvmsNotifyType*,OvmsNotifyEntry*)> OvmsNotifyCallback_t;
typedef std::function<bool(OvmsNotifyType*,const char*)> OvmsNotifyFilterCallback_t;

class OvmsNotifyCallbackEntry
  {
  public:
    OvmsNotifyCallbackEntry(const char* caller, size_t reader, int verbosity, OvmsNotifyCallback_t callback,
                            bool configfiltered=false, OvmsNotifyFilterCallback_t filtercallback=NULL);
    virtual ~OvmsNotifyCallbackEntry();

  public:
    bool Accepts(OvmsNotifyType* type, const char* subtype, size_t size=0);

  public:
    const char *m_caller;
    bool m_configfiltered;
    size_t m_reader;
    int m_verbosity;
    OvmsNotifyCallback_t m_callback;
    OvmsNotifyFilterCallback_t m_filtercallback;
  };

typedef struct
  {
  uint32_t raised;
  uint32_t updated;
  uint32_t lastdata;
  bool active;
  } OvmsNotifyErrorCodeEntry_t;

typedef std::map<size_t, OvmsNotifyCallbackEntry*, std::less<size_t>,
  ExtRamAllocator<std::pair<size_t, OvmsNotifyCallbackEntry*>>> OvmsNotifyCallbackMap_t;
typedef std::map<const char*, OvmsNotifyType*, CmpStrOp,
  ExtRamAllocator<std::pair<const char*, OvmsNotifyCallbackEntry*>>> OvmsNotifyTypeMap_t;
typedef std::map<uint32_t, OvmsNotifyErrorCodeEntry_t*> OvmsNotifyErrorCodeMap_t;

class OvmsNotify : public ExternalRamAllocated
  {
  public:
    OvmsNotify();
    virtual ~OvmsNotify();

  public:
    size_t RegisterReader(const char* caller, int verbosity, OvmsNotifyCallback_t callback,
                          bool configfiltered=false, OvmsNotifyFilterCallback_t filtercallback=NULL);
    void RegisterReader(size_t reader, const char* caller, int verbosity, OvmsNotifyCallback_t callback,
                          bool configfiltered=false, OvmsNotifyFilterCallback_t filtercallback=NULL);
    void ClearReader(size_t reader);
    size_t CountReaders();
    OvmsNotifyType* GetType(const char* type);
    bool HasReader(const char* type, const char* subtype, size_t size=0);
    void NotifyReaders(OvmsNotifyType* type, OvmsNotifyEntry* entry);

  public:
    void RegisterType(const char* type);
    uint32_t NotifyString(const char* type, const char* subtype, const char* value);
    uint32_t NotifyStringf(const char* type, const char* subtype, const char* fmt, ...);
    uint32_t NotifyCommand(const char* type, const char* subtype, const char* cmd);
    uint32_t NotifyCommandf(const char* type, const char* subtype, const char* fmt, ...);
    void NotifyErrorCode(uint32_t code, uint32_t data, bool raised, bool force=false);

  public:
    OvmsNotifyCallbackMap_t m_readers;

  protected:
    size_t m_nextreader;

  public:
    OvmsNotifyTypeMap_t m_types;
    OvmsNotifyErrorCodeMap_t m_errorcodes;
    bool m_trace;
  };

extern OvmsNotify MyNotify;

#endif //#ifndef __OVMS_NOTIFY_H__
