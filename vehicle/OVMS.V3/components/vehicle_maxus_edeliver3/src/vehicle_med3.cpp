/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th March 2021
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
static const char *TAG = "v-maxed3";

#include "vehicle_med3.h"
#include "metrics_standard.h"
#include "ovms_metrics.h"
#include <string>



static const OvmsVehicle::poll_pid_t obdii_polls[] =
    {
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe002u, {  0, 120, 120, 120  }, 0, ISOTP_STD }, //soc?? +1 may need scaling
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe003u, {  0, 120, 120, 120  }, 0, ISOTP_STD }, //SOH
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe005u, {  0, 120, 120, 120  }, 0, ISOTP_STD }, //temp
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe006u, {  0, 120, 120, 120  }, 0, ISOTP_STD }, //temp
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe019u, {  0, 120, 120, 120  }, 0, ISOTP_STD }, //Pack Voltage
        { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
    };


OvmsVehicleMaxed3::OvmsVehicleMaxed3()
    {
        ESP_LOGI(TAG, "Start Maxus eDeliver3 vehicle module");
      
        memset(m_vin, 0, sizeof(m_vin));
      
        RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
        PollSetPidList(m_can1,obdii_polls);
        PollSetState(0);
    }

OvmsVehicleMaxed3::~OvmsVehicleMaxed3()
    {
        ESP_LOGI(TAG, "Stop eDeliver3 vehicle module");
    }


//testing polls

void OvmsVehicleMaxed3::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
    {
        string& rxbuf = med_obd_rxbuf;
        int value1 = (int)data[0];
        int value2 = ((int)data[0] << 8) + (int)data[1];

        switch (pid)
            {
                case 0xf190:  // VIN
                    strncat(m_vin,(char*)data,length);
                    if (mlremain==0)
                    {
                        StandardMetrics.ms_v_vin->SetValue(m_vin);
                        m_vin[0] = 0;
                    }
                    break;
                case 0xe002: //soc?? +1 may need scaling
                    StandardMetrics.ms_v_bat_soc->SetValue(value1 + 1);
                    break;
                case 0xe003: //soh??
                    StandardMetrics.ms_v_bat_soh->SetValue(value1 / 100);
                    break;
                case 0xe005:  // temperature??
                    StandardMetrics.ms_v_mot_temp->SetValue(value1 / 10.0f);
                    break;
                case 0xe006:  // temperature??
                    StandardMetrics.ms_v_env_temp->SetValue(value1 / 10.0f);
                    break;
                case 0xe019:  // Pack Voltage
                    StandardMetrics.ms_v_bat_voltage->SetValue(value2 / 10.0f);  ///if(soc>100)soc=100;???????
                    break;
            }
    

        // init / fill rx buffer:
        if (m_poll_ml_frame == 0)
            {
                rxbuf.clear();
                rxbuf.reserve(length + mlremain);
            }
                rxbuf.append((char*)data, length);
        if (mlremain)
            return;

    }


// end testing polls



void OvmsVehicleMaxed3::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
      
//testing
      
      uint8_t *d = p_frame->data.u8;

      switch (p_frame->MsgID)
        {
        case 0x6f2: // broadcast
          {

          uint8_t meds_status = (d[0] & 0x07); // contains status bits
          
              switch (meds_status)
              break;
          
              
          
          
          //Set ideal, est and amps when CANdata received
          float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
              if(soc>100)soc=100;
                StandardMetrics.ms_v_bat_range_ideal->SetValue(241 * soc / 100);
                StandardMetrics.ms_v_bat_range_est->SetValue(241 * soc / 108);
                StandardMetrics.ms_v_bat_current->SetValue((StandardMetrics.ms_v_bat_power->AsFloat() / (StandardMetrics.ms_v_bat_voltage->AsFloat() )) * 1000); // work out current untill pid found
      
              break;
        
          
            }
        default:
          break;
                
                
            case 0x604:  // Setup
                {
                float power = d[5];
                    StandardMetrics.ms_v_bat_power->SetValue((power * 42) / 1000);// actual power in watts on AC converted to kw
                    if (StandardMetrics.ms_v_bat_power->AsFloat() >=  1.000f)
                        {
                            StandardMetrics.ms_v_charge_inprogress->SetValue(true);
                            PollSetState(1);
                        }
                    else
                        {
                            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
                            PollSetState(0);
                        }
                    break;
                }
/*            case 0x375:  // Setup
                {
                float van_on = d[5];
                    if (van_on == 18)
                    {
                        StandardMetrics.ms_v_env_on->SetValue(true);
                        PollSetState(2);
                    }
                    break;
                }
*/
            case 0x540:  // odometer in KM
                {
                    StandardMetrics.ms_v_pos_odometer->SetValue(d[0] << 16 | d[1] << 8 | d[2]);// odometer
                }
                break;
            }
    }



void OvmsVehicleMaxed3::HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain)
    {
   
        {
            // Vin
            StandardMetrics.ms_v_vin->SetValue(&m_vin[1]);
            memset(m_vin, 0, sizeof(m_vin));
        }
    }


void OvmsVehicleMaxed3::Ticker1(uint32_t ticker)
  {
  }

class OvmsVehicleMaxed3Init
  {
  public: OvmsVehicleMaxed3Init();
  } MyOvmsVehicleMaxed3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMaxed3Init::OvmsVehicleMaxed3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Maxus eDeliver3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxed3>("MED3","Maxus eDeliver3");
  }
