/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th May 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019 Michael Stegen / Stegen Electronics
;    (C) 2019 Mark Webb-Johnson
;    (C) 2026 Craig Leres
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
#include "ovms_peripherals.h"
static const char *TAG = "v-cadillacct5";

#include <stdio.h>
#include "vehicle_cadillac_ct5.h"

OvmsVehicleCadillaccCT5* MyCadillaccCT5 = NULL;

OvmsVehicleCadillaccCT5::OvmsVehicleCadillaccCT5()
  {
  ESP_LOGI(TAG, "Cadillac CT5 vehicle module");
  memset(m_vin, 0, sizeof(m_vin));
  RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);

  /* Set acceptance filter to allow 0x063 (v.on) and 0x5E4 (v.b.soc) */
  esp32can_filter_config_t can1_hwflt = {
    .code = {.u32 = (0x063U << (5 + 16)) | 0x5E4U << 5},
    .mask = {.u32 = (0x7FFU << (5 + 16)) | 0x7FFU << 5},
    .single_filter = true,
  };
  MyPeripherals->m_esp32can->SetAcceptanceFilter(can1_hwflt);

  MyCadillaccCT5 = this;
  }

OvmsVehicleCadillaccCT5::~OvmsVehicleCadillaccCT5()
  {
  ESP_LOGI(TAG, "Shutdown Cadillac CT5 vehicle module");
  MyCadillaccCT5 = NULL;
  }

void OvmsVehicleCadillaccCT5::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  int i, len;
  uint8_t *d;
  bool isRunning;

  d = p_frame->data.u8;
  len = p_frame->FIR.B.DLC;
  if (len > sizeof(p_frame->data))
    len = sizeof(p_frame->data);

  switch (p_frame->MsgID)
    {
    case 0x063:
      /* Unknown pid tells us when the engine is running */
      if (len == 6)
        {
        isRunning = (d[2] != 0 || d[3] != 0);
        if (StandardMetrics.ms_v_env_on->AsBool() != isRunning)
          {
          ESP_LOGI(TAG, "running: \"%s\"", isRunning ? "yes" : "no");
          StdMetrics.ms_v_env_on->SetValue(isRunning);
          StdMetrics.ms_v_env_charging12v->SetValue(isRunning);
          }
        }
      break;

    case 0x49B:
      /* Last 8 of vin */
      if (m_vin[1 + 8] == '\0')
        {
        i = len;
        if (i > 8)
          i = 8;
        memcpy(m_vin + 1 + 8, d, i);
        /* Publish once we have the whole VIN */
        if (m_vin[0] != '\0')
          {
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          ESP_LOGI(TAG, "VIN: %s (pid %04x)", m_vin, p_frame->MsgID);
          }
        }
      break;

    case 0x5E4:
      /* Fuel level */
      StandardMetrics.ms_v_bat_soc->SetValue((d[0] * 100) >> 8);
      break;

    case 0x75F:
      /* First 8 of vin */
      if (m_vin[0] == '\0')
        {
        i = len;
        if (i > 8)
          i = 8;
        /* The World Manufacturer number: United States */
        m_vin[0] = '1';
        memcpy(m_vin + 1, d, i);
        /* Publish once we have the whole VIN */
        if (m_vin[1 + 8] != '\0')
          {
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          ESP_LOGI(TAG, "VIN: %s (pid %04x)", m_vin, p_frame->MsgID);
	  }
        }
      break;

    default:
      break;
    }
  }

class OvmsVehicleCadillaccCT5Init
  {
  public: OvmsVehicleCadillaccCT5Init();
} MyOvmsVehicleCadillaccCT5Init  __attribute__ ((init_priority (9000)));

OvmsVehicleCadillaccCT5Init::OvmsVehicleCadillaccCT5Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Cadillac CT5 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleCadillaccCT5>("CT5",
    "Cadillac CT5");
  }
