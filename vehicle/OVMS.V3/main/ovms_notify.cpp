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

#include "ovms_log.h"
static const char *TAG = "notify";

#include <stdlib.h>
#include <stdio.h>
#include <sstream>
#include "ovms.h"
#include "ovms_notify.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "vehicle.h"
#include "buffered_shell.h"
#include "string.h"
#include "ovms_mutex.h"

using namespace std;

////////////////////////////////////////////////////////////////////////
// Console commands...

OvmsNotify       MyNotify       __attribute__ ((init_priority (1820)));

void notify_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(),"on")==0)
    MyNotify.m_trace = true;
  else
    MyNotify.m_trace = false;

  writer->printf("Notification tracing is now %s\n",cmd->GetName());
  }

void notify_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsRecMutexLock lock(&MyNotify.m_mutex);
  writer->printf("Notification system has %d readers registered\n",
    MyNotify.CountReaders());
  for (OvmsNotifyCallbackMap_t::iterator itc=MyNotify.m_readers.begin(); itc!=MyNotify.m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    writer->printf("  %s(%d): verbosity=%d\n", mc->m_caller, mc->m_reader, mc->m_verbosity);
    }

  if (MyNotify.m_types.size() > 0)
    {
    writer->puts("Notify types:");
    for (OvmsNotifyTypeMap_t::iterator itm=MyNotify.m_types.begin(); itm!=MyNotify.m_types.end(); ++itm)
      {
      OvmsNotifyType* mt = itm->second;
      OvmsRecMutexLock lock(&mt->m_mutex);
      writer->printf("  %s: %d entries\n",
        mt->m_name, mt->m_entries.size());
      for (NotifyEntryMap_t::iterator ite=mt->m_entries.begin(); ite!=mt->m_entries.end(); ++ite)
        {
        OvmsNotifyEntry* e = ite->second;
        writer->printf("    %d: [%d pending] %s\n",
          ite->first, e->CountPending(), e->GetValue().c_str());
        }
      }
    }
  }

void notify_raise(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Raise %s notification for %s/%s as %s\n",
    cmd->GetName(), argv[0], argv[1], argv[2]);

  if (strcmp(cmd->GetName(),"text")==0)
    MyNotify.NotifyString(argv[0],argv[1],argv[2]);
  else if (strcmp(cmd->GetName(),"command")==0)
    MyNotify.NotifyCommand(argv[0],argv[1],argv[2]);
  else
    MyNotify.NotifyErrorCode(atol(argv[0]),atol(argv[1]),(strcmp(argv[2],"yes")==0));
  }

void notify_errorcode_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (OvmsNotifyErrorCodeMap_t::iterator it=MyNotify.m_errorcodes.begin(); it!=MyNotify.m_errorcodes.end(); ++it)
    {
    writer->printf("%u(0x%04.4x) %s raised %d, updated %d, sec(s) ago\n",
      it->first,
      it->second->lastdata,
      (it->second->active)?"active":"cleared",
      monotonictime-it->second->raised,
      monotonictime-it->second->updated);
    }
  }

void notify_errorcode_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (OvmsNotifyErrorCodeMap_t::iterator it=MyNotify.m_errorcodes.begin(); it!=MyNotify.m_errorcodes.end(); ++it)
    {
    delete it->second;
    }
  MyNotify.m_errorcodes.clear();
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntry is the virtual object for notification entries.
// These are added to a particular OvmsNotifyType (as a member of a list)
// and readers are notified. Once a reader has processed an entry, the
// MarkRead function is called. The framework can test IsAllRead() to
// see if all readers have processed the entry and housekeep cleanup
// appropriately.

OvmsNotifyEntry::OvmsNotifyEntry(const char* subtype)
  {
  m_pendingreaders = 0;
  m_id = 0;
  m_created = monotonictime;
  m_type = NULL;
  m_subtype = strdup(subtype);
  }

OvmsNotifyEntry::~OvmsNotifyEntry()
  {
  if (m_subtype) free(m_subtype);
  }

bool OvmsNotifyEntry::IsRead(size_t reader)
  {
  return !(m_pendingreaders & 1ul << reader);
  }

int OvmsNotifyEntry::CountPending()
  {
  int cnt = 0;
  unsigned long pend = m_pendingreaders;
  for (int i=0; i<NOTIFY_MAX_READERS; i++)
    {
    cnt += pend & 1;
    pend >>= 1;
    }
  return cnt;
  }

bool OvmsNotifyEntry::IsAllRead()
  {
  return (m_pendingreaders == 0);
  }

const extram::string OvmsNotifyEntry::GetValue()
  {
  return extram::string("");
  }

const char* OvmsNotifyEntry::GetSubType()
  {
  return m_subtype;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryString is the notification entry for a constant
// string type.

OvmsNotifyEntryString::OvmsNotifyEntryString(const char* subtype, const char* value)
  : OvmsNotifyEntry(subtype)
  {
  m_value = extram::string(value);
  }

OvmsNotifyEntryString::~OvmsNotifyEntryString()
  {
  }

const extram::string OvmsNotifyEntryString::GetValue()
  {
  return m_value;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryCommand is the notification entry for a command
// callback type.

OvmsNotifyEntryCommand::OvmsNotifyEntryCommand(const char* subtype, int verbosity, const char* cmd)
  : OvmsNotifyEntry(subtype)
  {
  m_cmd = new char[strlen(cmd)+1];
  strcpy(m_cmd,cmd);

  BufferedShell* bs = new BufferedShell(false, verbosity);
  // command notifications can only be raised by the system or "notify raise" in enabled mode,
  // so we can assume this is a secure shell:
  bs->SetSecure(true);
  bs->ProcessChars(m_cmd, strlen(m_cmd));
  bs->ProcessChar('\n');
  bs->Dump(m_value);
  delete bs;
  }

OvmsNotifyEntryCommand::~OvmsNotifyEntryCommand()
  {
  if (m_cmd)
    {
    delete [] m_cmd;
    m_cmd = NULL;
    }
  }

const extram::string OvmsNotifyEntryCommand::GetValue()
  {
  return m_value;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyType is the container for an ordered list of
// OvmsNotifyEntry objects (being the notification data queued)

OvmsNotifyType::OvmsNotifyType(const char* name)
  {
  m_name = name;
  m_nextid = 1;
  }

OvmsNotifyType::~OvmsNotifyType()
  {
  }

uint32_t OvmsNotifyType::QueueEntry(OvmsNotifyEntry* entry)
  {
  OvmsRecMutexLock lock(&m_mutex);
  uint32_t id = m_nextid++;

  entry->m_id = id;
  entry->m_type = this;
  m_entries[id] = entry;

  if (strcmp(m_name, "data") != 0 &&
      strcmp(m_name, "stream") != 0)
    {
    std::string event("notify.");
    event.append(m_name);
    event.append(".");
    event.append(entry->m_subtype);
    MyEvents.SignalEvent(event, (void*)id);
    }

  // Dispatch the callbacks...
  MyNotify.NotifyReaders(this, entry);

  // Check if we can cleanup...
  Cleanup(entry);

  return id;
  }

uint32_t OvmsNotifyType::AllocateNextID()
  {
  OvmsRecMutexLock lock(&m_mutex);
  return m_nextid++;
  }

void OvmsNotifyType::ClearReader(size_t reader)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (m_entries.size() > 0)
    {
    NotifyEntryMap_t::iterator next;
    for (NotifyEntryMap_t::iterator ite=m_entries.begin(); ite!=m_entries.end(); )
      {
      OvmsNotifyEntry* e = ite->second;
      ++ite;
      e->m_pendingreaders &= ~(1ul << reader);
      Cleanup(e, &ite);
      }
    }
  }

OvmsNotifyEntry* OvmsNotifyType::FirstUnreadEntry(size_t reader, uint32_t floor)
  {
  OvmsRecMutexLock lock(&m_mutex);
  for (NotifyEntryMap_t::iterator ite=m_entries.begin(); ite!=m_entries.end(); ++ite)
    {
    OvmsNotifyEntry* e = ite->second;
    if ((!e->IsRead(reader))&&(e->m_id > floor))
      return e;
    }
  return NULL;
  }

OvmsNotifyEntry* OvmsNotifyType::FindEntry(uint32_t id)
  {
  OvmsRecMutexLock lock(&m_mutex);
  auto k = m_entries.find(id);
  if (k == m_entries.end())
    return NULL;
  else
    return k->second;
  }

void OvmsNotifyType::MarkRead(size_t reader, OvmsNotifyEntry* entry)
  {
  OvmsRecMutexLock lock(&m_mutex);
  entry->m_pendingreaders &= ~(1ul << reader);
  Cleanup(entry);
  }

void OvmsNotifyType::Cleanup(OvmsNotifyEntry* entry, NotifyEntryMap_t::iterator* next /*=NULL*/)
  {
  if (entry->IsAllRead())
    {
    // We can cleanup...
    auto k = m_entries.find(entry->m_id);
    if (k != m_entries.end())
      {
      NotifyEntryMap_t::iterator it = m_entries.erase(k);
      if (next) *next = it;
      }
    if (MyNotify.m_trace)
      ESP_LOGI(TAG,"Cleanup type %s id %d",m_name,entry->m_id);
    delete entry;
    }
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyCallbackEntry contains the callback function for a
// particular reader

OvmsNotifyCallbackEntry::OvmsNotifyCallbackEntry(const char* caller, size_t reader, int verbosity, OvmsNotifyCallback_t callback,
                                                 bool configfiltered/*=true*/, OvmsNotifyFilterCallback_t filtercallback/*=NULL*/)
  {
  m_caller = caller;
  m_reader = reader;
  m_verbosity = verbosity;
  m_callback = callback;
  m_configfiltered = configfiltered;
  m_filtercallback = filtercallback;
  }

OvmsNotifyCallbackEntry::~OvmsNotifyCallbackEntry()
  {
  }

bool OvmsNotifyCallbackEntry::Accepts(OvmsNotifyType* type, const char* subtype, size_t size)
  {
  // Check size
  if (size > m_verbosity)
    return false;
  // Check filter by config:
  if (m_configfiltered)
    {
    std::string filter = MyConfig.GetParamValue("notify", subtype);
    if (!filter.empty() && filter.find(m_caller) == string::npos)
      return false;
    }
  // Check filter by callback:
  if (m_filtercallback)
    {
    if (m_filtercallback(type, subtype) == false)
      return false;
    }
  return true;
  }


////////////////////////////////////////////////////////////////////////
// OvmsNotifyCallbackEntry contains the callback function for a
//

OvmsNotify::OvmsNotify()
  {
  ESP_LOGI(TAG, "Initialising NOTIFICATIONS (1820)");

  m_nextreader = 1;

#ifdef CONFIG_OVMS_DEV_DEBUGNOTIFICATIONS
  m_trace = true;
#else
  m_trace = false;
#endif // #ifdef CONFIG_OVMS_DEV_DEBUGNOTIFICATIONS

  MyConfig.RegisterParam("notify", "Notification filters", true, true);

  // Register our commands
  OvmsCommand* cmd_notify = MyCommandApp.RegisterCommand("notify","NOTIFICATION framework");
  cmd_notify->RegisterCommand("status","Show notification status",notify_status);
  OvmsCommand* cmd_notifyraise = cmd_notify->RegisterCommand("raise","NOTIFICATION raise framework");
  cmd_notifyraise->RegisterCommand("text","Raise a textual notification",notify_raise,"<type><subtype><message>", 3, 3);
  cmd_notifyraise->RegisterCommand("command","Raise a command callback notification",notify_raise,"<type><subtype><command>", 3, 3);
  cmd_notifyraise->RegisterCommand("errorcode","Raise an error code notification",notify_raise,"<code><data><raised>", 3, 3);
  OvmsCommand* cmd_notifyerrorcode = cmd_notify->RegisterCommand("errorcode","NOTIFICATION error code framework");
  cmd_notifyerrorcode->RegisterCommand("list","List error codes raised",notify_errorcode_list);
  cmd_notifyerrorcode->RegisterCommand("clear","Clear error code list",notify_errorcode_clear);
  OvmsCommand* cmd_notifytrace = cmd_notify->RegisterCommand("trace","NOTIFICATION trace framework");
  cmd_notifytrace->RegisterCommand("on","Turn notification tracing ON",notify_trace);
  cmd_notifytrace->RegisterCommand("off","Turn notification tracing OFF",notify_trace);

  RegisterType("info");     // payload: human readable text message
  RegisterType("error");    // payload: "<vehicletype>,<errorcode>,<errordata>"
  RegisterType("alert");    // payload: human readable text message
  RegisterType("data");     // payload: MP historical data record (tagged CSV, see MP documentation)
  RegisterType("stream");   // payload: subtype specific, use for high volume / short latency data streams
  }

OvmsNotify::~OvmsNotify()
  {
  }

size_t OvmsNotify::RegisterReader(const char* caller, int verbosity, OvmsNotifyCallback_t callback,
                                  bool configfiltered/*=false*/, OvmsNotifyFilterCallback_t filtercallback/*=NULL*/)
  {
  OvmsRecMutexLock lock(&m_mutex);
  size_t reader = m_nextreader++;

  m_readers[reader] = new OvmsNotifyCallbackEntry(caller, reader, verbosity, callback, configfiltered, filtercallback);

  return reader;
  }

void OvmsNotify::RegisterReader(size_t reader, const char* caller, int verbosity, OvmsNotifyCallback_t callback,
                                bool configfiltered/*=false*/, OvmsNotifyFilterCallback_t filtercallback/*=NULL*/)
  {
  OvmsRecMutexLock lock(&m_mutex);
  m_readers[reader] = new OvmsNotifyCallbackEntry(caller, reader, verbosity, callback, configfiltered, filtercallback);
  }

void OvmsNotify::ClearReader(size_t reader)
  {
  OvmsRecMutexLock lock(&m_mutex);
  auto k = m_readers.find(reader);
  if (k != m_readers.end())
    {
    for (OvmsNotifyTypeMap_t::iterator itt=m_types.begin(); itt!=m_types.end(); ++itt)
      {
      OvmsNotifyType* t = itt->second;
      t->ClearReader(k->second->m_reader);
      }
    OvmsNotifyCallbackEntry* ec = k->second;
    m_readers.erase(k);
    delete ec;
    }
  }

size_t OvmsNotify::CountReaders()
  {
  OvmsRecMutexLock lock(&m_mutex);
  return m_readers.size();
  }

OvmsNotifyType* OvmsNotify::GetType(const char* type)
  {
  auto k = m_types.find(type);
  if (k == m_types.end())
    return NULL;
  else
    return k->second;
  }

void OvmsNotify::NotifyReaders(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  OvmsRecMutexLock lock(&m_mutex);
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (mc->Accepts(type, entry->GetSubType(), entry->GetValueSize()))
      {
      // deliver notification:
      if (mc->m_callback(type,entry) == true)
        entry->m_pendingreaders &= ~(1ul << mc->m_reader);
      }
    else
      {
      // in case the acceptance filter changed since queueing:
      entry->m_pendingreaders &= ~(1ul << mc->m_reader);
      }
    }
  }

bool OvmsNotify::HasReader(const char* type, const char* subtype, size_t size)
  {
  OvmsRecMutexLock lock(&m_mutex);
  OvmsNotifyType* mt = GetType(type);
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (mc->Accepts(mt, subtype, size))
      return true;
    }
  return false;
  }

void OvmsNotify::RegisterType(const char* type)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    mt = new OvmsNotifyType(type);
    m_types[type] = mt;
    ESP_LOGI(TAG,"Registered notification type %s",type);
    }
  }

uint32_t OvmsNotify::NotifyString(const char* type, const char* subtype, const char* value)
  {
  OvmsRecMutexLock lock(&m_mutex);
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, value);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise text %s: %s", type, value);

  // determine all currently active readers accepting the message:
  std::bitset<NOTIFY_MAX_READERS> readers;
  size_t size = strlen(value);
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (mc->Accepts(mt, subtype, size))
      readers.set(mc->m_reader);
    }
  if (readers.count() == 0)
    {
    ESP_LOGD(TAG, "Abort: no readers for type '%s' subtype '%s' size %d", type, subtype, size);
    return 0;
    }

  // create message:
  OvmsNotifyEntry* msg = (OvmsNotifyEntry*) new OvmsNotifyEntryString(subtype, value);
  msg->m_pendingreaders = readers.to_ulong();

  ESP_LOGD(TAG, "Created entry type '%s' subtype '%s' size %d has %d readers pending", type, subtype, size, readers.count());

  return mt->QueueEntry(msg);
  }

uint32_t OvmsNotify::NotifyCommand(const char* type, const char* subtype, const char* cmd)
  {
  OvmsRecMutexLock lock(&m_mutex);
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, cmd);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise command %s: %s", type, cmd);

  // Strategy:
  //  to minimize RAM usage and command calls we try to reuse higher verbosity messages
  //  if their result length fits for lower verbosity readers as well.

  // get verbosity levels needed by readers accepting the message:
  std::map<int, OvmsNotifyEntryCommand*> verbosity_msgs;
  std::bitset<NOTIFY_MAX_READERS> readers;
  for (auto itc=m_readers.begin(); itc!=m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (mc->Accepts(mt, subtype))
      {
      verbosity_msgs[mc->m_verbosity] = NULL;
      readers.set(mc->m_reader); // cache acceptance
      }
    }
  if (verbosity_msgs.size() == 0)
    {
    ESP_LOGD(TAG, "Abort: no readers for type '%s' subtype '%s'", type, subtype);
    return 0;
    }

  // fetch verbosity levels beginning at highest verbosity:
  OvmsNotifyEntryCommand *msg = NULL;
  size_t msglen = 0;
  for (auto ritm=verbosity_msgs.rbegin(); ritm!=verbosity_msgs.rend(); ritm++)
    {
    int verbosity = ritm->first;
    if (msg && msglen <= verbosity)
      {
      // reuse last verbosity level message:
      verbosity_msgs[verbosity] = msg;
      }
    else
      {
      msg = verbosity_msgs[verbosity];
      if (!msg)
        {
        // create verbosity level message:
        msg = new OvmsNotifyEntryCommand(subtype, verbosity, cmd);
        msglen = msg->GetValueSize();
        verbosity_msgs[verbosity] = msg;
        }
      }
    }

  // add readers:
  for (auto itc=m_readers.begin(); itc!=m_readers.end(); itc++)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    if (readers.test(mc->m_reader))
      {
      msg = verbosity_msgs[mc->m_verbosity];
      msg->m_pendingreaders |= (1ul << mc->m_reader);
      }
    }

  // queue all verbosity level messages beginning at lowest verbosity (fastest delivery):
  msg = NULL;
  uint32_t queue_id = 0;
  for (auto itm=verbosity_msgs.begin(); itm!=verbosity_msgs.end(); itm++)
    {
    if (itm->second == msg)
      continue; // already queued
    msg = itm->second;
    ESP_LOGD(TAG, "Created entry type '%s' subtype '%s' verbosity %d has %d readers pending",
             type, subtype, itm->first, msg->CountPending());
    queue_id = mt->QueueEntry(msg);
    }

  return queue_id;
  }


/**
 * NotifyStringf: printf style API
 */
uint32_t OvmsNotify::NotifyStringf(const char* type, const char* subtype, const char* fmt, ...)
  {
  char *buffer = NULL;
  uint32_t res = 0;
  va_list args;
  va_start(args, fmt);
  int len = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (len >= 0)
    {
    res = NotifyString(type, subtype, buffer);
    free(buffer);
    }
  return res;
  }


/**
 * NotifyCommandf: printf style API
 */
uint32_t OvmsNotify::NotifyCommandf(const char* type, const char* subtype, const char* fmt, ...)
  {
  char *buffer = NULL;
  uint32_t res = 0;
  va_list args;
  va_start(args, fmt);
  int len = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (len >= 0)
    {
    res = NotifyCommand(type, subtype, buffer);
    free(buffer);
    }
  return res;
  }

void OvmsNotify::NotifyErrorCode(uint32_t code, uint32_t data, bool raised, bool force)
  {
  // Raise a notification of an error code
  //   <code> error code (key field)
  //   <data> data associated with the error
  //   <raised> true if error is being raised, else false
  //   <force> true if notification should be forced, otherwise auto-suppress dupes

  // Firstly, let's see if we have a notification record for it already
  auto k = m_errorcodes.find(code);
  OvmsNotifyErrorCodeEntry_t* entry;
  if (k == m_errorcodes.end())
    {
    entry = new OvmsNotifyErrorCodeEntry_t;
    entry->raised = monotonictime;
    entry->updated = 0;
    entry->lastdata = 0;
    m_errorcodes[code] = entry;
    }
  else
    {
    entry = k->second;
    }
  entry->active = raised;
  if (raised) entry->lastdata = data;

  // Handle auto-suppression, if necessary
  if (!force && (entry->updated>0) && (monotonictime-entry->updated < NOTIFY_ERROR_AUTOSUPPRESS))
    {
    // We need to auto-suppress
    entry->updated = monotonictime;
    return;
    }

  // OK to raise...
  entry->updated = monotonictime;
  if (raised)
    {
    NotifyStringf("error", "code", "%s,%u,%u",
      MyVehicleFactory.ActiveVehicleType(),
      code,
      data);
    }
  }
