/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
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
;    Subproject:    Integration of support for the VW e-UP via Comfort CAN
;    Date:          28th January 2021
;
;    Changes:
;    0.1.0  Initial code
:           Code frame with correct TAG and vehicle/can registration
;
;    0.1.1  Started with some SOC capture demo code
;
;    0.1.2  Added VIN, speed, 12 volt battery detection
;
;    0.1.3  Added ODO (Dimitrie78), fixed speed
;
;    0.1.4  Added WLTP based ideal range, uncertain outdoor temperature
;
;    0.1.5  Finalized SOC calculation (sharkcow), added estimated range
;
;    0.1.6  Created a climate control first try, removed 12 volt battery status
;
;    0.1.7  Added status of doors
;
;    0.1.8  Added config page for the webfrontend. Features canwrite and modelyear.
;           Differentiation between model years is now possible.
;
;    0.1.9  "Fixed" crash on climate control. Added A/C indicator.
;           First shot on battery temperatur.
;
;    0.2.0  Added key detection and car_on / pollingstate routine
;
;    0.2.1  Removed battery temperature, corrected outdoor temperature
;
;    0.2.2  Collect VIN only once
;
;    0.2.3  Redesign climate control, removed bluetooth template
;
;    0.2.4  First implementation of the ringbus and ocu heartbeat
;
;    0.2.5  Fixed heartbeat
;
;    0.2.6  Refined climate control template
;
;    0.2.7  Implemented dev_mode, 5A7, 69E climate control messages

;    0.2.8  Refactoring for T26A <-> OBD source separation (by SokoFromNZ)
;
;    0.2.9  Fixed climate control
;
;    0.3.0  Added upgrade policy for pre vehicle id splitting versions
;
;    0.3.1  Removed alpha OBD source, corrected class name
;
;    0.3.2  First implementation of charging detection
;
;    0.3.3  Buffer 0x61C ghost messages
;
;    0.3.4  Climate Control is now beginning to work
:
;    0.3.5  Stabilized Climate Control
;
;    0.3.6  Corrected log tag and namespaces
;
;    0.3.7  Add locked detection, add climate control via Homelink for iOS
;
;    0.3.8  Add lights, rear doors and trunk detection
;
;    0.3.9  Corrected estimated range
;
;    0.4.0  Implemnted ICCB charging detection
;
;    0.4.1  Corrected estimated range
;
;    0.4.2  Corrected locked status, cabin temperature
:
;    0.4.3  Added "feature" parameters for model year and cabin temperature setting
;
;    0.4.4  Respond to vehicle turning climate control off
;
;    0.4.5  Moved to unified OBD/T26 code
;
;    0.4.6  Corrected cabin temperature selection
;
;    0.4.7  Beautified code
;
;    0.4.8  Started using ms_v_env_charging12v
;
;    0.4.9  Added T26 awake detection for OBD
;
;    0.5.0 fix CAN awake/asleep loop (sharkcow)

;    0.6.0  complete rework of climatecontrol, added charge control by Urwa Khattak & sharkcow
;
;    (C) 2021       Chris van der Meijden
;        2023       Urwa Khattak, sharkcow      
;
;    Big thanx to sharkcow, Dimitrie78, E-lmo, Dexter and 'der kleine Nik'.
*/

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#include <stdio.h>
#include "pcp.h"
#include "vehicle_vweup.h"
#include "vweup_t26.h"
#include "metrics_standard.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

void OvmsVehicleVWeUp::sendOcuHeartbeat(TimerHandle_t timer)
{
  // Workaround for FreeRTOS duplicate timer callback bug
  // (see https://github.com/espressif/esp-idf/issues/8234)
  static TickType_t last_tick = 0;
  TickType_t tick = xTaskGetTickCount();
  if (tick < last_tick + xTimerGetPeriod(timer) - 3) return;
  last_tick = tick;

  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->SendOcuHeartbeat();
}

void OvmsVehicleVWeUp::T26Init()
{
  ESP_LOGI(TAG, "T26: Starting connection: T26A (Comfort CAN)");
  memset(m_vin, 0, sizeof(m_vin));

  RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

  MyConfig.RegisterParam("xvu", "VW e-Up", true, true);
  vin_part1 = false;
  vin_part2 = false;
  vin_part3 = false;
  ocu_awake = false;
  ocu_working = false;
  ocu_what = false;
  ocu_wait = false;
  vweup_cc_on = false;
  fas_counter_on = 0;
  fas_counter_off = 0;
  signal_ok = false;
  cd_count = 0;
  t26_12v_boost = false;
  t26_car_on = false;
  t26_ring_awake = false;
  t26_12v_boost_cnt = 0;
  t26_12v_wait_off = 0;
  profile0_recv = false;
  profile0_idx = 0;
  profile0_chan = 0;
  profile0_mode = 0;
  profile0_charge_current = 0;
  profile0_cc_temp = 0;
  profile0_charge_current_old = 0;
  profile0_cc_temp_old = 0;
  profile0_delay = 1000;
  profile0_retries = 5;
  memset(profile0_cntr, 0, sizeof(profile0_cntr));
  profile0_state = PROFILE0_IDLE;
  profile0_key = 0;
  profile0_val = 0;
  profile0_activate = false;
  profile0_1sttime = true;
  profile0_success = false;

  dev_mode = false; // true disables writing on the comfort CAN. For code debugging only.

  StandardMetrics.ms_v_env_locked->SetValue(true);
  StandardMetrics.ms_v_env_headlights->SetValue(false);
  StandardMetrics.ms_v_env_charging12v->SetValue(false);
  StandardMetrics.ms_v_env_awake->SetValue(false);
  StandardMetrics.ms_v_env_aux12v->SetValue(false);
  StandardMetrics.ms_v_env_on->SetValue(false);

  if (HasNoOBD()) {
    StandardMetrics.ms_v_charge_mode->SetValue("standard");
  }
}


void OvmsVehicleVWeUp::T26Ticker1(uint32_t ticker)
{
  if (StdMetrics.ms_v_bat_12v_voltage->AsFloat() < 13 && !t26_ring_awake && StandardMetrics.ms_v_env_charging12v->AsBool()) {
    // Wait for 12v voltage to come up to 13.2v while getting a boost:
    t26_12v_boost_cnt++;
    if (t26_12v_boost_cnt > 20) {
      ESP_LOGI(TAG, "T26: Car stopped itself charging the 12v battery");
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
      StandardMetrics.ms_v_env_aux12v->SetValue(false);
      t26_12v_boost = false;
      t26_12v_boost_cnt = 0;

      // Clear powers & currents that are not supported by T26:
      StdMetrics.ms_v_bat_current->SetValue(0);
      StdMetrics.ms_v_bat_power->SetValue(0);
      StdMetrics.ms_v_bat_12v_current->SetValue(0);
      StdMetrics.ms_v_charge_12v_current->SetValue(0);
      StdMetrics.ms_v_charge_12v_power->SetValue(0);
      t26_12v_wait_off = 120; // Wait for two minutes before allowing new polling
      PollSetState(VWEUP_OFF);
    }
  }

  if (StdMetrics.ms_v_bat_12v_voltage->AsFloat() >= 13 && t26_12v_boost_cnt == 0) {
    t26_12v_boost_cnt = 20;
  }
  if (t26_12v_boost_last_cnt == t26_12v_boost_cnt && t26_12v_boost_cnt != 0 && t26_12v_boost_cnt != 20) {
    // We are not waiting to charging 12v to come up anymore:
    t26_12v_boost_cnt = 0;
  }
  if (t26_12v_wait_off != 0) {
    t26_12v_wait_off--;
  }
  t26_12v_boost_last_cnt = t26_12v_boost_cnt;
}


// Takes care of setting all the state appropriate when the car is on
// or off.
//
void OvmsVehicleVWeUp::vehicle_vweup_car_on(bool turnOn)
{
  if (turnOn && !StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG, "T26: CAR IS ON");
    StandardMetrics.ms_v_env_on->SetValue(true);
    if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
      t26_12v_boost_cnt = 0;
      t26_12v_wait_off = 0;
      SetUsePhase(UP_Driving);
      StdMetrics.ms_v_door_chargeport->SetValue(false);
      PollSetState(VWEUP_ON);
    } else {
      PollSetState(VWEUP_CHARGING);
    }
    ResetTripCounters();
    if (ocu_awake && m_sendOcuHeartbeat != NULL) {
      ESP_LOGD(TAG, "T26: stopping OCU heartbeat timer");
      xTimerStop(m_sendOcuHeartbeat, 0);
      xTimerDelete(m_sendOcuHeartbeat, 0);
      m_sendOcuHeartbeat = NULL;
    }
    ocu_awake = false;
    ocu_working = false;
    fas_counter_on = 0;
    fas_counter_off = 0;
    t26_car_on = true;
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    StandardMetrics.ms_v_env_aux12v->SetValue(true);
  }
  else if (!turnOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG, "T26: CAR IS OFF");
    t26_car_on = false;
    t26_12v_boost = false;
    StandardMetrics.ms_v_env_on->SetValue(false);
    // StandardMetrics.ms_v_charge_voltage->SetValue(0);
    // StandardMetrics.ms_v_charge_current->SetValue(0);
    if (StandardMetrics.ms_v_charge_inprogress->AsBool()) {
       PollSetState(VWEUP_CHARGING);
    } else {
       PollSetState(VWEUP_AWAKE);
    }
  }
}

void OvmsVehicleVWeUp::IncomingFrameCan3(CAN_frame_t *p_frame)
{
  uint8_t *d = p_frame->data.u8;

  static bool isCharging = false;
  static bool lastCharging = false;

  // This will log all incoming frames
  // ESP_LOGD(TAG, "T26: IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

  switch (p_frame->MsgID) {

    case 0x61A: // SOC.
      // If available, OBD is normally responsible for the SOC, but K-CAN occasionally
      // sends SOC updates while OBD is in state OFF:
      if (HasNoOBD() || IsOff()) {
        StandardMetrics.ms_v_bat_soc->SetValue(d[7] / 2.0);
      }
      if (HasNoOBD()) {
        if (vweup_modelyear >= 2020) {
          StandardMetrics.ms_v_bat_range_ideal->SetValue((260 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
        else {
          StandardMetrics.ms_v_bat_range_ideal->SetValue((160 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
      }
      break;

    case 0x52D: // KM range left (estimated).
      if (d[0] != 0xFE) {
        if (d[1] == 0x41) {
          StandardMetrics.ms_v_bat_range_est->SetValue(d[0] + 255);
        }
        else {
          StandardMetrics.ms_v_bat_range_est->SetValue(d[0]);
        }
      }
      break;

    case 0x65F: // VIN
      switch (d[0]) {
        case 0x00:
          // Part 1
          m_vin[0] = d[5];
          m_vin[1] = d[6];
          m_vin[2] = d[7];
          vin_part1 = true;
          break;
        case 0x01:
          // Part 2
          if (vin_part1) {
            m_vin[3] = d[1];
            m_vin[4] = d[2];
            m_vin[5] = d[3];
            m_vin[6] = d[4];
            m_vin[7] = d[5];
            m_vin[8] = d[6];
            m_vin[9] = d[7];
            vin_part2 = true;
          }
          break;
        case 0x02:
          // Part 3 - VIN complete
          if (vin_part2 && !vin_part3) {
            m_vin[10] = d[1];
            m_vin[11] = d[2];
            m_vin[12] = d[3];
            m_vin[13] = d[4];
            m_vin[14] = d[5];
            m_vin[15] = d[6];
            m_vin[16] = d[7];
            m_vin[17] = 0;
            vin_part3 = true;
            StandardMetrics.ms_v_vin->SetValue((string)m_vin);
          }
          break;
      }
      break;

    case 0x65D: { // ODO
      float odo = (float) (((uint32_t)(d[3] & 0xf) << 16) | ((UINT)d[2] << 8) | d[1]);
      StandardMetrics.ms_v_pos_odometer->SetValue(odo);
      break;
    }

    case 0x320: // Speed
      StandardMetrics.ms_v_pos_speed->SetValue(((d[4] << 8) + d[3] - 1) / 190);
      UpdateTripOdo();
      if (HasNoOBD())
        CalculateAcceleration(); // only necessary until we find acceleration on T26
      break;

    case 0x527: // Outdoor temperature
      StandardMetrics.ms_v_env_temp->SetValue((d[5] - 100) / 2);
      break;

    case 0x381: // Vehicle locked
      if (d[0] == 0x02) {
        StandardMetrics.ms_v_env_locked->SetValue(true);
      }
      else {
        StandardMetrics.ms_v_env_locked->SetValue(false);
      }
      break;

    case 0x3E3: // Cabin temperature
      if (d[2] != 0xFF) {
        StandardMetrics.ms_v_env_cabintemp->SetValue((d[2] - 100) / 2);
        // Set PEM inv temp to support older app version with cabin temp workaround display
        // StandardMetrics.ms_v_inv_temp->SetValue((d[2] - 100) / 2);
      }
      break;

    case 0x470: // Doors
      StandardMetrics.ms_v_door_fl->SetValue((d[1] & 0x01) > 0);
      StandardMetrics.ms_v_door_fr->SetValue((d[1] & 0x02) > 0);
      StandardMetrics.ms_v_door_rl->SetValue((d[1] & 0x04) > 0);
      StandardMetrics.ms_v_door_rr->SetValue((d[1] & 0x08) > 0);
      StandardMetrics.ms_v_door_trunk->SetValue((d[1] & 0x20) > 0);
      StandardMetrics.ms_v_door_hood->SetValue((d[1] & 0x10) > 0);
      break;

    case 0x531: // Head lights
      if (d[0] > 0) {
        StandardMetrics.ms_v_env_headlights->SetValue(true);
      }
      else {
        StandardMetrics.ms_v_env_headlights->SetValue(false);
      }
      break;

    // Check for running hvac.
    case 0x3E1:
      if (d[4] > 0) {
        StandardMetrics.ms_v_env_hvac->SetValue(true);
      }
      else {
        StandardMetrics.ms_v_env_hvac->SetValue(false);
      }
      break;

    case 0x571: // 12 Volt
      StandardMetrics.ms_v_charge_12v_voltage->SetValue(5 + (0.05 * d[0]));
      break;

    case 0x61C: // Charge detection
      cd_count++;
      if (d[1] == 0xF0 && (d[2] == 0x07 || d[2] == 0x0F)) { // seems to differ for pre-2020 model (0x07)
        isCharging = false;
      }
      else {
        isCharging = true;
      }
      if (isCharging != lastCharging) {
        // count till 3 messages in a row to stop ghost triggering
        if (isCharging && cd_count == 3) {
          cd_count = 0;
          SetUsePhase(UP_Charging);
          ResetChargeCounters();
          StandardMetrics.ms_v_door_chargeport->SetValue(true);
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          SetChargeState(true);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          StandardMetrics.ms_v_env_aux12v->SetValue(true);
          ESP_LOGI(TAG, "T26: Car charge session started");
          t26_12v_wait_off = 0;
          t26_12v_boost_cnt = 0;
          PollSetState(VWEUP_CHARGING);
        }
        if (!isCharging && cd_count == 3) {
          cd_count = 0;
          StandardMetrics.ms_v_charge_pilot->SetValue(false);
          SetChargeState(false);
          if (StandardMetrics.ms_v_env_on->AsBool()) {
            SetUsePhase(UP_Driving);
            StdMetrics.ms_v_door_chargeport->SetValue(false);
            PollSetState(VWEUP_ON);
          } else {
            PollSetState(VWEUP_AWAKE);
          }
          ESP_LOGI(TAG, "T26: Car charge session ended");
        }
      }
      else {
        cd_count = 0;
      }
      if (cd_count == 0) {
        lastCharging = isCharging;
      }
      break;

    case 0x575: // Key position
      switch (d[0]) {
        case 0x00: // No key
          vehicle_vweup_car_on(false);
          StandardMetrics.ms_v_env_awake->SetValue(false);
          break;
        case 0x01: // Key in position 1, no ignition
          StandardMetrics.ms_v_env_awake->SetValue(true);
          vehicle_vweup_car_on(false);
          break;
        case 0x03: // Ignition is turned off
          break;
        case 0x05: // Ignition is turned on
          break;
        case 0x07: // Key in position 2, ignition on
          vehicle_vweup_car_on(true);
          break;
        case 0x0F: // Key in position 3, start the engine
          break;
      }
      break;

    case 0x400: // Welcome to the ring from ILM
    case 0x40C: // We know this one too. Climatronic.
    case 0x436: // Working in the ring.
    case 0x439: // Who are 436 and 439 and why do they differ on some cars?
      if (d[0] == 0x00 && !ocu_awake && !StandardMetrics.ms_v_charge_inprogress->AsBool() && !t26_12v_boost && !t26_car_on && d[1] != 0x31 && t26_12v_wait_off == 0) {
        // The car wakes up to charge the 12v battery 
        StandardMetrics.ms_v_env_charging12v->SetValue(true);
        StandardMetrics.ms_v_env_aux12v->SetValue(true);
        t26_ring_awake = true;
        t26_12v_boost = true;
        t26_12v_boost_cnt = 0;
        PollSetState(VWEUP_AWAKE);
        ESP_LOGI(TAG, "T26: Car woke up. Will try to charge 12v battery");
      }
      if (d[1] == 0x31) {
        if (m_sendOcuHeartbeat != NULL) {
          ESP_LOGD(TAG, "T26: stopping OCU heartbeat timer");
          xTimerStop(m_sendOcuHeartbeat, 0);
          xTimerDelete(m_sendOcuHeartbeat, 0);
          m_sendOcuHeartbeat = NULL;
        }
        if (ocu_awake) { // We should go to sleep, no matter what
          ESP_LOGI(TAG, "T26: Comfort CAN calls for sleep");
          ocu_awake = false;
          ocu_working = false;
          fas_counter_on = 0;
          fas_counter_off = 0;
          t26_12v_boost = false;
          if (StandardMetrics.ms_v_charge_inprogress->AsBool()) {
            PollSetState(VWEUP_CHARGING);
          } else {
            t26_ring_awake = false;
            PollSetState(VWEUP_AWAKE);
          }
          break;
        }
        else if (t26_ring_awake) {
          t26_ring_awake = false;
          ESP_LOGI(TAG, "T26: Ring asleep");
        }
      }
      if (d[0] == 0x00 && d[1] != 0x31 && !t26_ring_awake) {
        t26_ring_awake = true;
        ESP_LOGI(TAG, "T26: Ring awake");
        if (t26_12v_wait_off != 0) {
          ESP_LOGI(TAG, "T26: OBD AWAKE ist still blocked");
        }
      }
      if (d[0] == 0x1D) {
        // We are called in the ring

        unsigned char data[8];
        uint8_t length;
        length = 8;

        canbus *comfBus;
        comfBus = m_can3;

        data[0] = 0x00; // As it seems that we are always the highest in the ring we always send to 0x400
        ESP_LOGV(TAG, "OCU called in ring");
        if (ocu_working) {
          data[1] = 0x01;
          if (dev_mode) {
            ESP_LOGI(TAG, "T26: OCU working");
          }
        }
        else {
          data[1] = 0x11; // Ready to sleep. This is very important!
          if (dev_mode) {
            ESP_LOGI(TAG, "T26: OCU ready to sleep");
          }
        }
        data[2] = 0x02;
        data[3] = d[3]; // Not understood
        data[4] = 0x00;
        if (ocu_what) {
          // This is not understood and not implemented yet.
          data[5] = 0x14;
        }
        else {
          data[5] = 0x10;
        }
        data[6] = 0x00;
        data[7] = 0x00;
        vTaskDelay(pdMS_TO_TICKS(50));
        if (vweup_enable_write && !dev_mode) {
          ESP_LOGV(TAG, "OCU answers");
          comfBus->WriteStandard(0x43D, length, data);  // We answer
        }
        else ESP_LOGE(TAG, "T26: Can't answer to ring request (enable_write: %d, dev_mode: %d)", vweup_enable_write, dev_mode);

        ocu_awake = true;
        break;
      }
      break;

    case 0x69C:
      if((d[0] == 0x80 || d[0] == 0x90 || d[0] == 0xA0 || d[0] == 0xB0) && d[4]== 0x27 && !profile0_recv) // channel ID in d[4] needs to match the one from request
      {
        profile0_chan = (d[0] & 0x7F) >> 4;
        ESP_LOGD(TAG, "T26: profile0 message, header %02x -> channel %d", d[0], profile0_chan);
        profile0_recv = true;
        profile0_idx = 0;
        profile0_cntr[1] = 0;
        ReadProfile0(d);
        }
      else if (profile0_recv)
      {
        if(profile0_chan == 0 && (d[0] & 0xF0) == 0xC0) {           
          ReadProfile0(d);
        }
        else if(profile0_chan == 1 && (d[0] & 0xF0) == 0xD0) {           
          ReadProfile0(d);
        }
        else if(profile0_chan == 2 && (d[0] & 0xF0) == 0xE0) {           
          ReadProfile0(d);
        }
        else if(profile0_chan == 3 && (d[0] & 0xF0) == 0xF0) {           
          ReadProfile0(d);
        }        
      }
      break;

    default:
      // This will log all unknown incoming frames
      // ESP_LOGD(TAG, "T26: IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::RemoteCommandHandler(RemoteCommand command)
{
  ESP_LOGI(TAG, "T26: RemoteCommandHandler command=%d", command);

  if (HasNoT26()) {
    ESP_LOGE(TAG, "T26: RemoteCommandHandler failed: T26 not available");
    return NotImplemented;
  }
  if (!IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: RemoteCommandHandler failed: T26 not ready / no write access");
    return Fail;
  }

  vweup_remote_command = command;
  SendCommand(vweup_remote_command);
  if (signal_ok) {
    signal_ok = false;
    return Success;
  }
  return Fail;
}

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
//
// Does nothing if @command is out of range
//
void OvmsVehicleVWeUp::SendCommand(RemoteCommand command)
{
  if (!IsT26Ready()) {
    ESP_LOGE(TAG, "T26: SendCommand %d failed: T26 not ready", command);
    return;
  }
  switch (command) {
    default:
      signal_ok = true;
      return;
  }
}

// Wakeup implentation over the VW ring commands
// We need to register in the ring with a call to our self from 43D with 0x1D in the first byte
//
OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandWakeup()
{
  if (HasNoT26()) {
    ESP_LOGE(TAG, "T26: CommandWakeup failed: T26 not available");
    return OvmsVehicle::CommandWakeup();
  }
  if (!IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CommandWakeup failed: T26 not ready / no write access");
    return Fail;
  }

  if (!ocu_awake && !StandardMetrics.ms_v_env_on->AsBool()) { //&& profile0_state == PROFILE0_IDLE) {

    unsigned char data[8];
    uint8_t length;
    length = 8;

    unsigned char data2[2];
    uint8_t length2;
    length2 = 2;

    canbus *comfBus;
    comfBus = m_can3;

    // We send 0x69E a request for a (currently unknown) profile on PID 941.
    // This wakes 0x400, we wait two seconds and then call us in the ring
    data2[0] = 0x19;
    data2[1] = 0x41;
    if (!dev_mode) {
      comfBus->WriteStandard(0x69E, length2, data2);
    }
    vTaskDelay(pdMS_TO_TICKS(1000));

    ESP_LOGI(TAG, "T26: Sent Wakeup Command - stage 1");

    data[0] = 0x1D; // We call ourself to wake up the ring
    data[1] = 0x02;
    data[2] = 0x02;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    if (!dev_mode) {
      comfBus->WriteStandard(0x43D, length, data);
    }

    vTaskDelay(pdMS_TO_TICKS(50));

    data[0] = 0x00; // We need to talk to 0x400 first to get accepted in the ring
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x04; // This could be a problem
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    if (!dev_mode) {
      comfBus->WriteStandard(0x43D, length, data);
    }

    ocu_working = true;
    ocu_awake = true;

    vTaskDelay(pdMS_TO_TICKS(50));

    m_sendOcuHeartbeat = xTimerCreate("VW e-Up OCU heartbeat", pdMS_TO_TICKS(1000), pdTRUE, this, sendOcuHeartbeat);
    xTimerStart(m_sendOcuHeartbeat, 0);

    ESP_LOGI(TAG, "T26: Sent Wakeup Command - stage 2");
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    StandardMetrics.ms_v_env_aux12v->SetValue(true);
    t26_12v_boost_cnt = 0;
    t26_12v_wait_off = 0;
    if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
       PollSetState(VWEUP_AWAKE);
    } else {
       PollSetState(VWEUP_CHARGING);
    }
  // This can be done better. Gives always success, even when already awake.
  ESP_LOGD(TAG, "T26: wait after wakeup...");
  vTaskDelay(pdMS_TO_TICKS(1000));
  return Success;
  }
  else 
  {
    ESP_LOGD(TAG, "T26: vehcile already awake, skipping wakeup command %d, %d", ocu_awake, StandardMetrics.ms_v_env_on->AsBool());
    return Fail;
  }
}

void OvmsVehicleVWeUp::SendOcuHeartbeat()
{
  if (!vweup_cc_on) {
    fas_counter_on = 0;
    if (fas_counter_off < 10 && profile0_state == PROFILE0_IDLE) {
      fas_counter_off++;
      ocu_working = true;
      if (dev_mode) {
        ESP_LOGI(TAG, "T26: OCU working");
      }
    }
    else {
      if (profile0_state != PROFILE0_IDLE) {
        ocu_working = true;
        if (dev_mode) {
          ESP_LOGI(TAG, "T26: OCU working");
        }
      }
      else {
        ocu_working = false;
        if (dev_mode) {
          ESP_LOGI(TAG, "T26: OCU ready to sleep");
        }
      }
    }
  }
  else {
    fas_counter_off = 0;
    ocu_working = true;
    if (dev_mode) {
      ESP_LOGI(TAG, "T26: OCU working");
    }
    if (fas_counter_on < 10) {
      fas_counter_on++;
    }
  }

  unsigned char data[8];
  uint8_t length;
  length = 8;

  canbus *comfBus;
  comfBus = m_can3;

  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (comfBus && vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A9, length, data);
  }

  if ((fas_counter_on > 0 && fas_counter_on < 10) || (fas_counter_off > 0 && fas_counter_off < 10)) {
    data[0] = 0x60;
    if (dev_mode) {
      ESP_LOGI(TAG, "T26: OCU Heartbeat - 5A7 60");
    }
  }
  else {
    data[0] = 0x00;
    if (dev_mode) {
      ESP_LOGI(TAG, "T26: OCU Heartbeat - 5A7 00");
    }
  }
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (comfBus && vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandLock(const char *pin)
{
  ESP_LOGI(TAG, "T26: CommandLock");
  // fallback to default implementation:
  return OvmsVehicle::CommandLock(pin);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandUnlock(const char *pin)
{
  ESP_LOGI(TAG, "T26: CommandUnlock");
  // fallback to default implementation:
  return OvmsVehicle::CommandUnlock(pin);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandActivateValet(const char *pin)
{
  ESP_LOGI(TAG, "T26: CommandActivateValet");
  // fallback to default implementation:
  return OvmsVehicle::CommandActivateValet(pin);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandDeactivateValet(const char *pin)
{
  ESP_LOGI(TAG, "T26: CommandLDeactivateValet");
  // fallback to default implementation:
  return OvmsVehicle::CommandDeactivateValet(pin);
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandStartCharge()
{
  ESP_LOGI(TAG, "T26: CommandStartCharge");
  if (profile0_charge_current == 1) { // recover from autostop with current of 1A
    SetChargeCurrent(MyConfig.GetParamValueInt("xvu", "chg_climit", 16));
    ESP_LOGD(TAG, "T26: Setting charge current back to %d A...", MyConfig.GetParamValueInt("xvu", "chg_climit", 16));
    int ctr = 0;
    profile0_state = PROFILE0_REQUEST; // hacky but should not be a problem
    while (profile0_state != PROFILE0_IDLE) {
      ctr++;
      vTaskDelay(pdMS_TO_TICKS(250));
      if (ctr > 40) {
        ESP_LOGW(TAG, "T26: timeout waiting for charge current to be reset, continuing anyway...");
        profile0_state = PROFILE0_IDLE;
        break;
      }
    }
  }
  StartStopChargeT26(true);
  int ctr = 0;
  while (profile0_state != PROFILE0_IDLE) {
    ctr++;
    vTaskDelay(pdMS_TO_TICKS(250));
    if (ctr > 100) {
      ESP_LOGE(TAG, "T26: timeout waiting for CommandStartCharge to finish!");
      return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0...
    }
  }
  ESP_LOGD(TAG, "T26: profile0 start charge wait for success over");
  if (profile0_success)
    return Success;
  else
    return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0...
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandStopCharge()
{
  ESP_LOGI(TAG, "T26: CommandStopCharge");
  StartStopChargeT26(false);
  int ctr = 0;
  while (profile0_state != PROFILE0_IDLE) {
    ctr++;
    vTaskDelay(pdMS_TO_TICKS(250));
    if (ctr > 40) {
      ESP_LOGE(TAG, "T26: timeout waiting for CommandStopCharge to finish!");
      return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0...
    }
  }
  ESP_LOGD(TAG, "T26: profile0 stop charge wait for success over");
  if (profile0_success)
    return Success;
  else
    return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0...
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandHomelink(int button, int durationms)
{
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "T26: CommandHomelink button=%d durationms=%d", button, durationms);

  OvmsVehicle::vehicle_command_t res = NotImplemented;
  res = CommandClimateControl(button);

  // fallback to default implementation?
  if (res == NotImplemented) {
    res = OvmsVehicle::CommandHomelink(button, durationms);
  }
  return res;
}

void OvmsVehicleVWeUp::RequestProfile0()
{
  ESP_LOGD(TAG, "T26: RequestProfile0 called in state %d (key %d, val %d, mode %d, current %d, temp %d, recv %d)", profile0_state, profile0_key, profile0_val, profile0_mode, profile0_charge_current, profile0_cc_temp, profile0_recv);
  uint8_t length = 8;
  unsigned char data[length];
  canbus *comfBus;
  comfBus = m_can3;
  data[0] = 0x90;
  data[1] = 0x04; // length
  data[2] = 0x19; // PID 959
  data[3] = 0x59;
  data[4] = 0x27; // channel ID 10..1f or 20..2f for OCU writes; XXX should check if already in use!
  data[5] = 0x00; // all parts
  data[6] = 0x00; // start from index 0
  data[7] = 0x01; // request one profile
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
    ESP_LOGD(TAG, "T26: Profile0 requested");
  }
  else {
    ESP_LOGE(TAG, "T26: Profile0 request failed!");
  }
  ESP_LOGD(TAG, "T26: Starting profile0 request timer...");
  profile0_timer = xTimerCreate("VW e-Up Profile0 Retry", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
  xTimerStart(profile0_timer, 0);
}

void OvmsVehicleVWeUp::ReadProfile0(uint8_t *data)
{
  ESP_LOGD(TAG, "T26: ReadProfile0 called in state %d (cntr: %d,%d,%d, key %d, val %d, mode %d, current %d, temp %d, recv %d)", profile0_state, profile0_cntr[0], profile0_cntr[1], profile0_cntr[2], profile0_key, profile0_val, profile0_mode, profile0_charge_current, profile0_cc_temp, profile0_recv);
  for(int i = 0; i < 8; i++ )
  {
    profile0[profile0_idx+i] = data[i]; // append current received byte to profile0 array
  }
  profile0_idx += 8;
  if (profile0_idx == 8) {
    // stop any retry timer
    if (profile0_timer != NULL)
    {
      ESP_LOGD(TAG, "T26: Stopping profile0 timer...");
      xTimerStop(profile0_timer , 0);
      xTimerDelete(profile0_timer , 0);
      profile0_timer  = NULL;
    }
    if (profile0_state == PROFILE0_REQUEST) {
      profile0_cntr[1] = 0;
      profile0_state = PROFILE0_READ;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
    else if (profile0_state == PROFILE0_WRITE) { // now we need to check whether written and re-read profile are same
      profile0_cntr[2] = 0;
      profile0_state = PROFILE0_WRITEREAD;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
    ESP_LOGD(TAG, "T26: Starting profile0 read timer...");
    profile0_timer = xTimerCreate("VW e-Up Profile0 Retry", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
    xTimerStart(profile0_timer, 0);
  }
  else if (profile0_idx == profile0_len)
  {
    ESP_LOGD(TAG, "T26: ReadProfile0 complete");
    for (int i = 0; i < profile0_len; i+=8) 
      ESP_LOGD(TAG, "T26: Profile0: %02x %02x %02x %02x %02x %02x %02x %02x", profile0[i+0], profile0[i+1], profile0[i+2], profile0[i+3],profile0[i+4], profile0[i+5], profile0[i+6], profile0[i+7]);
    if (profile0_1sttime) { 
      for (int i = 0; i < profile0_len; i+=8)
        ESP_LOGI(TAG, "T26: Profile0: %02x %02x %02x %02x %02x %02x %02x %02x", profile0[i+0], profile0[i+1], profile0[i+2], profile0[i+3],profile0[i+4], profile0[i+5], profile0[i+6], profile0[i+7]);
      profile0_1sttime = false;
    }
    if(profile0[28] != 0 && profile0[29] != 0)
    {
      // stop any retry timer
    if (profile0_timer != NULL)
      {
        ESP_LOGD(TAG, "T26: Stopping profile0 timer...");
        xTimerStop(profile0_timer , 0);
        xTimerDelete(profile0_timer , 0);
        profile0_timer  = NULL;
      }
      profile0_mode = profile0[11];
      profile0_charge_current = profile0[13];
      StdMetrics.ms_v_charge_climit->SetValue(profile0_charge_current);
      profile0_cc_temp = (profile0[25]/10)+10;
      profile0_recv = false;
      ESP_LOGI(TAG, "T26: ReadProfile0: got mode: %d, charge_current: %d, cc_temp: %d", profile0_mode, profile0_charge_current, profile0_cc_temp);
      bool value_match = false;
      switch (profile0_key) {
        case 0:
          ESP_LOGD(TAG, "T26: Profile0 key=0, print profile0");
          for (int i = 0; i < profile0_len; i+=8)
            ESP_LOGI(TAG, "T26: Profile0: %02x %02x %02x %02x %02x %02x %02x %02x", profile0[i+0], profile0[i+1], profile0[i+2], profile0[i+3],profile0[i+4], profile0[i+5], profile0[i+6], profile0[i+7]);
          break;
        case 20:
          if (profile0_mode == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 mode value match");
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 mode value mismatch");
          }
          break;
        case 21:
          if (profile0_charge_current == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 charge current value match");
            profile0_charge_current_old = profile0_charge_current;
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 charge current value mismatch");
          }
          break;
        case 22:
          if (profile0_cc_temp == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 cc temperature value match");
            profile0_cc_temp_old = profile0_cc_temp;
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 cc temperature value mismatch");
          }
          break;
        default:
          ESP_LOGE(TAG, "T26: Profile0 write ERROR: key=%d!", profile0_key);
      }
      if (value_match) { 
        if (profile0_key == 20){ // we have a switch request, so try to switch now
          ESP_LOGI(TAG, "T26:Profile0 value matches setting, ready to activate");
          ActivateProfile0();
        }
        else {
          ESP_LOGI(TAG, "T26:Profile0 value matches setting, done!");
          profile0_state = PROFILE0_IDLE;
          ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
          profile0_key = 0;
          profile0_success = true;
        }
      }
      else if (profile0_state == PROFILE0_READ) {
        ESP_LOGD(TAG, "T26: Profile0: ready to write!");
        profile0_cntr[2] = 0;
        WriteProfile0();
      }
      else if (profile0_state == PROFILE0_WRITEREAD) {
        ESP_LOGD(TAG, "T26: Profile0 not written successfully, trying again...");
        WriteProfile0();
      }
//      }
      else {
        ESP_LOGD(TAG, "T26: ReadProfile0 this shouldn't happen! Stopping.");
        profile0_state = PROFILE0_IDLE;
        ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
        profile0_key = 0;
      }
    }
    else
    {
      ESP_LOGD(TAG, "T26: ReadProfile0 invalid data!");
      // empty profile, set state for existing timer to retry request
      profile0_cntr[0] = 0;
      profile0_state = PROFILE0_REQUEST;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
  }
}

void OvmsVehicleVWeUp::WriteProfile0()
{
  ESP_LOGD(TAG, "T26: WriteProfile0 called");
  if (vweup_enable_write && !dev_mode) {
    uint8_t length = 8;
    unsigned char data[length];
    canbus *comfBus;
    comfBus = m_can3;
    data[0] = 0x90; // XXX detect free channel! (always use channel 2 (0x90 / 0x0d headers) for now)
    data[1] = 0x20;
    data[2] = 0x29;
    data[3] = 0x59;
    data[4] = 0x2c;
    data[5] = 0x00;
    data[6] = 0x00;
    data[7] = 0x01;
    comfBus->WriteStandard(0x69E, length, data);
      
    data[0] = 0xd0;
    data[1] = (profile0_key == 20) ? profile0_val : profile0[11]; // key twenty to change mode byte in basic configuration (check Multiplex PID Document)
    data[2] = profile0[12];
    data[3] = (profile0_key == 21) ? profile0_val : profile0[13];// key twenty-one to change max current byte in basic configuration (check Multiplex PID Document)
    data[4] = profile0[14];
    data[5] = profile0[15];
    data[6] = profile0[17];
    data[7] = profile0[18];
    comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xd1;
    data[1] = profile0[19];
    data[2] = profile0[20];
    data[3] = profile0[21];
    data[4] = profile0[22];
    data[5] = profile0[23];
    data[6] = (profile0_key == 22) ? (profile0_val-10)*10 : profile0[25]; // key twenty-two to change climate contol temperature in basic configuration (check Multiplex PID Document)
    data[7] = profile0[26];
    comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xd2;
    data[1] = profile0[27];
    data[2] = profile0[28];
    data[3] = profile0[29];
    data[4] = profile0[30];
    data[5] = profile0[31];
    data[6] = profile0[33];
    data[7] = profile0[34];
    comfBus->WriteStandard(0x69E, length, data);

    data[0] = 0xd3;
    data[1] = profile0[35];
    data[2] = profile0[36];
    data[3] = profile0[37];
    data[4] = profile0[38];
    data[5] = profile0[39];
    data[6] = profile0[41];
    data[7] = profile0[42];
    comfBus->WriteStandard(0x69E, length, data);

    // start timer to check if profile was written correctly
    if (profile0_state != PROFILE0_WRITEREAD){
      profile0_state = PROFILE0_WRITE;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
    ESP_LOGD(TAG, "T26: Starting profile0 write timer...");
    profile0_timer = xTimerCreate("VW e-Up Profile0 Retry", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
    xTimerStart(profile0_timer, 0);
  }
  else {
    if (!vweup_enable_write) {
      ESP_LOGE(TAG, "T26: Profile0 can't be written, CAN write not enabled!");  
    }
    else {
      ESP_LOGI(TAG, "T26:Profile0 can't be written, turn off dev mode!");
    }
  }
}

void OvmsVehicleVWeUp::Profile0_Retry_Timer(TimerHandle_t timer)
{
  // Workaround for FreeRTOS duplicate timer callback bug
  // (see https://github.com/espressif/esp-idf/issues/8234)
  static TickType_t last_tick = 0;
  TickType_t tick = xTaskGetTickCount();
  if (tick < last_tick + xTimerGetPeriod(timer) - 3) return;
  last_tick = tick;

  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->Profile0_Retry_CallBack();
}

void OvmsVehicleVWeUp::Profile0_Retry_CallBack()
{
  ESP_LOGD(TAG, "T26: Profile0_Retry_Callback in state %d (cntr: %d,%d,%d, key %d, val %d, mode %d, current %d, temp %d, recv %d)", profile0_state, profile0_cntr[0], profile0_cntr[1], profile0_cntr[2], profile0_key, profile0_val, profile0_mode, profile0_charge_current, profile0_cc_temp, profile0_recv);
  if (profile0_timer != NULL)
  {
    ESP_LOGD(TAG, "T26: Stopping profile0 timer...");
    xTimerStop(profile0_timer , 0);
    xTimerDelete(profile0_timer , 0);
    profile0_timer  = NULL;
  }
  if (*max_element(profile0_cntr,profile0_cntr+3) >= profile0_retries) {
    ESP_LOGE(TAG, "T26: Profile0 max. retries exceeded!");
    profile0_state = PROFILE0_IDLE;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    if (profile0_key == 21)
      MyConfig.SetParamValueInt("xvu", "chg_climit", profile0_charge_current_old);
    else if (profile0_key == 22)
      MyConfig.SetParamValueInt("xvu", "cc_temp", profile0_cc_temp_old);
    profile0_key = 0;
  }
  else {
    switch (profile0_state){
      case PROFILE0_IDLE:
        ESP_LOGE(TAG, "T26: Profile0_Retry_Callback in state PROFILE0_IDLE shouldn't happen!");
        break;
      case PROFILE0_REQUEST:
        if (profile0_cntr[0] < profile0_retries) {
          profile0_cntr[0]++;
          ESP_LOGI(TAG, "T26:Profile0 request attempt %d...", profile0_cntr[0]);
          RequestProfile0();
        }
        break;
      case PROFILE0_READ:
        if (profile0_cntr[1] < profile0_retries) {
          profile0_cntr[1]++;
          ESP_LOGW(TAG,"Profile0 read failed! Retrying... %d", profile0_cntr[1]);
          profile0_cntr[0] = 0;
          RequestProfile0();
        }
        break;
      case PROFILE0_WRITE:
      case PROFILE0_WRITEREAD:
        if (profile0_cntr[2] < profile0_retries) {
          profile0_cntr[2]++;
          ESP_LOGI(TAG, "T26:Profile0 write attempt %d...", profile0_cntr[2]);
          profile0_cntr[0] = 0;
          RequestProfile0();
        }
        break;
      case PROFILE0_SWITCH:
        if (profile0_cntr[0] < profile0_retries) {
          profile0_cntr[0]++;
          ESP_LOGD(TAG, "T26: Profile0 switch attempt %d...", profile0_cntr[0]);
          if (profile0_val == 1){ // charge start/stop
            ESP_LOGD(TAG, "T26: Profile0_Retry_Callback to start/stop charge");
            if (profile0_activate == StandardMetrics.ms_v_charge_inprogress->AsBool()) {
              ESP_LOGI(TAG, "T26:Profile0 charge successfully turned %s",(profile0_activate)? "ON" : "OFF");
              profile0_state = PROFILE0_IDLE;
              ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
              profile0_key = 0;          
              profile0_success = true;
            }
            else {
              ESP_LOGI(TAG, "T26:Profile0 charge %s attempt %d...", (profile0_activate)? "start" : "stop", profile0_cntr[0]);
              ActivateProfile0();
            }
          }
          else if ((profile0_val && 2) || (profile0_val && 4)) { // climatecontrol on/off
            ESP_LOGD(TAG, "T26: Profile0_Retry_Callback to start/stop climate control");
            if (profile0_cntr[0] == 5 && !MyConfig.GetParamValueBool("xvu", "cc_onbat"))
              ESP_LOGW(TAG, "Climatecontrol on battery disabled, is the car plugged in?");
            if (profile0_activate == StandardMetrics.ms_v_env_hvac->AsBool()) {
              ESP_LOGI(TAG, "T26:Profile0 climatecontrol successfully turned %s",(profile0_activate)? "ON" : "OFF");
              vweup_cc_on = profile0_activate;
              profile0_state = PROFILE0_IDLE;
              ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
              profile0_key = 0;
              profile0_success = true;  
            }
            else {
              ESP_LOGI(TAG, "T26:Profile0 climatecontrol %s attempt %d...", (profile0_activate)? "start" : "stop", profile0_cntr[0]);
              ActivateProfile0();
            }
          }
          else {
            ESP_LOGE(TAG, "T26: Profile0_Retry_Callback ERROR: key=%d and val=%d!", profile0_key, profile0_val);
          }
        }
        break;
      default:
        ESP_LOGE(TAG, "T26: Profile0 Retry Callback ERROR: profile0_state=%d!", profile0_state);
        profile0_state = PROFILE0_IDLE;
        ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
        profile0_key = 0;
    }
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandSetChargeCurrent(uint16_t climit)
{
  ESP_LOGI(TAG, "T26: CommandSetChargeCurrent called with %dA", climit);
  MyConfig.SetParamValueInt("xvu", "chg_climit", climit);
  profile0_state = PROFILE0_REQUEST; // hacky but should not be a problem
  int ctr = 0;
  while (profile0_state != PROFILE0_IDLE) {
    ctr++;
    vTaskDelay(pdMS_TO_TICKS(250));
    if (ctr > 40) {
      ESP_LOGE(TAG, "T26: timeout waiting for CommandSetChargeCurrent to finish!");
      profile0_state = PROFILE0_IDLE;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
      return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0!
    }
  }
  ESP_LOGD(TAG, "T26: profile0 set charge current wait for success over");
  if (profile0_success)
    return Success;
  else
    return Fail; // XXX set xvu value back to previous w/o triggering another write to profile0!
}

void OvmsVehicleVWeUp::SetChargeCurrent(uint16_t climit)
{
  ESP_LOGI(TAG, "T26:Charge current changed from %d to %d", profile0_charge_current, climit);
  if (!IsT26Ready()) {
    ESP_LOGE(TAG, "T26: SetChargeCurrent: T26 not ready");
    return;
  }
  if (profile0_charge_current == climit)
  {
    ESP_LOGD(TAG, "T26: No change in charge current, ignoring request");
    profile0_state = PROFILE0_IDLE;
    return;
  } 
  profile0_1sttime = true;
  profile0_success = false;
  CommandWakeup(); // wakeup vehicle to enable communication with all ECUs
  profile0_key = 21;
  profile0_val = climit;
  memset(profile0_cntr, 0, sizeof(profile0_cntr));
  profile0_state = PROFILE0_REQUEST;
  ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
  profile0_timer = xTimerCreate("VW e-Up Profile0", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
  xTimerStart(profile0_timer, 0); // Charge current will be set after timer runs RequestProfile0
} 

bool OvmsVehicleVWeUp::StartStopChargeT26(bool chargestart)
{
  if (chargestart != StandardMetrics.ms_v_charge_inprogress->AsBool()) {
    if (HasNoT26()) {
        ESP_LOGE(TAG, "T26: Charge command can only be used with comfort CAN connection (T26)!");
        return Fail;
    }
    else if (!IsT26Ready()) {
        ESP_LOGE(TAG, "T26: Charge command ERROR: T26 not ready!");
        return Fail;
    }
    ESP_LOGI(TAG, "T26: Charge turning %s", chargestart ? "ON" : "OFF");
    profile0_1sttime = true;
    profile0_success = false;
    CommandWakeup(); // wakeup vehicle to enable communication with all ECUs
    profile0_activate = chargestart;
    profile0_key = 20;
    profile0_val = 1;
    memset(profile0_cntr, 0, sizeof(profile0_cntr));
    ESP_LOGD(TAG, "T26: cntr: %d, %d, %d", profile0_cntr[0], profile0_cntr[1], profile0_cntr[2]);
    profile0_state = PROFILE0_REQUEST;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    profile0_timer = xTimerCreate("VW e-Up Profile0", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
    xTimerStart(profile0_timer, 0); // Start/stop charge will be called after timer runs RequestProfile0
    return Success;
  }
  else {
    ESP_LOGW(TAG, "T26: Charging already %s, no change necessary", chargestart ? "on" : "off");
    return Fail;
  }
}

void OvmsVehicleVWeUp::CCTempSet(uint16_t temperature) //to send the can message to set the configured cabin temperature
{
  ESP_LOGI(TAG, "T26:Climatecontrol temperature changed from %d to %d", profile0_cc_temp, temperature);
  if (!IsT26Ready()) {
    ESP_LOGE(TAG, "T26: CCTempSet: T26 not ready");
    return;
  }
  if (profile0_cc_temp == temperature)
  {
    ESP_LOGD(TAG, "T26: No change in temperature, ignoring request");
    return;
  }
  profile0_1sttime = true;
  CommandWakeup(); // wakeup vehicle to enable communication with all ECUs
  profile0_key = 22;
  profile0_val = temperature;
  memset(profile0_cntr, 0, sizeof(profile0_cntr));
  profile0_state = PROFILE0_REQUEST;
  ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
  profile0_timer = xTimerCreate("VW e-Up Profile0", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
  xTimerStart(profile0_timer, 0); // Temperature will be set after timer runs RequestProfile0
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandClimateControl(bool climatecontrolon)
{
  if (climatecontrolon != StandardMetrics.ms_v_env_hvac->AsBool() && profile0_state == PROFILE0_IDLE) 
  {
    if (HasNoT26()) {
        ESP_LOGE(TAG, "T26: ClimateControl can only be used with comfort CAN connection (T26)!");
        return Fail;
    }
    else if (!IsT26Ready()) {
        ESP_LOGE(TAG, "T26: CommandClimateControl ERROR: T26 not ready!");
        return Fail;
    }
    if (climatecontrolon && !MyConfig.GetParamValueBool("xvu", "cc_onbat"))
      ESP_LOGW(TAG, "Climatecontrol on battery disabled, may not start... (plug state detection not implemented yet)");
/*    {
      if (HasNoOBD())
        ESP_LOGW(TAG, "No OBD connection, can't check if car is plugged! Starting climatecontrol even though CC on battery is disabled...");
      else if (StdMetrics.ms_v_charge_voltage->AsFloat() < 200) {
          ESP_LOGE(TAG, "T26: Climatecontrol on battery disabled and unplugged, can't start!");
          return Fail;
      }
    }*/
    ESP_LOGI(TAG, "T26: CommandClimateControl turning %s", climatecontrolon ? "ON" : "OFF");
    profile0_1sttime = true;
    CommandWakeup(); // wakeup vehicle to enable communication with all ECUs
    profile0_activate = climatecontrolon;
    profile0_key = 20;
    profile0_val = MyConfig.GetParamValueBool("xvu", "cc_onbat")? 6 : 2;
    memset(profile0_cntr, 0, sizeof(profile0_cntr));
    profile0_state = PROFILE0_REQUEST;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    profile0_timer = xTimerCreate("VW e-Up Profile0", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
    xTimerStart(profile0_timer, 0); // ClimateControl will be called after timer runs RequestProfile0
  return Success;
  }
  else if (profile0_state != PROFILE0_IDLE)
    ESP_LOGE(TAG, "T26: Climatecontrol already starting...");
  else
    ESP_LOGE(TAG, "T26: Climatecontrol already %s!",climatecontrolon? "on" : "off");
  return Fail;
}

void OvmsVehicleVWeUp::ActivateProfile0() // only sends on/off command, profile0[11] has to be set correctly before!
{
  uint8_t length = 4;
  unsigned char data[length];
  canbus *comfBus;
  comfBus = m_can3;
  data[0] = 0x29;
  data[1] = 0x58;
  data[2] = 0x00;
  data[3] = profile0_activate;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }
  if (profile0_state != PROFILE0_SWITCH) {
    profile0_cntr[0] = 0;
    profile0_state = PROFILE0_SWITCH;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
  }
  ESP_LOGD(TAG, "T26: Starting profile0 activate timer...");
  profile0_timer = xTimerCreate("VW e-Up Profile0 Retry", pdMS_TO_TICKS(4*profile0_delay), pdFALSE, this, Profile0_Retry_Timer);
  xTimerStart(profile0_timer, 0);
}
