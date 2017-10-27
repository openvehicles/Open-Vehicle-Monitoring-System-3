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
static const char *TAG = "v-teslaroadster";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslaroadster.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

OvmsVehicleTeslaRoadster::OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Tesla Roadster v1.x, v2.x and v3.0 vehicle module");

  memset(m_vin,0,sizeof(m_vin));

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_1000KBPS);
  }

OvmsVehicleTeslaRoadster::~OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Roadster vehicle module");
  }

const std::string OvmsVehicleTeslaRoadster::VehicleName()
  {
  return std::string("Tesla Roadster");
  }

void OvmsVehicleTeslaRoadster::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x100: // VMS->VDS
      {
      switch (d[0])
        {
        case 0x80: // Range / State of Charge
          {
          StandardMetrics.ms_v_bat_soc->SetValue(d[1]);
          int idealrange = (int)d[2] + ((int)d[3]<<8);
          if (idealrange>6000) idealrange=0; // Sanity check (limit rng->std)
          StandardMetrics.ms_v_bat_range_ideal->SetValue(idealrange, Miles);
          int estrange = (int)d[6] + ((int)d[7]<<8);
          if (estrange>6000) estrange=0; // Sanity check (limit rng->std)
          StandardMetrics.ms_v_bat_range_est->SetValue(estrange, Miles);
          break;
          }
        case 0x82: // Ambient Temperature
          StandardMetrics.ms_v_env_temp->SetValue(d[1]);
          break;
        case 0xA3: // PEM, MOTOR, BATTERY temperatures
          StandardMetrics.ms_v_inv_temp->SetValue(d[1]);
          StandardMetrics.ms_v_charge_temp->SetValue(d[1]);
          StandardMetrics.ms_v_mot_temp->SetValue(d[2]);
          StandardMetrics.ms_v_bat_temp->SetValue(d[6]);
          break;
        case 0xA4: // 7 bytes start of VIN bytes i.e. "SFZRE2B"
          memcpy(m_vin,d+1,7);
          break;
        case 0xA5: // 7 bytes mid of VIN bytes i.e. "39A3000"
          memcpy(m_vin+7,d+1,7);
          m_type[0] = 'T';
          m_type[1] = 'R';
          if ((d[3]=='A')||(d[3]=='B')) m_type[2] = '2'; else m_type[2] = '1';
          if (d[1] == '3') m_type[3] = 'S'; else m_type[3] = 'N';
          m_type[4] = 0;
          StandardMetrics.ms_v_type->SetValue(m_type);
          break;
        case 0xA6: // 3 bytes last of VIN bytes i.e. "359"
          if ((m_vin[0] != 0)&&(m_vin[7] != 0))
            {
            memcpy(m_vin+14,p_frame->data.u8+1,3);
            m_vin[17] = 0;
            StandardMetrics.ms_v_vin->SetValue(m_vin);
            }
          break;
        default:
          break;
        }
      break;
      }
    case 0x344: // TPMS
      {
      if (d[1]>0) // Front-left
        {
        StandardMetrics.ms_v_tpms_fl_p->SetValue((float)d[0] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_fl_t->SetValue((float)d[1] - 40);
        }
      if (d[3]>0) // Front-right
        {
        StandardMetrics.ms_v_tpms_fr_p->SetValue((float)d[2] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_fr_t->SetValue((float)d[3] - 40);
        }
      if (d[5]>0) // Rear-left
        {
        StandardMetrics.ms_v_tpms_rl_p->SetValue((float)d[4] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_rl_p->SetValue((float)d[5] - 40);
        }
      if (d[7]>0) // Rear-right
        {
        StandardMetrics.ms_v_tpms_rr_p->SetValue((float)d[6] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_rr_p->SetValue((float)d[7] - 40);
        }
      break;
      }
    case 0x400:
      {
      break;
      }
    case 0x402:
      {
      unsigned int odometer = (unsigned int)d[3]
                            + ((unsigned int)d[4]<<8)
                            + ((unsigned int)d[5]<<16);
      StandardMetrics.ms_v_pos_odometer->SetValue((float)odometer/10, Miles);
      unsigned int trip = (unsigned int)d[6]
                        + ((unsigned int)d[7]<<8);
      StandardMetrics.ms_v_pos_trip->SetValue((float)trip/10, Miles);
      break;
      }
    }
  }

class OvmsVehicleTeslaRoadsterInit
  {
  public: OvmsVehicleTeslaRoadsterInit();
} MyOvmsVehicleTeslaRoadsterInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaRoadsterInit::OvmsVehicleTeslaRoadsterInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Roadster (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaRoadster>("TR");
  }
