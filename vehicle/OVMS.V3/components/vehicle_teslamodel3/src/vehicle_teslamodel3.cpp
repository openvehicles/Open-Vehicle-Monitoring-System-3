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
static const char *TAG = "v-teslamodel3";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslamodel3.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

OvmsVehicleTeslaModel3::OvmsVehicleTeslaModel3()
  {
  ESP_LOGI(TAG, "Tesla Model 3 vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  }

OvmsVehicleTeslaModel3::~OvmsVehicleTeslaModel3()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Model 3 vehicle module");
  }

void OvmsVehicleTeslaModel3::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleTeslaModel3::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleTeslaModel3::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleTeslaModel3::Notify12vCritical()
  { // Not supported on Model 3
  }

void OvmsVehicleTeslaModel3::Notify12vRecovered()
  { // Not supported on Model 3
  }

void OvmsVehicleTeslaModel3::NotifyBmsAlerts()
  { // Not supported on Model 3
  }

class OvmsVehicleTeslaModel3Init
  {
  public: OvmsVehicleTeslaModel3Init();
} MyOvmsVehicleTeslaModel3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaModel3Init::OvmsVehicleTeslaModel3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Model 3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaModel3>("T3","Tesla Model 3");
  }
