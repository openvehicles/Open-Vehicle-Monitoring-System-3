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

dbc MyDBC __attribute__ ((init_priority (4510)));

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
  dbcfile* dbc = MyDBC.Find(argv[0]);
  if (dbc == NULL)
    {
    writer->printf("Cannot find DBD file: %s",argv[0]);
    return;
    }

  writer->printf("DBC:     %s\n",argv[0]);

  using std::placeholders::_1;
  using std::placeholders::_2;
  dbc->WriteSummary(std::bind(dbc_show_callback,_1,_2), writer);
  }

void dbc_dump(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  dbcfile* dbc = MyDBC.Find(argv[0]);
  if (dbc == NULL)
    {
    writer->printf("Cannot find DBD file: %s",argv[0]);
    return;
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
  dbcfile* dbc = MyDBC.Find(argv[0]);
  if (dbc == NULL)
    {
    writer->printf("Cannot find DBD file: %s",argv[0]);
    return;
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

dbc::dbc()
  {
  ESP_LOGI(TAG, "Initialising DBC (4510)");

  OvmsCommand* cmd_dbc = MyCommandApp.RegisterCommand("dbc","DBC framework",NULL, "", 0, 0, true);

  cmd_dbc->RegisterCommand("list", "List DBC status", dbc_list, "", 0, 0, true);
  cmd_dbc->RegisterCommand("load", "Load DBC file", dbc_load, "<name> <path>", 2, 2, true);
  cmd_dbc->RegisterCommand("unload", "Unload DBC file", dbc_unload, "<name>", 1, 1, true);
  cmd_dbc->RegisterCommand("save", "Save DBC file", dbc_save, "<name>", 1, 3, true);
  cmd_dbc->RegisterCommand("dump", "Dump DBC file", dbc_dump, "<name>", 1, 3, true);
  cmd_dbc->RegisterCommand("show", "Show DBC file", dbc_show, "<name>", 1, 3, true);
  cmd_dbc->RegisterCommand("autoload", "Autoload DBC files", dbc_autoload, "", 0, 0, true);

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

void dbc::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "dbc", false))
    LoadDirectory("/store/dbc", true);
  }
