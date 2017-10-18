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
static const char *TAG = "vehicle";

#include <stdio.h>
#include <ovms_command.h>
#include <ovms_metrics.h>
#include <metrics_standard.h>
#include "vehicle.h"

OvmsVehicleFactory MyVehicleFactory __attribute__ ((init_priority (2000)));

void vehicle_module(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (argc == 0)
    {
    MyVehicleFactory.ClearVehicle();
    }
  else
    {
    MyVehicleFactory.SetVehicle(argv[0]);
    }
  }

OvmsVehicleFactory::OvmsVehicleFactory()
  {
  ESP_LOGI(TAG, "Initialising VEHICLE Factory (2000)");

  m_currentvehicle = NULL;

  OvmsCommand* cmd_vehicle = MyCommandApp.RegisterCommand("vehicle","Vehicle framework",NULL,"",1,1);
  cmd_vehicle->RegisterCommand("module","Set (or clear) vehicle module",vehicle_module,"<type>",0,1);
  }

OvmsVehicleFactory::~OvmsVehicleFactory()
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    }
  }

OvmsVehicle* OvmsVehicleFactory::NewVehicle(std::string VehicleType)
  {
  OvmsVehicleFactory::map_type::iterator iter = m_map.find(VehicleType);
  if (iter != m_map.end())
    {
    return iter->second();
    }
  return NULL;
  }

void OvmsVehicleFactory::ClearVehicle()
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    StandardMetrics.ms_v_type->SetValue("");
    }
  }

void OvmsVehicleFactory::SetVehicle(std::string type)
  {
  if (m_currentvehicle)
    {
    delete m_currentvehicle;
    m_currentvehicle = NULL;
    }
  m_currentvehicle = NewVehicle(type);
  StandardMetrics.ms_v_type->SetValue(type.c_str());
  }

OvmsVehicle::OvmsVehicle()
  {
  }

OvmsVehicle::~OvmsVehicle()
  {
  }

const std::string OvmsVehicle::VehicleName()
  {
  return std::string("unknown");
  }
