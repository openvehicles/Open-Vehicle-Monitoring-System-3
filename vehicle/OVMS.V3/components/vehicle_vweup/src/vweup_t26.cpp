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
;    0.5.0 try to fix CAN awake/asleep loop (sharkcow)

;    0.6.0 complete rework of climatecontrol, added charge control by Urwa Khattak & sharkcow
;
;    0.6.1 rework of CommandWakeup, wait for success, add charge start/stop workaround, remove dev_mode
;
;    0.6.2 add switch for charge workaround, chargeport detection (OBD), retries for wakeup
;
;    0.6.3 read profile0 on boot
;
;    (C) 2021       Chris van der Meijden
;        2024       Urwa Khattak, sharkcow      
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
  profile0_cc_onbat = false;
  profile0_charge_current_old = 0;
  profile0_cc_temp_old = 0;
  profile0_delay = 1500;
  profile0_retries = 5;
  memset(profile0_cntr, 0, sizeof(profile0_cntr)); // 3 counters to keep track of retries in different states of profile0 state machine
  profile0_state = PROFILE0_IDLE;
  profile0_key = P0_KEY_NOP;
  profile0_val = 0;
  profile0_activate = false;
  xWakeSemaphore = xSemaphoreCreateBinary();
  xChargeSemaphore = xSemaphoreCreateBinary();
  xCurrentSemaphore = xSemaphoreCreateBinary();
  ocu_response = false;
  fakestop = false;
  climit_max = (vweup_modelyear > 2019)? 32 : 16;
  chg_workaround = MyConfig.GetParamValueBool("xvu","chg_workaround");
  wakeup_success = false;
  charge_timeout = false;
  lever = 0x20; // assume position "P" on startup so climatecontrol works
  lever0_cnt = 0;
  p_problem = false;
  chargestartstop = false;

  StdMetrics.ms_v_env_locked->SetValue(true);
  StdMetrics.ms_v_env_headlights->SetValue(false);
  StdMetrics.ms_v_env_charging12v->SetValue(false);
  StdMetrics.ms_v_env_awake->SetValue(false);
  StdMetrics.ms_v_env_aux12v->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);

  if (HasNoOBD()) {
    StdMetrics.ms_v_charge_mode->SetValue("standard");
  }

  // create OCU heartbeat timer:
  if (!m_sendOcuHeartbeat) {
    m_sendOcuHeartbeat = xTimerCreate("VW e-Up OCU heartbeat", pdMS_TO_TICKS(1000), pdTRUE, this, sendOcuHeartbeat);
  }

  // update values from ECU:
  t26_init = true;
  profile0_state = PROFILE0_INIT;
  if (!profile0_timer) {
    profile0_timer = xTimerCreate("VW e-Up Profile0 Retry", pdMS_TO_TICKS(profile0_delay), pdFALSE, this, Profile0RetryTimer);
  }
  xTimerStart(profile0_timer, 0);
}

void OvmsVehicleVWeUp::T26Ticker1(uint32_t ticker)
{
  ESP_LOGV(TAG, "T26: Ticker1: t26_ring_awake: %d, ms_v_bat_12v_voltage: %0.2f, ms_v_env_charging12v: %d, t26_12v_boost_cnt: %d, t26_12v_boost_last_cnt: %d, t26_12v_wait_off: %d", t26_ring_awake, StdMetrics.ms_v_bat_12v_voltage->AsFloat(), StdMetrics.ms_v_env_charging12v->AsBool(), t26_12v_boost_cnt, t26_12v_boost_last_cnt, t26_12v_wait_off);
  if (StdMetrics.ms_v_bat_12v_voltage->AsFloat() < 13 && !t26_ring_awake && StdMetrics.ms_v_env_charging12v->AsBool()) {
    // Wait for 12v voltage to come up to 13.2v while getting a boost:
    t26_12v_boost_cnt++;
    if (t26_12v_boost_cnt > 20) {
      ESP_LOGI(TAG, "T26: Car stopped itself charging the 12v battery");
      StdMetrics.ms_v_env_charging12v->SetValue(false);
      StdMetrics.ms_v_env_aux12v->SetValue(false);
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
  if (turnOn && !StdMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned on
    ESP_LOGI(TAG, "T26: CAR IS ON");
    StdMetrics.ms_v_env_on->SetValue(true);
    if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
      t26_12v_boost_cnt = 0;
      t26_12v_wait_off = 0;
      SetUsePhase(UP_Driving);
      if (HasNoOBD())
        StdMetrics.ms_v_door_chargeport->SetValue(false);
      PollSetState(VWEUP_ON);
    } else {
      PollSetState(VWEUP_CHARGING);
    }
    ResetTripCounters();
    if (ocu_awake && m_sendOcuHeartbeat != NULL) {
      ESP_LOGD(TAG, "T26: stopping OCU heartbeat timer");
      xTimerStop(m_sendOcuHeartbeat, pdMS_TO_TICKS(100));
    }
    ocu_awake = false;
    ocu_working = false;
    ocu_response = false;
    fas_counter_on = 0;
    fas_counter_off = 0;
    t26_car_on = true;
    StdMetrics.ms_v_env_charging12v->SetValue(true);
    StdMetrics.ms_v_env_aux12v->SetValue(true);
  }
  else if (!turnOn && StdMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG, "T26: CAR IS OFF");
    t26_car_on = false;
    t26_12v_boost = false;
    StdMetrics.ms_v_env_on->SetValue(false);
    // StdMetrics.ms_v_charge_voltage->SetValue(0);
    // StdMetrics.ms_v_charge_current->SetValue(0);
    PollSetState(StdMetrics.ms_v_charge_inprogress->AsBool()? VWEUP_CHARGING : VWEUP_AWAKE);
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

    case 0x61A: // SOC (byte 7) & drive mode lever position (byte 3): P: 0x20, R: 0x30, N: 0x40, D: 0x50, B: 0x60
      // If available, OBD is normally responsible for the SOC, but K-CAN occasionally
      // sends SOC updates while OBD is in state OFF:
      if (HasNoOBD() || IsOff()) {
        StdMetrics.ms_v_bat_soc->SetValue(d[7] / 2.0);
      }
      if (HasNoOBD()) {
        if (vweup_modelyear >= 2020) {
          StdMetrics.ms_v_bat_range_ideal->SetValue((260 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
        else {
          StdMetrics.ms_v_bat_range_ideal->SetValue((160 * (d[7] / 2.0)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
      }
      if (lever != d[3]) {
        ESP_LOGI(TAG, "Drive mode lever switched to 0x%02x", d[3]);
        lever = d[3];
        if (lever != 0) {
          lever0_cnt = 0;
          p_problem = false;
        }
      }
      if (lever == 0) { // detect "P-problem": when 12V battery is detected as defective, lever byte sometimes returns 0 when car is off. Then charge can't start since lever has to be in P position (0x20) for that...
        if (lever0_cnt < 10) // wait for 10 occurences to suppress ghost triggering
          lever0_cnt++;
        else if (lever0_cnt == 10) {
          lever0_cnt++;
          if (HasOBD() && StdMetrics.ms_v_door_chargeport->AsBool()) {
            ESP_LOGE(TAG, "T26: invalid selector lever value, charging impossible! Microswitch possibly defective?");
            MyNotify.NotifyStringf("alert", "Drive Mode Selector", "Invalid selector lever value, charging impossible! Microswitch possibly defective?");
          }
          p_problem = true;
        }
      }
      break;

    case 0x52D: // KM range left (estimated).
      if (d[0] != 0xFE) {
        if (d[1] == 0x41) {
          StdMetrics.ms_v_bat_range_est->SetValue(d[0] + 255);
        }
        else {
          StdMetrics.ms_v_bat_range_est->SetValue(d[0]);
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
            StdMetrics.ms_v_vin->SetValue((string)m_vin);
          }
          break;
      }
      break;

    case 0x65D: { // ODO
      float odo = (float) (((uint32_t)(d[3] & 0xf) << 16) | ((UINT)d[2] << 8) | d[1]);
      StdMetrics.ms_v_pos_odometer->SetValue(odo);
      break;
    }

    case 0x320: // Speed
      StdMetrics.ms_v_pos_speed->SetValue(((d[4] << 8) + d[3] - 1) / 190);
      UpdateTripOdo();
      if (HasNoOBD())
        CalculateAcceleration(); // only necessary until we find acceleration on T26
      break;

    case 0x527: // Outdoor temperature
      StdMetrics.ms_v_env_temp->SetValue((d[5] - 100) / 2);
      break;

    case 0x381: // Vehicle locked
      if (d[0] == 0x02) {
        StdMetrics.ms_v_env_locked->SetValue(true);
      }
      else {
        StdMetrics.ms_v_env_locked->SetValue(false);
      }
      break;

    case 0x3E3: // Cabin temperature
      if (d[2] != 0xFF) {
        StdMetrics.ms_v_env_cabintemp->SetValue((d[2] - 100) / 2);
        // Set PEM inv temp to support older app version with cabin temp workaround display
        // StdMetrics.ms_v_inv_temp->SetValue((d[2] - 100) / 2);
      }
      break;

    case 0x470: // Doors
      StdMetrics.ms_v_door_fl->SetValue((d[1] & 0x01) > 0);
      StdMetrics.ms_v_door_fr->SetValue((d[1] & 0x02) > 0);
      StdMetrics.ms_v_door_rl->SetValue((d[1] & 0x04) > 0);
      StdMetrics.ms_v_door_rr->SetValue((d[1] & 0x08) > 0);
      StdMetrics.ms_v_door_trunk->SetValue((d[1] & 0x20) > 0);
      StdMetrics.ms_v_door_hood->SetValue((d[1] & 0x10) > 0);
      break;

    case 0x531: // Head lights
      if (d[0] > 0) {
        StdMetrics.ms_v_env_headlights->SetValue(true);
      }
      else {
        StdMetrics.ms_v_env_headlights->SetValue(false);
      }
      break;

    // Check for running hvac.
    case 0x3E1:
      if (d[4] > 0) {
        StdMetrics.ms_v_env_hvac->SetValue(true);
      }
      else {
        StdMetrics.ms_v_env_hvac->SetValue(false);
      }
      break;

    case 0x571: // 12 Volt
      StdMetrics.ms_v_charge_12v_voltage->SetValue(5 + (0.05 * d[0]));
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
          if (HasNoOBD())
            StdMetrics.ms_v_door_chargeport->SetValue(true);
          StdMetrics.ms_v_charge_pilot->SetValue(true);
          SetChargeState(true);
          StdMetrics.ms_v_env_charging12v->SetValue(true);
          StdMetrics.ms_v_env_aux12v->SetValue(true);
          ESP_LOGI(TAG, "T26: Car charge session started");
          t26_12v_wait_off = 0;
          t26_12v_boost_cnt = 0;
          PollSetState(VWEUP_CHARGING);
        }
        if (!isCharging && cd_count == 3) {
          cd_count = 0;
          StdMetrics.ms_v_charge_pilot->SetValue(false);
          SetChargeState(false);
          if (StdMetrics.ms_v_env_on->AsBool()) {
            if (HasNoOBD())
              StdMetrics.ms_v_door_chargeport->SetValue(false);
            SetUsePhase(UP_Driving);
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
          StdMetrics.ms_v_env_awake->SetValue(false);
          break;
        case 0x01: // Key in position 1, no ignition
          StdMetrics.ms_v_env_awake->SetValue(true);
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
      if (d[0] == 0x00 && !ocu_awake && !StdMetrics.ms_v_charge_inprogress->AsBool() && !t26_12v_boost && !t26_car_on && d[1] != 0x31 && t26_12v_wait_off == 0) {
        // The car wakes up to charge the 12v battery 
        StdMetrics.ms_v_env_charging12v->SetValue(true);
        StdMetrics.ms_v_env_aux12v->SetValue(true);
        t26_ring_awake = true;
        t26_12v_boost = true;
        t26_12v_boost_cnt = 0;
        PollSetState(VWEUP_AWAKE);
        ESP_LOGI(TAG, "T26: Car woke up. Will try to charge 12v battery");
      }
      if (d[1] == 0x31) {
        if (m_sendOcuHeartbeat != NULL) {
          ESP_LOGD(TAG, "T26: stopping OCU heartbeat timer");
          xTimerStop(m_sendOcuHeartbeat, pdMS_TO_TICKS(100));
        }
        if (ocu_awake) { // We should go to sleep, no matter what
          ESP_LOGI(TAG, "T26: Comfort CAN calls for sleep");
          ocu_awake = false;
          ocu_working = false;
          ocu_response = false;
          fas_counter_on = 0;
          fas_counter_off = 0;
          t26_12v_boost = false;
          if (StdMetrics.ms_v_charge_inprogress->AsBool()) {
            PollSetState(VWEUP_CHARGING);
          } else {
            t26_ring_awake = false;
            ocu_response = false;
            PollSetState(VWEUP_AWAKE);
          }
          break;
        }
        else if (t26_ring_awake) {
          t26_ring_awake = false;
          ocu_response = false;
          ESP_LOGI(TAG, "T26: Ring asleep");
        }
      }
      if (d[0] == 0x00 && d[1] != 0x31 && !t26_ring_awake) {
        t26_ring_awake = true;
        ESP_LOGI(TAG, "T26: Ring awake");
        if (t26_12v_wait_off != 0) {
          ESP_LOGI(TAG, "T26: OBD AWAKE is still blocked");
        }
      }
      if (d[0] == 0x1D) {
        // We are called in the ring
        canbus *comfBus;
        comfBus = m_can3;
        uint8_t length = 8;
        unsigned char data[length];
        data[0] = 0x00; // As it seems that we are always the highest in the ring we always send to 0x400
        ESP_LOGV(TAG, "OCU called in ring");
        if (ocu_working) {
          data[1] = 0x01;
          ESP_LOGV(TAG, "T26: OCU working");
        }
        else {
          data[1] = 0x11; // Ready to sleep. This is very important!
          ESP_LOGV(TAG, "T26: OCU ready to sleep");
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
        if (comfBus && HasT26() && IsT26Ready() && vweup_enable_write) {
          ESP_LOGV(TAG, "OCU answers: byte1 = %02x", data[1]);
          comfBus->WriteStandard(0x43D, length, data);  // We answer
        }
        else
          ESP_LOGE(TAG, "T26: Can't answer to ring request! (HasT26: %d, T26ready: %d, enable_write: %d)", HasT26(), IsT26Ready(), vweup_enable_write);
        ocu_awake = true;
        break;
      }
      break;

    case 0x69C:
      if (!ocu_response) {
        ESP_LOGD(TAG, "T26: 69C received, switching ocu_response to true");
        ocu_response = true;
      }
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
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: RemoteCommandHandler failed: T26 not ready / no write access!");
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
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: SendCommand %d failed: T26 not ready / no write access!", command);
    return;
  }
  switch (command) {
    default:
      signal_ok = true;
      return;
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandWakeup()
{
  ESP_LOGD(TAG, "T26: CommandWakeup called");
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CommandWakeup failed: T26 not ready / no write access!");
    return Fail;
  }
  profile0_key = P0_KEY_NOP;
  xSemaphoreTake(xWakeSemaphore, 0);
  ESP_LOGD(TAG, "T26: CommandWakeup calling WakeupT26");
  WakeupT26();
  if (xSemaphoreTake(xWakeSemaphore, pdMS_TO_TICKS(2*profile0_retries*profile0_delay)))
  {
    ESP_LOGD(TAG, "T26: CommandWakeup wait for success over");
    xSemaphoreGive(xWakeSemaphore);
    if (wakeup_success)
      return Success;
    else
      return Fail;
  }
  else
  {
    ESP_LOGE(TAG, "T26: timeout waiting for CommandWakeup to finish!");
    xSemaphoreGive(xWakeSemaphore);
    return Fail;
  }
}

// Wakeup implementation over the VW ring commands
// We need to register in the ring with a call to ourself from 43D with 0x1D in the first byte
//
void OvmsVehicleVWeUp::WakeupT26()
{
  ESP_LOGD(TAG, "T26: WakeupT26 called");
  if (profile0_state != PROFILE0_IDLE && profile0_state != PROFILE0_INIT) {
    ESP_LOGE(TAG, "T26: Profile0 command %d already running, aborting wakeup!", profile0_state);
    wakeup_success = false;
    if (xWakeSemaphore != NULL)
      xSemaphoreGive(xWakeSemaphore);
    return;
  }
  memset(profile0_cntr, 0, sizeof(profile0_cntr));
  WakeupT26Stage2();  
}

void OvmsVehicleVWeUp::WakeupT26Stage2()
{
  ESP_LOGD(TAG, "T26: WakeupT26Stage2 called");
  canbus *comfBus;
  comfBus = m_can3;

  if (!comfBus || HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: WakeupT26Stage2 failed: T26 not ready / no write access!");
    wakeup_success = false;
    if (xWakeSemaphore != NULL)
      xSemaphoreGive(xWakeSemaphore);
    return;
  }

  ESP_LOGI(TAG, "T26: Waking Comfort CAN");
  if (!ocu_response && !StdMetrics.ms_v_env_on->AsBool()) {
    unsigned char data[8];
    uint8_t length = 2;
    // We send a request on 69E for the current time (PID 451)
    // This wakes 0x400, we then call ourself in the ring
    data[0] = 0x14;
    data[1] = 0x51;
    comfBus->WriteStandard(0x69E, length, data);
    length = 8;
    data[0] = 0x1D; // We call ourself to wake up the ring
    data[1] = 0x02;
    data[2] = 0x02;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    comfBus->WriteStandard(0x43D, length, data);
    vTaskDelay(pdMS_TO_TICKS(50));
    data[0] = 0x00; // We need to talk to 0x400 first to get accepted in the ring
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x04; // This could be a problem
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    comfBus->WriteStandard(0x43D, length, data);

    // start sending OCU heartbeat:
    if (xTimerStart(m_sendOcuHeartbeat, pdMS_TO_TICKS(100)) == pdFAIL) {
      ESP_LOGE(TAG, "T26: WakeupT26Stage2: could not start OCU heartbeat, please retry wakeup");
    } else {
      // timer running, send first heartbeat:
      SendOcuHeartbeat();
    }

    ocu_working = true; // XXX put all of this in WakeupT26Stage3?
    ocu_awake = true;
    StdMetrics.ms_v_env_charging12v->SetValue(true);
    StdMetrics.ms_v_env_aux12v->SetValue(true);
    t26_12v_boost_cnt = 0;
    t26_12v_wait_off = 0;
    PollSetState(StdMetrics.ms_v_charge_inprogress->AsBool()? VWEUP_CHARGING : VWEUP_AWAKE);
    profile0_state = PROFILE0_WAKE;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    if (ocu_response)
      WakeupT26Stage3();
    else {
      xTimerStart(profile0_timer, 0);
    }
  }
  else {
    ESP_LOGD(TAG, "T26: Already awake, skipping wakeup command (response: %d, ocu: %d, on: %d)", ocu_response, ocu_awake, StdMetrics.ms_v_env_on->AsBool());
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    WakeupT26Stage3();
  }
}

void OvmsVehicleVWeUp::WakeupT26Stage3()
{
  ESP_LOGD(TAG, "T26: WakeupT26Stage3 called");
  if (profile0_key != P0_KEY_NOP) {
      memset(profile0_cntr, 0, sizeof(profile0_cntr));
      profile0_state = PROFILE0_REQUEST;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
      xTimerStart(profile0_timer, 0);
    }
  else
    profile0_state = PROFILE0_IDLE;
  if (xWakeSemaphore != NULL) {
    wakeup_success = true;
    xSemaphoreGive(xWakeSemaphore);
  }
  ESP_LOGI(TAG, "T26: Vehicle has been woken");
}


void OvmsVehicleVWeUp::sendOcuHeartbeat(TimerHandle_t timer)
{
  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->SendOcuHeartbeat();
}

void OvmsVehicleVWeUp::SendOcuHeartbeat()
{
  ESP_LOGV(TAG, "OCU heartbeat called (cc: %d, fas_on: %d, fas_off: %d, p0: %d)", vweup_cc_on, fas_counter_on, fas_counter_off, profile0_state);
  canbus *comfBus;
  comfBus = m_can3;
  uint8_t length = 8;
  unsigned char data[length];

  if (!comfBus || HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: SendOcuHeartbeat failed: T26 not ready / no write access!");
    // stop our timer:
    xTimerStop(m_sendOcuHeartbeat, 0);
    return;
  }
  else if (!vweup_cc_on) {
    fas_counter_on = 0;
    if (fas_counter_off < 10 && profile0_state == PROFILE0_IDLE) {
      fas_counter_off++;
      ocu_working = true;
      ESP_LOGV(TAG, "T26: OCU working");
    }
    else {
      if (profile0_state != PROFILE0_IDLE) {
        ocu_working = true;
        ESP_LOGV(TAG, "T26: OCU working");
        if (fas_counter_on < 10)
          fas_counter_on++;
      }
      else {
        ocu_working = false;
        ESP_LOGV(TAG, "T26: OCU ready to sleep");
      }
    }
  }
  else {
    fas_counter_off = 0;
    ocu_working = true;
    ESP_LOGV(TAG, "T26: OCU working");
    if (fas_counter_on < 10)
      fas_counter_on++;
  }

  ESP_LOGV(TAG, "sending OCU heartbeat (cc: %d, fas_on: %d, fas_off: %d, p0: %d)", vweup_cc_on, fas_counter_on, fas_counter_off, profile0_state);
  data[0] = 0x00;
  data[1] = 0x00;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  comfBus->WriteStandard(0x5A9, length, data);

  if ((fas_counter_on > 0 && fas_counter_on < 10) || (fas_counter_off > 0 && fas_counter_off < 10)) {
    data[0] = 0x60;
    ESP_LOGV(TAG, "T26: OCU Heartbeat - 5A7 60");
  }
  else {
    data[0] = 0x00;
    ESP_LOGV(TAG, "T26: OCU Heartbeat - 5A7 00");
  }
  data[1] = 0x16;
  comfBus->WriteStandard(0x5A7, length, data);
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
  ESP_LOGD(TAG, "T26: CommandStartCharge called");
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CommandStartCharge failed: T26 not ready / no write access!");
    chargestartstop = false;
    return Fail;
  }
  else if (StdMetrics.ms_v_charge_inprogress->AsBool()) {
    ESP_LOGE(TAG, "Vehicle is already charging!");
    chargestartstop = false;
    return Fail;
  }
  else if (!StdMetrics.ms_v_door_chargeport->AsBool()) {
    ESP_LOGE(TAG, "T26: Vehicle is not plugged!");
    if (xChargeSemaphore != NULL)
      xSemaphoreGive(xChargeSemaphore);
    chargestartstop = false;
    return Fail;
  }
  else if (p_problem) {
    ESP_LOGE(TAG, "T26: invalid selector lever value, can't start charge! Microswitch possibly defective?");
    MyNotify.NotifyStringf("alert", "Drive Mode Selector", "Invalid selector lever value, can't start charge! Microswitch possibly defective?");
    p_problem = false;
    chargestartstop = false;
    return Fail;
  }
  fakestop = false; // reset possible workaround charge stop
  xSemaphoreTake(xChargeSemaphore, 0);
  StartStopChargeT26(true);
  if (xSemaphoreTake(xChargeSemaphore, pdMS_TO_TICKS(3*profile0_retries*profile0_delay)))
  {
    ESP_LOGD(TAG, "T26: CommandStartCharge wait for success over");
    xSemaphoreGive(xChargeSemaphore);
    if (charge_timeout) {
      charge_timeout = false;
      return Fail;
    }
    else {
      chargestartstop = false;
      return Success;
    }
  }
  else
  {
    ESP_LOGE(TAG, "T26: timeout waiting for CommandStartCharge to finish!");
    xSemaphoreGive(xChargeSemaphore);
    chargestartstop = false;
    return Fail;
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandStopCharge()
{
  ESP_LOGD(TAG, "T26: CommandStopCharge called");
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CommandStopCharge failed: T26 not ready / no write access!");
    chargestartstop = false;
    return Fail;
  }
  else if (!StdMetrics.ms_v_charge_inprogress->AsBool()) {
    ESP_LOGE(TAG, "Vehicle is not charging!");
    chargestartstop = false;
    return Fail;
  }
  else if (StdMetrics.ms_v_env_on->AsBool() && !chg_workaround) {
    ESP_LOGE(TAG, "Vehicle is on, can't stop charge without workaround!");
    MyNotify.NotifyStringf("alert", "Charge control", "Vehicle is on, can't stop charge without workaround!");
    chargestartstop = false;
    return Fail;
  }
  xSemaphoreTake(xChargeSemaphore, 0);
  StartStopChargeT26(false);
  if (xSemaphoreTake(xChargeSemaphore, pdMS_TO_TICKS(2*profile0_retries*profile0_delay)))
  {
    ESP_LOGD(TAG, "T26: CommandStopCharge wait for success over");
    xSemaphoreGive(xChargeSemaphore);
    chargestartstop = false;
    return Success;
  }
  else
  {
    ESP_LOGE(TAG, "T26: timeout waiting for CommandStopCharge to finish!");
    xSemaphoreGive(xChargeSemaphore);
    chargestartstop = false;
    return Fail;
  }
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
  canbus *comfBus;
  comfBus = m_can3;
  if (!comfBus || HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: RequestProfile0 failed: T26 not ready / no write access!");
    return;
  }
  uint8_t length = 8;
  unsigned char data[length];
  data[0] = 0x90;
  data[1] = 0x04; // length
  data[2] = 0x19; // 1: read, PID 959
  data[3] = 0x59;
  data[4] = 0x27; // channel ID 10..1f or 20..2f for OCU writes; XXX should check if already in use!
  data[5] = 0x00; // all parts
  data[6] = 0x00; // start from index 0
  data[7] = 0x01; // request one profile
  comfBus->WriteStandard(0x69E, length, data);
  ESP_LOGD(TAG, "T26: Starting profile0 request timer...");
  xTimerStart(profile0_timer, 0);
}

void OvmsVehicleVWeUp::ReadProfile0(uint8_t *data)
{
  ESP_LOGD(TAG, "T26: ReadProfile0 called in state %d (cntr: %d,%d,%d, key %d, val %d, mode %d, current %d, temp %d, recv %d)", profile0_state, profile0_cntr[0], profile0_cntr[1], profile0_cntr[2], profile0_key, profile0_val, profile0_mode, profile0_charge_current, profile0_cc_temp, profile0_recv);
  for (uint8_t i = 0; i < 8; i++)
  {
    profile0[profile0_idx+i] = data[i];
  }
  profile0_idx += 8;
  if (profile0_idx == 8) {
      ESP_LOGD(TAG, "T26: Stopping profile0 timer...");
      int timer_stopped = xTimerStop(profile0_timer, pdMS_TO_TICKS(100));
      ESP_LOGD(TAG, "T26: Timer %s", timer_stopped? "stopped" : "failed to stop!");
    if (profile0_state == PROFILE0_REQUEST) {
      profile0_cntr[1] = 0;
      profile0_state = PROFILE0_READ;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
    else if (profile0_state == PROFILE0_WRITE) { // now we check whether written and re-read profile are same
      profile0_cntr[2] = 0;
      profile0_state = PROFILE0_WRITEREAD;
      ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    }
    ESP_LOGD(TAG, "T26: Starting profile0 read timer...");
    xTimerStart(profile0_timer, 0);
  }
  else if (profile0_idx == profile0_len)
  {
    ESP_LOGD(TAG, "T26: ReadProfile0 complete");
    for (uint8_t i = 0; i < profile0_len; i+=8) 
      ESP_LOGD(TAG, "T26: Profile0: %02x %02x %02x %02x %02x %02x %02x %02x", profile0[i+0], profile0[i+1], profile0[i+2], profile0[i+3],profile0[i+4], profile0[i+5], profile0[i+6], profile0[i+7]);
    if(profile0[28] != 0 && profile0[29] != 0)
    {
      ESP_LOGD(TAG, "T26: Stopping profile0 timer...");
      int timer_stopped = xTimerStop(profile0_timer, pdMS_TO_TICKS(100));
      ESP_LOGD(TAG, "T26: Timer %s", timer_stopped? "stopped" : "failed to stop!");
      profile0_recv = false;
      profile0_mode = profile0[11];
      profile0_charge_current = profile0[13];
      if (profile0_charge_current != 1) // don't communicate workaround fake stop current
        StdMetrics.ms_v_charge_climit->SetValue(profile0_charge_current);
      profile0_cc_temp = (profile0[25]/10)+10;
      ESP_LOGI(TAG, "T26: ReadProfile0: got mode: %d, current limit: %dA, min SoC: %d%%, remote AC temp: %d°C", profile0_mode, profile0_charge_current, profile0[14], profile0_cc_temp);
      bool value_match = false;
      switch (profile0_key) {
        case P0_KEY_READ:
          ESP_LOGD(TAG, "T26: Profile0 key=0, print profile0");
          for (uint8_t i = 0; i < profile0_len; i+=8)
            ESP_LOGI(TAG, "T26: Profile0: %02x %02x %02x %02x %02x %02x %02x %02x", profile0[i+0], profile0[i+1], profile0[i+2], profile0[i+3],profile0[i+4], profile0[i+5], profile0[i+6], profile0[i+7]);
          profile0_state = PROFILE0_IDLE;
          ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
          if (t26_init) {
            profile0_charge_current_old = (profile0_charge_current < 4)? climit_max : profile0_charge_current;
            MyConfig.SetParamValueInt("xvu", "chg_climit", profile0_charge_current);
            profile0_cc_temp_old = profile0_cc_temp;
            MyConfig.SetParamValueInt("xvu", "cc_temp", profile0_cc_temp);
            profile0_cc_onbat = profile0_mode & 4;
            MyConfig.SetParamValueBool("xvu", "cc_onbat", profile0_cc_onbat);
            t26_init = false;
          }
          return;
        case P0_KEY_SWITCH:
          if (profile0_mode == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 mode value match");
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 mode value mismatch");
          }
          break;
        case P0_KEY_CLIMIT:
          if (profile0_charge_current == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 charge current value match");
            if (profile0_charge_current != 1) { // hide current limit change for charge/stop workaround; don't save previous value!
              MyNotify.NotifyStringf("info", "Charge control", "Maximum charge current set to %dA", profile0_charge_current);
              profile0_charge_current_old = (profile0_charge_current > 3)? profile0_charge_current : 4; // safeguard against currents too low for charge to (re-)start reproducibly
            }
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 charge current value mismatch");
          }
          break;
        case P0_KEY_CC_TEMP:
          if (profile0_cc_temp == profile0_val) {
            ESP_LOGD(TAG, "T26: Profile0 cc temperature value match");
            MyNotify.NotifyStringf("info", "Climatecontrol", "Climatecontrol temperature set to %d°C", profile0_cc_temp);
            profile0_cc_temp_old = profile0_cc_temp;
            value_match = true;
          }
          else {
            ESP_LOGD(TAG, "T26: Profile0 cc temperature value mismatch");
          }
          break;
        default:
          ESP_LOGE(TAG, "T26: Profile0 write error: key=%d!", profile0_key);
      }
      if (value_match) {
        if (profile0_key == P0_KEY_SWITCH){ // we have a switch request, so try to switch now
          ESP_LOGI(TAG, "T26: Profile0 value matches setting, ready to activate");
          ActivateProfile0();
        }
        else {
          ESP_LOGI(TAG, "T26: Profile0 value matches setting, done!");
          profile0_state = PROFILE0_IDLE;
          ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
          profile0_key = P0_KEY_NOP;
          if (xCurrentSemaphore != NULL)
            xSemaphoreGive(xCurrentSemaphore);
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
      else {
        ESP_LOGD(TAG, "T26: ReadProfile0 this shouldn't happen! Stopping.");
        profile0_state = PROFILE0_IDLE;
        ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
        profile0_key = P0_KEY_NOP;
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
  canbus *comfBus;
  comfBus = m_can3;
  if (!comfBus || HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: WriteProfile0 failed: T26 not ready / no write access!");
    return;
  }
  if (profile0_state != PROFILE0_WRITEREAD){
    profile0_state = PROFILE0_WRITE;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
  }
  uint8_t length = 8;
  unsigned char data[length];
  data[0] = 0x90; // XXX detect free channel! (always use channel 2 (0x90 / 0xd headers) for now)
  data[1] = 0x20;
  data[2] = 0x29; // 2: write, PID 959
  data[3] = 0x59;
  data[4] = 0x2c;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x01;
  comfBus->WriteStandard(0x69E, length, data);
    
  data[0] = 0xd0;
  data[1] = (profile0_key == P0_KEY_SWITCH) ? profile0_val : profile0[11]; // key twenty to change mode byte in basic configuration (check Multiplex PID Document)
  data[2] = profile0[12];
  data[3] = (profile0_key == P0_KEY_CLIMIT) ? profile0_val : profile0[13];// key twenty-one to change max current byte in basic configuration (check Multiplex PID Document)
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
  data[6] = (profile0_key == P0_KEY_CC_TEMP) ? (profile0_val-10)*10 : profile0[25]; // key twenty-two to change climate contol temperature in basic configuration (check Multiplex PID Document)
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
  ESP_LOGD(TAG, "T26: Starting profile0 write timer...");
  xTimerStart(profile0_timer, 0);
}

void OvmsVehicleVWeUp::Profile0RetryTimer(TimerHandle_t timer)
{
  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->Profile0RetryCallBack();
}

void OvmsVehicleVWeUp::Profile0RetryCallBack()
{
  ESP_LOGD(TAG, "T26: Profile0RetryCallBack in state %d (cntr: %d,%d,%d, key %d, val %d, mode %d, current %d, temp %d, recv %d)", profile0_state, profile0_cntr[0], profile0_cntr[1], profile0_cntr[2], profile0_key, profile0_val, profile0_mode, profile0_charge_current, profile0_cc_temp, profile0_recv);

  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: Profile0RetryCallBack failed: T26 not ready / no write access! (%d, %d, %d)", HasOBD(), IsT26Ready(), vweup_enable_write);
    return;
  }
  else if (*max_element(profile0_cntr,profile0_cntr+3) >= profile0_retries) {
    ESP_LOGE(TAG, "T26: Profile0 max retries exceeded!");
    ESP_LOGD(TAG, "T26: voltage of 12V battery: %0.2f", StdMetrics.ms_v_bat_12v_voltage->AsFloat());
    if (profile0_key == P0_KEY_SWITCH) {
      if (chargestartstop) {
        if (!fakestop)
          charge_timeout = true;
        if (xChargeSemaphore != NULL)
          xSemaphoreGive(xChargeSemaphore);
        MyNotify.NotifyStringf("alert", "Charge control", "Failed to %s charge!", (profile0_activate)? "start" : "stop");
        chargestartstop = false;
      }
      else
        MyNotify.NotifyStringf("alert", "Climatecontrol", "Failed to %s remote climate control!", (profile0_activate)? "start" : "stop");
    }
    else if (profile0_key == P0_KEY_CLIMIT) {
      charge_timeout = true;
      if (xCurrentSemaphore != NULL)
        xSemaphoreGive(xCurrentSemaphore);
      MyNotify.NotifyString("alert", "Charge control", "Failed to change charge current!");
      MyConfig.SetParamValueInt("xvu", "chg_climit", profile0_charge_current_old);
    }
    else if (profile0_key == P0_KEY_CC_TEMP) {
      MyNotify.NotifyString("alert", "Climatecontrol", "Failed to change remote climatecontrol temperature!");
      MyConfig.SetParamValueInt("xvu", "cc_temp", profile0_cc_temp_old);
    }
    else if (profile0_state == PROFILE0_WAKE) {
      if (xWakeSemaphore != NULL)
        xSemaphoreGive(xWakeSemaphore);
    }
    memset(profile0_cntr, 0, sizeof(profile0_cntr));
    profile0_state = PROFILE0_IDLE;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    profile0_key = P0_KEY_NOP;
  }
  else {
    switch (profile0_state){
      case PROFILE0_IDLE:
        ESP_LOGE(TAG, "T26: Profile0RetryCallBack in state PROFILE0_IDLE shouldn't happen!");
        break;
      case PROFILE0_INIT:
        profile0_key = P0_KEY_READ;
        WakeupT26();
        profile0_cntr[2]++;
        break;
      case PROFILE0_WAKE:
        profile0_cntr[0]++;
        ESP_LOGI(TAG, "T26: Profile0 wake retry %d...", profile0_cntr[0]);
        WakeupT26Stage2();
        break;
      case PROFILE0_REQUEST:
        profile0_cntr[0]++;
        ESP_LOGI(TAG, "T26: Profile0 request attempt %d...", profile0_cntr[0]);
        RequestProfile0();
        break;
      case PROFILE0_READ:
        profile0_cntr[1]++;
        ESP_LOGW(TAG, "T26: Profile0 read failed! Retrying... %d", profile0_cntr[1]);
        profile0_cntr[0] = 0;
        RequestProfile0();
        break;
      case PROFILE0_WRITE:
      case PROFILE0_WRITEREAD:
        profile0_cntr[2]++;
        ESP_LOGI(TAG, "T26: Profile0 write attempt %d...", profile0_cntr[2]);
        profile0_cntr[0] = 0;
        RequestProfile0();
        break;
      case PROFILE0_SWITCH:
        profile0_cntr[0]++;
        ESP_LOGD(TAG, "T26: Profile0 switch attempt %d...", profile0_cntr[0]);
        if (chargestartstop) {
          ESP_LOGD(TAG, "T26: Profile0RetryCallBack to start/stop charge");
          if (profile0_activate == StdMetrics.ms_v_charge_inprogress->AsBool()) {
            ESP_LOGI(TAG, "T26: Profile0 charge successfully turned %s",(profile0_activate)? "ON" : "OFF");
            profile0_state = PROFILE0_IDLE;
            ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
            profile0_key = P0_KEY_NOP;          
            if (xChargeSemaphore != NULL)
              xSemaphoreGive(xChargeSemaphore);
            if (profile0_activate) {
              fakestop = false;
              if (StdMetrics.ms_v_bat_soc->AsInt() < MyConfig.GetParamValueInt("xvu", "chg_soclimit", 80))
                StdMetrics.ms_v_charge_state->SetValue("charging");
              else
                StdMetrics.ms_v_charge_state->SetValue("topoff");
            }
            else
              StdMetrics.ms_v_charge_state->SetValue("stopped");
          }
          else {
            ESP_LOGI(TAG, "T26: Profile0 charge %s attempt %d...", (profile0_activate)? "start" : "stop", profile0_cntr[0]);
            ActivateProfile0();
          }
        }
        else if (profile0_val & 2) { //|| (profile0_val & 4)) { // climatecontrol on/off
          ESP_LOGD(TAG, "T26: Profile0RetryCallBack to start/stop climate control");
          if (profile0_cntr[0] == 5 && !MyConfig.GetParamValueBool("xvu", "cc_onbat"))
            ESP_LOGW(TAG, "Climatecontrol on battery disabled, is the car plugged in?");
          if (profile0_activate == StdMetrics.ms_v_env_hvac->AsBool()) {
            ESP_LOGI(TAG, "T26: Profile0 climatecontrol successfully turned %s",(profile0_activate)? "ON" : "OFF");
            MyNotify.NotifyStringf("info", "Climatecontrol", "Remote climatecontrol turned %s", (profile0_activate)? "on" : "off");
            vweup_cc_on = profile0_activate;
            profile0_state = PROFILE0_IDLE;
            ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
            profile0_key = P0_KEY_NOP;
          }
          else {
            ESP_LOGI(TAG, "T26: Profile0 climatecontrol %s attempt %d...", (profile0_activate)? "start" : "stop", profile0_cntr[0]);
            ActivateProfile0();
          }
        }
        else {
          ESP_LOGE(TAG, "T26: Profile0RetryCallBack ERROR: key=%d and val=%d!", profile0_key, profile0_val);
        }
        break;
      default:
        ESP_LOGE(TAG, "T26: Profile0 Retry Callback ERROR: profile0_state=%d!", profile0_state);
        profile0_state = PROFILE0_IDLE;
        ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
        profile0_key = P0_KEY_NOP;
    }
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandSetChargeCurrent(uint16_t climit)
{
  ESP_LOGD(TAG, "T26: CommandSetChargeCurrent called with %dA", climit);
  if (profile0_charge_current == climit)
  {
    ESP_LOGD(TAG, "T26: No change in charge current, ignoring request");
    profile0_state = PROFILE0_IDLE;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
    return Success;
  } 
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CommandSetChargeCurrent failed: T26 not ready / no write access! (setting back to %d))", profile0_charge_current_old);
    MyConfig.SetParamValueInt("xvu", "chg_climit", profile0_charge_current_old);
    return Fail;
  }
  xSemaphoreTake(xCurrentSemaphore, 0);
  if (climit !=1) { // don't set charge start/stop workaround current of 1A in xvu.chg_climit / recover from charge stop workaround
    ESP_LOGD(TAG, "calling SetParamValueInt with chg_climit = %d", climit);
    MyConfig.SetParamValueInt("xvu", "chg_climit", climit); // SetChargeCurrent is called from ConfigChanged
  }
  else {
    ESP_LOGD(TAG, "calling SetChargeCurrent with climit = %d", climit);
  }
  SetChargeCurrent(climit); // XXX seems that MyConfig.SetParamValue above doesn't get called?! This way it works... :-/
  if (xSemaphoreTake(xCurrentSemaphore, pdMS_TO_TICKS(2*profile0_retries*profile0_delay)))
  {
    ESP_LOGD(TAG, "T26: CommandSetChargeCurrent wait for success over");
    xSemaphoreGive(xCurrentSemaphore);
    if (charge_timeout) {
      charge_timeout = false;
      return Fail;
    }
    else
      return Success;
  }
  else
  {
    ESP_LOGE(TAG, "T26: timeout waiting for CommandSetChargeCurrent to finish!");
    xSemaphoreGive(xCurrentSemaphore);
    return Fail;
  }
}

void OvmsVehicleVWeUp::SetChargeCurrent(int climit)
{
  ESP_LOGI(TAG, "T26: Charge current changed from %d to %d", profile0_charge_current, climit);
  if (climit < 0) {
    ESP_LOGI(TAG, "T26: Charge current below limit of 0A, setting to lower limit");
    MyConfig.SetParamValueInt("xvu", "chg_climit", 0);
    return;
  }
  else if (climit > climit_max) {
    ESP_LOGI(TAG, "T26: Charge current above limit of %dA, setting to upper limit", climit_max);
    MyConfig.SetParamValueInt("xvu", "chg_climit", climit_max);
    return;
  }
  profile0_key = P0_KEY_CLIMIT;
  profile0_val = climit;
  ESP_LOGD(TAG, "T26: SetChargeCurrent calling WakeupT26");
  WakeupT26();
} 

// "official" start/stop function; unfortunately sometimes charge keeps reverting to previous state after issuing (even multiple) PID 958 switch commands
void OvmsVehicleVWeUp::StartStopChargeT26(bool chargestart)
{
  if (!StdMetrics.ms_v_door_chargeport->AsBool()) {
    ESP_LOGE(TAG, "T26: Vehicle is not plugged!");
    if (xChargeSemaphore != NULL)
      xSemaphoreGive(xChargeSemaphore);
    return;
  }
  else 
    ESP_LOGD(TAG, "T26: OK, plugged -> start charge");
  ESP_LOGI(TAG, "T26: Charge turning %s", chargestart ? "ON" : "OFF");
  if (chg_workaround)
    StartStopChargeT26Workaround(chargestart);
  profile0_key = P0_KEY_SWITCH;
  profile0_val = MyConfig.GetParamValueBool("xvu", "cc_onbat")? 5 : 1;
  if (chargestart && StdMetrics.ms_v_env_hvac->AsBool()) profile0_val += 2;
  profile0_activate = chargestart;
  chargestartstop = true;
  ESP_LOGD(TAG, "T26: StartStopChargeT26 calling WakeupT26");
  WakeupT26();
}

// Since "official" start/stop via PID 958 doesn't always work, we use a charge current of 1A as charge stop workaround
void OvmsVehicleVWeUp::StartStopChargeT26Workaround(bool chargestart)
{
  profile0_charge_current_old = (profile0_charge_current_old < 4)? climit_max : profile0_charge_current_old; // should never happen (if reading of profile0 at boot works...)
  ESP_LOGD(TAG, "T26: ChargeStartStopT26Workaround called, setting charge current to %dA", chargestart? profile0_charge_current_old : 1);
    CommandSetChargeCurrent(chargestart? profile0_charge_current_old : 1); // need to wait for current to be set before continuing
}

void OvmsVehicleVWeUp::CCTempSet(int temperature)
{
  ESP_LOGI(TAG, "T26: Climatecontrol temperature changed from %d to %d", profile0_cc_temp, temperature);
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: CCTempSet failed: T26 not ready / no write access! (setting back to %d))", profile0_cc_temp_old);
    MyConfig.SetParamValueInt("xvu", "cc_temp", profile0_cc_temp_old);
    return;
  }
  else if (temperature < CC_TEMP_MIN) {
    ESP_LOGI(TAG, "T26: Climatecontrol temperature below limit of %d°C, setting to lower limit", CC_TEMP_MIN);
    MyConfig.SetParamValueInt("xvu", "cc_temp", CC_TEMP_MIN);
    return;
  }
  else if (temperature > CC_TEMP_MAX) {
    ESP_LOGI(TAG, "T26: Climatecontrol temperature above limit of %d°C, setting to upper limit", CC_TEMP_MAX);
    MyConfig.SetParamValueInt("xvu", "cc_temp", CC_TEMP_MAX);
    return;
  }
  else if (temperature == profile0_cc_temp)
  {
    ESP_LOGD(TAG, "T26: No change in temperature, ignoring request");
    return;
  }
  profile0_key = P0_KEY_CC_TEMP;
  profile0_val = temperature;
  ESP_LOGD(TAG, "T26: CCTempSet calling WakeupT26");
  WakeupT26();
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandClimateControl(bool climatecontrolon)
{
  ESP_LOGD(TAG, "T26: CommandClimateControl called");
  if (HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
      ESP_LOGE(TAG, "T26: CommandClimateControl failed: T26 not ready / no write access!");
      MyNotify.NotifyString("alert", "Climatecontrol", "Climatecontrol failed: CAN bus not ready / no write access!");
      return Fail;
  }
  else if (climatecontrolon == StdMetrics.ms_v_env_hvac->AsBool()) {
    ESP_LOGE(TAG, "T26: Climatecontrol already %s!", climatecontrolon? "on" : "off");
    MyNotify.NotifyStringf("alert", "Climatecontrol", "Climatecontrol already %s!", climatecontrolon? "on" : "off");
    return Fail;
  }
  else if (t26_car_on) {
    ESP_LOGE(TAG, "T26: CommandClimateControl failed: car is on!");
    MyNotify.NotifyStringf("alert", "Climatecontrol", "Can't turn %s climatecontrol when car is on!", climatecontrolon? "on remote" : "off");
    return Fail;
  }
  else if (climatecontrolon && lever != 0x20) {
    ESP_LOGE(TAG, "T26: CommandClimateControl failed: selector lever not in position P!");
    MyNotify.NotifyStringf("alert", "Climatecontrol", "Selector lever not in position P, can't turn on climatecontrol!");
    return Fail;
  }
  else if (!StdMetrics.ms_v_door_chargeport->AsBool() && StdMetrics.ms_v_bat_soc->AsFloat() < 20) { // XXX should check for power when plugged...
    ESP_LOGE(TAG, "T26: Can't start climatecontrol on battery with SoC below 20%%!");
    MyNotify.NotifyString("alert", "Climatecontrol", "Can't start climatecontrol on battery with SoC below 20%%!");
    return Fail;
  }
  else if (climatecontrolon && !MyConfig.GetParamValueBool("xvu", "cc_onbat"))
    {
    if (HasNoOBD()) {
      ESP_LOGW(TAG, "T26: Climatecontrol on battery disabled and no OBD connection, can't check if car is plugged! Climatecontrol may not start...");
      MyNotify.NotifyString("info", "Climatecontrol", "Climatecontrol on battery disabled and no OBD connection, can't check if car is plugged! Climatecontrol may not start...");
    }
    else if (!StdMetrics.ms_v_door_chargeport->AsBool()) {
      ESP_LOGE(TAG, "T26: Climatecontrol on battery disabled and unplugged, can't start!");
      MyNotify.NotifyString("alert", "Climatecontrol", "Climatecontrol on battery disabled and unplugged, can't start!");
      return Fail;
    }
  }
  ESP_LOGI(TAG, "T26: CommandClimateControl turning %s", climatecontrolon ? "ON" : "OFF");
  profile0_key = P0_KEY_SWITCH;
  profile0_val = MyConfig.GetParamValueBool("xvu", "cc_onbat")? 6 : 2;
  if (climatecontrolon && StdMetrics.ms_v_charge_inprogress->AsBool()) profile0_val += 1;
  profile0_activate = climatecontrolon;
  ESP_LOGD(TAG, "T26: CommandClimateControl calling WakeupT26");
  WakeupT26();
  return Success;
}

void OvmsVehicleVWeUp::ActivateProfile0() // only sends on/off command, profile0[11] has to be set correctly before!
{
  ESP_LOGD(TAG, "T26: ActivateProfile0 called");
  canbus *comfBus;
  comfBus = m_can3;
  if (!comfBus || HasNoT26() || !IsT26Ready() || !vweup_enable_write) {
    ESP_LOGE(TAG, "T26: ActivateProfile0 failed: T26 not ready / no write access!");
    return;
  }
  uint8_t length = 4;
  unsigned char data[length];
  data[0] = 0x29;
  data[1] = 0x58;
  data[2] = 0x00;
  data[3] = profile0_activate;
  comfBus->WriteStandard(0x69E, length, data);
  if (profile0_state != PROFILE0_SWITCH) {
    profile0_cntr[0] = 0;
    profile0_state = PROFILE0_SWITCH;
    ESP_LOGD(TAG, "T26: profile0 state change to %d", profile0_state);
  }
  ESP_LOGD(TAG, "T26: Starting profile0 activate timer...");
  xTimerStart(profile0_timer, 0);
}

void OvmsVehicleVWeUp::CommandReadProfile0(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  ESP_LOGD(TAG, "T26: CommandReadProfile0 called");
  OvmsVehicleVWeUp* eup = OvmsVehicleVWeUp::GetInstance(writer);
  eup->profile0_key = P0_KEY_READ; // set read request, rest runs asynchronously after wakeup
  ESP_LOGD(TAG, "T26: CommandReadProfile0 calling WakeupT26");
  eup->WakeupT26();
// XXX return profile0 so it is shown in app
}

void OvmsVehicleVWeUp::CommandResetProfile0(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
{
  ESP_LOGD(TAG, "T26: CommandResetProfile0 called");
  OvmsVehicleVWeUp* eup = OvmsVehicleVWeUp::GetInstance(writer);
  uint8_t mode = 0x02; // cc on battery disabled
  uint8_t minSoC = 30;
  uint8_t cc_temp_xvu = MyConfig.GetParamValueInt("xvu", "cc_temp", 20);
  uint8_t cc_temp = (cc_temp_xvu >= CC_TEMP_MIN && cc_temp_xvu <= CC_TEMP_MAX) ? cc_temp_xvu : 20;
  uint8_t unknown = 0x1e; // [27] differs, values from 0..0xff have been observed
  ESP_LOGI(TAG, "T26: Resetting charge/climate settings to default (mode: %d, current limit: %dA, min SoC: %d%%, remote AC temp: %d°C", mode, eup->climit_max, minSoC, cc_temp);
  uint8_t profile0_default[] = {0x90, 0x22, 0x49, 0x59, 0x27, 0x02, 0x40, 0x00, 
                                0xd0, 0x01, 0x00, mode, 0x00, eup->climit_max, minSoC, 0xff,
                                0xd1, 0xff, 0x00, 0xff, 0xff, 0xff, 0xff, 0x01, 
                                0xd2, cc_temp, 0x00, unknown, 0x1e, 0x0a, 0x00, 0x00,
                                0xd3, 0x08, 0x4f, 0x70, 0x74, 0x69, 0x6f, 0x6e, 
                                0xd4, 0x65, 0x6e};
  std::copy(profile0_default, profile0_default + 43, eup->profile0);
  eup->profile0_key = P0_KEY_NOP; // don't go through all the loops & checks, just wake & write
  ESP_LOGD(TAG, "T26: CommandResetProfile0 calling WakeupT26");
  eup->WakeupT26();
  eup->profile0_key = P0_KEY_CLIMIT; // need to supply a fake writing key; climit will give user feedback that current has been set
  eup->profile0_val = eup->climit_max;
  eup->WriteProfile0();
}
