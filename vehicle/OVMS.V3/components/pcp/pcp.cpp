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
static const char *TAG = "pcp";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "ovms_events.h"

pcpapp MyPcpApp __attribute__ ((init_priority (4000)));
OvmsCommand* powercmd = NULL;

void power_cmd(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* devname = cmd->GetParent()->GetName();
  const char* pmname = cmd->GetName();

  pcp* device = MyPcpApp.FindDeviceByName(devname);
  PowerMode pm = MyPcpApp.FindPowerModeByName(pmname);
  if ((device == NULL)||(pm == PowerMode::Undefined))
    {
    writer->puts("Internal error: finding device or power mode name failed");
    return;
    }

  device->SetPowerMode(pm);
  writer->printf("Power mode of %s is now %s\n",devname,pmname);
  }

void power_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* devname = cmd->GetParent()->GetName();

  pcp* device = MyPcpApp.FindDeviceByName(devname);
  const char* pmname = MyPcpApp.FindPowerModeByType(device->GetPowerMode());

  writer->printf("Power for %s is %s\n",devname,pmname);
  }

pcpapp::pcpapp()
  {
  ESP_LOGI(TAG, "Initialising POWER (4000)");

  m_mappm["on"] = On;
  m_mappm["sleep"] = Sleep;
  m_mappm["deepsleep"] = DeepSleep;
  m_mappm["off"] = Off;
  m_mappm["devel"] = Devel;
  powercmd = MyCommandApp.RegisterCommand("power","Power control",NULL,"$C $G$");
  }

pcpapp::~pcpapp()
  {
  }

void pcpapp::Register(const char* name, pcp* device)
  {
  m_map[name] = device;
  OvmsCommand* devcmd = powercmd->RegisterCommand(name,"Power control");
  for (auto it=m_mappm.begin(); it!=m_mappm.end(); ++it)
    {
    devcmd->RegisterCommand(it->first,"Power control",power_cmd);
    }
  devcmd->RegisterCommand("status","Power control status",power_status);
  }

void pcpapp::Deregister(const char* name)
  {
  m_map.erase(name);
  }

pcp* pcpapp::FindDeviceByName(const char* name)
  {
  auto iter = m_map.find(name);
  if (iter != m_map.end())
    {
    return iter->second;
    }
  return NULL;
  }

PowerMode pcpapp::FindPowerModeByName(const char* name)
  {
  auto iter = m_mappm.find(name);
  if (iter != m_mappm.end())
    {
    return iter->second;
    }
  return PowerMode::Undefined;
  }

const char* pcpapp::FindPowerModeByType(PowerMode mode)
  {
  const char* pmname= NULL;
  for (auto it=m_mappm.begin(); it!=m_mappm.end(); ++it)
    {
    if (it->second == mode)
      pmname = it->first;
    }
  if (pmname == NULL) pmname = "undefined";
  return pmname;
  }

pcp::pcp(const char* name)
  {
  m_name = name;
  m_powermode = On;
  MyPcpApp.Register(name,this);
  }

pcp::~pcp()
  {
  MyPcpApp.Deregister(m_name);
  }

void pcp::SetPowerMode(PowerMode powermode)
  {
  if (m_powermode != powermode)
    {
    m_powermode = powermode;
    const char* pmname = MyPcpApp.FindPowerModeByType(m_powermode);
    char event[32] = "power.";
    strcat(event,m_name);
    strcat(event,".");
    strcat(event,pmname);
    MyEvents.SignalEvent(event,NULL);
    }
  }

PowerMode pcp::GetPowerMode()
  {
  return m_powermode;
  }

const char* pcp::GetName()
  {
  return m_name;
  }
