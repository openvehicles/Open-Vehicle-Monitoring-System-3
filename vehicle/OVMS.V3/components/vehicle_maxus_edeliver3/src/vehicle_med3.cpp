/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th March 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Shane Hunns
;    (C) 2021       Peter Harry
;    (C) 2021       Michael Balzer
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

#include <stdio.h>
#include "vehicle_med3.h"
#include "med3_pids.h"
#include "ovms_webserver.h"

// Vehicle states:
#define STATE_OFF             0     // Pollstate 0 - POLLSTATE_OFF      - car is off
#define STATE_ON              1     // Pollstate 1 - POLLSTATE_ON       - car is on
#define STATE_RUNNING         2     // Pollstate 2 - POLLSTATE_RUNNING  - car is driving
#define STATE_CHARGING        3     // Pollstate 3 - POLLSTATE_CHARGING - car is charging

// RX buffer access macros: b=byte#
#define RXB_BYTE(b)           m_rxbuf[b]
#define RXB_UINT16(b)         (((uint16_t)RXB_BYTE(b) << 8) | RXB_BYTE(b+1))
#define RXB_UINT24(b)         (((uint32_t)RXB_BYTE(b) << 16) | ((uint32_t)RXB_BYTE(b+1) << 8) \
                               | RXB_BYTE(b+2))
#define RXB_UINT32(b)         (((uint32_t)RXB_BYTE(b) << 24) | ((uint32_t)RXB_BYTE(b+1) << 16) \
                               | ((uint32_t)RXB_BYTE(b+2) << 8) | RXB_BYTE(b+3))
#define RXB_INT8(b)           ((int8_t)RXB_BYTE(b))
#define RXB_INT16(b)          ((int16_t)RXB_UINT16(b))
#define RXB_INT32(b)          ((int32_t)RXB_UINT32(b))

namespace
{

static const OvmsVehicle::poll_pid_t obdii_polls[] =
    {
        // VCU Polls
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcusoh, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //SOH
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcusoc, {  360, 30, 30, 30  }, 0, ISOTP_STD }, //SOC Scaled below
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcutemp1, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //temp
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcutemp2, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //temp
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcupackvolts, {  360, 30, 30, 30  }, 0, ISOTP_STD }, //Pack Voltage
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcu12vamps, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //12v amps?
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuchargervolts, {  0, 30, 30, 30  }, 0, ISOTP_STD }, //charger volts at a guess?
        { vcutx, vcurx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, vcuchargeramps, {  0, 60, 60, 60  }, 0, ISOTP_STD }, //charger amps?
        // BMS Polls
        { bmstx, bmsrx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, cellvolts, {  0, 60, 60, 60  }, 0, ISOTP_STD }, //cell volts
        { bmstx, bmsrx, VEHICLE_POLL_TYPE_OBDIIEXTENDED, celltemps, {  0, 60, 60, 60  }, 0, ISOTP_STD }, //cell temps
        { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0, 0 }
    };
}  // anon namespace

// OvmsVehicleMaxed3 constructor
 
OvmsVehicleMaxed3::OvmsVehicleMaxed3()
    {
        ESP_LOGI(TAG, "Start Maxus eDeliver3 vehicle module");

        // Init CAN:
        RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

        // Init BMS:
        BmsSetCellArrangementVoltage(96, 16);
        BmsSetCellArrangementTemperature(16, 1);
        BmsSetCellLimitsVoltage(2.0, 5.0);
        BmsSetCellLimitsTemperature(-39, 200);
        BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
        BmsSetCellDefaultThresholdsTemperature(2.0, 3.0);

        // Init polling:
        PollSetThrottling(0);
        PollSetResponseSeparationTime(1);
        PollSetState(STATE_OFF);
        PollSetPidList(m_can1, obdii_polls);
        
        // Init VIN:
        memset(m_vin, 0, sizeof(m_vin));
        
        // Init Energy:
        med3_cum_energy_charge_wh = 0;
        
        // Init WebServer:
        #ifdef CONFIG_OVMS_COMP_WEBSERVER
            WebInit();
        #endif
    }

//OvmsVehicleMaxed3 destructor

OvmsVehicleMaxed3::~OvmsVehicleMaxed3()
    {
        ESP_LOGI(TAG, "Stop eDeliver3 vehicle module");
        PollSetPidList(m_can1, NULL);
    }

// IncomingPollReply:

void OvmsVehicleMaxed3::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
{
  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    m_rxbuf.clear();
    m_rxbuf.reserve(length + mlremain);
  }
  m_rxbuf.append((char*)data, length);
  if (mlremain)
    return;

  // response complete:
    ESP_LOGV(TAG, "IncomingPollReply: PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
    
    int value1 = (int)data[0];
    int value2 = ((int)data[0] << 8) + (int)data[1];
    
  switch (pid)
  {
      case vcuvin:  // VIN
          StandardMetrics.ms_v_vin->SetValue(m_rxbuf);
          break;
      case vcusoh: //soh
          StandardMetrics.ms_v_bat_soh->SetValue(value1);
          break;
      case vcusoc: //soc scaled from 2 - 99
          StandardMetrics.ms_v_bat_soc->SetValue(value1);
          break;
      case vcutemp1:  // temperature??
          StandardMetrics.ms_v_mot_temp->SetValue(value1 / 10.0f);
          break;
      case vcutemp2:  // temperature??
          StandardMetrics.ms_v_env_temp->SetValue(value1 / 10.0f);
          break;
      case vcupackvolts:  // Pack Voltage
          StandardMetrics.ms_v_bat_voltage->SetValue(value2 / 10.0f);
          break;
      case vcu12vamps:
          StandardMetrics.ms_v_bat_12v_current->SetValue(value1 / 10.0f);
          break;
      case vcuchargervolts:
          StandardMetrics.ms_v_charge_voltage->SetValue(value1); // possible but always 224 untill found only
          break;
      case vcuchargeramps:
          StandardMetrics.ms_v_charge_current->SetValue(value1);
          StandardMetrics.ms_v_charge_climit->SetValue(value1);
          StandardMetrics.ms_v_charge_power->SetValue((StandardMetrics.ms_v_charge_current->AsFloat() * (StandardMetrics.ms_v_charge_voltage->AsFloat() )) / 1000); // work out current untill pid found * (value / 10.0f) / 1000.0f
          break;
      case cellvolts:
        {
            // Read battery cell voltages 1-96:
            BmsRestartCellVoltages();
            for (int i = 0; i < 96; i++)
                {
                    BmsSetCellVoltage(i, RXB_UINT16(i*2) * 0.001f);
                }
            break;
        }
        case celltemps:
        {
            // Read battery cell temperatures 1-16:
                BmsRestartCellTemperatures();
            for (int i = 0; i < 16; i++)
                {
                    BmsSetCellTemperature(i, RXB_UINT16(i*2) * 0.001f);
                }
            break;
        }

    default:
    {
      ESP_LOGW(TAG, "IncomingPollReply: unhandled PID %02X: len=%d %s", pid, m_rxbuf.size(), hexencode(m_rxbuf).c_str());
    }
  }
}

// Can Frames

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
                
                
            case 0x604:  // power
                {
                float power = d[5];
                StandardMetrics.ms_v_bat_power->SetValue((power * 42.0f) / 1000.0f);// actual power in watts on AC converted to kw
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

// PollerStateTicker: framework callback: check for state changes
// This is called by VehicleTicker1() just before the next PollerSend().

void OvmsVehicleMaxed3::PollerStateTicker()
{
    bool charging12v = StandardMetrics.ms_v_env_charging12v->AsBool();
    bool vanIsCharging = StandardMetrics.ms_v_charge_inprogress->AsBool();
    StandardMetrics.ms_v_env_charging12v->SetValue(StandardMetrics.ms_v_bat_12v_voltage->AsFloat() >= 12.9);
    StandardMetrics.ms_v_charge_inprogress->SetValue(StandardMetrics.ms_v_bat_power->AsFloat() >=  1.000f);

  // Determine new polling state:
  int poll_state;
  if (!charging12v)
    poll_state = STATE_OFF;
  else if (vanIsCharging)
    poll_state = STATE_CHARGING;
  else
    poll_state = STATE_ON;

  // Set base state flags:
  StdMetrics.ms_v_env_awake->SetValue(poll_state == STATE_ON);
  StdMetrics.ms_v_env_on->SetValue(poll_state == STATE_ON);

  // Handle polling state change
  if (poll_state == STATE_CHARGING)
  {
    if (m_poll_state != STATE_CHARGING)
    {
      // Charge started:
      StdMetrics.ms_v_door_chargeport->SetValue(true);
      StdMetrics.ms_v_charge_pilot->SetValue(true);
      StdMetrics.ms_v_charge_inprogress->SetValue(true);
      StdMetrics.ms_v_charge_substate->SetValue("onrequest");
      StdMetrics.ms_v_charge_state->SetValue("charging");
      PollSetState(STATE_CHARGING);
    }
  }
  else
  {
    if (m_poll_state == STATE_CHARGING) {
      // Charge stopped:
      StdMetrics.ms_v_charge_pilot->SetValue(false);
      StdMetrics.ms_v_charge_inprogress->SetValue(false);
      int soc = StdMetrics.ms_v_bat_soc->AsInt();
      if (soc >= 99) {
        StdMetrics.ms_v_charge_substate->SetValue("stopped");
        StdMetrics.ms_v_charge_state->SetValue("done");
      } else {
        StdMetrics.ms_v_charge_substate->SetValue("interrupted");
        StdMetrics.ms_v_charge_state->SetValue("stopped");
      }
    }
  }

  if (poll_state == STATE_ON)
  {
    if (m_poll_state != STATE_ON) {
      // Switched on:
      StdMetrics.ms_v_door_chargeport->SetValue(false);
      StdMetrics.ms_v_charge_substate->SetValue("");
      StdMetrics.ms_v_charge_state->SetValue("");
      PollSetState(STATE_ON);
    }
  }

  if (poll_state == STATE_OFF)
  {
    if (m_poll_state != STATE_OFF) {
      // Switched off:
      StdMetrics.ms_v_bat_current->SetValue(0);
      StdMetrics.ms_v_bat_power->SetValue(0);
      PollSetState(STATE_OFF);
    }
  }
}


// Vehicle framework registration

class OvmsVehicleMaxed3Init
  {
  public: OvmsVehicleMaxed3Init();
  } MyOvmsVehicleMaxed3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleMaxed3Init::OvmsVehicleMaxed3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Maxus eDeliver3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxed3>("MED3","Maxus eDeliver3");
  }
