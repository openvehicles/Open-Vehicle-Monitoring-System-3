/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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

/*
; CREDIT
; Thanks to Scott451 for figuring out many of the Roadster CAN bus messages used by the OVMS,
; and the pinout of the CAN bus socket in the Roadster.
; http://www.teslamotorsclub.com/showthread.php/4388-iPhone-app?p=49456&viewfull=1#post49456"]iPhone app
; Thanks to fuzzylogic for further analysis and messages such as door status, unlock/lock, speed, VIN, etc.
; Thanks to markwj for further analysis and messages such as Trip, Odometer, TPMS, etc.
*/

#include "ovms_log.h"
static const char *TAG = "v-teslaroadster";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslaroadster.h"
#include "vehicle_teslaroadster_ctp.h"
#include "ovms_metrics.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"
#include "ovms_utils.h"
#include "ovms_time.h"
#include "driver/uart.h"
#include "driver/gpio.h"

OvmsVehicleTeslaRoadster* MyTeslaRoadster = NULL;

void TeslaRoadsterSpeedoTimer( TimerHandle_t timer )
  {
  if (MyTeslaRoadster==NULL)
    {
    xTimerStop(timer,0);
    xTimerDelete(timer,0);
    return;
    }

  if (!MyTeslaRoadster->m_speedo_running) return;

  if (MyTeslaRoadster->m_speedo_ticker_count++ >= 100)
    {
    MyTeslaRoadster->m_speedo_ticker_count = 0;
    MyTeslaRoadster->m_speedo_ticker = MyTeslaRoadster->m_speedo_ticker_max;
    }

  if (MyTeslaRoadster->m_speedo_ticker > 0)
    {
    MyTeslaRoadster->m_speedo_ticker--;
    MyTeslaRoadster->m_can1->Write(&MyTeslaRoadster->m_speedo_frame);
    }
  }

float TeslaRoadsterLatLon(int v)
  {
  return (((float)v) / 2048 / 3600);
  }

OvmsVehicleTeslaRoadster::OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Tesla Roadster v1.x, v2.x and v3.0 vehicle module");

  memset(m_type,0,sizeof(m_type));
  memset(m_vin,0,sizeof(m_vin));
  m_aux12v = false;
  m_requesting_cac = false;
  m_starting_charge = INACTIVE;

  m_cooldown_running = false;
  m_cooldown_prev_chargemode = Standard;
  m_cooldown_prev_chargelimit = 0;
  m_cooldown_prev_charging = false;
  m_cooldown_limit_temp = 0;
  m_cooldown_limit_minutes = 0;
  m_cooldown_current = 12;
  m_cooldown_cycles_done = 0;
  m_cooldown_recycle_ticker = -1;

  m_speedo_running = false;
  memset(&m_speedo_frame,0,sizeof(CAN_frame_t));
  m_speedo_frame.origin = m_can1;
  m_speedo_frame.FIR.U = 0;
  m_speedo_frame.FIR.B.DLC = 8;
  m_speedo_frame.FIR.B.FF = CAN_frame_std;
  m_speedo_frame.MsgID = 0x400;
  m_speedo_rawspeed = 0;
  m_speedo_ticker = 0;
  m_speedo_ticker_max = 0;
  m_speedo_ticker_count = 0;
  m_speedo_timer = NULL;

  MyConfig.RegisterParam("xtr", "Tesla Roadster", true, true);

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_1000KBPS);

  MyTeslaRoadster = this;
  }

OvmsVehicleTeslaRoadster::~OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Roadster vehicle module");

  MyTeslaRoadster = NULL;

  if (m_speedo_running)
    {
    xTimerStop(m_speedo_timer,0);
    xTimerDelete(m_speedo_timer,0);
    m_speedo_timer = NULL;
    m_speedo_running = false;
    }
  }

void OvmsVehicleTeslaRoadster::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle: Tesla Roadster %s (%s)\n",m_type,m_vin);

  if (m_cooldown_running)
    {
    writer->printf("\nCooldown is running (with %d cycle(s) so far)\n",
      m_cooldown_cycles_done);
    writer->printf("  ESS temperature: %dC/%dC",
      StandardMetrics.ms_v_bat_temp->AsInt(),
      m_cooldown_limit_temp);
    writer->printf("  Cooldown remaining: %d minutes\n",
      m_cooldown_limit_minutes);
    }

  if (m_speedo_running)
    {
    writer->puts("\nDigital speedo is running");
    }
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
        case 0x06: // Timer/Plugin setting and timer confirmation
          {
          if (d[1] == 0x1b)
            {
            StandardMetrics.ms_v_charge_timermode->SetValue(d[4] != 0);
            }
          else if (d[1] == 0x1a)
            {
            StandardMetrics.ms_v_charge_timerstart->SetValue(((int)d[4]*60+d[5])*60);
            }
          break;
          }
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
        case 0x81: // Time/Date UTC
          {
          int tm = ((int)d[7]<<24) + ((int)d[6]<<16) + ((int)d[5]<<8) + d[4];
          MyTime.Set(TAG, 2, true, tm);
          break;
          }
        case 0x82: // Ambient Temperature
          {
          if (m_aux12v)
            {
            StandardMetrics.ms_v_env_temp->SetValue((int8_t)d[1]);
            }
          break;
          }
        case 0x83: // GPS Latitude
          {
          StandardMetrics.ms_v_pos_latitude->SetValue(
            TeslaRoadsterLatLon(((int)d[7]<<24)+
                                ((int)d[6]<<16)+
                                ((int)d[5]<<8)+
                                d[4]));
          break;
          }
        case 0x84: // GPS Longitude
          {
          StandardMetrics.ms_v_pos_longitude->SetValue(
            TeslaRoadsterLatLon(((int)d[7]<<24)+
                                ((int)d[6]<<16)+
                                ((int)d[5]<<8)+
                                d[4]));
          break;
          }
        case 0x85: // GPS direction and altitude
          {
          StandardMetrics.ms_v_pos_gpslock->SetValue(d[1] == 1);
          StandardMetrics.ms_v_pos_direction->SetValue((((int)d[3]<<8)+d[2])%360);
          if (d[5] & 0xf0)
            StandardMetrics.ms_v_pos_altitude->SetValue(0);
          else
            StandardMetrics.ms_v_pos_altitude->SetValue(((int)d[5]<<8)+d[4]);
          break;
          }
        case 0x87: // Gear shift status every 10 seconds for v2.x cars
          {
          if (m_type[2] != '2')	// This only works for v2.x cars
            break;
          switch(d[1] & 0xE0)
            {
            case 0x20:  // Off or Park
              StandardMetrics.ms_v_env_gear->SetValue(0);
              break;
            case 0x40:  // Reverse
              StandardMetrics.ms_v_env_gear->SetValue(-1);
              break;
            case 0x60:  // Neutral
              StandardMetrics.ms_v_env_gear->SetValue(0);
              break;
            case 0x80:  // Drive
              StandardMetrics.ms_v_env_gear->SetValue(1);
              break;
            default:    // Ignore any transitional values
              break;
            }
          break;
          }
        case 0x88: // Charging Current / Duration
          {
          StandardMetrics.ms_v_charge_current->SetValue((float)d[1]);
          StandardMetrics.ms_v_charge_climit->SetValue((float)d[6]);
          break;
          }
        case 0x89:  // Charging Voltage / Iavailable
          {
          m_speedo_rawspeed = (int)d[1];
          StandardMetrics.ms_v_pos_speed->SetValue((float)d[1],Mph);
          // https://en.wikipedia.org/wiki/Tesla_Roadster#Transmission
          // Motor RPM given as 14,000rpm for top speed 125mph (201kph) = 112rpm/mph
          StandardMetrics.ms_v_mot_rpm->SetValue((int)d[1]*112);
          StandardMetrics.ms_v_charge_voltage->SetValue(((float)d[3]*256)+d[2]);
          break;
          }
        case 0x8f: // HVAC#1 message
          {
          uint32_t hvacrpm = ((int)d[7]<<8)+d[6];
          if (hvacrpm > 0)
            {
            // HVAC is running, so stop the recycle attempts
            m_cooldown_recycle_ticker = -1;
            }
          else if ((m_cooldown_running)&&(StandardMetrics.ms_v_env_hvac->AsBool()))
            {
            // Car is cooling down, and HVAC has just gone off - end of cycle
            m_cooldown_cycles_done++;
            m_cooldown_recycle_ticker = 60; // Try to recycle in 60 seconds
            }
          StandardMetrics.ms_v_env_hvac->SetValue(hvacrpm > 0);
          break;
          }
        case 0x93: // VDS vehicle error
          {
          uint32_t k1 = ((uint32_t)d[3]<<8) + d[2];
          uint32_t k2 = ((uint32_t)d[7]<<24) +
                        ((uint32_t)d[6]<<16) +
                        ((uint32_t)d[5]<<8) +
                        d[4];
          if (k1 != 0xffff)
            {
            if ((d[1]==0x14)&&(k1 == 25))
              {
              // Special case of 100% vehicle logs
              MyNotify.NotifyErrorCode(k1, k2, true, true); // force code 25
              }
            else if (d[1] & 0x01)
              {
              MyNotify.NotifyErrorCode(k1, k2, true); // Raise error notification
              }
            else
              {
              MyNotify.NotifyErrorCode(k1, 0, false); // Clear error notification
              }
            }
          break;
          }
        case 0x95: // Charging mode
          {
          StandardMetrics.ms_v_env_heating->SetValue(d[1]==0x0f);
          switch (d[1]) // Charge state
            {
            case 0x01: // Charging
              m_starting_charge = INACTIVE; // Charging started, return to idle state.
              StandardMetrics.ms_v_charge_state->SetValue("charging"); break;
            case 0x02: // Top-off
              m_starting_charge = INACTIVE; // Charging started, return to idle state.
              StandardMetrics.ms_v_charge_state->SetValue("topoff"); break;
            case 0x04: // done
              // Don't change state, we see this while preparing to restart after done.
              StandardMetrics.ms_v_charge_state->SetValue("done"); break;
            case 0x0c: // interrupted
            case 0x16: // interrupted
            case 0x17: // interrupted
            case 0x18: // interrupted
            case 0x19: // interrupted
              m_starting_charge = INACTIVE; // Charging stopped, return to idle state.
              StandardMetrics.ms_v_charge_state->SetValue("stopped"); break;
            case 0x0d: // preparing
              StandardMetrics.ms_v_charge_state->SetValue("prepare"); break;
            case 0x0e: // timer wait
              // If charging is manually started while the car is asleep and
              // waiting for a scheduled start time then we need to report the
              // state as "prepare" while the car wakes up and checks for a
              // pilot signal.
              StandardMetrics.ms_v_charge_state->SetValue(m_starting_charge == INACTIVE ?
                "timerwait" : "prepare"); break;
            case 0x0f: // heating
              StandardMetrics.ms_v_charge_state->SetValue("heating"); break;
            case 0x15: // interrupted
              // Don't change state when charging has been stopped by request,
              // we see this while preparing to restart.  State will already
              // have been reset when charging started.
              if (d[2] != 0x03)
                m_starting_charge = INACTIVE; // Charging stopped, return to idle state.
              StandardMetrics.ms_v_charge_state->SetValue("stopped"); break;
            default:
              break;
            }
          switch (d[2]) // Charge sub-state
            {
            case 0x02: // Scheduled start
              StandardMetrics.ms_v_charge_substate->SetValue("scheduledstart");
              if (m_starting_charge == POWERWAIT)
                CommandStartCharge();
              break;
            case 0x03: // On request
              StandardMetrics.ms_v_charge_substate->SetValue("onrequest"); break;
            case 0x05: // Timer wait
              StandardMetrics.ms_v_charge_substate->SetValue("timerwait"); break;
            case 0x07: // Power wait
              // If charging is manually started while the car is waiting for a
              // scheduled start time then the substate will temporarily change
              // to 'powerwait' while the car checks for a pilot signal.  This
              // would cause app to hide the charge connector icon, so when
              // manually starting a charge from the app we continue to report
              // 'scheduledstart' instead and advance to next state.
              StandardMetrics.ms_v_charge_substate->SetValue(m_starting_charge == INACTIVE ?
                "powerwait" : "scheduledstart");
              if (m_starting_charge == SCHEDULED)
                m_starting_charge = POWERWAIT;
              break;
            case 0x0d: // interrupted
              StandardMetrics.ms_v_charge_substate->SetValue("stopped"); break;
            case 0x09: // xxMinutes - yykWh
              break;
            default:
              break;
            }
          if (m_type[2] == '1')
            { // v1.x cars
            StandardMetrics.ms_v_charge_mode->SetValue(chargemode_code(d[4] & 0x0f));
            }
          else if (m_type[2] == '2')
            { // v2.x cars
            StandardMetrics.ms_v_charge_mode->SetValue(chargemode_code((d[5]>>4) & 0x0f));
            }
          StandardMetrics.ms_v_charge_kwh->SetValue((float)d[7]);
          break;
          }
        case 0x96: // Doors / Charging yes/no
          {
          m_aux12v = d[3] & 0x02;
          StandardMetrics.ms_v_door_fl->SetValue(d[1] & 0x01);
          StandardMetrics.ms_v_door_fr->SetValue(d[1] & 0x02);
          StandardMetrics.ms_v_door_chargeport->SetValue(d[1] & 0x04);
          StandardMetrics.ms_v_charge_pilot->SetValue(d[1] & 0x08);
          StandardMetrics.ms_v_charge_inprogress->SetValue(d[1] & 0x10);
          StandardMetrics.ms_v_env_handbrake->SetValue(d[1] & 0x40);
          StandardMetrics.ms_v_env_locked->SetValue(d[2] & 0x08);
          StandardMetrics.ms_v_env_valet->SetValue(d[2] & 0x10);
          StandardMetrics.ms_v_env_headlights->SetValue(d[2] & 0x20);
          StandardMetrics.ms_v_door_hood->SetValue(d[2] & 0x40);
          StandardMetrics.ms_v_door_trunk->SetValue(d[2] & 0x80);
          StandardMetrics.ms_v_env_aux12v->SetValue(m_aux12v);
          StandardMetrics.ms_v_env_charging12v->SetValue(d[3] & 0x01);
          StandardMetrics.ms_v_env_cooling->SetValue(d[3] & 0x02);
          StandardMetrics.ms_v_env_alarm->SetValue(d[4] & 0x02);
          if (d[1] & 0x80)
            {
            StandardMetrics.ms_v_env_on->SetValue(true);
            StandardMetrics.ms_v_env_awake->SetValue(true);
            if (MyConfig.GetParamValueBool("xtr", "digital.speedo", false))
              {
              // Digital speedo is enabled
              m_speedo_ticker = 0;
              m_speedo_ticker_max = MyConfig.GetParamValueInt("xtr", "digital.speedo.reps", 3);
              m_speedo_running = true;
              int timerticks = pdMS_TO_TICKS(1); if (timerticks<1) timerticks=1;
              m_speedo_timer = xTimerCreate("TR ticker", timerticks, pdTRUE, this, TeslaRoadsterSpeedoTimer);
              xTimerStart(m_speedo_timer,0);
              }
            }
          else
            {
            StandardMetrics.ms_v_env_on->SetValue(false);
            StandardMetrics.ms_v_env_awake->SetValue(false);
            if (m_speedo_running)
              {
              m_speedo_running = false;
              xTimerStop(m_speedo_timer,0);
              xTimerDelete(m_speedo_timer,0);
              m_speedo_timer = NULL;
              }
            }

          break;
          }
        case 0x9e: // CAC
          {
          int lim = d[1];
          int min = d[6];
          int max = d[7];
          float cac = (float)d[3] + ((float)d[2]/256);
          StandardMetrics.ms_v_bat_cac->SetValue(cac);
          StandardMetrics.ms_v_bat_pack_level_min->SetValue((float)min);
          StandardMetrics.ms_v_bat_pack_level_max->SetValue((float)max);
          if (cac > 165)
            { // Assume a R80 roadster
            StandardMetrics.ms_v_bat_soh->SetValue(
              (cac/215.0)*
              ((100*min)/max));
            }
          else
            { // Assume a standard 1.5/2.x roadster
            StandardMetrics.ms_v_bat_soh->SetValue(
              (cac/160.0)*
              ((100*min)/max));
            }
          char *buf = new char[128];
          sprintf(buf,"CAC %0.2fAh SOC %d%% LIM %d%% MIN %d%% MAX %d%%",
            cac,
            StandardMetrics.ms_v_bat_soc->AsInt(),
            lim, min, max);
          StandardMetrics.ms_v_bat_health->SetValue(buf);
          delete [] buf;
          RequestStreamStopCAC();
          break;
          }
        case 0xA3: // PEM, MOTOR, BATTERY temperatures
          {
          if (m_aux12v)
            {
            StandardMetrics.ms_v_inv_temp->SetValue(d[1]);
            StandardMetrics.ms_v_charge_temp->SetValue(d[1]);
            StandardMetrics.ms_v_mot_temp->SetValue(d[2]);
            StandardMetrics.ms_v_bat_temp->SetValue(d[6]);
            }
          break;
          }
        case 0xA4: // 7 bytes start of VIN bytes i.e. "SFZRE2B"
          {
          memcpy(m_vin,d+1,7);
          break;
          }
        case 0xA5: // 7 bytes mid of VIN bytes i.e. "39A3000"
          {
          memcpy(m_vin+7,d+1,7);
          m_type[0] = 'T';
          m_type[1] = 'R';
          if ((d[3]=='A')||(d[3]=='B')||(d[3]=='C')) m_type[2] = '2'; else m_type[2] = '1';
          if (d[1] == '3') m_type[3] = 'S'; else m_type[3] = 'N';
          m_type[4] = 0;
          StandardMetrics.ms_v_type->SetValue(m_type);
          break;
          }
        case 0xA6: // 3 bytes last of VIN bytes i.e. "359"
          {
          if ((m_vin[0] != 0)&&(m_vin[7] != 0))
            {
            memcpy(m_vin+14,p_frame->data.u8+1,3);
            m_vin[17] = 0;
            StandardMetrics.ms_v_vin->SetValue(m_vin);
            }
          break;
          }
        default:
          break;
        }
      break;
      }
    case 0x102: // Messages from VDS
      {
      switch (d[0])
        {
        case 0x0e: // Lock/Unlock state on ID#102
          {
          StandardMetrics.ms_v_env_locked->SetValue(d[1] == 0x04);
          break;
          }
        }
      break;
      }
    case 0x344: // TPMS
      {
      if (d[1]>0) // Front-left
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, (float)d[0] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, (float)d[1] - 40);
        }
      if (d[3]>0) // Front-right
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, (float)d[2] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, (float)d[3] - 40);
        }
      if (d[5]>0) // Rear-left
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, (float)d[4] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, (float)d[5] - 40);
        }
      if (d[7]>0) // Rear-right
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, (float)d[6] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, (float)d[7] - 40);
        }
      break;
      }
    case 0x400:
      {
      switch(d[0])
        {
        case 0x01:   // Data to dashboard PRND display 10x per second
          {
          if (m_type[2] != '1')	// This only works for v1.x cars
            break;
          switch(d[3])
            {
            case 0:  // Off
            case 0x3f:  // Park
              StandardMetrics.ms_v_env_gear->SetValue(0);
              break;
            case 0x3e:  // Reverse
              StandardMetrics.ms_v_env_gear->SetValue(-1);
              break;
            case 0x3b:  // Neutral
              StandardMetrics.ms_v_env_gear->SetValue(0);
              break;
            case 0x2f:  // Drive
              StandardMetrics.ms_v_env_gear->SetValue(1);
              break;
            default:    // Ignore some transitional values
              break;
            }
          break;
          }
        case 0x02:   // Data to dashboard
          {
          if ((m_speedo_running)&&(d[2] != m_speedo_rawspeed))
            {
            // Digital speedo is running
            m_speedo_frame.data.u8[0] = d[0];
            m_speedo_frame.data.u8[1] = d[1];
            m_speedo_frame.data.u8[2] = m_speedo_rawspeed;
            m_speedo_frame.data.u8[3] = d[3] & 0x0f0;
            m_speedo_frame.data.u8[4] = d[4];
            m_speedo_frame.data.u8[5] = d[5];
            m_speedo_frame.data.u8[6] = d[6];
            m_speedo_frame.data.u8[7] = d[7];
            m_can1->Write(&m_speedo_frame);
            m_speedo_ticker = m_speedo_ticker_max;
            }

          unsigned int amps = (unsigned int)d[2]
                            + (((unsigned int)d[3]&0x7f)<<8);
          StandardMetrics.ms_v_bat_current->SetValue((float)amps);

          float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
          if(soc != 0)
            {
            float power = amps * (((400.0-370.0)*soc/100.0)+370) / 1000.0;     // estimate Voltage by SOC
            StandardMetrics.ms_v_bat_power->SetValue((float)power);
            }
          break;
          }
        }
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

int OvmsVehicleTeslaRoadster::GetNotifyChargeStateDelay(const char* state)
  {
  if (strcmp(state,"charging")==0 || strcmp(state,"topoff")==0 || strcmp(state,"heating")==0)
    return 60;
  else
    return 0;
  }

void OvmsVehicleTeslaRoadster::NotifiedVehicleChargeStart()
  {
  RequestStreamStartCAC();
  ChargeTimePredictor();
  }

void OvmsVehicleTeslaRoadster::NotifiedVehicleOn()
  {
  RequestStreamStartCAC();
  }

void OvmsVehicleTeslaRoadster::NotifiedVehicleOff()
  {
  RequestStreamStartCAC();
  }

void OvmsVehicleTeslaRoadster::Ticker1(uint32_t ticker)
  {
  if ((m_cooldown_running)&&(StandardMetrics.ms_v_charge_inprogress->AsBool()))
    {
    // Cooldown is running, and car is charging
    if (m_cooldown_recycle_ticker > 0)
      {
      m_cooldown_recycle_ticker--;
      if (m_cooldown_recycle_ticker == 10)
        {
        // Switch to PERFORMANCE mode for 10 seconds
        ESP_LOGI(TAG, "Cooldown: Cycle %d PERFORMANCE mode",m_cooldown_cycles_done);
        CommandSetChargeMode(Performance);
        }
      else if (m_cooldown_recycle_ticker == 0)
        {
        // Switch back to RANGE mode, and reset cycle
        ESP_LOGI(TAG, "Cooldown: Cycle %d RANGE mode",m_cooldown_cycles_done);
        CommandSetChargeMode(Range);
        m_cooldown_recycle_ticker = 60;
        }
      if (StandardMetrics.ms_v_charge_climit->AsInt() != m_cooldown_current)
        {
        // Low current charge when cooling down
        ESP_LOGI(TAG, "Cooldown: Cycle %d fix charge current to %dA",m_cooldown_cycles_done,m_cooldown_current);
        CommandSetChargeCurrent(m_cooldown_current);
        }
      }
    }
  }

void OvmsVehicleTeslaRoadster::Ticker60(uint32_t ticker)
  {
  if (m_cooldown_running)
    {
    ESP_LOGI(TAG,"Cooldown: %d cycles %dC/%dC %dA (with %d minute(s) remaining)",
      m_cooldown_cycles_done,
      StandardMetrics.ms_v_bat_temp->AsInt(),
      m_cooldown_limit_temp,
      m_cooldown_current,
      m_cooldown_limit_minutes);
    }

  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
    {
    // Vehicle is charging
    ChargeTimePredictor();

    if (m_cooldown_running)
      {
      m_cooldown_limit_minutes--;
      if ((m_cooldown_limit_minutes == 0) ||
          (StandardMetrics.ms_v_bat_temp->AsInt() <= m_cooldown_limit_temp))
        {
        // Stop the cooldown
        ESP_LOGI(TAG, "Cooldown: Cycle %d cooldown completed",m_cooldown_cycles_done);
        CommandSetChargeCurrent(m_cooldown_prev_chargelimit);
        CommandSetChargeMode(m_cooldown_prev_chargemode);
        m_cooldown_running = false;
        m_cooldown_recycle_ticker = -1;
        if (m_cooldown_prev_charging)
          {
          CommandStartCharge();
          }
        else
          {
          CommandStopCharge();
          }
        }
      }
    }
  else
    {
    // Vehicle is not charging
    if (m_cooldown_running)
      {
      ESP_LOGI(TAG,"Cooldown: Aborted as car stopped charging");
      CommandSetChargeCurrent(m_cooldown_prev_chargelimit);
      CommandSetChargeMode(m_cooldown_prev_chargemode);
      m_cooldown_running = false;
      m_cooldown_recycle_ticker = -1;
      }
    }
  }

void OvmsVehicleTeslaRoadster::RequestStreamStartCAC()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  m_requesting_cac = true;

  // Request CAC streaming...
  // 102 06 D0 07 00 00 00 00 40
  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x06;
  frame.data.u8[1] = 0xd0;
  frame.data.u8[2] = 0x07;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x40;
  m_can1->Write(&frame);
  }

void OvmsVehicleTeslaRoadster::RequestStreamStartHVAC()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  // Request HVAC streaming...
  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x06;
  frame.data.u8[1] = 0xd0;
  frame.data.u8[2] = 0x07;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = 0x80;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x08;
  m_can1->Write(&frame);
  }

void OvmsVehicleTeslaRoadster::RequestStreamStopCAC()
  {
  if (m_requesting_cac)
    {
    CAN_frame_t frame;
    memset(&frame,0,sizeof(frame));

    // Cancel CAC streaming...
    // 102 06 00 00 00 00 00 00 40
    frame.origin = m_can1;
    frame.FIR.U = 0;
    frame.FIR.B.DLC = 8;
    frame.FIR.B.FF = CAN_frame_std;
    frame.MsgID = 0x102;
    frame.data.u8[0] = 0x06;
    frame.data.u8[1] = 0x00;
    frame.data.u8[2] = 0x00;
    frame.data.u8[3] = 0x00;
    frame.data.u8[4] = 0x00;
    frame.data.u8[5] = 0x00;
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x40;
    m_can1->Write(&frame);

    m_requesting_cac = false;
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeMode(vehicle_mode_t mode)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x19;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = (uint8_t)mode;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeCurrent(uint16_t limit)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x02;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = (uint8_t)limit;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandStartCharge()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));
  // If the car is asleep waiting for a scheduled start time or after charging
  // has finished or been stopped by request we will need to repeat the command
  // to start charging after the car wakes up and detects the pilot signal.  We
  // need to sequence through states to track that.
  enum StartingCharge state = m_starting_charge;
  if (!StandardMetrics.ms_v_env_cooling->AsBool() && state == INACTIVE)
    state = SCHEDULED;
  // If this is the repeat of the command to start charging, advance the state.
  if (state == POWERWAIT)
    state = STARTED;
  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0a;
  m_can1->Write(&frame);
  vTaskDelay(150 / portTICK_PERIOD_MS);

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x03;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 1;    // Start charge
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  // Set the StartingCharge state after issuing the command to start the charge
  // in case a CAN status message that could change the state was issued since
  // we selected the next state above.
  m_starting_charge = state;
  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandStopCharge()
  {
  m_starting_charge = INACTIVE;
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x03;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0;    // Stop charge
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x1B;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = timeron;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  if (timeron)
    {
    frame.data.u8[0] = 0x05;
    frame.data.u8[1] = 0x1A;
    frame.data.u8[2] = 0x00;
    frame.data.u8[3] = 0x00;
    frame.data.u8[4] = (timerstart>>8)&0xff;
    frame.data.u8[5] = (timerstart & 0xff);
    frame.data.u8[6] = 0x00;
    frame.data.u8[7] = 0x00;
    m_can1->Write(&frame);
    }

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandCooldown(bool cooldownon)
  {
  if (m_cooldown_running)
    {
    // Cooldown is already in progress, so just ignore it
    return Success;
    }

  m_cooldown_prev_charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
  m_cooldown_prev_chargemode = VehicleModeKey(StandardMetrics.ms_v_charge_mode->AsString());
  m_cooldown_prev_chargelimit = StandardMetrics.ms_v_charge_climit->AsInt();
  m_cooldown_limit_minutes = MyConfig.GetParamValueInt("xtr", "cooldown.timelimit", 60);
  m_cooldown_limit_temp = MyConfig.GetParamValueInt("xtr", "cooldown.templimit", 31);
  m_cooldown_current = MyConfig.GetParamValueInt("xtr", "cooldown.current", 12);

  if ((StandardMetrics.ms_v_bat_temp->AsInt() <= m_cooldown_limit_temp) ||
      (StandardMetrics.ms_v_env_on->AsBool()) ||
      (!StandardMetrics.ms_v_door_chargeport->AsBool()))
    {
    // Either the battery is already cool enough, or the car is ON, or
    // the chargport is shut. In these cases, we can't do a cooldown
    return Fail;
    }

  CommandWakeup(); // Wake up the car
  vTaskDelay(100 / portTICK_PERIOD_MS);
  for (int k=0; k<2; k++)
    {
    CommandSetChargeCurrent(m_cooldown_current); // Set a low-current charge
    vTaskDelay(100 / portTICK_PERIOD_MS);
    CommandSetChargeMode(Range); // Switch to RANGE mode
    vTaskDelay(100 / portTICK_PERIOD_MS);
    CommandStartCharge(); // Force START charge
    vTaskDelay(100 / portTICK_PERIOD_MS);
    }
  RequestStreamStartHVAC();

  m_cooldown_recycle_ticker = -1;
  m_cooldown_running = true;

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandWakeup()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0a;
  m_can1->Write(&frame);

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x06;
  frame.data.u8[1] = 0x2c;
  frame.data.u8[2] = 0x01;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = 0x09;
  frame.data.u8[6] = 0x10;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandLock(const char* pin)
  {
  long lpin = atol(pin);

  if (!StandardMetrics.ms_v_env_handbrake->AsBool())
    {
    // We should refuse to lock a car that has the handbrake off
    ESP_LOGI(TAG, "Lock: Refuse to lock car with handbrake off");
    return Fail;
    }
  if (MyConfig.GetParamValueBool("xtr", "protect.lock", true))
    {
    // We should not lock a car that is ON
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
      ESP_LOGI(TAG, "Lock: Refuse to lock car that is switched on");
      return Fail;
      }
    }

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 2;   // Lock car
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandUnlock(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 3;   // Unlock car
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandActivateValet(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 0;   // Activate valet mode
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandDeactivateValet(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 1;   // Deactivate valet mode
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

void TeslaRoadsterHomelinkTimer(TimerHandle_t timer)
  {
  xTimerStop(timer, 0);
  OvmsVehicleTeslaRoadster* tr = (OvmsVehicleTeslaRoadster*) pvTimerGetTimerID(timer);
  tr->DoHomelinkStop();
  xTimerDelete(timer, 0);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandHomelink(int button, int durationms)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 3;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x09;
  frame.data.u8[1] = 0x02;  // Stop homelink transmission
  frame.data.u8[2] = button;
  m_can1->Write(&frame);

  frame.data.u8[1] = 0x00;  // Start homelink transmission
  m_can1->Write(&frame);

  m_homelink_timerbutton = button;

  int timerticks = pdMS_TO_TICKS(durationms); if (timerticks<1) timerticks=1;
  m_homelink_timer = xTimerCreate("Tesla Roadster Homelink Timer", timerticks, pdTRUE, this, TeslaRoadsterHomelinkTimer);
  xTimerStart(m_homelink_timer, 0);

  return Success;
  }

void OvmsVehicleTeslaRoadster::DoHomelinkStop()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 3;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x09;
  frame.data.u8[1] = 0x02;  // Stop homelink transmission
  frame.data.u8[2] = m_homelink_timerbutton;
  m_can1->Write(&frame);
  }

#ifdef CONFIG_OVMS_COMP_TPMS

bool OvmsVehicleTeslaRoadster::TPMSRead(std::vector<uint32_t> *tpms)
  {
  gpio_set_direction((gpio_num_t)33, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)32, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)33, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)32, GPIO_PULLUP_ONLY);

#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(9, 1); // Enable HIGH
  vTaskDelay(200 / portTICK_PERIOD_MS);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

  uart_port_t uart = UART_NUM_2;
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .use_ref_tick = 0,
  };
  uart_param_config(uart, &uart_config);
  uart_set_pin(uart, 33, 32, 0, 0);
  uart_driver_install(uart, 256, 256, 0, NULL, ESP_INTR_FLAG_LEVEL2);

  const char req_msg[] = "\x0f\x04\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0";

  uart_flush(uart);
  uart_write_bytes(uart, req_msg, 19);

  uint8_t data[38];
  int length = uart_read_bytes(uart, data, 19*2, 5000 / portTICK_PERIOD_MS);
  if (length > 0)
    {
    MyCommandApp.HexDump(TAG, "tpms", (const char*)data, length);
    }

#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(9, 0); // Enable LOW
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  uart_flush(uart);
  uart_driver_delete(uart);

  if (length != 38)
    {
    ESP_LOGE(TAG,"TPMS read was %d bytes (38 expected)",length);
    return false;
    }
  else if ((data[19]!=0x0f) || (data[20]!= 0x05) || (data[37]!=0xf0))
    {
    ESP_LOGE(TAG,"TPMS read (38 bytes) was not formatted as expected");
    return false;
    }

  for (int k=0;k<4;k++)
    {
    int offset = 21+(k*4);
    uint32_t id = ((uint32_t)data[offset] << 24) +
                      ((uint32_t)data[offset+1] << 16) +
                      ((uint32_t)data[offset+2] << 8) +
                      ((uint32_t)data[offset+3]);
    ESP_LOGD(TAG,"TPMS read ID %08x",id);
    tpms->push_back( id );
    }
  return true;
  }

bool OvmsVehicleTeslaRoadster::TPMSWrite(std::vector<uint32_t> &tpms)
  {
  if (tpms.size() != 4)
    {
    ESP_LOGE(TAG,"Tesla Roadster only supports TPMS sets with 4 IDs");
    return false;
    }

  gpio_set_direction((gpio_num_t)33, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)32, GPIO_MODE_INPUT);
  gpio_set_pull_mode((gpio_num_t)33, GPIO_PULLUP_ONLY);
  gpio_set_pull_mode((gpio_num_t)32, GPIO_PULLUP_ONLY);

#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(9, 1); // Enable HIGH
  vTaskDelay(200 / portTICK_PERIOD_MS);
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317

  uart_port_t uart = UART_NUM_2;
  uart_config_t uart_config = {
    .baud_rate = 9600,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    .use_ref_tick = 0,
  };
  uart_param_config(uart, &uart_config);
  uart_set_pin(uart, 33, 32, 0, 0);
  uart_driver_install(uart, 256, 256, 0, NULL, ESP_INTR_FLAG_LEVEL2);

  char req_msg[] = "\x0f\x03\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\x00\xf0";
  int offset = 2;
  for(uint32_t id : tpms)
    {
    ESP_LOGD(TAG,"TPMS write ID %08x",id);
    req_msg[offset++] = (id>>24) & 0xff;
    req_msg[offset++] = (id>>16) & 0xff;
    req_msg[offset++] = (id>>8) & 0xff;
    req_msg[offset++] = id & 0xff;
    }

  uart_flush(uart);
  uart_write_bytes(uart, req_msg, 19);

  uint8_t data[38];
  int length = uart_read_bytes(uart, data, 19*2, 5000 / portTICK_PERIOD_MS);
  if (length > 0)
    {
    MyCommandApp.HexDump(TAG, "tpms", (const char*)data, length);
    }

#ifdef CONFIG_OVMS_COMP_MAX7317
  MyPeripherals->m_max7317->Output(9, 0); // Enable LOW
#endif // #ifdef CONFIG_OVMS_COMP_MAX7317
  uart_flush(uart);
  uart_driver_delete(uart);

  length = 38;
  if (length != 38)
    {
    ESP_LOGE(TAG,"TPMS write was %d bytes (38 expected)",length);
    return false;
    }
  else if ((data[19]!=0x0f) || (data[20]!= 0x09) || (data[37]!=0xf0))
    {
    ESP_LOGE(TAG,"TPMS write (38 bytes) was not formatted as expected");
    return false;
    }

  for (int k=2;k<19;k++)
    {
    if (data[k] != data[k+19])
      {
      ESP_LOGE(TAG,"TPMS write did not verify correctly");
      return false;
      }
    }

  return true;
  }

#endif // #ifdef CONFIG_OVMS_COMP_TPMS

void OvmsVehicleTeslaRoadster::Notify12vCritical()
  { // Not supported on Tesla Roadster
  }

void OvmsVehicleTeslaRoadster::Notify12vRecovered()
  { // Not supported on Tesla Roadster
  }

void OvmsVehicleTeslaRoadster::ChargeTimePredictor()
  { // Predict the charge time remaining
  unsigned char chgmod = chargemode_key(StandardMetrics.ms_v_charge_mode->AsString());
  int wAvailWall = (int)(StandardMetrics.ms_v_charge_voltage->AsFloat(0) *
                         StandardMetrics.ms_v_charge_climit->AsFloat(0));
  int imStart = StandardMetrics.ms_v_bat_range_ideal->AsInt(0,Miles);
  float cac = StandardMetrics.ms_v_bat_cac->AsFloat(160);
  signed char degAmbient = StandardMetrics.ms_v_env_temp->AsInt(0);

  int mins = vehicle_teslaroadster_minutestocharge(
    chgmod, wAvailWall, imStart, -1, 100, cac, degAmbient, NULL);
  StandardMetrics.ms_v_charge_duration_full->SetValue(mins);

  float limitrange = StandardMetrics.ms_v_charge_limit_range->AsFloat();
  if (limitrange > 0)
    {
    mins = vehicle_teslaroadster_minutestocharge(
      chgmod, wAvailWall, imStart, (int)limitrange, 100, cac, degAmbient, NULL);
    StandardMetrics.ms_v_charge_duration_range->SetValue(mins);
    }

  float limitsoc = StandardMetrics.ms_v_charge_limit_soc->AsFloat();
  if (limitsoc > 0)
    {
    mins = vehicle_teslaroadster_minutestocharge(
      chgmod, wAvailWall, imStart, -1, (int)limitsoc, cac, degAmbient, NULL);
    StandardMetrics.ms_v_charge_duration_soc->SetValue(mins);
    }
  }

class OvmsVehicleTeslaRoadsterInit
  {
  public: OvmsVehicleTeslaRoadsterInit();
} MyOvmsVehicleTeslaRoadsterInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaRoadsterInit::OvmsVehicleTeslaRoadsterInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Roadster (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaRoadster>("TR","Tesla Roadster");
  }
