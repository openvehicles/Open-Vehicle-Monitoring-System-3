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

#include <string.h>
#include "obd2ecu.h"
#include "command.h"
#include "peripherals.h"

static void OBD2ECU_task(void *pvParameters)
  {
  //Uncomment this if you need it later
  //obd2ecu *me = (obd2ecu*)pvParameters;

  while(1)
    {
    vTaskDelay(1000 / portTICK_PERIOD_MS);
    }
  }

obd2ecu::obd2ecu(std::string name, canbus* can)
  : pcp(name)
  {
  m_can = can;
  xTaskCreatePinnedToCore(OBD2ECU_task, "OBDII ECU Task", 4096, (void*)this, 5, &m_task, 1);
  }

obd2ecu::~obd2ecu()
  {
  vTaskDelete(m_task);
  }

void obd2ecu::SetPowerMode(PowerMode powermode)
  {
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

void obd2ecu_create(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (! MyPeripherals->m_obd2ecu)
    {
    std::string bus = cmd->GetName();
    MyPeripherals->m_obd2ecu = new obd2ecu("OBD2ECU", (canbus*)MyPcpApp.FindDeviceByName(bus));
    writer->puts("OBDII ECU has been created");
    }
  }

void obd2ecu_destroy(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_obd2ecu)
    {
    delete MyPeripherals->m_obd2ecu;
    MyPeripherals->m_obd2ecu = NULL;
    writer->puts("OBDII ECU has been destroyed");
    }
  }

class obd2ecuInit
    {
    public: obd2ecuInit();
  } obd2ecuInit  __attribute__ ((init_priority (7000)));

obd2ecuInit::obd2ecuInit()
  {
  puts("Framework: Initialising OBD2ECU (7000)");

  OvmsCommand* cmd_obdii = MyCommandApp.RegisterCommand("obdii","OBDII framework",NULL, "", 1);
  OvmsCommand* cmd_ecu = cmd_obdii->RegisterCommand("ecu","OBDII ECU framework",NULL, "", 1);
  OvmsCommand* cmd_create = cmd_ecu->RegisterCommand("create","Create an OBDII ECU",NULL, "", 1);
  cmd_create->RegisterCommand("can1","Create an OBDII ECU on can1",obd2ecu_create, "", 0, 0);
  cmd_create->RegisterCommand("can2","Create an OBDII ECU on can2",obd2ecu_create, "", 0, 0);
  cmd_create->RegisterCommand("can3","Create an OBDII ECU on can3",obd2ecu_create, "", 0, 0);
  cmd_ecu->RegisterCommand("destroy","Destroy the OBDII ECU",obd2ecu_destroy, "", 0, 0);
  }
