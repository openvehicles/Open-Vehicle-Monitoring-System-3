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
static const char *TAG = "v-cadillacct5";

#include <stdio.h>
#include "vehicle_cadillac_ct5.h"

OvmsVehicleCadillaccCT5* MyCadillaccCT5 = NULL;

#ifdef notdef
static const OvmsPoller::poll_pid_t obdii_polls[] =
  {
    // Engine coolant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x05, {  0, 30 }, 0, ISOTP_STD },
    // Speed
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0d, {  0, 10 }, 0, ISOTP_STD },
    // Engine air intake temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0f, {  0, 30 }, 0, ISOTP_STD },
    // Ambient temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x46, {  0, 30 }, 0, ISOTP_STD },
    // Engine oil temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x5c, {  0, 30 }, 0, ISOTP_STD },
    POLL_LIST_END
  };
#endif

OvmsVehicleCadillaccCT5::OvmsVehicleCadillaccCT5()
  {
  ESP_LOGI(TAG, "Cadillac CT5 vehicle module");
  memset(m_vin, 0, sizeof(m_vin));
#ifdef notdef
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, obdii_polls);
  PollSetState(0);
#else
  RegisterCanBus(1, CAN_MODE_LISTEN, CAN_SPEED_500KBPS);
#endif
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
#ifdef notdef
          PollSetState(isRunning ? 1 : 0);
#endif
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

#ifdef notdef
void
OvmsVehicleCadillaccCT5::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
  // int value1 = (int)data[0];

  switch (job.pid)
    {
#ifdef notdef
    case 0x05:  // Engine coolant temperature
      StandardMetrics.ms_v_bat_temp->SetValue(value1 - 0x28);
      break;
#endif

#ifdef notdef
    case 0x0f:  // Engine intake air temperature
      StandardMetrics.ms_v_inv_temp->SetValue(value1 - 0x28);
      break;
#endif

#ifdef notdef
    case 0x5c:  // Engine oil temperature
      StandardMetrics.ms_v_mot_temp->SetValue(value1 - 0x28);
      break;
#endif

#ifdef notdef
    case 0x46:  // Ambient temperature
      StandardMetrics.ms_v_env_temp->SetValue(value1 - 0x28);
      break;
#endif

#ifdef notdef
    case 0x0d:  // Speed
      StandardMetrics.ms_v_pos_speed->SetValue(value1);
      break;
#endif

    default:
      ESP_LOGI(TAG, "IncomingPollReply: pid %04x", job.pid);
      break;
    }
  }
#endif

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
