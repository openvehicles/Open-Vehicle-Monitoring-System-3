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

#include "esp_log.h"
static const char *TAG = "config";

#include <unistd.h>
#include <sys/stat.h>
#include <string.h>
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"

#define OVMS_CONFIGPATH "/store/ovms_config"
#define OVMS_MAXVALSIZE 1024

OvmsConfig MyConfig __attribute__ ((init_priority (1400)));

void store_mount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.mount();
  writer->puts("Mounted STORE");
  }

void store_unmount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.unmount();
  writer->puts("Unmounted STORE");
  }

void config_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted()) return;

  if (argc == 0)
    {
    // Show all parameters
    for (std::map<std::string, OvmsConfigParam*>::iterator it=MyConfig.m_map.begin(); it!=MyConfig.m_map.end(); ++it)
      {
      writer->puts(it->first.c_str());
      }
    }
  else
    {
    // Show all instances for a particular parameter
    OvmsConfigParam *p = MyConfig.CachedParam(argv[0]);
    if (p)
      {
      writer->puts(argv[0]);
      for (std::map<std::string, std::string>::iterator it=p->m_map.begin(); it!=p->m_map.end(); ++it)
        {
        if (p->Readable())
          { writer->printf("  %s: %s\n",it->first.c_str(), it->second.c_str()); }
        else
          { writer->printf("  %s\n",it->first.c_str()); }
        }
      }
    }
  }

void config_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyConfig.ismounted()) return;

  OvmsConfigParam *p = MyConfig.CachedParam(argv[0]);
  if (p==NULL)
    {
    writer->puts("Error: parameter not found");
    return;
    }

  if (!p->Writable())
    {
    writer->puts("Error: parameter is not writeable");
    return;
    }

  p->SetValue(argv[1],argv[2]);
  }

OvmsConfig::OvmsConfig()
  {
  ESP_LOGI(TAG, "Initialising CONFIG (1400)");

  OvmsCommand* cmd_store = MyCommandApp.RegisterCommand("store","STORE framework",NULL,"<$C>",1,1);
  cmd_store->RegisterCommand("mount","Mount STORE",store_mount,"",0,0);
  cmd_store->RegisterCommand("unmount","Unmount STORE",store_unmount,"",0,0);

  OvmsCommand* cmd_config = MyCommandApp.RegisterCommand("config","CONFIG framework",NULL,"<$C>",1,1);
  cmd_config->RegisterCommand("list","Show configuration parameters/instances",config_list,"",0,1);
  cmd_config->RegisterCommand("set","Set parameter:instance=value",config_set,"<param> <instance> <value>",3,3);
  }

OvmsConfig::~OvmsConfig()
  {
  }

esp_err_t OvmsConfig::mount()
  {
//  if (!spiffs_is_registered)
//    vfs_spiffs_register();

//  if (!spiffs_is_mounted)
//    spiffs_mount();

  if (! m_mounted)
    {
    memset(&m_store_fat,0,sizeof(esp_vfs_fat_sdmmc_mount_config_t));
    m_store_fat.format_if_mount_failed = true;
    m_store_fat.max_files = 5;
    esp_vfs_fat_spiflash_mount("/store", "store", &m_store_fat, &m_store_wlh);
    m_mounted = true;
    for (std::map<std::string, OvmsConfigParam*>::iterator it=MyConfig.m_map.begin(); it!=MyConfig.m_map.end(); ++it)
      {
      it->second->Load();
      }
    MyEvents.SignalEvent("config.mounted", NULL);
    }

  struct stat ds;
  if (stat(OVMS_CONFIGPATH, &ds) != 0)
    {
    puts("Initialising OVMS CONFIG within STORE");
    mkdir(OVMS_CONFIGPATH,0);
    }

  return ESP_OK;
  }

esp_err_t OvmsConfig::unmount()
  {
//  if (spiffs_is_mounted)
//    spiffs_unmount(0);

  if (m_mounted)
    {
    esp_vfs_fat_spiflash_unmount("/store", m_store_wlh);
    m_mounted = false;
    MyEvents.SignalEvent("config.unmounted", NULL);
    }

  return ESP_OK;
  }

bool OvmsConfig::ismounted()
  {
  return m_mounted;
  }

void OvmsConfig::RegisterParam(std::string name, std::string title, bool writable, bool readable)
  {
  auto k = m_map.find(name);
  if (k == m_map.end())
    {
    OvmsConfigParam* p = new OvmsConfigParam(name, title, writable, readable);
    m_map[name] = p;
    }
  }

void OvmsConfig::DeregisterParam(std::string name)
  {
  auto k = m_map.find(name);
  if (k != m_map.end())
    {
    k->second->DeleteParam();
    delete k->second;
    m_map.erase(k);
    }
  }

void OvmsConfig::SetParamValue(std::string param, std::string instance, std::string value)
  {
  OvmsConfigParam *p = CachedParam(param);
  if (p)
    {
    p->SetValue(instance,value);
    }
  }

void OvmsConfig::DeleteInstance(std::string param, std::string instance)
  {
  OvmsConfigParam *p = CachedParam(param);
  if (p)
    {
    p->DeleteInstance(instance);
    }
  }

std::string OvmsConfig::GetParamValue(std::string param, std::string instance)
  {
  OvmsConfigParam *p = CachedParam(param);
  if (p)
    {
    return p->GetValue(instance);
    }
  else
    {
    return std::string("");
    }
  }

int OvmsConfig::GetParamValueInt(std::string param, std::string instance)
  {
  return atoi(GetParamValue(param,instance).c_str());
  }

float OvmsConfig::GetParamValueFloat(std::string param, std::string instance)
  {
  return atof(GetParamValue(param,instance).c_str());
  }

OvmsConfigParam* OvmsConfig::CachedParam(std::string param)
  {
  if (!m_mounted) return NULL;

  auto k = m_map.find(param);
  if (k == m_map.end())
    return NULL;
  else
    return k->second;
  }

bool OvmsConfig::ProtectedPath(std::string path)
  {
#ifdef CONFIG_OVMS_DEV_CONFIGVFS
  return false;
#else
  return (path.find(OVMS_CONFIGPATH) != std::string::npos);
#endif // #ifdef CONFIG_OVMS_DEV_CONFIGVFS
  }

OvmsConfigParam::OvmsConfigParam(std::string name, std::string title, bool writable, bool readable)
  {
  m_name = name;
  m_title = title;
  m_writable = writable;
  m_readable = readable;
  m_loaded = false;

  if (MyConfig.ismounted())
    {
    LoadConfig();
    }
  }

OvmsConfigParam::~OvmsConfigParam()
  {
  }

void OvmsConfigParam::LoadConfig()
  {
  if (m_loaded) return;  // Protected against loading more tghan once

  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  // ESP_LOGI(TAG, "Trying %s",path.c_str());
  FILE* f = fopen(path.c_str(), "r");
  if (f)
    {
    char buf[OVMS_MAXVALSIZE];
    while (fgets(buf,sizeof(buf),f))
      {
      buf[strlen(buf)-1] = 0; // Remove trailing newline
      char *p = index(buf,' ');
      if (p)
        {
        *p = 0; // Null terminate the key
        p++;    // and point to the value
        m_map[std::string(buf)] = std::string(p);
        // ESP_LOGI(TAG, "Loaded %s/%s=%s", m_name.c_str(), buf, p);
        }
      }
    fclose(f);
    }
  m_loaded = true;
  }

void OvmsConfigParam::SetValue(std::string instance, std::string value)
  {
  m_map[instance] = value;
  RewriteConfig();
  }

void OvmsConfigParam::DeleteParam()
  {
  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  unlink(path.c_str());
  }

void OvmsConfigParam::DeleteInstance(std::string instance)
  {
  auto k = m_map.find(instance);
  if (k != m_map.end())
    {
    m_map.erase(k);
    RewriteConfig();
    }
  }

std::string OvmsConfigParam::GetValue(std::string instance)
  {
  auto k = m_map.find(instance);
  if (k == m_map.end())
    return std::string("");
  else
    return k->second;
  }

bool OvmsConfigParam::Writable()
  {
  return m_writable;
  }

bool OvmsConfigParam::Readable()
  {
  return m_readable;
  }

void OvmsConfigParam::RewriteConfig()
  {
  std::string path(OVMS_CONFIGPATH);
  path.append("/");
  path.append(m_name);
  FILE* f = fopen(path.c_str(), "w");
  if (f)
    {
    for (std::map<std::string, std::string>::iterator it=m_map.begin(); it!=m_map.end(); ++it)
      {
      fprintf(f,"%s %s\n",it->first.c_str(),it->second.c_str());
      }
    fclose(f);
    }
  }

void OvmsConfigParam::Load()
  {
  if (!m_loaded) LoadConfig();
  }
