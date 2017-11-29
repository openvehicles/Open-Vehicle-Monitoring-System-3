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
          ite->first, e->GetValue(COMMAND_RESULT_VERBOSE));
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
  m_id = 0;
  }

OvmsNotifyEntry::~OvmsNotifyEntry()
  {
  }

bool OvmsNotifyEntry::IsAllRead()
  {
  return (m_readers == 0);
  }

void OvmsNotifyEntry::MarkRead(size_t reader)
  {
  m_readers.reset(reader);
  }

const char* OvmsNotifyEntry::GetValue(int verbosity)
  {
  return "";
  }

////////////////////////////////////////////////////////////////////////
// OvmsNotifyEntryString is the notification entry for a constant
// string type.

OvmsNotifyEntryString::OvmsNotifyEntryString(const char* value)
  {
  m_value = new char[strlen(value)+1];
  strcpy(m_value,value);
  }

OvmsNotifyEntryString::~OvmsNotifyEntryString()
  {
  if (m_value)
    {
    delete [] m_value;
    m_value = NULL;
    }
  }

const char* OvmsNotifyEntryString::GetValue(int verbosity)
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

const char* OvmsNotifyEntryCommand::GetValue(int verbosity)
  {
  return m_cmd; // TODO: Should be execute command, and return result
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

  return id;
  }

uint32_t OvmsNotifyType::AllocateNextID()
  {
  return m_nextid++;
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
