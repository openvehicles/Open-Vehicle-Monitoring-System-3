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
#include "ovms_events.h"
#include "buffered_shell.h"
#include "string.h"

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
  writer->printf("Notification system has %d readers registered\n",
    MyNotify.CountReaders());

  if (MyNotify.m_types.size() > 0)
    {
    writer->puts("Notify types:");
    for (OvmsNotifyTypeMap_t::iterator itm=MyNotify.m_types.begin(); itm!=MyNotify.m_types.end(); ++itm)
      {
      OvmsNotifyType* mt = itm->second;
      writer->printf("  %s: %d entries\n",
        mt->m_name, mt->m_entries.size());
      for (NotifyEntryMap_t::iterator ite=mt->m_entries.begin(); ite!=mt->m_entries.end(); ++ite)
        {
        OvmsNotifyEntry* e = ite->second;
        writer->printf("    %d: %s\n",
          ite->first, e->GetValue(COMMAND_RESULT_VERBOSE).c_str());
        }
      }
    }
  }

void notify_raise(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->printf("Raise %s notification for %s as %s\n",
    cmd->GetName(), argv[0], argv[1]);

  if (strcmp(cmd->GetName(),"text")==0)
    MyNotify.NotifyString(argv[0],argv[1]);
  else
    MyNotify.NotifyCommand(argv[0],argv[1]);
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntry is the virtual object for notification entries.
// These are added to a particular OvmsNotifyType (as a member of a list)
// and readers are notified. Once a reader has processed an entry, the
// MarkRead function is called. The framework can test IsAllRead() to
// see if all readers have processed the entry and housekeep cleanup
// appropriately.

OvmsNotifyEntry::OvmsNotifyEntry()
  {
  m_readers.reset();
  for (size_t k = 0; k < MyNotify.CountReaders(); k++)
    {
    m_readers.set(k+1);
    }
  m_id = 0;
  m_created = monotonictime;
  ESP_LOGD(TAG,"Created entry has %d readers pending",m_readers.count());
  }

OvmsNotifyEntry::~OvmsNotifyEntry()
  {
  }

bool OvmsNotifyEntry::IsRead(size_t reader)
  {
  return !m_readers[reader];
  }

bool OvmsNotifyEntry::IsAllRead()
  {
  return (m_readers.count() == 0);
  }

const std::string OvmsNotifyEntry::GetValue(int verbosity)
  {
  return std::string("");
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryString is the notification entry for a constant
// string type.

OvmsNotifyEntryString::OvmsNotifyEntryString(const char* value)
  {
  m_value = std::string(value);
  }

OvmsNotifyEntryString::~OvmsNotifyEntryString()
  {
  }

const std::string OvmsNotifyEntryString::GetValue(int verbosity)
  {
  return m_value;
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryCommand is the notification entry for a command
// callback type.

OvmsNotifyEntryCommand::OvmsNotifyEntryCommand(const char* cmd)
  {
  m_cmd = new char[strlen(cmd)+1];
  strcpy(m_cmd,cmd);
  }

OvmsNotifyEntryCommand::~OvmsNotifyEntryCommand()
  {
  if (m_cmd)
    {
    delete [] m_cmd;
    m_cmd = NULL;
    }
  }

const std::string OvmsNotifyEntryCommand::GetValue(int verbosity)
  {
  if (m_value.empty())
    {
    BufferedShell* bs = new BufferedShell(false, verbosity);
    bs->ProcessChars(m_cmd, strlen(m_cmd));
    bs->ProcessChar('\n');
    char* ret = bs->Dump();
    m_value = std::string(ret);
    free(ret);
    delete bs;
    }
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
  uint32_t id = m_nextid++;

  entry->m_id = id;
  m_entries[id] = entry;

  std::string event("notify.");
  event.append(m_name);
  MyEvents.SignalEvent(event, (void*)entry);

  // Dispatch the callbacks...
  MyNotify.NotifyReaders(this, entry);

  // Check if we can cleanup...
  Cleanup(entry);

  return id;
  }

uint32_t OvmsNotifyType::AllocateNextID()
  {
  return m_nextid++;
  }

void OvmsNotifyType::ClearReader(size_t reader)
  {
  if (m_entries.size() > 0)
    {
    for (NotifyEntryMap_t::iterator ite=m_entries.begin(); ite!=m_entries.end(); )
      {
      OvmsNotifyEntry* e = ite->second;
      ++ite;
      e->m_readers.reset(reader);
      Cleanup(e);
      }
    }
  }

OvmsNotifyEntry* OvmsNotifyType::FirstUnreadEntry(size_t reader, uint32_t floor)
  {
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
  auto k = m_entries.find(id);
  if (k == m_entries.end())
    return NULL;
  else
    return k->second;
  }

void OvmsNotifyType::MarkRead(size_t reader, OvmsNotifyEntry* entry)
  {
  entry->m_readers.reset(reader);
  Cleanup(entry);
  }

void OvmsNotifyType::Cleanup(OvmsNotifyEntry* entry)
  {
  if (entry->IsAllRead())
    {
    // We can cleanup...
    auto k = m_entries.find(entry->m_id);
    if (k != m_entries.end())
       {
       m_entries.erase(k);
       }
    if (MyNotify.m_trace)
      ESP_LOGI(TAG,"Cleanup type %s id %d",m_name,entry->m_id);
    delete entry;
    }
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyCallbackEntry contains the callback function for a
// particular reader

OvmsNotifyCallbackEntry::OvmsNotifyCallbackEntry(const char* caller, size_t reader, OvmsNotifyCallback_t callback)
  {
  m_caller = caller;
  m_reader = reader;
  m_callback = callback;
  }

OvmsNotifyCallbackEntry::~OvmsNotifyCallbackEntry()
  {
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

  // Register our commands
  OvmsCommand* cmd_notify = MyCommandApp.RegisterCommand("notify","NOTIFICATION framework",NULL, "", 1);
  cmd_notify->RegisterCommand("status","Show notification status",notify_status,"", 0, 0, false);
  OvmsCommand* cmd_notifyraise = cmd_notify->RegisterCommand("raise","NOTIFICATION raise framework", NULL, "", 0, 0, false);
  cmd_notifyraise->RegisterCommand("text","Raise a textual notification",notify_raise,"<type><message>", 2, 2, true);
  cmd_notifyraise->RegisterCommand("command","Raise a command callback notification",notify_raise,"<type><command>", 2, 2, true);
  OvmsCommand* cmd_notifytrace = cmd_notify->RegisterCommand("trace","NOTIFICATION trace framework", NULL, "", 0, 0, false);
  cmd_notifytrace->RegisterCommand("on","Turn notification tracing ON",notify_trace,"", 0, 0, false);
  cmd_notifytrace->RegisterCommand("off","Turn notification tracing OFF",notify_trace,"", 0, 0, false);

  RegisterType("info");
  RegisterType("alert");
  RegisterType("data");
  }

OvmsNotify::~OvmsNotify()
  {
  }

size_t OvmsNotify::RegisterReader(const char* caller, OvmsNotifyCallback_t callback)
  {
  size_t reader = m_nextreader++;

  m_readers[caller] = new OvmsNotifyCallbackEntry(caller,reader,callback);

  return reader;
  }

void OvmsNotify::ClearReader(const char* caller)
  {
  auto k = m_readers.find(caller);
  if (k != m_readers.end())
    {
    for (OvmsNotifyTypeMap_t::iterator itt=m_types.begin(); itt!=m_types.end(); ++itt)
      {
      OvmsNotifyType* t = itt->second;
      t->ClearReader(k->second->m_reader);
      }
    }
  }

size_t OvmsNotify::CountReaders()
  {
  return m_nextreader-1;
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
  for (OvmsNotifyCallbackMap_t::iterator itc=m_readers.begin(); itc!=m_readers.end(); ++itc)
    {
    OvmsNotifyCallbackEntry* mc = itc->second;
    bool result = mc->m_callback(type,entry);
    if (result) entry->m_readers.reset(mc->m_reader);
    }
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

uint32_t OvmsNotify::NotifyString(const char* type, const char* value)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, value);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise text %s: %s", type, value);

  return mt->QueueEntry((OvmsNotifyEntry*)new OvmsNotifyEntryString(value));
  }

uint32_t OvmsNotify::NotifyCommand(const char* type, const char* cmd)
  {
  OvmsNotifyType* mt = GetType(type);
  if (mt == NULL)
    {
    ESP_LOGW(TAG, "Notification raised for non-existent type %s: %s", type, cmd);
    return 0;
    }

  if (m_trace) ESP_LOGI(TAG, "Raise command %s: %s", type, cmd);

  return mt->QueueEntry((OvmsNotifyEntry*)new OvmsNotifyEntryCommand(cmd));
  }
