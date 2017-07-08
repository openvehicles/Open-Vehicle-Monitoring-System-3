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

#include <stdio.h>
#include "pcp.h"

pcpapp MyPcpApp __attribute__ ((init_priority (1100)));

void power_cmd(int verbosity, OvmsWriter* writer, int argc, const char* const* argv)
  {
  if (argc != 2)
    {
    writer->puts("Syntax: power <device> <on|sleep|deepsleep|off>");
    writer->printf("  Available devices: ");
    MyPcpApp.ShowDevices(writer);
    return;
    }

  pcp* device = MyPcpApp.FindDeviceByName(argv[0]);
  PowerMode pm = MyPcpApp.FindPowerModeByName(argv[1]);
  if ((device == NULL)||(pm == PowerMode::Undefined))
    {
    writer->puts("Syntax: power <device> <on|sleep|deepsleep|off>");
    writer->printf("  Available devices: ");
    MyPcpApp.ShowDevices(writer);
    return;
    }

  device->SetPowerMode(pm);
  writer->printf("Power mode of %s is now %s\n",argv[0],argv[1]);
  }

pcpapp::pcpapp()
  {
  puts("Initialised Power Controlled Peripherals App");
  m_mappm["on"] = On;
  m_mappm["sleep"] = Sleep;
  m_mappm["deepsleep"] = DeepSleep;
  m_mappm["off"] = Off;
  MyCommandApp.RegisterCommand("power","Power control",power_cmd,
    "power <device> <on|sleep|deepsleep|off>",2,2);
  }

pcpapp::~pcpapp()
  {
  }

void pcpapp::Register(std::string name, pcp* device)
  {
  m_map[name] = device;
  }

void pcpapp::Deregister(std::string name)
  {
  m_map.erase(name);
  }

pcp* pcpapp::FindDeviceByName(std::string name)
  {
  auto iter = m_map.find(name);
  if (iter != m_map.end())
    {
    return iter->second;
    }
  return NULL;
  }

PowerMode pcpapp::FindPowerModeByName(std::string name)
  {
  auto iter = m_mappm.find(name);
  if (iter != m_mappm.end())
    {
    return iter->second;
    }
  return PowerMode::Undefined;
  }

void pcpapp::ShowDevices(OvmsWriter* writer)
  {
  for (auto it=m_map.begin(); it!=m_map.end(); ++it)
    {
    writer->printf("%s ",it->first.c_str());
    }
  writer->puts("");
  }

pcp::pcp(std::string name)
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
  m_powermode = powermode;
  }

std::string pcp::GetName()
  {
  return m_name;
  }
