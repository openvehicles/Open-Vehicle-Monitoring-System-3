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
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include "vehicle_voltampera.h"

OvmsVehicleVoltAmpera::OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Volt/Ampera vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  m_type[0] = 'V';
  m_type[1] = 'A';
  m_type[2] = 0;
  m_type[3] = 0;

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  }

OvmsVehicleVoltAmpera::~OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Shutdown Volt/Ampera vehicle module");
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  int k;

  switch (p_frame->MsgID)
    {
    case 0x4e1:
      {
      for (k=0;k<8;k++)
        m_vin[k+9] = d[k];
      break;
      }
    case 0x514:
      {
      for (k=0;k<8;k++)
        m_vin[k+1] = d[k];
      m_vin[0] = '1';
      m_vin[17] = 0;
      if (m_vin[9] != 0)
        {
        StandardMetrics.ms_v_vin->SetValue(m_vin);
        StandardMetrics.ms_v_type->SetValue(m_type);
        }
      break;
      }
    case 0x135:
      {
      if (d[0] == 0)
        {
        // Car is in PARK
        StandardMetrics.ms_v_env_gear->SetValue(0);
        StandardMetrics.ms_v_env_on->SetValue(false);
        StandardMetrics.ms_v_env_awake->SetValue(false);
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
        }
      else
        {
        // Car is not in PARK
        StandardMetrics.ms_v_env_gear->SetValue(0);
        StandardMetrics.ms_v_env_on->SetValue(true);
        StandardMetrics.ms_v_env_awake->SetValue(true);
        StandardMetrics.ms_v_env_handbrake->SetValue(false);
        }
      break;
      }
    default:
      break;
    }
  }

void OvmsVehicleVoltAmpera::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  }

void OvmsVehicleVoltAmpera::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleVoltAmperaInit
  {
  public: OvmsVehicleVoltAmperaInit();
} MyOvmsVehicleVoltAmperaInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVoltAmperaInit::OvmsVehicleVoltAmperaInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Volt/Ampera (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVoltAmpera>("VA","Volt/Ampera");
  }
