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
#include <stdint.h>
#include "ovms_utils.h"

#define NOTIFY_MAX_READERS 32

using namespace std;

class OvmsNotifyType;

class OvmsNotifyEntry
  {
  public:
    OvmsNotifyEntry();
    virtual ~OvmsNotifyEntry();

  public:
    virtual const char* GetValue(int verbosity);
    virtual bool IsRead(size_t reader);
    virtual bool IsAllRead();

  public:
    std::bitset<NOTIFY_MAX_READERS> m_readers;
    uint32_t m_id;
    uint32_t m_created;
  };

class OvmsNotifyEntryString : public OvmsNotifyEntry
  {
  public:
    OvmsNotifyEntryString(const char* value);
    virtual ~OvmsNotifyEntryString();

  public:
    virtual const char* GetValue(int verbosity);

  public:
     char* m_value;
  };

class OvmsNotifyEntryCommand : public OvmsNotifyEntry
  {
  public:
    OvmsNotifyEntryCommand(const char* cmd);
    virtual ~OvmsNotifyEntryCommand();

  public:
    virtual const char* GetValue(int verbosity);

  public:
     char* m_cmd;
  };

typedef std::map<uint32_t, OvmsNotifyEntry*> NotifyEntryMap_t;

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
    void MarkRead(size_t reader, OvmsNotifyEntry* entry);

  protected:
    void Cleanup(OvmsNotifyEntry* entry);

  public:
    const char* m_name;
    uint32_t m_nextid;
    NotifyEntryMap_t m_entries;
  };

typedef std::function<bool(OvmsNotifyType*,OvmsNotifyEntry*)> OvmsNotifyCallback_t;

class OvmsNotifyCallbackEntry
  {
  public:
    OvmsNotifyCallbackEntry(const char* caller, size_t reader, OvmsNotifyCallback_t callback);
    virtual ~OvmsNotifyCallbackEntry();

  public:
    const char *m_caller;
    size_t m_reader;
    OvmsNotifyCallback_t m_callback;
  };

typedef std::map<const char*, OvmsNotifyCallbackEntry*, CmpStrOp> OvmsNotifyCallbackMap_t;
typedef std::map<const char*, OvmsNotifyType*, CmpStrOp> OvmsNotifyTypeMap_t;

class OvmsNotify
  {
  public:
    OvmsNotify();
    virtual ~OvmsNotify();

  public:
    size_t RegisterReader(const char* caller, OvmsNotifyCallback_t callback);
    void ClearReader(const char* caller);
    size_t CountReaders();
    OvmsNotifyType* GetType(const char* type);
    void NotifyReaders(OvmsNotifyType* type, OvmsNotifyEntry* entry);

  public:
    void RegisterType(const char* type);
    uint32_t NotifyString(const char* type, const char* value);
    uint32_t NotifyCommand(const char* type, const char* cmd);

  protected:
    OvmsNotifyCallbackMap_t m_readers;

  protected:
    size_t m_nextreader;

  public:
    OvmsNotifyTypeMap_t m_types;
    bool m_trace;
  };

extern OvmsNotify MyNotify;

#endif //#ifndef __OVMS_NOTIFY_H__
