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
;    0.4.5  Moved to unified ODB/T26 code
;
;    0.4.6  Corrected cabin temperature selection
;
;    0.4.7  Beautified code
;
;    0.4.8  Started using ms_v_env_charging12v
;
;    0.4.9  Added T26 awake detection for OBD
;
;    (C) 2021       Chris van der Meijden
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
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"

void OvmsVehicleVWeUp::sendOcuHeartbeat(TimerHandle_t timer)
{
  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->SendOcuHeartbeat();
}

void OvmsVehicleVWeUp::ccCountdown(TimerHandle_t timer)
{
  OvmsVehicleVWeUp *vweup = (OvmsVehicleVWeUp *)pvTimerGetTimerID(timer);
  vweup->CCCountdown();
}

void OvmsVehicleVWeUp::T26Init()
{
  ESP_LOGI(TAG, "Starting connection: T26A (Comfort CAN)");
  memset(m_vin, 0, sizeof(m_vin));

  RegisterCanBus(3, CAN_MODE_ACTIVE, CAN_SPEED_100KBPS);

  MyConfig.RegisterParam("xvu", "VW e-Up", true, true);
  vin_part1 = false;
  vin_part2 = false;
  vin_part3 = false;
  vweup_remote_climate_ticker = 0;
  ocu_awake = false;
  ocu_working = false;
  ocu_what = false;
  ocu_wait = false;
  vweup_cc_on = false;
  vweup_cc_turning_on = false;
  vweup_cc_temp_int = 22;
  fas_counter_on = 0;
  fas_counter_off = 0;
  signal_ok = false;
  cc_count = 0;
  cd_count = 0;
  t26_12v_boost = false;
  t26_car_on = false;
  t26_ring_awake = false;
  t26_12v_boost_cnt = 0;
  t26_12v_wait_off = 0;

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
  // Autodisable climate control ticker (30 min.)
  if (vweup_remote_climate_ticker != 0) {
    vweup_remote_climate_ticker--;
    if (vweup_remote_climate_ticker == 1) {
      SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
    }
  }

  // Car disabled climate control
  if (!StandardMetrics.ms_v_env_on->AsBool() &&
      vweup_remote_climate_ticker < 1770 &&
      vweup_remote_climate_ticker != 0 &&
      !StandardMetrics.ms_v_env_hvac->AsBool())
  {
    ESP_LOGI(TAG, "Car disabled Climate Control or cc did not turn on");
    vweup_remote_climate_ticker = 0;
    vweup_cc_on = false;
    ocu_awake = true;
  }

  if (StdMetrics.ms_v_bat_12v_voltage->AsFloat() < 13 && !t26_ring_awake && StandardMetrics.ms_v_env_charging12v->AsBool()) {
    // Wait for 12v voltage to come up to 13.2v while getting a boost:
    t26_12v_boost_cnt++;
    if (t26_12v_boost_cnt > 20) {
      ESP_LOGI(TAG, "Car stopped itself charging the 12v battery");
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
    ESP_LOGI(TAG, "CAR IS ON");
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
    // Turn off possibly running climate control timer
    if (ocu_awake) {
      xTimerStop(m_sendOcuHeartbeat, 0);
      xTimerDelete(m_sendOcuHeartbeat, 0);
      m_sendOcuHeartbeat = NULL;
    }
    if (cc_count != 0) {
      xTimerStop(m_ccCountdown, 0);
      xTimerDelete(m_ccCountdown, 0);
      m_ccCountdown = NULL;
    }
    ocu_awake = false;
    ocu_working = false;
    vweup_remote_climate_ticker = 0;
    fas_counter_on = 0;
    fas_counter_off = 0;
    t26_car_on = true;
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    StandardMetrics.ms_v_env_aux12v->SetValue(true);
  }
  else if (!turnOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Log once that car is being turned off
    ESP_LOGI(TAG, "CAR IS OFF");
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
  // ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);

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
      float odo = (float) (((uint32_t)(d[3] & 0xf) << 12) | ((UINT)d[2] << 8) | d[1]);
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
      if (d[2] < 0x07) {
        isCharging = true;
      }
      else {
        isCharging = false;
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
          ESP_LOGI(TAG, "Car charge session started");
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
          ESP_LOGI(TAG, "Car charge session ended");
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
        ESP_LOGI(TAG, "Car woke up. Will try to charge 12v battery");
      }
      if (d[1] == 0x31 && ocu_awake) {
        // We should go to sleep, no matter what
        ESP_LOGI(TAG, "Comfort CAN calls for sleep");
        xTimerStop(m_sendOcuHeartbeat, 0);
        xTimerDelete(m_sendOcuHeartbeat, 0);
        m_sendOcuHeartbeat = NULL;
        if (cc_count != 0) {
          xTimerStop(m_ccCountdown, 0);
          xTimerDelete(m_ccCountdown, 0);
          m_ccCountdown = NULL;
        }
        ocu_awake = false;
        ocu_working = false;
        vweup_remote_climate_ticker = 0;
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
      if (d[0] == 0x00 && d[1] != 0x31 && !t26_ring_awake) {
        t26_ring_awake = true;
        ESP_LOGI(TAG, "Ring awake");
        if (t26_12v_wait_off != 0) {
          ESP_LOGI(TAG, "ODB AWAKE ist still blocked");
        }
      }
      if (d[1] == 0x31 && t26_ring_awake) {
         t26_ring_awake = false;
         ESP_LOGI(TAG, "Ring asleep");
      }
      if (d[0] == 0x1D) {
        // We are called in the ring

        unsigned char data[8];
        uint8_t length;
        length = 8;

        canbus *comfBus;
        comfBus = m_can3;

        data[0] = 0x00; // As it seems that we are always the highest in the ring we always send to 0x400
        if (ocu_working) {
          data[1] = 0x01;
          if (dev_mode) {
            ESP_LOGI(TAG, "OCU working");
          }
        }
        else {
          data[1] = 0x11; // Ready to sleep. This is very important!
          if (dev_mode) {
            ESP_LOGI(TAG, "OCU ready to sleep");
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
        vTaskDelay(50 / portTICK_PERIOD_MS);
        if (vweup_enable_write && !dev_mode) {
          comfBus->WriteStandard(0x43D, length, data);  // We answer
        }

        ocu_awake = true;
        break;
      }
      break;

    default:
      // This will log all unknown incoming frames
      // ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
  }
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::RemoteCommandHandler(RemoteCommand command)
{
  ESP_LOGI(TAG, "RemoteCommandHandler");

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

  switch (command) {
    case ENABLE_CLIMATE_CONTROL:
      if (StandardMetrics.ms_v_env_on->AsBool()) {
        ESP_LOGI(TAG, "Climate Control can't be enabled - car is on");
        break;
      }

      if (fas_counter_off > 0 && fas_counter_off < 10) {
        ESP_LOGI(TAG, "Climate Control can't be enabled - CC off is still running");
        break;
      }

      if (vweup_remote_climate_ticker == 0) {
        vweup_remote_climate_ticker = 1800;
      }
      else {
        ESP_LOGI(TAG, "Enable Climate Control - already enabled");
        break;
      }

      vweup_cc_turning_on = true;

      CommandWakeup();

      ESP_LOGI(TAG, "Enable Climate Control");

      m_ccCountdown = xTimerCreate("VW e-Up CC Countdown", 1000 / portTICK_PERIOD_MS, pdTRUE, this, ccCountdown);
      xTimerStart(m_ccCountdown, 0);

      signal_ok = true;
      break;
    case DISABLE_CLIMATE_CONTROL:
      if (!vweup_cc_on) {
        ESP_LOGI(TAG, "Climate Control can't be disabled - CC is not enabled.");
        break;
      }
      if (StandardMetrics.ms_v_env_on->AsBool()) {
        ESP_LOGI(TAG, "Climate Control is already disabled - car is on");
        break;
      }

      if ((fas_counter_on > 0 && fas_counter_on < 10) || ocu_wait) {
        // Should we implement a "wait for turn off" timer here?
        ESP_LOGI(TAG, "Climate Control can't be disabled - CC on is still running");
        break;
      }

      if (vweup_remote_climate_ticker == 0) {
        ESP_LOGI(TAG, "Disable Climate Control - already disabled");
        break;
      }

      ESP_LOGI(TAG, "Disable Climate Control");

      vweup_remote_climate_ticker = 0;

      CCOff();

      ocu_awake = true;
      signal_ok = true;

      break;
    case AUTO_DISABLE_CLIMATE_CONTROL:
      if (StandardMetrics.ms_v_env_on->AsBool()) {
        // Should not be possible
        ESP_LOGI(TAG, "Error: CC AUTO_DISABLE when car is on");
        break;
      }

      vweup_remote_climate_ticker = 0;

      ESP_LOGI(TAG, "Auto Disable Climate Control");

      CCOff();

      ocu_awake = true;
      signal_ok = true;

      break;
    default:
      return;
  }
}

// Wakeup implentation over the VW ring commands
// We need to register in the ring with a call to our self from 43D with 0x1D in the first byte
//
OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandWakeup()
{

  if (!vweup_enable_write) {
    return Fail;
  }

  if (!ocu_awake && !StandardMetrics.ms_v_env_on->AsBool()) {

    unsigned char data[8];
    uint8_t length;
    length = 8;

    unsigned char data2[2];
    uint8_t length2;
    length2 = 2;

    canbus *comfBus;
    comfBus = m_can3;

    // This is a very dirty workaround to get the wakup of the ring working.
    // We send 0x69E some values without knowing what they do.
    // This throws an AsynchronousInterruptHandler error, but wakes 0x400
    // So we wait two seconds and then call us in the ring

    data[0] = 0x14;
    data[1] = 0x42;
    if (vweup_enable_write && !dev_mode) {
      comfBus->WriteStandard(0x69E, length2, data2);
    }

    vTaskDelay(2000 / portTICK_PERIOD_MS);

    ESP_LOGI(TAG, "Sent Wakeup Command - stage 1");

    data[0] = 0x1D; // We call ourself to wake up the ring
    data[1] = 0x02;
    data[2] = 0x02;
    data[3] = 0x00;
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    if (vweup_enable_write && !dev_mode) {
      comfBus->WriteStandard(0x43D, length, data);
    }

    vTaskDelay(50 / portTICK_PERIOD_MS);

    data[0] = 0x00; // We need to talk to 0x400 first to get accepted in the ring
    data[1] = 0x01;
    data[2] = 0x02;
    data[3] = 0x04; // This could be a problem
    data[4] = 0x00;
    data[5] = 0x14; // What does this do?
    data[6] = 0x00;
    data[7] = 0x00;
    if (vweup_enable_write && !dev_mode) {
      comfBus->WriteStandard(0x43D, length, data);
    }

    ocu_working = true;
    ocu_awake = true;

    vTaskDelay(50 / portTICK_PERIOD_MS);

    m_sendOcuHeartbeat = xTimerCreate("VW e-Up OCU heartbeat", 1000 / portTICK_PERIOD_MS, pdTRUE, this, sendOcuHeartbeat);
    xTimerStart(m_sendOcuHeartbeat, 0);

    ESP_LOGI(TAG, "Sent Wakeup Command - stage 2");
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    StandardMetrics.ms_v_env_aux12v->SetValue(true);
    t26_12v_boost_cnt = 0;
    t26_12v_wait_off = 0;
    if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
       PollSetState(VWEUP_AWAKE);
    } else {
       PollSetState(VWEUP_CHARGING);
    }
  }
  // This can be done better. Gives always success, even when already awake.
  return Success;
}

void OvmsVehicleVWeUp::SendOcuHeartbeat()
{

  if (!vweup_cc_on) {
    fas_counter_on = 0;
    if (fas_counter_off < 10 && !vweup_cc_turning_on) {
      fas_counter_off++;
      ocu_working = true;
      if (dev_mode) {
        ESP_LOGI(TAG, "OCU working");
      }
    }
    else {
      if (vweup_cc_turning_on) {
        ocu_working = true;
        if (dev_mode) {
          ESP_LOGI(TAG, "OCU working");
        }
      }
      else {
        ocu_working = false;
        if (dev_mode) {
          ESP_LOGI(TAG, "OCU ready to sleep");
        }
      }
    }
  }
  else {
    fas_counter_off = 0;
    ocu_working = true;
    if (dev_mode) {
      ESP_LOGI(TAG, "OCU working");
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
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A9, length, data);
  }

  if ((fas_counter_on > 0 && fas_counter_on < 10) || (fas_counter_off > 0 && fas_counter_off < 10)) {
    data[0] = 0x60;
    if (dev_mode) {
      ESP_LOGI(TAG, "OCU Heartbeat - 5A7 60");
    }
  }
  else {
    data[0] = 0x00;
    if (dev_mode) {
      ESP_LOGI(TAG, "OCU Heartbeat - 5A7 00");
    }
  }
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }
}

void OvmsVehicleVWeUp::CCCountdown()
{
  cc_count++;
  ocu_wait = true;
  if (cc_count == 5) {
    CCOnP(); // This is weird. We first need to send a Climate Contol Off before it will turn on ???
  }
  if (cc_count == 10) {
    CCOn();
    xTimerStop(m_ccCountdown, 0);
    xTimerDelete(m_ccCountdown, 0);
    m_ccCountdown = NULL;
    ocu_wait = false;
    ocu_awake = true;
    cc_count = 0;
  }
}

void OvmsVehicleVWeUp::CCOn()
{
  unsigned char data[8];
  uint8_t length;
  length = 8;

  unsigned char data_s[4];
  uint8_t length_s;
  length_s = 4;

  canbus *comfBus;
  comfBus = m_can3;

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  // d5 could be a counter?
  data[0] = 0x80;
  data[1] = 0x20;
  data[2] = 0x29;
  data[3] = 0x59;
  data[4] = 0x21;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x01;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC0;
  data[1] = 0x06;
  data[2] = 0x00;
  data[3] = 0x10;
  data[4] = 0x1E;
  data[5] = 0xFF;
  data[6] = 0xFF;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC1;
  data[1] = 0xFF;
  data[2] = 0xFF;
  data[3] = 0xFF;
  data[4] = 0xFF;
  data[5] = 0x01;
  data[6] = 0x78; // This is the target temperature. T = 10 + d6/10

  if (vweup_cc_temp_int == 15) {
    data[6] = 0x32;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 15");
    }
  }
  if (vweup_cc_temp_int == 16) {
    data[6] = 0x3C;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 16");
    }
  }
  if (vweup_cc_temp_int == 17) {
    data[6] = 0x46;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 17");
    }
  }
  if (vweup_cc_temp_int == 18) {
    data[6] = 0x50;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 18");
    }
  }
  if (vweup_cc_temp_int == 19) {
    data[6] = 0x5A;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
  }
  if (vweup_cc_temp_int == 20) {
    data[6] = 0x64;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
  }
  if (vweup_cc_temp_int == 21) {
    data[6] = 0x6E;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
  }
  if (vweup_cc_temp_int == 22) {
    data[6] = 0x78;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
  }
  if (vweup_cc_temp_int == 23) {
    data[6] = 0x82;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 23");
    }
  }
  if (vweup_cc_temp_int == 24) {
    data[6] = 0x8C;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 24");
    }
  }
  if (vweup_cc_temp_int == 25) {
    data[6] = 0x96;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 25");
    }
  }
  if (vweup_cc_temp_int == 26) {
    data[6] = 0xA0;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 26");
    }
  }
  if (vweup_cc_temp_int == 27) {
    data[6] = 0xAA;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 27");
    }
  }
  if (vweup_cc_temp_int == 28) {
    data[6] = 0xB4;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 28");
    }
  }
  if (vweup_cc_temp_int == 29) {
    data[6] = 0xBE;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 29");
    }
  }
  if (vweup_cc_temp_int == 30) {
    data[6] = 0xC8;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 30");
    }
  }

  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC2;
  data[1] = 0x1E;
  data[2] = 0x1E;
  data[3] = 0x0A;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x08;
  data[7] = 0x4F;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC3;
  data[1] = 0x70;
  data[2] = 0x74;
  data[3] = 0x69;
  data[4] = 0x6F;
  data[5] = 0x6E;
  data[6] = 0x65;
  data[7] = 0x6E;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  data_s[0] = 0x29;
  data_s[1] = 0x58;
  data_s[2] = 0x00;
  data_s[3] = 0x01;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length_s, data_s);
  }

  ESP_LOGI(TAG, "Wrote second stage Climate Control On Messages to Comfort CAN.");
  vweup_cc_on = true;
  vweup_cc_turning_on = false;
  StandardMetrics.ms_v_env_charging12v->SetValue(true);
  StandardMetrics.ms_v_env_aux12v->SetValue(true);
}

void OvmsVehicleVWeUp::CCOnP()
{
  unsigned char data[8];
  uint8_t length;
  length = 8;

  unsigned char data_s[4];
  uint8_t length_s;
  length_s = 4;

  canbus *comfBus;
  comfBus = m_can3;

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  // d5 could be a counter
  data[0] = 0x80;
  data[1] = 0x20;
  data[2] = 0x29;
  data[3] = 0x59;
  data[4] = 0x22;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x01;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  data[0] = 0xC0;
  data[1] = 0x06;
  data[2] = 0x00;
  data[3] = 0x10;
  data[4] = 0x1E;
  data[5] = 0xFF;
  data[6] = 0xFF;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC1;
  data[1] = 0xFF;
  data[2] = 0xFF;
  data[3] = 0xFF;
  data[4] = 0xFF;
  data[5] = 0x01;
  data[6] = 0x78; // This is the target temperature. T = 10 + d6/10

  if (vweup_cc_temp_int == 15) {
    data[6] = 0x32;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 15");
    }
  }
  if (vweup_cc_temp_int == 16) {
    data[6] = 0x3C;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 16");
    }
  }
  if (vweup_cc_temp_int == 17) {
    data[6] = 0x46;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 17");
    }
  }
  if (vweup_cc_temp_int == 18) {
    data[6] = 0x50;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 18");
    }
  }
  if (vweup_cc_temp_int == 19) {
    data[6] = 0x5A;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
  }
  if (vweup_cc_temp_int == 20) {
    data[6] = 0x64;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
  }
  if (vweup_cc_temp_int == 21) {
    data[6] = 0x6E;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
  }
  if (vweup_cc_temp_int == 22) {
    data[6] = 0x78;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
  }
  if (vweup_cc_temp_int == 23) {
    data[6] = 0x82;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 23");
    }
  }
  if (vweup_cc_temp_int == 24) {
    data[6] = 0x8C;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 24");
    }
  }
  if (vweup_cc_temp_int == 25) {
    data[6] = 0x96;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 25");
    }
  }
  if (vweup_cc_temp_int == 26) {
    data[6] = 0xA0;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 26");
    }
  }
  if (vweup_cc_temp_int == 27) {
    data[6] = 0xAA;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 27");
    }
  }
  if (vweup_cc_temp_int == 28) {
    data[6] = 0xB4;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 28");
    }
  }
  if (vweup_cc_temp_int == 29) {
    data[6] = 0xBE;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 29");
    }
  }
  if (vweup_cc_temp_int == 30) {
    data[6] = 0xC8;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 30");
    }
  }

  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC2;
  data[1] = 0x1E;
  data[2] = 0x1E;
  data[3] = 0x0A;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x08;
  data[7] = 0x4F;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC3;
  data[1] = 0x70;
  data[2] = 0x74;
  data[3] = 0x69;
  data[4] = 0x6F;
  data[5] = 0x6E;
  data[6] = 0x65;
  data[7] = 0x6E;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data_s[0] = 0x29;
  data_s[1] = 0x58;
  data_s[2] = 0x00;
  data_s[3] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length_s, data_s);
  }

  ESP_LOGI(TAG, "Wrote first stage Climate Control On Messages to Comfort CAN.");
}

void OvmsVehicleVWeUp::CCOff()
{
  unsigned char data[8];
  uint8_t length;
  length = 8;

  unsigned char data_s[4];
  uint8_t length_s;
  length_s = 4;

  canbus *comfBus;
  comfBus = m_can3;

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  // d5 could be a counter
  data[0] = 0x80;
  data[1] = 0x20;
  data[2] = 0x29;
  data[3] = 0x59;
  data[4] = 0x22;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x01;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0x60;
  data[1] = 0x16;
  data[2] = 0x00;
  data[3] = 0x00;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x00;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x5A7, length, data);
  }

  data[0] = 0xC0;
  data[1] = 0x06;
  data[2] = 0x00;
  data[3] = 0x10;
  data[4] = 0x1E;
  data[5] = 0xFF;
  data[6] = 0xFF;
  data[7] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC1;
  data[1] = 0xFF;
  data[2] = 0xFF;
  data[3] = 0xFF;
  data[4] = 0xFF;
  data[5] = 0x01;
  data[6] = 0x78; // This is the target temperature. T = 10 + d6/10

  if (vweup_cc_temp_int == 19) {
    data[6] = 0x5A;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 19");
    }
  }
  if (vweup_cc_temp_int == 20) {
    data[6] = 0x64;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 20");
    }
  }
  if (vweup_cc_temp_int == 21) {
    data[6] = 0x6E;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 21");
    }
  }
  if (vweup_cc_temp_int == 22) {
    data[6] = 0x78;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 22");
    }
  }
  if (vweup_cc_temp_int == 23) {
    data[6] = 0x82;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 23");
    }
  }
  if (vweup_cc_temp_int == 24) {
    data[6] = 0x8C;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 24");
    }
  }
  if (vweup_cc_temp_int == 25) {
    data[6] = 0x96;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 25");
    }
  }
  if (vweup_cc_temp_int == 26) {
    data[6] = 0xA0;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 26");
    }
  }
  if (vweup_cc_temp_int == 27) {
    data[6] = 0xAA;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 27");
    }
  }
  if (vweup_cc_temp_int == 28) {
    data[6] = 0xB4;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 28");
    }
  }
  if (vweup_cc_temp_int == 29) {
    data[6] = 0xBE;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 29");
    }
  }
  if (vweup_cc_temp_int == 30) {
    data[6] = 0xC8;
    if (dev_mode) {
      ESP_LOGI(TAG, "Cabin temperature set: 30");
    }
  }

  data[0] = 0xC2;
  data[1] = 0x1E;
  data[2] = 0x1E;
  data[3] = 0x0A;
  data[4] = 0x00;
  data[5] = 0x00;
  data[6] = 0x08;
  data[7] = 0x4F;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data[0] = 0xC3;
  data[1] = 0x70;
  data[2] = 0x74;
  data[3] = 0x69;
  data[4] = 0x6F;
  data[5] = 0x6E;
  data[6] = 0x65;
  data[7] = 0x6E;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length, data);
  }

  data_s[0] = 0x29;
  data_s[1] = 0x58;
  data_s[2] = 0x00;
  data_s[3] = 0x00;
  if (vweup_enable_write && !dev_mode) {
    comfBus->WriteStandard(0x69E, length_s, data_s);
  }

  ESP_LOGI(TAG, "Wrote Climate Control Off Message to Comfort CAN.");
  vweup_cc_on = false;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandLock(const char *pin)
{
  ESP_LOGI(TAG, "CommandLock");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandUnlock(const char *pin)
{
  ESP_LOGI(TAG, "CommandUnlock");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandActivateValet(const char *pin)
{
  ESP_LOGI(TAG, "CommandActivateValet");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandDeactivateValet(const char *pin)
{
  ESP_LOGI(TAG, "CommandLDeactivateValet");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandStartCharge()
{
  ESP_LOGI(TAG, "CommandStartCharge");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandStopCharge()
{
  ESP_LOGI(TAG, "CommandStopCharge");
  return NotImplemented;
}

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandHomelink(int button, int durationms)
{
  // This is needed to enable climate control via Homelink for the iOS app
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0 && vweup_enable_write) {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
  }
  if (button == 1 && vweup_enable_write) {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
  }
  return NotImplemented;
}


OvmsVehicle::vehicle_command_t OvmsVehicleVWeUp::CommandClimateControl(bool climatecontrolon)
{
  ESP_LOGI(TAG, "CommandClimateControl");
  if (vweup_enable_write) {
    return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
  }
  else {
    return NotImplemented;
  }
}
