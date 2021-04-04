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
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_notify.h"


static const OvmsVehicle::poll_pid_t obdii_polls[] =
    {
        // VCU Polls
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe001u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //SOH
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe002u, {  360, 30, 30, 30  }, 0, ISOTP_STD }, //SOC Scaled below
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe005u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //temp
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe006u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //temp
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe019u, {  360, 30, 30, 30  }, 0, ISOTP_STD }, //Pack Voltage
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe022u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //12v amps?
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe036u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //charger volts at a guess?
        { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe039u, {  0, 60, 60, 60  }, 0, ISOTP_STD }, //charger amps?
        // BMS Polls
//        { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe015u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //cell max
//        { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe017u, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //cell min
//        { 0x748, 0x7c8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xe019u, {  0, 60, 60, 60  }, 0, ISOTP_STD }, //cell avg
        { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
    };


OvmsVehicleMaxed3::OvmsVehicleMaxed3()
    {
        ESP_LOGI(TAG, "Start Maxus eDeliver3 vehicle module");
      
        memset(m_vin, 0, sizeof(m_vin));
      
        RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
        PollSetPidList(m_can1,obdii_polls);
        PollSetState(0);
        
        med3_cum_energy_charge_wh = 0;
        
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
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
                case 0xe001: //soh
                    StandardMetrics.ms_v_bat_soh->SetValue(value1);
                    break;
                case 0xe002: //soc scaled from 2 - 99
                    StandardMetrics.ms_v_bat_soc->SetValue(value1);
                    break;
                case 0xe005:  // temperature??
                    StandardMetrics.ms_v_mot_temp->SetValue(value1 / 10.0f);
                    break;
                case 0xe006:  // temperature??
                    StandardMetrics.ms_v_env_temp->SetValue(value1 / 10.0f);
                    break;
                case 0xe019:  // Pack Voltage
                    StandardMetrics.ms_v_bat_voltage->SetValue(value2 / 10.0f);
                    break;
                case 0xe022:
                    StandardMetrics.ms_v_bat_12v_current->SetValue(value1 / 10.0f);
                    break;
                case 0xe036:
                    StandardMetrics.ms_v_charge_voltage->SetValue(value1); // possible but always 224 untill found only
                    break;
                case 0xe039:
                    StandardMetrics.ms_v_charge_current->SetValue(value1);
                    StandardMetrics.ms_v_charge_climit->SetValue(value1);
                    StandardMetrics.ms_v_charge_power->SetValue((StandardMetrics.ms_v_charge_current->AsFloat() * (StandardMetrics.ms_v_charge_voltage->AsFloat() )) / 1000); // work out current untill pid found * (value / 10.0f) / 1000.0f
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
        {}
    }

// end testing polls

void OvmsVehicleMaxed3::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
      
//setup
      StandardMetrics.ms_v_env_charging12v->SetValue(StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9);
      StandardMetrics.ms_v_bat_temp->SetValue(m_poll_state); // temp to view poll state
      
      if (StandardMetrics.ms_v_env_charging12v->AsBool())
          StandardMetrics.ms_v_env_awake->SetValue(true);
      else
          StandardMetrics.ms_v_env_awake->SetValue(false);
      
// count cumalitive energy
      if(StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
          med3_cum_energy_charge_wh += StandardMetrics.ms_v_charge_power->AsFloat()/3600;
          StandardMetrics.ms_v_charge_kwh->SetValue((med3_cum_energy_charge_wh/1000) * 1.609f);
      // When we are not charging set back to zero ready for next charge.
      }
      else
      {
          med3_cum_energy_charge_wh=0;
      }
// end count cumalitive energy
      
      uint8_t *d = p_frame->data.u8;

      switch (p_frame->MsgID)
        {
        case 0x6f2: // broadcast
          {

          uint8_t meds_status = (d[0] & 0x07); // contains status bits
          
              switch (meds_status)
                  // Pollstate 0 - POLLSTATE_OFF      - car is off
                  // Pollstate 1 - POLLSTATE_ON       - car is on
                  // Pollstate 2 - POLLSTATE_RUNNING  - car is driving
                  // Pollstate 3 - POLLSTATE_CHARGING - car is charging
              break;
          
              
          //Set ideal, est and amps when CANdata received
              float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
              float vbp = (StandardMetrics.ms_v_bat_power->AsFloat()  / (StandardMetrics.ms_v_bat_voltage->AsFloat() ) * 1000); // work out current untill pid found
                StandardMetrics.ms_v_bat_range_ideal->SetValue(241 * soc / 100);
                StandardMetrics.ms_v_bat_range_est->SetValue(241 * soc / 108);
                StandardMetrics.ms_v_bat_current->SetValue(vbp - (vbp * 2)); // convert to negative
              break;
        }
        default:
          break;
                
                
            case 0x604:  // Setup
                {
                float power = d[5];
                StandardMetrics.ms_v_bat_power->SetValue((power * 42.0f) / 1000.0f);// actual power in watts on AC converted to kw
                    if (StandardMetrics.ms_v_bat_power->AsFloat() >=  1.000f)
                    {
                        StandardMetrics.ms_v_door_chargeport->SetValue (true);
                        StandardMetrics.ms_v_charge_pilot->SetValue(true);
                        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
                        StandardMetrics.ms_v_charge_type->SetValue("type2");
                        StandardMetrics.ms_v_charge_mode->SetValue("standard");
                        StandardMetrics.ms_v_charge_state->SetValue("charging");
                        PollSetState(3);
                    }
                else
                    {
                        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
                        StandardMetrics.ms_v_door_chargeport->SetValue (false);
                        StandardMetrics.ms_v_charge_state->SetValue("standard");
                        StandardMetrics.ms_v_charge_type->SetValue("stopped");
                        StandardMetrics.ms_v_charge_mode->SetValue("stopped");
                        PollSetState(0);
                    }
                break;
            
                }
            case 0x373: // set status to on
                  {
                      StandardMetrics.ms_v_env_on->SetValue(bool( d[7] & 0x10 ));
                      if (StandardMetrics.ms_v_env_on->AsBool())
                          PollSetState(1);
                      
                      break;
                  }

//            case 0x375:  // set status to driving
//                {
//                    StandardMetrics.ms_v_env_on->SetValue(bool( d[5] & 0x10 ));
//                          PollSetState(2);
//                    break;
//                }
                
            case 0x540:  // odometer in KM
                {
                    StandardMetrics.ms_v_pos_odometer->SetValue(d[0] << 16 | d[1] << 8 | d[2]);// odometer
                    break;
                }
      
            case 0x389:  // charger or battery temp
                {
                float invtemp = d[1];
                    StandardMetrics.ms_v_inv_temp->SetValue(invtemp / 10);
                break;
                }
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
