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
#include "ovms_script.h"
#include "ovms_command.h"
#include "ovms_command.h"
#include "ovms_events.h"

OvmsScripts MyScripts __attribute__ ((init_priority (1600)));

void script_ovms(int verbosity, OvmsWriter* writer, FILE* sf)
  {
  char cmdline[_COMMAND_LINE_LEN];

  while(fgets(cmdline, _COMMAND_LINE_LEN, sf) != NULL )
    {
    char* tkn_arr [_COMMAND_TOKEN_NMB];
    while ((strlen(cmdline)>0)&&
           ((cmdline[strlen(cmdline)-1]=='\n')||
            (cmdline[strlen(cmdline)-1]=='\r')))
      cmdline[strlen(cmdline)-1] = 0;
    if (cmdline[0] != 0)
      {
      // Tokenise the command line...
      int nt = 0;
      int ind = 0;
      int limit = strlen(cmdline);
      for (int k=0;k<limit;k++) { if (cmdline[k]==' ') cmdline[k]=0; }
      while (ind < limit)
        {
        // go past the first whitespace
        while ((cmdline[ind] == 0) && (ind < limit))
          {
          ind++;
          }
        if (ind < limit)
          {
          tkn_arr[nt++] = cmdline + ind;
          if (nt >= _COMMAND_TOKEN_NMB)
            {
            writer->puts("Error: command too long");
            return;
            }
          // go to the first NOT whitespace
          while ((cmdline [ind] != 0) && (ind < limit))
            { ind++; }
          }
        }
      // OK, at this point the command line is tokenised and we are ready to go...
      MyCommandApp.Execute(verbosity, writer, nt, tkn_arr);
      }
    }
  }

void script_run(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  FILE *sf;

  if (argv[0][0] == '/')
    {
    // A direct path specification
    sf = fopen(argv[0], "r");
    }
  else
    {
    std::string path("/sdcard/scripts/");
    path.append(argv[0]);
    sf = fopen(path.c_str(), "r");
    if (sf == NULL)
      {
      path = std::string("/store/scripts");
      path.append(argv[0]);
      sf = fopen(path.c_str(), "r");
      }
    }
  if (sf == NULL)
    {
    writer->puts("Error: Script not found");
    return;
    }
  script_ovms(verbosity, writer, sf);
  fclose(sf);
  }

OvmsScripts::OvmsScripts()
  {
  ESP_LOGI(TAG, "Initialising SCRIPTS (1600)");
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
  ESP_LOGI(TAG, "No javascript engines enabled (command scripting only)");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_NONE
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  ESP_LOGI(TAG, "Using DUKTAPE javascript engine");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_MJS
  ESP_LOGI(TAG, "Using MJS javascript engine");
#endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_MJS

  MyCommandApp.RegisterCommand("script","Run a script",script_run,"<path>",1,1);
  MyCommandApp.RegisterCommand(".","Run a script",script_run,"<path>",1,1);
  }

OvmsScripts::~OvmsScripts()
  {
  }
