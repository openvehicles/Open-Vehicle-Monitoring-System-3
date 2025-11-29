/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; https://github.com/MyLab-odyssey/ED4scan
 */

static const char *TAG = "v-smarteq";

#include "vehicle_smarteq.h"

// Abort frame processing early on short DLC.
#define REQ_DLC(minlen)                                                \
  if (p_frame->FIR.B.DLC < (minlen)) {                                 \
    ESP_LOGV(TAG,"CAN %03X: DLC %u < %u -> ignore frame",              \
      p_frame->MsgID, p_frame->FIR.B.DLC, (unsigned)(minlen));         \
    return;                                                            \
  }


void OvmsVehicleSmartEQ::IncomingFrameCan1(CAN_frame_t* p_frame) {
  uint8_t *data = p_frame->data.u8;
  uint64_t c = swap_uint64(p_frame->data.u64);
  
  static bool isCharging = false;
  static bool lastCharging = false;
  bool _bool = false;
  float _range_est;
  float _bat_temp;
  float _full_km;
  float _range_cac;
  float _soc;
  float _soh;
  float _temp;
  int _duration_full;
  //char buf[10];

  switch (p_frame->MsgID) {
    case 0x17e: //gear shift
    {
      REQ_DLC(7);      // uses bytes up to at least index 6, so DLC must be 7 or more
      switch(CAN_BYTE(6)) {
        case 0x00: // Parking
          StdMetrics.ms_v_env_gear->SetValue(0);
          StdMetrics.ms_v_gen_limit_soc->SetValue(1);
          break;
        case 0x10: // Rear
          StdMetrics.ms_v_env_gear->SetValue(-1);
          StdMetrics.ms_v_gen_limit_soc->SetValue(2);
          break;
        case 0x20: // Neutral
          StdMetrics.ms_v_env_gear->SetValue(1);
          StdMetrics.ms_v_gen_limit_soc->SetValue(3);
          break;
        case 0x70: // Drive
          StdMetrics.ms_v_env_gear->SetValue(2);
          StdMetrics.ms_v_gen_limit_soc->SetValue(4);
          break;
      }
      break;
    }
    case 0x350:
      REQ_DLC(7);
      _bool = (CAN_BYTE(0) > 0xc0);
      StdMetrics.ms_v_env_locked->SetValue((CAN_BYTE(6) == 0x96));
      StdMetrics.ms_v_env_awake->SetValue(_bool);
      if (_bool && !mt_bus_awake->AsBool())
        {
        ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
        mt_bus_awake->SetValue(true);
        m_candata_poll = false;
        m_candata_timer = -1;
        }
      break;
    case 0x392:
      REQ_DLC(6);
      StdMetrics.ms_v_env_hvac->SetValue((CAN_BYTE(1) & 0x40) > 0);
      StdMetrics.ms_v_env_cabintemp->SetValue(CAN_BYTE(5) - 40.0f);
      break;
    case 0x42E:        // HV voltage / temp frame
      {
      REQ_DLC(5);
      uint8_t raw_temp = (c >> 13) & 0x7Fu;
      _temp = (float)raw_temp - 40.0f;
      
      // Ignore invalid sensor reading (0x7F = 127 → 87°C after offset)
      if (raw_temp != 0x7F) 
        {
        StdMetrics.ms_v_bat_temp->SetValue(_temp);
        }
      
      StdMetrics.ms_v_bat_voltage->SetValue((float)((CAN_UINT(3) >> 5) & 0x3ff) / 2.0f);
      StdMetrics.ms_v_charge_climit->SetValue((c >> 20) & 0x3Fu);
        
      break;
      }
    case 0x4F8:
      REQ_DLC(3);
      StdMetrics.ms_v_env_handbrake->SetValue((CAN_BYTE(0) & 0x08) > 0);
      //StdMetrics.ms_v_env_awake->SetValue((CAN_BYTE(0) & 0x40) > 0); // Ignition on
      break;
    case 0x5D7: // Speed, ODO
      REQ_DLC(6);
      StdMetrics.ms_v_pos_speed->SetValue((float) CAN_UINT(0) / 100.0f);
      StdMetrics.ms_v_pos_odometer->SetValue((float) (CAN_UINT32(2)>>4) / 100.0f);
      mt_pos_odometer_trip->SetValue((float) (CAN_UINT(4)>>4) / 100.0f);
      break;
    case 0x5de:
      REQ_DLC(8);
      StdMetrics.ms_v_env_headlights->SetValue((CAN_BYTE(0) & 0x04) > 0);
      StdMetrics.ms_v_door_fl->SetValue((CAN_BYTE(1) & 0x08) > 0);
      StdMetrics.ms_v_door_fr->SetValue((CAN_BYTE(1) & 0x02) > 0);
      StdMetrics.ms_v_door_rl->SetValue((CAN_BYTE(2) & 0x40) > 0);
      StdMetrics.ms_v_door_rr->SetValue((CAN_BYTE(2) & 0x10) > 0);
      StdMetrics.ms_v_door_trunk->SetValue((CAN_BYTE(7) & 0x10) > 0);
      break;
    case 0x62d:
      REQ_DLC(3);
      {
      uint16_t raw_worst_cons = ((CAN_BYTE(0) << 2) | (CAN_BYTE(1) >> 6)) & 0x3FF;
      float worst_consumption = (float)raw_worst_cons * 0.1f;
      uint16_t raw_best_cons = ((CAN_BYTE(1) & 0x3F) << 4) | (CAN_BYTE(2) >> 4);
      float best_consumption = (float)raw_best_cons * 0.1f;
      uint16_t raw_bcb_power = ((CAN_BYTE(2) & 0x0F) << 5) | (CAN_BYTE(3) >> 3);
      float bcb_power_mains = (float)raw_bcb_power * 100.0f;

      mt_worst_consumption->SetValue(worst_consumption);
      mt_best_consumption->SetValue(best_consumption);
      mt_bcb_power_mains->SetValue(bcb_power_mains);
      break;
      }    
    case 0x634:
      REQ_DLC(2);
      {
      uint8_t raw_timer = CAN_BYTE(1) & 0x7F;
      uint16_t charging_timer_value = (uint16_t)raw_timer * 15;
      uint8_t charging_timer_status = CAN_BYTE(2) & 0x03;
      uint8_t charge_prohibited = (CAN_BYTE(2) >> 2) & 0x03;
      uint8_t charge_authorization = (CAN_BYTE(2) >> 4) & 0x03;
      uint8_t ext_charge_manager = (CAN_BYTE(2) >> 6) & 0x03;
      
      mt_charging_timer_value->SetValue(charging_timer_value);
      mt_charging_timer_status->SetValue(charging_timer_status);
      mt_charge_prohibited->SetValue(charge_prohibited);
      mt_charge_authorization->SetValue(charge_authorization);
      mt_ext_charge_manager->SetValue(ext_charge_manager);
      break;
      }
    case 0x637:
      REQ_DLC(6);
      {
      uint16_t raw_consumption = ((CAN_BYTE(0) << 2) | (CAN_BYTE(1) >> 6)) & 0x3FF;
      float consumption_mission = (float)raw_consumption / 10.0f;
      uint16_t raw_recovery = ((CAN_BYTE(1) & 0x3F) << 4) | (CAN_BYTE(2) >> 4);
      float recovery_mission = (float)raw_recovery / 10.0f;
      uint16_t raw_aux = ((CAN_BYTE(2) & 0x0F) << 6) | (CAN_BYTE(3) >> 2);
      float aux_consumption = (float)raw_aux / 10.0f;

      mt_energy_used->SetValue(consumption_mission);
      mt_energy_recd->SetValue(recovery_mission);
      mt_aux_consumption->SetValue(aux_consumption);
      }
    case 0x646:
      {
      REQ_DLC(7);
      // Extract multi-byte values
      uint16_t rest_consumption = (CAN_BYTE(1) * 0.1);
      uint32_t trip_distance = ((CAN_BYTE(2) << 9) | (CAN_BYTE(3) << 1) | (CAN_BYTE(4) >> 7)) & 0x1FFFF;
      uint16_t trip_energy = ((CAN_BYTE(4) & 0x7F) << 8) | CAN_BYTE(5);
      uint16_t avg_speed = (CAN_BYTE(6) << 4) | (CAN_BYTE(7) >> 4);
      
      // Apply scaling (all × 0.1)
      mt_reset_consumption->SetValue((float)rest_consumption);
      StdMetrics.ms_v_bat_consumption->SetValue((float)rest_consumption * 10.0f); // current consumption
      mt_reset_distance->SetValue((float)trip_distance * 0.1f);
      if(trip_energy < 0x7FFF) 
        mt_reset_energy->SetValue((float)trip_energy * 0.1f);
      mt_reset_speed->SetValue((float)avg_speed * 0.1f);
      if(m_bcvalue){
        StdMetrics.ms_v_gen_kwh_grid_total->SetValue((float)rest_consumption); // not the best idea at the moment
      } else {
        StdMetrics.ms_v_gen_kwh_grid_total->SetValue(0.0f);
      }
      break;
      }
    case 0x654:        // SOC / charge port status
      REQ_DLC(4);
      _soc = (float) CAN_BYTE(3);
      if (_soc <= 100.0f) StdMetrics.ms_v_bat_soc->SetValue(_soc); // SOC
      StdMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(0) & 0x20) != 0); // ChargingPlugConnected
      _duration_full = (((c >> 22) & 0x3ffu) < 0x3ff) ? (c >> 22) & 0x3ffu : 0;
      mt_obd_duration->SetValue((int)(_duration_full), Minutes);
      _range_est = ((c >> 12) & 0x3FFu); // VehicleAutonomy
      _bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(0) - 20.0;
      _full_km = m_full_km;
      _range_cac = _full_km + (_bat_temp); // temperature compensation +/- range
      if (_range_est != 1023.0f) 
        {
        StdMetrics.ms_v_bat_range_est->SetValue(_range_est);

        if (_soc > 0.1f)  // prevent div/0
          {
          StdMetrics.ms_v_bat_range_full->SetValue((_range_est / _soc) * 100.0f);
          StdMetrics.ms_v_bat_range_ideal->SetValue((_range_cac * _soc) / 100.0f);
          }
        }
      break;
    case 0x658:
    {
      REQ_DLC(6);
      uint32_t bat_serial = CAN_UINT32(0);
      
      // Store battery serial number (only if not already set or changed)
      static uint32_t last_bat_serial = 0;
      if (bat_serial != 0 && bat_serial != 0xFFFFFFFF && bat_serial != last_bat_serial) {
        char serial_str[12];
        snprintf(serial_str, sizeof(serial_str), "%08X", bat_serial);
        mt_bat_serial->SetValue(serial_str);
        last_bat_serial = bat_serial;
      }
      _soh = (float)(CAN_BYTE(4) & 0x7Fu);
      if (_soh <= 100.0f) StdMetrics.ms_v_bat_soh->SetValue(_soh); // SOH
      StdMetrics.ms_v_bat_health->SetValue(
        (_soh >= 95.0f) ? "excellent"
      : (_soh >= 85.0f) ? "good"
      : (_soh >= 75.0f) ? "average"
      : (_soh >= 65.0f) ? "poor"
      : "consider replacement");

      isCharging = (CAN_BYTE(5) & 0x20); // ChargeInProgress
      if (isCharging) { // STATE charge in progress
        //StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
      }
      if (isCharging != lastCharging) { // EVENT charge state changed
        if (isCharging) { // EVENT started charging
          // Set charging metrics
          StdMetrics.ms_v_charge_pilot->SetValue(true);
          StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StdMetrics.ms_v_charge_mode->SetValue("standard");
          StdMetrics.ms_v_charge_type->SetValue("type2");
          StdMetrics.ms_v_charge_state->SetValue("charging");
          StdMetrics.ms_v_charge_substate->SetValue("onrequest");
          StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
          mt_bus_awake->SetValue(true);
        } else { // EVENT stopped charging
          StdMetrics.ms_v_charge_pilot->SetValue(false);
          StdMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StdMetrics.ms_v_charge_mode->SetValue("standard");
          StdMetrics.ms_v_charge_type->SetValue("type2");
          StdMetrics.ms_v_charge_duration_full->SetValue(0);
          StdMetrics.ms_v_charge_duration_soc->SetValue(0);
          StdMetrics.ms_v_charge_duration_range->SetValue(0);
          StdMetrics.ms_v_charge_power->SetValue(0);
          StdMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
          if (StdMetrics.ms_v_bat_soc->AsInt() < 95) {
            // Assume the charge was interrupted
            ESP_LOGI(TAG,"Car charge session was interrupted");
            StdMetrics.ms_v_charge_state->SetValue("stopped");
            StdMetrics.ms_v_charge_substate->SetValue("interrupted");
          } else {
            // Assume the charge completed normally
            ESP_LOGI(TAG,"Car charge session completed");
            StdMetrics.ms_v_charge_state->SetValue("done");
            StdMetrics.ms_v_charge_substate->SetValue("onrequest");
          }
        }
      }
      lastCharging = isCharging;
      break;
    }
    case 0x668:
      REQ_DLC(1);
      vehicle_smart_car_on((CAN_BYTE(0) & 0x40) > 0); // Drive Ready
      break;
    case 0x673:  
      // TPMS values only used, when CAN write is disabled, otherwise utilize PollReply_TPMS_InputCapt
      if ( !m_enable_write )
      {
        REQ_DLC(7);
        // Read TPMS pressure values:
        for (int i = 0; i < 4; i++) 
          {
          if (CAN_BYTE(2 + i) != 0xff) 
            {
            m_tpms_pressure[i] = (float) CAN_BYTE(2 + i) * 3.1; // kPa
          }
        }
      }
      break;
    default:
      //ESP_LOGI(TAG, "PID:%x DATA: %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
      break;
  }
}
