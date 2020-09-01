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
static const char *TAG = "script";

#include <string>
#include <fstream>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <esp_task_wdt.h>
#include "ovms_malloc.h"
#include "ovms_module.h"
#include "ovms_script.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "console_async.h"
#include "buffered_shell.h"
#include "ovms_netmanager.h"
#include "ovms_tls.h"

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#include "ovms_duktape.h"

static void script_reload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Reloading javascript engine");
  MyDuktape.DuktapeReload();
  }

static void script_eval(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyDuktape.DuktapeEvalNoResult(argv[0], writer);
  }

static void script_compact(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  writer->puts("Compacting javascript memory");
  MyDuktape.DuktapeCompact();
  }

#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

OvmsScripts MyScripts __attribute__ ((init_priority (1600)));

static void script_ovms(int verbosity, OvmsWriter* writer,
  const char* spath, FILE* sf, bool secure=false)
  {
  char *ext = rindex(spath, '.');
  if ((ext != NULL)&&(strcmp(ext,".js")==0))
    {
    // Javascript script
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    fseek(sf,0,SEEK_END);
    long slen = ftell(sf);
    fseek(sf,0,SEEK_SET);
    char *script = new char[slen+1];
    memset(script,0,slen+1);
    fread(script,1,slen,sf);
    MyDuktape.NotifyDuktapeModuleLoad(spath);
    MyDuktape.DuktapeEvalNoResult(script, writer, spath);
    MyDuktape.NotifyDuktapeModuleUnload(spath);
    delete [] script;
#else // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    if (writer)
      {
      writer->puts("Error: No javascript engine available");
      }
    else
      {
      ESP_LOGE(TAG, "Error: No javascript engine available");
      }
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    fclose(sf);
    }
  else
    {
    // Default: OVMS command script
    BufferedShell* bs = new BufferedShell(false, verbosity);
    if (secure) bs->SetSecure(true);
    char* cmdline = new char[_COMMAND_LINE_LEN];
    while(fgets(cmdline, _COMMAND_LINE_LEN, sf) != NULL )
      {
      bs->ProcessChars(cmdline, strlen(cmdline));
      }
    fclose(sf);
    if (writer)
      {
      bs->Output(writer);
      }
    else
      {
      extram::string output;
      bs->Dump(output);
      ESP_LOGI(TAG, "%s", output.c_str());
      }
    delete bs;
    delete [] cmdline;
    }
  }

static void script_run(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  FILE *sf = NULL;
  std::string path;

  if (argv[0][0] == '/')
    {
    // A direct path specification
    path = std::string(argv[0]);
    sf = fopen(argv[0], "r");
    }
  else
    {
#ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
    path = std::string("/sd/scripts/");
    path.append(argv[0]);
    sf = fopen(path.c_str(), "r");
#endif // #ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
    if (sf == NULL)
      {
      path = std::string("/store/scripts/");
      path.append(argv[0]);
      sf = fopen(path.c_str(), "r");
      }
    }
  if (sf == NULL)
    {
    writer->puts("Error: Script not found");
    return;
    }
  script_ovms(verbosity, writer, path.c_str(), sf, writer->IsSecure());
  }

void OvmsScripts::AllScripts(std::string path)
  {
  DIR *dir;
  struct dirent *dp;
  FILE *sf;
  std::set<std::string> files;

  // read dir, sort scripts by name:
  if ((dir = opendir (path.c_str())) != NULL)
    {
    while ((dp = readdir (dir)) != NULL)
      {
      std::string fpath = path;
      fpath.append("/");
      fpath.append(dp->d_name);
      files.insert(fpath);
      }
    closedir(dir);
    }

  // execute scripts:
  for (auto it = files.begin(); it != files.end(); it++)
    {
    std::string fpath = *it;
    sf = fopen(fpath.c_str(), "r");
    if (sf)
      {
      ESP_LOGI(TAG, "Running script %s", fpath.c_str());
      script_ovms(COMMAND_RESULT_MINIMAL, NULL, fpath.c_str(), sf, true);
      // script_ovms() closes sf
      }
    }
  }

void OvmsScripts::EventScript(std::string event, void* data)
  {
  std::string path;

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  MyDuktape.EventScript(event, data);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

#ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS
  // run event scripts on external storage:
  path=std::string("/sd/events/");
  path.append(event);
  AllScripts(path);
#endif // #ifdef CONFIG_OVMS_DEV_SDCARDSCRIPTS

  // run event scripts on internal storage:
  path=std::string("/store/events/");
  path.append(event);
  AllScripts(path);
  }

OvmsScripts::OvmsScripts()
  {
  ESP_LOGI(TAG, "Initialising SCRIPTS (1600)");

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  ESP_LOGI(TAG, "No javascript engines enabled (command scripting only)");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE

  OvmsCommand* cmd_script = MyCommandApp.RegisterCommand("script","SCRIPT framework");
  cmd_script->RegisterCommand("run","Run a script",script_run,"<path>",1,1);
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  cmd_script->RegisterCommand("reload","Reload javascript framework",script_reload);
  cmd_script->RegisterCommand("eval","Eval some javascript code",script_eval,"<code>",1,1);
  cmd_script->RegisterCommand("compact","Compact javascript heap",script_compact);
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  MyCommandApp.RegisterCommand(".","Run a script",script_run,"<path>",1,1);
  }

OvmsScripts::~OvmsScripts()
  {
  }
