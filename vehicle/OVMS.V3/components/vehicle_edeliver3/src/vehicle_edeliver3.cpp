/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Shane Hunns
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
static const char *TAG = "v-edeliver3";

#include <stdio.h>
#include "vehicle_edeliver3.h"
#include "metrics_standard.h"
#include "med3_obd_pids.h"

namespace  {

const OvmsVehicle::poll_pid_t obdii_polls[] =
{
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuVinPid, { 0, 999, 999, 0  }, 0, ISOTP_STD },
    { vcuId, vcuId | rxFlag, VEHICLE_POLL_TYPE_OBDIIEXTENDED, batterySoHPid, {  0, 120, 120, 0  }, 0, ISOTP_STD },
};
};

OvmsVehicleEdeliver3::OvmsVehicleEdeliver3()
  {
      ESP_LOGI(TAG, "Start eDeliver3 vehicle module");
      
      RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  }

OvmsVehicleEdeliver3::~OvmsVehicleEdeliver3()
  {
      ESP_LOGI(TAG, "Stop eDeliver3 vehicle module");
      
      memset(m_vin, 0, sizeof(m_vin));
  }

void OvmsVehicleEdeliver3::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
      
//testing
      
      uint8_t *d = p_frame->data.u8;

      switch (p_frame->MsgID)
        {
        case 0x6f2: // broadcast
          {

          uint8_t meds_status = (d[0] & 0x07); // contains status bits
          
          switch (meds_status)
          {
          case 0: // Idle
            {
             StandardMetrics.ms_v_env_on->SetValue(false);
             StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            break;
            }
          case 1: // Pre-charging
            {
             //StandardMetrics.ms_v_env_on->SetValue(false);
             //StandardMetrics.ms_v_charge_inprogress->SetValue(true);
            break;
            }
          case 2:  // Running
            {
             StandardMetrics.ms_v_env_on->SetValue(true);
             StandardMetrics.ms_v_charge_inprogress->SetValue(false);
             StandardMetrics.ms_v_charge_voltage->SetValue(0);
             StandardMetrics.ms_v_charge_current->SetValue(0);

            break;
            }
          case 3:  // Charging
            {
             StandardMetrics.ms_v_env_on->SetValue(false);
             StandardMetrics.ms_v_charge_inprogress->SetValue(true);
             StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsInt());
             StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsInt());


            break;

            }
          case 4:  // Setup
            {
             StandardMetrics.ms_v_env_on-> SetValue(false);
             StandardMetrics.ms_v_charge_inprogress->SetValue(false);
          
            break;
            }
          default:
            break;
         }
              
          
          
          //Set SOC when CANdata received
          float soc = d[1];
          StandardMetrics.ms_v_bat_soc->SetValue(soc);
          StandardMetrics.ms_v_bat_range_ideal->SetValue(241 * soc / 100);
          StandardMetrics.ms_v_bat_range_est->SetValue(241 * soc / 100);
          StandardMetrics.ms_v_bat_voltage->SetValue(d[6] * 2);
          StandardMetrics.ms_v_bat_current->SetValue(d[7] * 1);
      
          break;
          }
        default:
          break;
      }

  }


void OvmsVehicleEdeliver3::HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain)
{
   
    {
        // Seems to have a bit of rubbish at the start of the VIN
        StandardMetrics.ms_v_vin->SetValue(&m_vin[1]);
        memset(m_vin, 0, sizeof(m_vin));
    }
}


void OvmsVehicleEdeliver3::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleEdeliver3Init
  {
  public: OvmsVehicleEdeliver3Init();
} MyOvmsVehicleEdeliver3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleEdeliver3Init::OvmsVehicleEdeliver3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: eDeliver3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleEdeliver3>("MED3","eDeliver3");
  }

