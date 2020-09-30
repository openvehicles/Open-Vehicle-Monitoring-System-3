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
static const char *TAG = "pluginstore";

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include "strverscmp.h"
#include "ovms_plugins.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_http.h"
#include "ovms_buffer.h"
#include "ovms_netmanager.h"
#include "ovms_duktape.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER

OvmsPluginStore MyPluginStore __attribute__ ((init_priority (7100)));

void repo_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.RepoList(writer);
  }

void repo_install(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.RepoInstall(writer,std::string(argv[0]),std::string(argv[0]));
  }

void repo_remove(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.RepoRemove(writer,std::string(argv[0]));
  }

void repo_refresh(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.RepoRefresh(writer);
  }

void plugin_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.Summarise(writer);
  }

void plugin_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginList(writer);
  }

void plugin_show(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginShow(writer,std::string(argv[0]));
  }

void plugin_install(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginInstall(writer,std::string(argv[0]));
  }

void plugin_remove(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginRemove(writer,std::string(argv[0]));
  }

void plugin_enable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginEnable(writer,std::string(argv[0]));
  }

void plugin_disable(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPluginStore.PluginDisable(writer,std::string(argv[0]));
  }

void plugin_update(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    { MyPluginStore.PluginUpdateAll(writer); }
  else
    { MyPluginStore.PluginUpdate(writer,std::string(argv[0])); }
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsPluginStore

OvmsPluginStore::OvmsPluginStore()
  {
  ESP_LOGI(TAG, "Initialising PLUGINS (7100)");

  OvmsCommand* cmd_plugin = MyCommandApp.RegisterCommand("plugin","PLUGIN framework", plugin_status, "", 0, 0, false);
  cmd_plugin->RegisterCommand("status","Show status of plugins",plugin_status,"[nocheck]",0,1);
  cmd_plugin->RegisterCommand("list","Show list of plugins",plugin_list,"",0,0);
  cmd_plugin->RegisterCommand("show","Show plugin details",plugin_show,"<plugin>",1,1);
  cmd_plugin->RegisterCommand("install","Install a plugin",plugin_install,"<plugin>",1,1);
  cmd_plugin->RegisterCommand("remove","Remove a plugin",plugin_remove,"<plugin>",1,1);
  cmd_plugin->RegisterCommand("enable","Enable a plugin",plugin_enable,"<plugin>",1,1);
  cmd_plugin->RegisterCommand("disable","Disable a plugin",plugin_disable,"<plugin>",1,1);
  cmd_plugin->RegisterCommand("update","Update all or a specific plugin",plugin_update,"[plugin]",0,1);

  OvmsCommand* cmd_repo = cmd_plugin->RegisterCommand("repo","PLUGIN Repositories", repo_list, "", 0, 0);
  cmd_repo->RegisterCommand("list","List repositories",repo_list,"",0,0);
  cmd_repo->RegisterCommand("install","Install a repository",repo_install,"<repo> <path>",2,2);
  cmd_repo->RegisterCommand("remove","Remove a repository",repo_remove,"<repo>",1,1);
  cmd_repo->RegisterCommand("refresh","Refresh repository metadata",repo_refresh,"",0,0);

  MyConfig.RegisterParam("plugin", "PLUGIN store setup and status", true, true);
  MyConfig.RegisterParam("plugin.repos", "PLUGIN repositories", false, true);
  MyConfig.RegisterParam("plugin.enabled", "PLUGIN enabled", false, true);
  MyConfig.RegisterParam("plugin.disabled", "PLUGIN disabled", false, true);

  // Config Parameters:
  //   plugin:
  //     repo.refresh: Time (seconds) to cache repository data
  //   plugin.repos:
  //     A list of <name>=<url> for installed repositories
  //   plugin.enabled
  //     A list of <name>=<version> for installed and enabled plugins
  //   plugin.disabled
  //     A list of <name>=<version> for installed but disabled plugins
  }

OvmsPluginStore::~OvmsPluginStore()
  {
  for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
    delete it->second;
    }
  m_plugins.clear();

  for (auto it = m_repos.begin(); it != m_repos.end(); ++it)
    {
    delete it->second;
    }
  m_repos.clear();
  }

void OvmsPluginStore::Summarise(OvmsWriter* writer)
  {
  MyPluginStore.LoadRepoPlugins();

  writer->printf("Plugins:    %d found\n",m_plugins.size());

  for (auto it = m_repos.begin(); it != m_repos.end(); ++it)
    {
    writer->printf("Repository: %s version %s\n",
      it->first.c_str(), it->second->m_version.c_str());
    }
  }

void OvmsPluginStore::RepoList(OvmsWriter* writer)
  {
  MyPluginStore.LoadRepoPlugins();

  writer->puts("Repository       Version");
  for (auto it = m_repos.begin(); it != m_repos.end(); ++it)
    {
    writer->printf("%-16.16s %s\n",
      it->first.c_str(), it->second->m_version.c_str());
    }
  }

void OvmsPluginStore::RepoInstall(OvmsWriter* writer, std::string name, std::string path)
  {
  auto search = m_repos.find(name);
  if (search != m_repos.end())
    {
    writer->printf("Error: Repository '%s' was already installed\n",name.c_str());
    return;
    }

  OvmsRepository* r = new OvmsRepository(name, path);
  m_repos[name] = r;
  if (r->UpdateRepo())
    {
    writer->printf("Installed repository: %s\n", name.c_str());
    }
  else
    {
    writer->printf("Installed, but could not refresh, repository: %s\n", name.c_str());
    }
  }

void OvmsPluginStore::RepoRemove(OvmsWriter* writer, std::string name)
  {
  auto search = m_repos.find(name);
  if (search == m_repos.end())
    {
    writer->printf("Error: Repository '%s' is not installed\n",name.c_str());
    return;
    }

  delete search->second;
  m_repos.erase(search);

  writer->printf("Removed repository: %s\n", name.c_str());
  }

void OvmsPluginStore::RepoRefresh(OvmsWriter* writer)
  {
  for (auto it = m_repos.begin(); it != m_repos.end(); ++it)
    {
    writer->printf("Refreshing repository: %s\n", it->first.c_str());
    if (!it->second->UpdateRepo())
      {
      writer->printf("  Error: Failed to refresh repository: %s\n", it->first.c_str());
      }
    }
  }

void OvmsPluginStore::PluginList(OvmsWriter* writer)
  {
  MyPluginStore.LoadRepoPlugins();

  writer->puts("Plugin               Repo             Version    Description");
  for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
    OvmsPlugin *p = it->second;
    printf("%-20.20s %-16.16s %-10.10s %s\n",
      p->m_name.c_str(),
      p->m_repo.c_str(),
      p->m_version.c_str(),
      p->m_description.c_str());
    }
  }

void OvmsPluginStore::PluginShow(OvmsWriter* writer, std::string plugin)
  {
  MyPluginStore.LoadRepoPlugins();

  auto search = m_plugins.find(plugin);
  if (search == m_plugins.end())
    {
    writer->puts("Error: Cannot find that plugin in repository");
    return;
    }

  OvmsPlugin *p = search->second;
  p->Summarise(writer);
  }

void OvmsPluginStore::PluginInstall(OvmsWriter* writer, std::string plugin)
  {
  MyPluginStore.LoadRepoPlugins();

  OvmsPlugin* p = FindPlugin(plugin);
  if (p == NULL)
    {
    writer->printf("Error: Plugin '%s' not found\n", plugin.c_str());
    return;
    }

  if (!p->Download())
    {
    writer->printf("Error: Plugin '%s' could not be downloaded\n", plugin.c_str());
    return;
    }

  if (!p->Enable())
    {
    writer->printf("Error: Plugin '%s' could not be enabled\n", plugin.c_str());
    return;
    }

  writer->printf("Plugin: %s installed ok\n", plugin.c_str());
  }

void OvmsPluginStore::PluginRemove(OvmsWriter* writer, std::string plugin)
  {
  MyPluginStore.LoadRepoPlugins();

  writer->puts("Error: Not yet implemented");
  }

void OvmsPluginStore::PluginEnable(OvmsWriter* writer, std::string plugin)
  {
  if (MyConfig.IsDefined("plugin.disabled", plugin))
    {
    std::string version = MyConfig.GetParamValue("plugin.disabled", plugin);
    MyConfig.DeleteInstance("plugin.disabled", plugin);
    MyConfig.SetParamValue("plugin.enabled", plugin, version);
    writer->printf("Plugin: %s enabled\n", plugin.c_str());
    }
  else
    {
    writer->printf("Error: Plugin '%s' is not disabled\n", plugin.c_str());
    }
  }

void OvmsPluginStore::PluginDisable(OvmsWriter* writer, std::string plugin)
  {
  if (MyConfig.IsDefined("plugin.enabled", plugin))
    {
    std::string version = MyConfig.GetParamValue("plugin.enabled", plugin);
    MyConfig.DeleteInstance("plugin.enabled", plugin);
    MyConfig.SetParamValue("plugin.disabled", plugin, version);
    writer->printf("Plugin: %s disabled\n", plugin.c_str());
    }
  else
    {
    writer->printf("Error: Plugin '%s' is not enabled\n", plugin.c_str());
    }
  }

void OvmsPluginStore::PluginUpdate(OvmsWriter* writer, std::string plugin)
  {
  MyPluginStore.LoadRepoPlugins();

  OvmsPlugin* p = FindPlugin(plugin);
  if (p == NULL)
    {
    writer->printf("Error: Plugin '%s' not found\n", plugin.c_str());
    return;
    }

  std::string version = p->InstalledVersion();
  if (version.empty())
      {
      writer->printf("Error: Plugin '%s' is not installed\n", plugin.c_str());
      return;
      }

  int cmp = strverscmp(p->m_version.c_str(), version.c_str());
  if (cmp <= 0)
    {
    writer->printf("Plugin '%s' is already version %s (repo has %s)\n",
      plugin.c_str(), version.c_str(), p->m_version.c_str());
    return;
    }

  if (!p->Download())
    {
    writer->printf("Error: Plugin '%s' could not be downloaded\n", plugin.c_str());
    return;
    }

  writer->printf("Plugin: %s updated ok\n", plugin.c_str());
  }

void OvmsPluginStore::PluginUpdateAll(OvmsWriter* writer)
  {
  MyPluginStore.LoadRepoPlugins();

  for (auto it = m_plugins.begin(); it != m_plugins.end(); ++it)
    {
    OvmsPlugin *p = it->second;

    std::string version = p->InstalledVersion();
    if (! version.empty())
      {
      int cmp = strverscmp(p->m_version.c_str(), version.c_str());
      if (cmp > 0)
        {
        if (!p->Download())
          {
          writer->printf("Error: Plugin '%s' could not be downloaded\n", p->m_name.c_str());
          }
        else
          {
          writer->printf("Plugin: %s updated ok to %s\n", p->m_name.c_str(), p->m_version.c_str());
          }
        }
      }
    }
  }

bool OvmsPluginStore::LoadRepoPlugins()
  {
  const ConfigParamMap *cpm = MyConfig.GetParamMap("plugin.repos");
  if (cpm->size() == 0)
    {
    // Configure the default repo
    MyConfig.SetParamValue("plugin.repos","openvehicles","http://api.openvehicles.com/plugins");
    }

  if (m_repos.size() == 0)
    {
    // Load the repos from config
    for (auto it = cpm->begin(); it != cpm->end(); ++it)
      {
      OvmsRepository* r = new OvmsRepository(it->first, it->second);
      m_repos[it->first] = r;
      }
    }

  for (auto it = m_repos.begin(); it != m_repos.end(); ++it)
    {
    OvmsRepository* r = it->second;
    if (! r->CacheRepo())
      {
      ESP_LOGE(TAG, "Plugin repository '%s' failed to refresh", it->first.c_str());
      return false;
      }
    }

  return true;
  }

std::string OvmsPluginStore::RepoPath(std::string repo)
  {
  auto search = m_repos.find(repo);
  if (search == m_repos.end())
    {
    return std::string("");
    }
  else
    {
    return search->second->m_path;
    }
  }

OvmsPlugin* OvmsPluginStore::FindPlugin(std::string plugin)
  {
  auto search = m_plugins.find(plugin);
  if (search == m_plugins.end())
    {
    return NULL;
    }
  else
    {
    return search->second;
    }
  }

void OvmsPluginStore::LoadEnabledModules(plugin_element_type_t type)
  {
  const ConfigParamMap* enabled = MyConfig.GetParamMap("plugin.enabled");
  if (enabled != NULL)
    {
    ESP_LOGI(TAG, "Loading enabled plugins (%d)",type);
    for (auto it=enabled->begin(); it!=enabled->end(); ++it)
      {
      std::string path("/store/plugins/");
      path.append(it->first);
      path.append("/");
      path.append(it->first);
      path.append(".json");

      FILE *pf = fopen(path.c_str(), "r");
      if (pf == NULL)
        {
        ESP_LOGE(TAG,"Plugin %s: could not be found on disk",it->first.c_str());
        }
      else
        {
        fseek(pf,0,SEEK_END);
        long plen = ftell(pf);
        fseek(pf,0,SEEK_SET);
        char *pd = new char[plen+1];
        memset(pd,0,plen+1);
        fread(pd,1,plen,pf);
        fclose(pf);

        cJSON *json = cJSON_Parse(pd);
        delete [] pd;
        if (json == NULL)
          {
          ESP_LOGE(TAG, "Plugin %s: could not parse metadata", it->first.c_str());
          }
        else
          {
          OvmsPlugin p;
          p.LoadJSON(std::string("LoadEnabledModules"), json);

          // Iterate over the elements...
          for (OvmsPluginElement* e : p.m_elements)
            {
            if ((type==EL_MODULE) && (e->m_type == EL_MODULE))
              {
              // Load an enabled script module
              const char* filename = "LoadEnabledModules";
              duk_context* ctx = MyDuktape.DukTapeContext();

              // Prepare the javascript command
              std::string cmd(p.m_name);
              cmd.append(" = require(\"");
              cmd.append("plugin/");
              cmd.append(p.m_name);
              cmd.append("/");
              cmd.append(e->m_path);
              cmd.erase(cmd.length()-3,cmd.length());
              cmd.append("\");");

              // We are running within Duktape task so need to eval here
              duk_push_string(ctx, cmd.c_str());
              duk_push_string(ctx, filename);
              if (duk_pcompile(ctx, DUK_COMPILE_EVAL) != 0 || duk_pcall(ctx, 0) != 0)
                {
                DukOvmsErrorHandler(ctx, -1, NULL, filename);
                }
              duk_pop(ctx);
              ESP_LOGI(TAG,"  Load %s script %s", p.m_name.c_str(), e->m_path.c_str());
              }
            #ifdef CONFIG_OVMS_COMP_WEBSERVER
            else if ((type==EL_WEB_PAGE) && (e->m_type == EL_WEB_PAGE))
              {
              // Load an enabled web page
              MyWebServer.m_plugin_pages.insert({
                e->GetAttribute("page"),
                PagePluginContent(p.m_name + "/" + e->m_path, true) });
              MyWebServer.RegisterPage(
                e->GetAttribute("page"),
                e->GetAttribute("label"),
                OvmsWebServer::PluginHandler,
                Code2PageMenu(e->GetAttribute("menu")),
                Code2PageAuth(e->GetAttribute("auth")),
                -1);
              ESP_LOGI(TAG,"  Load %s web page %s",
                p.m_name.c_str(),
                e->GetAttribute("page").c_str());
              }
            else if ((type==EL_WEB_HOOK) && (e->m_type == EL_WEB_HOOK))
              {
              // Load an enabled web hook
              MyWebServer.m_plugin_parts.insert({
                e->GetAttribute("page") + ":" + e->GetAttribute("hook"),
                PagePluginContent(p.m_name + "/" + e->m_path, true) });
              MyWebServer.RegisterCallback(
                "http.plugin",
                 e->GetAttribute("page"),
                 OvmsWebServer::PluginCallback,
                 -1);
              ESP_LOGI(TAG,"  Load %s web hook %s:%s",
                p.m_name.c_str(),
                e->GetAttribute("page").c_str(),
                e->GetAttribute("hook").c_str());
              }
            #endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
            }
          // Clean up
          cJSON_Delete(json);
          }
        }
      }
    }
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsPluginPrerequisite

OvmsPluginPrerequisite::OvmsPluginPrerequisite()
  {
  }

OvmsPluginPrerequisite::~OvmsPluginPrerequisite()
  {
  }

void OvmsPluginPrerequisite::Summarise(OvmsWriter* writer)
  {
  writer->printf("    %s ", m_target.c_str());
  switch (m_operator)
    {
    case OP_EQUAL:
      writer->printf("= %s\n",m_value.c_str()); break;
    case OP_NOTEQUAL:
      writer->printf("!= %s\n",m_value.c_str()); break;
    case OP_LESSTHAN:
      writer->printf("< %s\n",m_value.c_str()); break;
    case OP_LESSTHANEQUAL:
      writer->printf("<= %s\n",m_value.c_str()); break;
    case OP_GREATERTHAN:
      writer->printf("> %s\n",m_value.c_str()); break;
    case OP_GREATERTHANEQUAL:
      writer->printf(">= %s\n",m_value.c_str()); break;
    case OP_EXISTS:
      writer->puts(m_value.c_str()); break;
    }
  }

bool OvmsPluginPrerequisite::LoadJSON(cJSON *json)
  {
  if (json->type != cJSON_String)
    {
    ESP_LOGE(TAG, "Prerequisite type %d unexpected",json->type);
    return false;
    }

  m_target = std::string(json->valuestring);

  size_t p = m_target.find("!=");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+2);
    m_target.resize(p);
    m_operator = OP_NOTEQUAL;
    return true;
    }

  p = m_target.find(">=");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+2);
    m_target.resize(p);
    m_operator = OP_GREATERTHANEQUAL;
    return true;
    }

  p = m_target.find("<=");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+2);
    m_target.resize(p);
    m_operator = OP_LESSTHANEQUAL;
    return true;
    }

  p = m_target.find("<");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+1);
    m_target.resize(p);
    m_operator = OP_LESSTHAN;
    return true;
    }

  p = m_target.find(">");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+1);
    m_target.resize(p);
    m_operator = OP_GREATERTHAN;
    return true;
    }

  p = m_target.find("=");
  if (p != std::string::npos)
    {
    m_value = m_target.substr(p+1);
    m_target.resize(p);
    m_operator = OP_EQUAL;
    return true;
    }

  m_operator = OP_EXISTS;
  ESP_LOGI(TAG,"Prerequisite '%s' T=%s O=%d V=%s",
    json->valuestring, m_target.c_str(), m_operator, m_value.c_str());
  return true;
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsPluginElement

OvmsPluginElement::OvmsPluginElement()
  {
  }

OvmsPluginElement::OvmsPluginElement(plugin_element_type_t type, std::string name, std::string path)
  {
  m_type = type;
  m_name = name;
  m_path = path;
  if ((m_path.empty())&&(m_type == EL_JSON))
    {
    // Helper for the JSON element
    m_path = name;
    m_path.append(".json");
    }
  }

OvmsPluginElement::~OvmsPluginElement()
  {
  }

void OvmsPluginElement::Summarise(OvmsWriter* writer)
  {
  writer->printf("    Name:   %s\n", m_name.c_str());
  writer->printf("      Path: %s\n", m_path.c_str());
  writer->printf("      Type: ");
  switch (m_type)
    {
    case EL_JSON:
      writer->puts("json"); break;
    case EL_MODULE:
      writer->puts("module"); break;
    case EL_WEB_PAGE:
      writer->puts("webpage"); break;
    case EL_WEB_HOOK:
      writer->puts("webhook"); break;
    case EL_WEB_RSC:
      writer->puts("webrsc"); break;
    }

  if (m_attr.size() > 0)
    {
    writer->puts("      Attr:");
    for (auto it = m_attr.begin(); it != m_attr.end(); ++it)
      {
      writer->printf("        %s:%s\n", it->first.c_str(), it->second.c_str());
      }
    }
  }

bool OvmsPluginElement::LoadJSON(cJSON *json)
  {
  if (json->type != cJSON_Object)
    {
    ESP_LOGE(TAG, "Element type %d unexpected",json->type);
    return false;
    }

  cJSON *el = json->child;
  while (el != NULL)
    {
    if ((el->type == cJSON_String) && (strcmp(el->string,"name")==0))
      { m_name = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"path")==0))
      { m_path = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"type")==0))
      {
      if (strcmp(el->valuestring,"module")==0)
        { m_type = EL_MODULE; }
      else if (strcmp(el->valuestring,"webpage")==0)
        { m_type = EL_WEB_PAGE; }
      else if (strcmp(el->valuestring,"webhook")==0)
        { m_type = EL_WEB_HOOK; }
      else
        { m_type = EL_WEB_RSC; }
      }
    else
      {
      // Treat it as an attribute
      m_attr[std::string(el->string)] = std::string(el->valuestring);
      }
    el = el->next;
    }
  return true;
  }

std::string OvmsPluginElement::GetAttribute(std::string name)
  {
  auto search = m_attr.find(name);
  if (search != m_attr.end())
    {
    return search->second;
    }
  else
    {
    return std::string("");
    }
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsPlugin

OvmsPlugin::OvmsPlugin()
  {
  }

OvmsPlugin::~OvmsPlugin()
  {
  for (OvmsPluginPrerequisite *p : m_prerequisites)
    {
    delete p;
    }
  m_prerequisites.clear();

  for (OvmsPluginElement *p : m_elements)
    {
    delete p;
    }
  m_elements.clear();
  }

void OvmsPlugin::Summarise(OvmsWriter* writer)
  {
  writer->printf("Plugin: %s\n\n",m_name.c_str());

  writer->printf("  Title:       %s\n",m_title.c_str());
  writer->printf("  Description: %s\n",m_description.c_str());
  writer->printf("  Group:       %s\n",m_group.c_str());
  writer->printf("  Info:        %s\n",m_info.c_str());
  writer->printf("  Maintainer:  %s\n",m_maintainer.c_str());
  writer->printf("  Repository:  %s\n",m_repo.c_str());
  writer->printf("  Version:     %s\n",m_version.c_str());

  if (m_prerequisites.size() > 0)
    {
    writer->printf("\n  Prerequisites:\n");
    for (OvmsPluginPrerequisite *p : m_prerequisites)
      {
      p->Summarise(writer);
      }
    }

  if (m_elements.size() > 0)
    {
    writer->printf("\n  Elements:\n");
    for (OvmsPluginElement *p : m_elements)
      {
      p->Summarise(writer);
      }
    }
  }

bool OvmsPlugin::LoadJSON(std::string repo, cJSON *json)
  {
  m_repo = repo;
  cJSON* el = json->child;
  while (el != NULL)
    {
    // Process the given element
    if ((el->type == cJSON_String) && (strcmp(el->string,"name")==0))
      { m_name = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"title")==0))
      { m_title = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"description")==0))
      { m_description = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"group")==0))
      { m_group = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"info")==0))
      { m_info = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"maintainer")==0))
      { m_maintainer = std::string(el->valuestring); }
    else if ((el->type == cJSON_String) && (strcmp(el->string,"version")==0))
      { m_version = std::string(el->valuestring); }
    else if ((el->type == cJSON_Array) && (strcmp(el->string,"prerequisites")==0))
      {
      cJSON* cel = el->child;
      while (cel != NULL)
        {
        OvmsPluginPrerequisite *p = new OvmsPluginPrerequisite();
        if (!p->LoadJSON(cel))
          {
          ESP_LOGE(TAG, "Invalid plugin prerequisite in metadata");
          delete p;
          return false;
          }
        m_prerequisites.push_back(p);
        cel = cel->next;
        }
      }
    else if ((el->type == cJSON_Array) && (strcmp(el->string,"elements")==0))
      {
      cJSON* cel = el->child;
      while (cel != NULL)
        {
        OvmsPluginElement *p = new OvmsPluginElement();
        if (!p->LoadJSON(cel))
          {
          ESP_LOGE(TAG, "Invalid plugin element in metadata");
          delete p;
          return false;
          }
        m_elements.push_back(p);
        cel = cel->next;
        }
      }
    else
      {
      return false;
      }
    el = el->next;
    }

  // Add on the JSON plugin descriptor file
  m_elements.push_back(new OvmsPluginElement(EL_JSON,m_name,std::string("")));

  return true;
  }

bool OvmsPlugin::Download()
  {
  std::string repopath = MyPluginStore.RepoPath(m_repo);
  if (repopath.empty())
    {
    ESP_LOGE(TAG, "Error: cannot find path for repository: %s",m_repo.c_str());
    return false;
    }

  ESP_LOGI(TAG, "Downloading plugin: %s, with %d element(s)",
    m_name.c_str(), m_elements.size());

  std::string pluginpath("/store/plugins");
  mkdir(pluginpath.c_str(),0);
  pluginpath.append("/");
  pluginpath.append(m_name.c_str());
  mkdir(pluginpath.c_str(),0);

  for (OvmsPluginElement *p : m_elements)
    {
    std::string url(repopath);
    url.append("/");
    url.append(m_name);
    url.append("/");
    url.append(p->m_path);

    OvmsSyncHttpClient http;
    if (!http.Request(url))
      {
      ESP_LOGE(TAG, "Element %s: HTTP request failed: %s",p->m_path.c_str(), http.GetError().c_str());
      return false;
      }

    std::string body = http.GetBodyAsString();

    std::string dest(pluginpath);
    dest.append("/");
    dest.append(p->m_path);
    FILE *pf = fopen(dest.c_str(), "w");
    size_t written = fwrite(body.c_str(), 1, body.length(), pf);
    fclose(pf);
    if (written != body.length())
      {
      ESP_LOGE(TAG, "Element: %s: VFS body size %d doesn't match %d",
        p->m_path.c_str(), written, body.length());
      return false;
      }

    ESP_LOGI(TAG, "Element: %s downloaded ok", p->m_path.c_str());
    }

  ESP_LOGI(TAG, "Plugin: %s downloaded ok", m_name.c_str());
  return true;
  }

bool OvmsPlugin::Enable()
  {
  if (MyConfig.IsDefined("plugin.disabled", m_name))
    {
    MyConfig.DeleteInstance("plugin.disabled", m_name);
    }

  MyConfig.SetParamValue("plugin.enabled", m_name, m_version);

  return true;
  }

bool OvmsPlugin::Disable()
  {
  if (MyConfig.IsDefined("plugin.enabled", m_name))
    {
    MyConfig.DeleteInstance("plugin.enabled", m_name);
    }

  MyConfig.SetParamValue("plugin.disabled", m_name, m_version);

  return true;
  }

std::string OvmsPlugin::InstalledVersion()
  {
  std::string version;
  if (MyConfig.IsDefined("plugin.enabled", m_name))
    {
    version = MyConfig.GetParamValue("plugin.enabled", m_name);
    }
  else if (MyConfig.IsDefined("plugin.disabled", m_name))
    {
    version = MyConfig.GetParamValue("plugin.disabled", m_name);
    }

  return version;
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsRepository

OvmsRepository::OvmsRepository(std::string name, std::string path)
  {
  m_name = name;
  m_path = path;
  m_lastrefresh = 0;
  }

OvmsRepository::~OvmsRepository()
  {
  }

bool OvmsRepository::CacheRepo()
  {
  if ((m_lastrefresh > 0) &&
      ((m_lastrefresh + MyConfig.GetParamValueInt("plugin","repo.refresh",3600)) > monotonictime))
    { return true; } // Repository does not need a refresh

  return UpdateRepo();
  }

bool OvmsRepository::UpdateRepo()
  {
  std::string url = m_path;
  url.append("/plugins.rev");

  OvmsSyncHttpClient http;
  if (!http.Request(url))
    {
    ESP_LOGE(TAG, "Repo %s: HTTP request failed: %s",m_name.c_str(), http.GetError().c_str());
    return false;
    }
  OvmsBuffer *result = http.GetBodyAsBuffer();
  if (!result->HasLine())
    {
    ESP_LOGE(TAG, "Repo %s:  HTTP response invalid",m_name.c_str());
    return false;
    }

  std::string version = result->ReadLine();

  if (version.compare(m_version) == 0)
    {
    ESP_LOGI(TAG, "Plugin repository %s version: %s",m_name.c_str(), version.c_str());
    return true;
    }

  ESP_LOGI(TAG, "Plugin repository %s has changed, retrieving latest version", m_name.c_str());
  url = m_path;
  url.append("/plugins.json");
  http.Reset();
  if (!http.Request(url))
    {
    ESP_LOGE(TAG, "Repo %s: HTTP request failed: %s",m_name.c_str(),http.GetError().c_str());
    return false;
    }

  std::string body = http.GetBodyAsString();
    ESP_LOGD(TAG, "Retrieved %d byte(s) of store metadata",body.length());

  const char *bp = body.c_str();
  cJSON *json = cJSON_Parse(bp);
  if (json == NULL)
    {
    ESP_LOGE(TAG, "Repo %s: Could not parse metadata (%d)", m_name.c_str(), cJSON_GetErrorPtr()-bp);
    return false;
    }

  for (auto it = MyPluginStore.m_plugins.begin(); it != MyPluginStore.m_plugins.end(); ++it)
    {
    if (it->second->m_repo.compare(m_name) == 0)
      {
      delete it->second;
      MyPluginStore.m_plugins.erase(it);
      }
    }

  if (!LoadPlugins(json))
    {
    ESP_LOGE(TAG, "Repo %s: Could not log plugins from repository metadata", m_name.c_str());
    cJSON_Delete(json);
    return false;
    }

  // Free up the cJSON structure
  cJSON_Delete(json);

  // Update version, and quit
  ESP_LOGI(TAG, "Plugin repository %s version: %s",m_name.c_str(), version.c_str());
  m_version = version;
  m_lastrefresh = monotonictime;
  return true;
  }

bool OvmsRepository::LoadPlugins(cJSON *json)
  {
  // Load the plugins from the provided cJSON structure
  if (json == NULL)
    {
    ESP_LOGE(TAG, "Repo %s: Plugin metadata is not defined",m_name.c_str());
    return false;
    }
  if (json->type != cJSON_Array)
    {
    ESP_LOGE(TAG, "Repo %s: Unexpected root type %d for plugin metadata",m_name.c_str(),json->type);
    return false;
    }
  if (json->child == NULL)
    {
    ESP_LOGE(TAG, "Repo %s: Unexpected empty list of plugins in metadata",m_name.c_str());
    return false;
    }

  cJSON* el = json->child;
  while (el != NULL)
    {
    // Process the given element
    if (el->type != cJSON_Object)
      {
      ESP_LOGE(TAG, "Repo %s: Invalid plugin type %d in metadata",m_name.c_str(),el->type);
      return false;
      }

    OvmsPlugin *p = new OvmsPlugin();
    if (!p->LoadJSON(m_name,el))
      {
      ESP_LOGE(TAG, "Repo %s: Invalid plugin in metadata",m_name.c_str());
      delete p;
      return false;
      }
    MyPluginStore.m_plugins[p->m_name] = p;
    el = el->next;
    }

  return true;
  }
