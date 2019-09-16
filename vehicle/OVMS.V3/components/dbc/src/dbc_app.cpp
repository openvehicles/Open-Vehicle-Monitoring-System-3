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
#include <string>
#include <sys/types.h>
#include <dirent.h>
#include "dbc.h"
#include "dbc_app.h"
#include "ovms_config.h"
#include "ovms_events.h"

dbc MyDBC __attribute__ ((init_priority (4520)));

void dbc_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  OvmsMutexLock ldbc(&MyDBC.m_mutex);

  dbcLoadedFiles_t::iterator it=MyDBC.m_dbclist.begin();
  while (it!=MyDBC.m_dbclist.end())
    {
    writer->printf("%s: ",it->first.c_str());
    writer->puts(it->second->Status().c_str());
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

void dbc_show_callback(void* param, const char* buffer)
  {
  OvmsWriter* writer = (OvmsWriter*)param;

  writer->write(buffer,strlen(buffer));
  }

void dbc_show(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  dbcfile* dbc;
  if (argc == 0)
    {
    dbc = MyDBC.m_selected;
    if (dbc == NULL)
      {
      writer->puts("Error: No selected DBC file");
      return;
      }
    }
  else
    {
    dbc = MyDBC.Find(argv[0]);
    if (dbc == NULL)
      {
      writer->printf("Cannot find DBC file: %s\n",argv[0]);
      return;
      }
    }

  writer->printf("DBC:     %s\n",dbc->GetName().c_str());

  using std::placeholders::_1;
  using std::placeholders::_2;
  dbc->WriteSummary(std::bind(dbc_show_callback,_1,_2), writer);
  }

void dbc_dump(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  dbcfile* dbc;
  if (argc == 0)
    {
    dbc = MyDBC.m_selected;
    if (dbc == NULL)
      {
      writer->puts("Error: No selected DBC file");
      return;
      }
    }
  else
    {
    dbc = MyDBC.Find(argv[0]);
    if (dbc == NULL)
      {
      writer->printf("Cannot find DBC file: %s\n",argv[0]);
      return;
      }
    }

  using std::placeholders::_1;
  using std::placeholders::_2;
  dbc->WriteFile(std::bind(dbc_show_callback,_1,_2), writer);
  }

void dbc_save_callback(void* param, const char* buffer)
  {
  FILE* fd = (FILE*)param;

  fwrite(buffer,strlen(buffer),1,fd);
  }

void dbc_save(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  dbcfile* dbc;
  if (argc == 0)
    {
    dbc = MyDBC.m_selected;
    if (dbc == NULL)
      {
      writer->puts("Error: No selected DBC file");
      return;
      }
    }
  else
    {
    dbc = MyDBC.Find(argv[0]);
    if (dbc == NULL)
      {
      writer->printf("Cannot find DBC file: %s\n",argv[0]);
      return;
      }
    }

  FILE* fd = fopen(dbc->m_path.c_str(), "w");
  if (fd == NULL)
    {
    writer->printf("Error: Could not open file '%s' for writing\n",dbc->m_path.c_str());
    return;
    }

  using std::placeholders::_1;
  using std::placeholders::_2;
  dbc->WriteFile(std::bind(dbc_save_callback,_1,_2), fd);
  fclose(fd);

  writer->printf("Saved to: %s\n",dbc->m_path.c_str());
  }

void dbc_autoload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Auto-loading DBC files...");
  MyDBC.LoadDirectory("/store/dbc", false);
  MyDBC.LoadAutoExtras(false);
  }

void dbc_sdmounted(std::string event, void* data)
  {
  if (MyConfig.GetParamValueBool("auto", "dbc", false))
    MyDBC.LoadAutoExtras(true);
  }

void dbc_select(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyDBC.DeselectFile();
    writer->puts("DBC deselected");
    return;
    }

  if (MyDBC.SelectFile(argv[0]))
    {
    writer->printf("DBC file %s selected\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Cannot find DBC file: %s\n",argv[0]);
    }
  }

void dbc_deselect(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyDBC.DeselectFile();
  writer->puts("DBC deselected");
  }

void dbc_set_version(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  MyDBC.m_selected->m_version = std::string(argv[0]);
  writer->printf("DBC: Version set to '%s'\n",argv[0]);
  }

void dbc_set_timing(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  MyDBC.m_selected->m_bittiming.SetBaud(atoi(argv[0]),atoi(argv[1]),atoi(argv[2]));
  writer->printf("DBC: Bit timing set to %s : %s,%s'\n",argv[0],argv[1],argv[2]);
  }

void dbc_node_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  MyDBC.m_selected->m_nodes.EmptyContent();
  writer->puts("DBC: Node table cleared");
  }

void dbc_node_add(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  if (MyDBC.m_selected->m_nodes.FindNode(argv[0]) != NULL)
    {
    writer->printf("Error: Node %s already exists\n",argv[0]);
    return;
    }

  dbcNode* node = new dbcNode(argv[0]);
  MyDBC.m_selected->m_nodes.AddNode(node);
  writer->printf("DBC: Added node %s\n",argv[0]);
  }

void dbc_node_remove(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("No DBC selected");
    return;
    }

  dbcNode* node = MyDBC.m_selected->m_nodes.FindNode(argv[0]);
  if (node != NULL)
    {
    MyDBC.m_selected->m_nodes.RemoveNode(node,true);
    writer->printf("DBC: Node %s removed\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Could not find node %s\n",argv[0]);
    }
  }

void dbc_message_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  MyDBC.m_selected->m_messages.EmptyContent();
  writer->puts("DBC: Message table cleared");
  }

void dbc_message_add(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  if (MyDBC.m_selected->m_messages.FindMessage(msgid))
    {
    writer->printf("Error: Message %s already exists\n",argv[0]);
    return;
    }

  dbcMessage* msg = new dbcMessage(msgid);
  msg->SetName(argv[1]);
  msg->SetSize(atoi(argv[2]));
  msg->SetTransmitterNode(argv[3]);
  MyDBC.m_selected->m_messages.AddMessage(msgid,msg);
  writer->printf("DBC: Added message %s\n",argv[0]);
  }

void dbc_message_remove(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg != NULL)
    {
    MyDBC.m_selected->m_messages.RemoveMessage(msg->GetID(),true);
    writer->printf("DBC: Message %s removed\n",argv[0]);
    }
  else
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    }
  }

void dbc_message_set_mux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg == NULL)
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    return;
    }

  if (argc == 1)
    {
    msg->SetMultiplexorSignal(NULL);
    writer->printf("DBC: Cleared mux for %s\n",argv[0]);
    return;
    }

  dbcSignal* signal = msg->FindSignal(argv[1]);
  if (signal == NULL)
    {
    writer->printf("Error: Could not find signal %s on messgae %s\n",argv[1],argv[0]);
    return;
    }

  msg->SetMultiplexorSignal(signal);
  writer->printf("DBC: Set mux for message %s to %s\n",argv[0],argv[1]);
  }

void dbc_signal_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg == NULL)
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    return;
    }
  else
    {
    msg->RemoveAllSignals(true);
    msg->SetMultiplexorSignal(NULL);
    writer->printf("DBC: Cleared all signals for %s\n",argv[0]);
    }
  }

void dbc_signal_add(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg == NULL)
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    return;
    }

  dbcSignal* signal = msg->FindSignal(argv[1]);
  if (signal != NULL)
    {
    writer->printf("Error: Signal %s on message %s already exists\n",argv[1],argv[0]);
    return;
    }

  signal = new dbcSignal(argv[1]);
  signal->SetStartSize(atoi(argv[2]),atoi(argv[3]));
  signal->SetByteOrder((dbcByteOrder_t)atoi(argv[4]));
  if (*argv[5] == '+')
    signal->SetValueType(DBC_VALUETYPE_UNSIGNED);
  else
    signal->SetValueType(DBC_VALUETYPE_SIGNED);
  signal->SetFactorOffset(atof(argv[6]),atof(argv[7]));
  signal->SetMinMax(atof(argv[8]),atof(argv[9]));
  signal->SetUnit(argv[10]);
  signal->AddReceiver(argv[11]);
  msg->AddSignal(signal);
  writer->printf("DBC: Added signal %s on message %s\n",argv[1],argv[0]);
  }

void dbc_signal_remove(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg == NULL)
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    return;
    }

  dbcSignal* signal = msg->FindSignal(argv[1]);
  if (signal == NULL)
    {
    writer->printf("Error: Could not find signal %s on message %s\n",argv[1],argv[0]);
    return;
    }
  else
    {
    msg->RemoveSignal(signal, true);
    writer->printf("DBC: Removed signal %s on message %s\n",argv[1],argv[0]);
    }
  }

void dbc_signal_set_mux(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyDBC.m_selected == NULL)
    {
    writer->puts("Error: No DBC selected");
    return;
    }

  uint32_t msgid = dbcMessageIdFromString(argv[0]);
  dbcMessage* msg = MyDBC.m_selected->m_messages.FindMessage(msgid);
  if (msg == NULL)
    {
    writer->printf("Error: Could not find message %s\n",argv[0]);
    return;
    }

  dbcSignal* signal = msg->FindSignal(argv[1]);
  if (signal == NULL)
    {
    writer->printf("Error: Could not find signal %s on message %s\n",argv[1],argv[0]);
    return;
    }

  if (argc > 2)
    {
    signal->SetMultiplexed(atoi(argv[2]));
    writer->printf("DBC: Set mux %s for signal %s on message %s\n",argv[2],argv[1],argv[0]);
    }
  else
    {
    signal->ClearMultiplexed();
    writer->printf("DBC: Cleared mux for signal %s on message %s\n",argv[1],argv[0]);
    }
  }

dbc::dbc()
  {
  ESP_LOGI(TAG, "Initialising DBC (4520)");
  m_selected = NULL;

  OvmsCommand* cmd_dbc = MyCommandApp.RegisterCommand("dbc","DBC framework");

  cmd_dbc->RegisterCommand("list", "List DBC status", dbc_list);
  cmd_dbc->RegisterCommand("load", "Load DBC file", dbc_load, "<name> <path>", 2, 2);
  cmd_dbc->RegisterCommand("unload", "Unload DBC file", dbc_unload, "<name>", 1, 1);
  cmd_dbc->RegisterCommand("save", "Save DBC file", dbc_save, "[<name>]", 0, 1);
  cmd_dbc->RegisterCommand("dump", "Dump DBC file", dbc_dump, "[<name>]", 0, 1);
  cmd_dbc->RegisterCommand("show", "Show DBC file", dbc_show, "[<name>]", 0, 1);
  cmd_dbc->RegisterCommand("autoload", "Autoload DBC files", dbc_autoload);
  cmd_dbc->RegisterCommand("select", "Select DBC file for editing", dbc_select, "[<name>]", 0, 1);
  cmd_dbc->RegisterCommand("deselect", "Deselect DBC file for editing", dbc_deselect);

  OvmsCommand* cmd_set = cmd_dbc->RegisterCommand("set","DBC Set framework");
  cmd_set->RegisterCommand("version", "Set version for selected DBC file", dbc_set_version, "<version>", 1, 1);
  cmd_set->RegisterCommand("timing", "Set bit timing for selected DBC file", dbc_set_timing, "<baud> <btr1> <btr2>", 3, 3);
  cmd_set->RegisterCommand("messagemux", "Set message mux for selected DBC file", dbc_message_set_mux, "<id> [<signal>]", 1, 2);
  cmd_set->RegisterCommand("signalmux", "Set signal mux for selected DBC file", dbc_signal_set_mux, "<id> <name> [<value>]", 2, 3);

  OvmsCommand* cmd_add = cmd_dbc->RegisterCommand("add","DBC Add framework");
  cmd_add->RegisterCommand("node", "Add node for selected DBC file", dbc_node_add, "<node>", 1, 1);
  cmd_add->RegisterCommand("message", "Add message for selected DBC file", dbc_message_add, "<id> <name> <size> <transmitter>", 4, 4);
  cmd_add->RegisterCommand("signal", "Add signal for selected DBC file", dbc_signal_add, "<id> <name> <start> <size> <order> <type> <factor> <offset> <min> <max> <unit> <receiver>", 12, 12);

  OvmsCommand* cmd_remove = cmd_dbc->RegisterCommand("remove","DBC Remove framework");
  cmd_remove->RegisterCommand("node", "Remove node for selected DBC file", dbc_node_remove, "<node>", 1, 1);
  cmd_remove->RegisterCommand("message", "Remove message for selected DBC file", dbc_message_remove, "<id>", 1, 1);
  cmd_remove->RegisterCommand("signal", "Remove signal for selected DBC file", dbc_signal_remove, "<id> <name>", 2, 2);

  OvmsCommand* cmd_clear = cmd_dbc->RegisterCommand("clear","DBC Clear framework");
  cmd_clear->RegisterCommand("node", "Clear all nodes for selected DBC file", dbc_node_clear);
  cmd_clear->RegisterCommand("message", "Clear all messages for selected DBC file", dbc_message_clear);
  cmd_clear->RegisterCommand("signal", "Clear all signals for selected DBC file", dbc_signal_clear, "<id>", 1, 1);

  MyConfig.RegisterParam("dbc", "DBC Configuration", true, true);
  // Our instances:
  //   'autodirs': Space separated list of directories to auto load DBC files from

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "sd.mounted", std::bind(&dbc_sdmounted, _1, _2));
  }

dbc::~dbc()
  {
  DeselectFile();
  }

bool dbc::LoadFile(const char* name, const char* path)
  {
  OvmsMutexLock ldbc(&m_mutex);

  dbcfile* ndbc = new dbcfile();
  if (!ndbc->LoadFile(name, path))
    {
    delete ndbc;
    return false;
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
    if (k->second->IsLocked())
      {
      ESP_LOGE(TAG,"DBC file %s is locked, so cannot be replaced",name);
      delete ndbc;
      return false;
      }
    else
      {
      delete k->second;
      m_dbclist[name] = ndbc;
      }
    }

  return true;
  }

dbcfile* dbc::LoadString(const char* name, const char* content)
  {
  dbcfile* ndbc = new dbcfile;
  if (!ndbc->LoadString(name, content, strlen(content)))
    {
    delete ndbc;
    return NULL;
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
    if (k->second->IsLocked())
      {
      ESP_LOGE(TAG,"DBC file %s is locked, so cannot be replaced",name);
      delete ndbc;
      return NULL;
      }
    else
      {
      delete k->second;
      m_dbclist[name] = ndbc;
      }
    }

  return ndbc;
  }

bool dbc::Unload(const char* name)
  {
  OvmsMutexLock ldbc(&m_mutex);

  auto k = m_dbclist.find(name);
  if (k == m_dbclist.end())
    {
    return false;
    }
  else if (k->second->IsLocked())
    {
    ESP_LOGE(TAG,"DBC file %s is locked, so cannot be unloaded",name);
    return false;
    }
  else
    {
    // Unload it...
    delete k->second;
    m_dbclist.erase(k);
    return true;
    }
  }

void dbc::LoadDirectory(const char* path, bool log)
  {
  // Load all DBC files in the specified directory
  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir(path)) == NULL) return;
  while ((dp = readdir(dir)) != NULL)
    {
    if ((strlen(dp->d_name)>4)&&
        (strcmp(dp->d_name+(strlen(dp->d_name)-4),".dbc")==0))
      {
      std::string name(dp->d_name,0,strlen(dp->d_name)-4);
      std::string fp(path);
      fp.append("/");
      fp.append(dp->d_name);
      if (log) ESP_LOGI(TAG,"Loading %s (%s)",name.c_str(),fp.c_str());
      LoadFile(name.c_str(),fp.c_str());
      }
    }
  closedir(dir);
  }

void dbc::LoadAutoExtras(bool log)
  {
  std::string extra = MyConfig.GetParamValue("dbc", "autodirs");
  if (!extra.empty())
    {
    size_t sep = 0;
    while (sep != std::string::npos)
      {
      // Set the specified DNS servers
      size_t next = extra.find(' ',sep);
      std::string cpath;
      if (next == std::string::npos)
        {
        // The last one
        cpath = std::string(extra,sep,std::string::npos);
        sep = std::string::npos;
        }
      else
        {
        // One of many
        cpath = std::string(extra,sep,next-sep);
        sep = next+1;
        }
      LoadDirectory(cpath.c_str(),log);
      }
    }
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

bool dbc::SelectFile(dbcfile* select)
  {
  DeselectFile();
  m_selected = select;
  m_selected->LockFile();
  return true;
  }

bool dbc::SelectFile(const char* name)
  {
  dbcfile* select = Find(name);
  if (select == NULL) return false;

  DeselectFile();
  m_selected = select;
  m_selected->LockFile();
  return true;
  }

void dbc::DeselectFile()
  {
  if (m_selected)
    {
    m_selected->UnlockFile();
    m_selected = NULL;
    }
  }

dbcfile* dbc::SelectedFile()
  {
  return m_selected;
  }

void dbc::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "dbc", false))
    LoadDirectory("/store/dbc", true);
  }
