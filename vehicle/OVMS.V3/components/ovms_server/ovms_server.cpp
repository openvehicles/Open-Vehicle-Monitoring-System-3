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
static const char *TAG = "ovms-server";

#include <string.h>
#include "ovms_server.h"
#include "ovms_command.h"

static void OvmsServer_task(void *pvParameters)
  {
  OvmsServer *me = (OvmsServer*)pvParameters;

  ESP_LOGI(TAG, "Launching OVMS Server V2 connection task (%s)",me->GetName().c_str());
  me->ServerTask();
  }

OvmsServer::OvmsServer(std::string name)
  : pcp(name)
  {
  xTaskCreatePinnedToCore(OvmsServer_task, "OVMS Server Task", 4096, (void*)this, 5, &m_task, 1);
  }

OvmsServer::~OvmsServer()
  {
  vTaskDelete(m_task);
  }

void OvmsServer::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
      break;
    case Sleep:
      break;
    case DeepSleep:
      break;
    case Off:
      break;
    default:
      break;
    }
  }

class OvmsServerInit
    {
    public: OvmsServerInit();
  } OvmsServerInit  __attribute__ ((init_priority (6000)));

OvmsServerInit::OvmsServerInit()
  {
  ESP_LOGI(TAG, "Initialising OVMS Server (6000)");

  OvmsCommand* cmd_server = MyCommandApp.RegisterCommand("server","OVMS Server Connection framework",NULL, "", 1);
  cmd_server->RegisterCommand("start","Start an Ovms Server Connection",NULL, "", 1);
  cmd_server->RegisterCommand("stop","Stop an Ovms Server Connection",NULL, "", 1);
  cmd_server->RegisterCommand("status","Show Ovms Server connection status",NULL, "", 1);
  }
