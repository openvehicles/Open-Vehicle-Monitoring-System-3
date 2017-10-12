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
static const char *TAG = "script";

#include <string>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include "ovms_script.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "console_async.h"
#include "log_buffers.h"
#include "buffered_shell.h"

OvmsScripts MyScripts __attribute__ ((init_priority (1600)));

static void script_ovms(bool print, int verbosity, OvmsWriter* writer, FILE* sf)
  {
  BufferedShell* bs = new BufferedShell(print, verbosity);
  char* cmdline = new char[_COMMAND_LINE_LEN];
  while(fgets(cmdline, _COMMAND_LINE_LEN, sf) != NULL )
    {
    bs->ProcessChars(cmdline, strlen(cmdline));
    }
  fclose(sf);
  bs->Output(writer);
  delete bs;
  delete [] cmdline;
  }

static void script_run(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  FILE *sf;

  if (argv[0][0] == '/')
    {
    // A direct path specification
    sf = fopen(argv[0], "r");
    }
  else
    {
    std::string path("/sd/scripts/");
    path.append(argv[0]);
    sf = fopen(path.c_str(), "r");
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
  script_ovms(true, verbosity, writer, sf);
  }

void OvmsScripts::AllScripts(std::string path)
  {
  DIR *dir;
  struct dirent *dp;
  FILE *sf;

  if ((dir = opendir (path.c_str())) != NULL)
    {
    while ((dp = readdir (dir)) != NULL)
      {
      std::string fpath = path;
      fpath.append("/");
      fpath.append(dp->d_name);
      sf = fopen(fpath.c_str(), "r");
      if (sf)
        {
        ESP_LOGI(TAG, "Running script %s", fpath.c_str());
        script_ovms(false, COMMAND_RESULT_MINIMAL, MyUsbConsole, sf);
        fclose(sf);
        }
      }
    closedir(dir);
    }
  }

void OvmsScripts::EventScript(std::string event, void* data)
  {
  std::string path("/sd/events/");
  path.append(event);
  AllScripts(path);

  path=std::string("/store/events/");
  path.append(event);
  AllScripts(path);
  }

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

duk_context* OvmsScripts::Duktape()
  {
  return m_dukctx;
  }

#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

OvmsScripts::OvmsScripts()
  {
  ESP_LOGI(TAG, "Initialising SCRIPTS (1600)");

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  ESP_LOGI(TAG, "No javascript engines enabled (command scripting only)");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  ESP_LOGI(TAG, "Using DUKTAPE javascript engine");
  m_dukctx = duk_create_heap_default();
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE

  MyCommandApp.RegisterCommand("script","Run a script",script_run,"<path>",1,1);
  MyCommandApp.RegisterCommand(".","Run a script",script_run,"<path>",1,1);
  }

OvmsScripts::~OvmsScripts()
  {
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  duk_destroy_heap(m_dukctx);
  m_dukctx = NULL;
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }
