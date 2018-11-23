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
static const char *TAG = "dbc-app";

#include <algorithm>
#include <list>
#include <vector>
#include "dbc.h"
#include "dbc_app.h"
#include "ovms_config.h"

dbc MyDBC __attribute__ ((init_priority (4510)));

void dbc_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock ldbc(&MyDBC.m_mutex);

  dbcLoadedFiles_t::iterator it=MyDBC.m_dbclist.begin();
  while (it!=MyDBC.m_dbclist.end())
    {
    writer->printf("%s: ",it->first.c_str());
    //dbcfile* dbcf = it->second;
    //dbcf->ShowStatusLine(writer);
    writer->puts("");
    ++it;
    }
  }

void dbc_load(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.LoadFile(argv[0],argv[1]))
    {
    writer->printf("Loaded DBC %s ok\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Failed to load DBC %s from %s\n",argv[0],argv[1]);
    }
  }

void dbc_unload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.Unload(argv[0]))
    {
    writer->printf("Unloaded DBC %s ok\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Failed to unload DBC %s\n",argv[0]);
    }
  }

void dbc_show(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  dbcfile* dbc = MyDBC.Find(argv[0]);
  if (dbc == NULL)
    {
    writer->printf("Cannot find DBD file: %s",argv[0]);
    return;
    }

  writer->printf("DBC: %s (version %s)\n",argv[0],dbc->m_version.c_str());
  if (!dbc->m_path.empty()) writer->printf("Source: %s\n",dbc->m_path.c_str());
  for (std::string c : dbc->m_comments.m_entrymap) writer->puts(c.c_str());
  writer->puts("");

  if (argc==1)
    {
    writer->printf("Nodes:");
    for (dbcNode* n : dbc->m_nodes.m_entrymap)
      {
      writer->printf(" %s",n->m_name.c_str());
      }
    writer->puts("");
    writer->printf("Messages:\n");
    dbcMessageEntry_t::iterator it=dbc->m_messages.m_entrymap.begin();
    while (it!=dbc->m_messages.m_entrymap.end())
      {
      writer->printf("  0x%x (%d): %s\n",
        it->first, it->first, it->second->m_name.c_str());
      ++it;
      }
    }
  else if (argc==2)
    {
    dbcMessage* m = dbc->m_messages.FindMessage(atoi(argv[1]));
    if (m==NULL)
      {
      writer->printf("Error: No message id #%s\n",argv[1]);
      return;
      }
    writer->printf("Message: 0x%x (%d): %s (%d byte(s) from %s)\n",
      m->m_id, m->m_id, m->m_name.c_str(),
      m->m_size, m->m_transmitter_node.c_str());
    for (std::string c : m->m_comments) writer->puts(c.c_str());
    for (dbcSignal* s : m->m_signals)
      {
      writer->printf("  %s %d|%d@%d%c (%g,%g) [%g|%g]\n",
        s->m_name.c_str(),
        s->m_start_bit, s->m_signal_size, s->m_byte_order, s->m_value_type,
        s->m_factor, s->m_offset, s->m_minimum, s->m_maximum);
      }
    }
  else if (argc==3)
    {
    dbcMessage* m = dbc->m_messages.FindMessage(atoi(argv[1]));
    if (m==NULL)
      {
      writer->printf("Error: No message id #%s\n",argv[1]);
      return;
      }
    dbcSignal* s = m->FindSignal(argv[2]);
    if (s==NULL)
      {
      writer->printf("Error: No signal %s on message id #%s\n",argv[2],argv[1]);
      return;
      }
    writer->printf("Message: 0x%x (%d): %s (%d byte(s) from %s)\n",
      m->m_id, m->m_id, m->m_name.c_str(),
      m->m_size, m->m_transmitter_node.c_str());
    for (std::string c : m->m_comments) writer->puts(c.c_str());
    writer->printf("Signal: %s %d|%d@%d%c (%g,%g) [%g|%g]\n",
      s->m_name.c_str(),
      s->m_start_bit, s->m_signal_size, s->m_byte_order, s->m_value_type,
      s->m_factor, s->m_offset, s->m_minimum, s->m_maximum);
    for (std::string c : s->m_comments) writer->puts(c.c_str());
    writer->printf("Receivers:");
    for (std::string r : s->m_receivers)
      {
      writer->printf(" %s",r.c_str());
      }
    writer->puts("");
    writer->printf("Values (%s):\n",s->m_values.m_name.c_str());
    dbcValueTableEntry_t::iterator it=s->m_values.m_entrymap.begin();
    while (it!=s->m_values.m_entrymap.end())
      {
      writer->printf("  %d: %s\n",
        it->first, it->second.c_str());
      ++it;
      }
    writer->puts("\n");
    for (std::string c : s->m_comments)
      {
      writer->puts(c.c_str());
      }
    }
  }

dbc::dbc()
  {
  ESP_LOGI(TAG, "Initialising DBC (4510)");

  OvmsCommand* cmd_dbc = MyCommandApp.RegisterCommand("dbc","DBC framework",NULL, "", 0, 0, true);

  cmd_dbc->RegisterCommand("list", "List DBC status", dbc_list, "", 0, 0, true);
  cmd_dbc->RegisterCommand("load", "Load DBC file", dbc_load, "<name> <path>", 2, 2, true);
  cmd_dbc->RegisterCommand("unload", "Unload DBC file", dbc_unload, "<name>", 1, 1, true);
  cmd_dbc->RegisterCommand("show", "Show DBC file", dbc_show, "<name>", 1, 3, true);
  }

dbc::~dbc()
  {
  }

bool dbc::LoadFile(const char* name, const char* path)
  {
  OvmsMutexLock ldbc(&m_mutex);

  dbcfile* ndbc = new dbcfile();
  if (!ndbc->LoadFile(path))
    {
//    delete ndbc;
//    return false;
    }

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    {
    // Create a new entry...
    m_dbclist[name] = ndbc;
    }
  else
    {
    // Replace it inline...
    delete k->second;
    m_dbclist[name] = ndbc;
    }

  return true;
  }

bool dbc::Unload(const char* name)
  {
  OvmsMutexLock ldbc(&m_mutex);

  return false;
  }

dbcfile* dbc::Find(const char* name)
  {
  OvmsMutexLock ldbc(&m_mutex);

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    return NULL;
  else
    return k->second;
  }
