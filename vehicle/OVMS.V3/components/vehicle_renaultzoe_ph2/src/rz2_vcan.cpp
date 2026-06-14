/*
;    Project:       Open Vehicle Monitor System
;    Module:        Renault Zoe Ph2 - V-CAN Protocol Handler
;
;    (C) 2022-2026  Carsten Schmiemann
;
; Permission is hereby granted, free of charge, to any person obtaining a copy
; of this software and associated documentation files (the "Software"), to deal
; in the Software without restriction, including without limitation the rights
; to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
; copies of the Software, and to permit persons to whom the Software is
; furnished to do so, subject to the following conditions:
;
; The above copyright notice and this permission notice shall be included in
; all copies or substantial portions of the Software;
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
#include "vehicle_renaultzoe_ph2.h"

// Handles incoming CAN-frames on bus 1
void OvmsVehicleRenaultZoePh2::IncomingFrameCan1(CAN_frame_t *p_frame)
{
  uint8_t *data = p_frame->data.u8;

  if (!mt_poller_inhibit->AsBool())
  // Inhibit poller if metric is set to true, for vehicle maintenance to not intefere with tester (tester presence detection will be added later),
  // Renault shops typically stop working on cars with CAN-bus attached 3rd party "accessories" and blame them for all malfunctions
  {
    if (m_vcan_enabled)
    {
      // V-CAN wake-up and sleep detection is done by counting frames. No reliance on the TCU is required. V-CAN is heavily loaded, with up to about 1600 messages per second.
      vcan_message_counter++;
    }
    else
    {
      // OBD port wakeup and sleep detection, we need to rely on a TCU request
      if (mt_bus_awake->AsBool() && data[0] == 0x83 && data[1] == 0xc0)
      {
        ESP_LOGI(TAG, "Zoe wants to sleep (ECU wake up request detected)");
        ZoeShutdown();
      }
      else if (!mt_bus_awake->AsBool() && p_frame->MsgID == 0x18daf1db)
      {
        // The TCU mainly requests battery statistics for rental batteries, but the software was never changed, even for vehicles with purchased batteries.
        // The TCU's UDS requests are falling through the OBD port (a separate CAN line to the gateway), and we use them to detect vehicle wake-up.
        // The TCU must be installed and working if only the OBD port is used for the OVMS module.
        ESP_LOGI(TAG, "TCU requested battery statistics, so Zoe is woken up.");
        ZoeWakeUp();
      }
    }
  }

  // Use data from V-CAN to populate metrics.
  // --
  // The DDT database contains a file named CAN_MESSAGE_SET_C1A_Q4_2017_(...).XML, which describes most of the CAN IDs present on V-CAN.
  // I converted this file into a CAN database file to analyze my dumps and live traffic from my Zoe with SavvyCAN.
  // This greatly speeds up finding and decoding metrics on V-CAN.
  // --
  // Message decoding is first written to temporary variables because V-CAN is very busy.
  // Ticker1 is then used to store the values in OvmsMetrics (as recommended by dexterbg, thanks!).
  // I remove identified metrics from the poller list; a reduced poller list is used for that.
  // --
  // I selected CAN IDs with a refresh rate of about 500 ms to 1 second to reduce load on the OVMS module.
  // As a result, not every metric is taken from its original source.
  // For example, vehicle speed is derived from the VCU (ABS ECU), which provides all four wheel speeds, but that message is refreshed at 100 Hz.
  // Instead, I use the vehicle speed sent by the instrument cluster, which updates every 500 ms (2 Hz).

  switch (p_frame->MsgID)
  {
  case HVAC_R3_ID:
  {
    // --- Cabin setpoint & Blower setting ---
    if (vcan_vehicle_ignition) // HVAC ECU is shutdown after 1 min after Iginition off and values will be zero, keep last setting
    {
      if (CAN_BYTE(0) == 0x08) // Low setting
      {
        vcan_hvac_cabin_setpoint = 14;
      }
      else if (CAN_BYTE(0) == 0x0A) // High setting
      {
        vcan_hvac_cabin_setpoint = 30;
      }
      else
      {
        vcan_hvac_cabin_setpoint = static_cast<float>(CAN_BYTE(0)) / 2.0f;
      }
    }

    vcan_hvac_blower = (CAN_BYTE(3) >> 4) & 0x0F; // Blower speed
    vcan_hvac_blower = vcan_hvac_blower * 12.5;   // Map 1-8 to 0-100%
    break;
  }

  case METER_A3_ID:
  {
    // --- ExternalTemperature ---
    // strange scaling

    // --- VehicleSpeedDisplayValue ---
    if (CAN_BYTE(7) < 250)
    {
      vcan_vehicle_speed = CAN_BYTE(7);
    }
    break;
  }

  case USM_A2_ID:
  {
    // --- Ignition state, DC DC charge voltage, engineHoodState ---
    vcan_vehicle_ignition = CAN_BIT(0, 5);
    vcan_dcdc_voltage = (CAN_BYTE(1) * 0.06) + 6.0;

    if (((CAN_BYTE(4) >> 6) & 0x03) == 1)
    {
      vcan_door_engineHood = true;
    }
    else
    {
      vcan_door_engineHood = false;
    }
    break;
  }

  case HEVC_A101_ID:
  {
    // --- Max charge power available from EVSE ---
    uint16_t rawSpotPwr = (((uint16_t)CAN_BYTE(0) << 1) | (CAN_BYTE(1) >> 7)) & 0x01FF;
    vcan_mains_available_chg_power = rawSpotPwr * 0.3;
    break;
  }

  case BCM_A7_ID:
  {
    // --- Door and light states ---
    if (((CAN_BYTE(2) >> 2) & 0x03) == 2)
    {
      vcan_door_trunk = true;
    }
    else
    {
      vcan_door_trunk = false;
    }

    if (((CAN_BYTE(3) >> 6) & 0x03) == 2)
    {
      vcan_door_fl = true;
    }
    else
    {
      vcan_door_fl = false;
    }

    if (((CAN_BYTE(3) >> 2) & 0x03) == 2)
    {
      vcan_door_fr = true;
    }
    else
    {
      vcan_door_fr = false;
    }

    if ((CAN_BYTE(3) & 0x03) == 2)
    {
      vcan_door_rl = true;
    }
    else
    {
      vcan_door_rl = false;
    }

    if (((CAN_BYTE(3) >> 4) & 0x03) == 2)
    {
      vcan_door_rr = true;
    }
    else
    {
      vcan_door_rr = false;
    }

    vcan_light_highbeam = CAN_BIT(6, 1);
    vcan_light_lowbeam = CAN_BIT(6, 5);
    break;
  }

  case HEVC_R17_ID:
  {
    // --- Available energy in battery ---
    vcan_hvBat_available_energy = ((((uint16_t)(CAN_BYTE(3) & 0x1F) << 6) | (CAN_BYTE(4) >> 2)) & 0x07FF) * 0.05;
    break;
  }

  case ATCU_A5_ID:
  {
    // --- Gear/direction; negative=reverse, 0=neutral, 1=forward ---
    // N/P = neutral, R = Reverse, CVT = forward
    uint8_t rawGear = (CAN_BYTE(2) >> 4) & 0x0F;
    switch (rawGear)
    {
    case 9:
      vcan_vehicle_gear = -1;
      break;
    case 10:
      vcan_vehicle_gear = 0;
      break;
    case 11:
      vcan_vehicle_gear = 1;
      break;
    }
    break;
  }

  case HEVC_R11_ID:
  {
    // --- Consumption and trip information ---
    uint16_t rawAuxCons = (((uint16_t)CAN_BYTE(0) << 4) | (CAN_BYTE(1) >> 4)) & 0x0FFF;
    vcan_lastTrip_AuxCons = rawAuxCons * 0.1;

    uint16_t rawAvgZEVCons = (((uint16_t)CAN_BYTE(3) << 2) | (CAN_BYTE(4) >> 6)) & 0x03FF;
    vcan_lastTrip_AvgCons = rawAvgZEVCons * 0.1;

    uint16_t rawAutonomyZEV = (((uint16_t)(CAN_BYTE(4) & 0x3F) << 4) | (CAN_BYTE(5) >> 4)) & 0x03FF;
    vcan_hvBat_estimatedRange = rawAutonomyZEV * 1.0;
    break;
  }

  case HEVC_R12_ID:
  {
    // --- Consumption and trip information ---
    uint16_t rawTotalCons = (((uint16_t)CAN_BYTE(0) << 4) | (CAN_BYTE(1) >> 4)) & 0x0FFF;
    vcan_lastTrip_Cons = rawTotalCons * 0.1;

    uint16_t rawTotalRec = CAN_BYTE(2);
    vcan_lastTrip_Recv = rawTotalRec * 0.1;
    break;
  }

  case BCM_A3_ID:
  {
    // --- Lights and PTC informations ---
    vcan_hvac_ptc_heater_active = (CAN_BYTE(3) >> 4) & 0x0F;
    vcan_light_fogFront = CAN_BIT(4, 7);
    vcan_light_brake = CAN_BIT(5, 6);
    vcan_light_fogRear = CAN_BIT(7, 0);

    break;
  }

  case HEVC_A22_ID:
  {
    // --- HV battery data ---
    uint16_t rawHVVolt = (((uint16_t)(CAN_BYTE(4) & 0x1F) << 8) | CAN_BYTE(5)) & 0x1FFF;
    vcan_hvBat_voltage = rawHVVolt * 0.1;

    break;
  }

  case HEVC_A1_ID:
  {
    // --- Plug and charge state ---
    uint8_t rawPlugState = (CAN_BYTE(0) >> 6) & 0x03;
    if (rawPlugState == 1 || rawPlugState == 2)
    {
      vcan_vehicle_ChgPlugState = true;
    }
    else
    {
      vcan_vehicle_ChgPlugState = false;
    }

    vcan_vehicle_ChargeState = (CAN_BYTE(2) >> 4) & 0x07;
    break;
  }

  case BCM_A2_ID:
  {
    // --- Tyre pressure ---
    vcan_tyrePressure_FL = CAN_BYTE(0) * 30.0;
    vcan_tyrePressure_FR = CAN_BYTE(1) * 30.0;
    vcan_tyrePressure_RL = CAN_BYTE(2) * 30.0;
    vcan_tyrePressure_RR = CAN_BYTE(3) * 30.0;
    break;
  }

  case HVAC_A2_ID:
  {
    // --- Compressor mode and power cons ---
    vcan_hvac_compressor_power = ((CAN_BYTE(1) >> 1) & 0x7F) * 50.0;
    break;
  }

  case BCM_A4_ID:
  {
    // --- Zoe "outside" lock state aka locked by HFM
    vcan_vehicle_LockState = CAN_BIT(2, 3);
    break;
  }

  case ECM_A6_ID:
  {
    // --- DC/DC Current and load ---
    vcan_dcdc_current = CAN_BYTE(2) * 1.0;
    vcan_dcdc_load = ((CAN_BYTE(4) >> 1) & 0x7F) * 1.0;
    break;
  }

  case BCM_A11_ID:
  {
    // --- TPMS alert states
    vcan_tyreState_FR = (CAN_BYTE(2) >> 2) & 0x07;
    vcan_tyreState_FL = (CAN_BYTE(2) >> 5) & 0x07;
    vcan_tyreState_RR = (CAN_BYTE(3) >> 2) & 0x07;
    vcan_tyreState_RL = (CAN_BYTE(3) >> 5) & 0x07;
    break;
  }

  case HEVC_A2_ID:
  {
    // --- HV battery SoC and Charge time remaining ---
    vcan_hvBat_SoC = (CAN_BYTE(3) & 0x7F) * 1.0;

    uint16_t rawRemTime = (((uint16_t)CAN_BYTE(2) << 1) | ((CAN_BYTE(3) >> 7) & 0x01)) & 0x01FF;
    vcan_hvBat_ChargeTimeRemaining = rawRemTime * 5.0;

    break;
  }

  case HEVC_A23_ID:
  {
    // --- HV battery allowed max charging / recuperating power ---
    uint16_t rawChargePwr = (((uint16_t)(CAN_BYTE(4) & 0x0F) << 8) | ((uint16_t)CAN_BYTE(5))) & 0x0FFF;
    vcan_hvBat_max_chg_power = rawChargePwr * 0.1;
    break;
  }

  case METER_A5_ID:
  {
    // --- Trip distance from METER ---
    uint32_t rawTripDist = (((uint32_t)CAN_BYTE(4) << 9) | ((uint32_t)CAN_BYTE(5) << 1) | ((CAN_BYTE(6) >> 7) & 0x01)) & 0x1FFFF;
    vcan_lastTrip_Distance = rawTripDist * 0.1;

    // read trip distance unit, later for other country usage
    // uint8_t rawTripUnit = (CAN_BYTE(3) >> 4) & 0x03;
    // const char *unitStr = (rawTripUnit == 1) ? "mi" : "km";

    break;
  }

  case HFM_A1_ID:
  {
    // --- HFM (Hand Free Module) commands for remote climate trigger ---
    //ESP_LOGD(TAG, "HFM_A1_ID: %02X %02X %02X %02X %02X %02X %02X %02X",
    //         data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);

    // Monitor for "Remote Lighting" button press (HFM_RKE_Request = 8) to trigger climate control
    if (m_remote_climate_enabled && m_vcan_enabled)
    {
      uint8_t hfm_rke_request = (CAN_BYTE(3) >> 3) & 0x1F;

      // Value 8 = "Short Press - Remote Lighting"
      // Implement debouncing to prevent multiple triggers from single button press
      if (hfm_rke_request == 8 && m_remote_climate_last_button_state != 8)
      {
        // Button state changed from not-pressed to pressed (edge detection)
        uint32_t current_time = esp_log_timestamp();
        uint32_t time_since_last_trigger = current_time - m_remote_climate_last_trigger_time;

        // Require at least 30 seconds between triggers to prevent accidental repeated activations,
        // this is the time the headlights stays on
        if (time_since_last_trigger > 30000 || m_remote_climate_last_trigger_time == 0)
        {
          ESP_LOGI(TAG, "Remote lighting button pressed on key fob, triggering climate control");
          CommandClimateControl(true);
          m_remote_climate_last_trigger_time = current_time;
        }
        else
        {
          ESP_LOGD_RZ2(TAG, "Remote lighting button pressed, but ignoring due to debounce (last trigger %dms ago)",
                   time_since_last_trigger);
        }
      }

      // Save current button state for next iteration
      m_remote_climate_last_button_state = hfm_rke_request;
    }
    break;
  }
  }
}

// Sync VCAN datapoints to OVMS metrics, called by Ticker1, because VCAN refresh rate is too high
void OvmsVehicleRenaultZoePh2::SyncVCANtoMetrics()
{
  // OVMS Standard and Zoe custom metrics
  // HVAC (ClimBox ECU)
  StandardMetrics.ms_v_env_cabinsetpoint->SetValue(vcan_hvac_cabin_setpoint, Celcius);
  StandardMetrics.ms_v_env_cabinfan->SetValue(vcan_hvac_blower, Percentage);
  mt_hvac_compressor_power->SetValue(vcan_hvac_compressor_power, Watts);
  // BCM (Body control module under steering wheel)
  StandardMetrics.ms_v_env_locked->SetValue(vcan_vehicle_LockState);
  StandardMetrics.ms_v_door_fl->SetValue(vcan_door_fl);
  StandardMetrics.ms_v_door_fr->SetValue(vcan_door_fr);
  StandardMetrics.ms_v_door_rl->SetValue(vcan_door_rl);
  StandardMetrics.ms_v_door_rr->SetValue(vcan_door_rr);
  StandardMetrics.ms_v_door_trunk->SetValue(vcan_door_trunk);
  StandardMetrics.ms_v_env_headlights->SetValue(vcan_light_lowbeam);
  mt_hvac_heating_ptc_req->SetValue(vcan_hvac_ptc_heater_active); // Command comes from HVAC sent to BCM which switches relays
  // USM aka UPC (Switching box under Engine hood)
  StandardMetrics.ms_v_env_on->SetValue(vcan_vehicle_ignition);
  StandardMetrics.ms_v_env_awake->SetValue(vcan_vehicle_ignition);
  StandardMetrics.ms_v_door_hood->SetValue(vcan_door_engineHood);
  StandardMetrics.ms_v_charge_12v_voltage->SetValue(vcan_dcdc_voltage);
  // METER (Instrument cluster aka TdB)
  StandardMetrics.ms_v_pos_speed->SetValue(vcan_vehicle_speed, Kph);
  StandardMetrics.ms_v_pos_trip->SetValue(vcan_lastTrip_Distance);
  // HEVC, ECM (Electric vehicle controller)
  StandardMetrics.ms_v_bat_12v_current->SetValue(vcan_dcdc_current, Amps);
  StandardMetrics.ms_v_charge_12v_current->SetValue(vcan_dcdc_current, Amps);
  StandardMetrics.ms_v_bat_capacity->SetValue(vcan_hvBat_available_energy, kWh);
  StandardMetrics.ms_v_bat_energy_used->SetValue(vcan_lastTrip_Cons, kWh);
  StandardMetrics.ms_v_bat_energy_recd->SetValue(vcan_lastTrip_Recv, kWh);
  StandardMetrics.ms_v_bat_consumption->SetValue(vcan_lastTrip_AvgCons, kWhP100K);
  StandardMetrics.ms_v_bat_voltage->SetValue(vcan_hvBat_voltage, Volts);
  StandardMetrics.ms_v_charge_pilot->SetValue(vcan_vehicle_ChgPlugState);
  mt_bat_max_charge_power->SetValue(vcan_hvBat_max_chg_power, kW);
  mt_main_power_available->SetValue(vcan_mains_available_chg_power, kW);
  mt_bat_cons_aux->SetValue(vcan_lastTrip_AuxCons, kWh);
  // ATCU (Automatic shifter control ECU, Zoe didnt have that, these message comes from HEVC I think)
  StandardMetrics.ms_v_env_gear->SetValue(vcan_vehicle_gear);

  // Some metrics needs a bit logic to suppress invalid or out of range values at boot up
  // --
  // DCDC load will set to 100% if DCDC is switched off
  if (vcan_dcdc_current != 0)
  {
    mt_12v_dcdc_load->SetValue(vcan_dcdc_load, Percentage);
  }
  else
  {
    mt_12v_dcdc_load->SetValue(0, Percentage);
  }

  // HV battery SoC, stay at 99% if charging is running, like dashboard and official app
  if (vcan_hvBat_SoC == 100 && StandardMetrics.ms_v_charge_inprogress->AsBool())
  {
    StandardMetrics.ms_v_bat_soc->SetValue(99, Percentage);
  }
  else
  {
    // LBC boot up, suppress invalid values
    if (vcan_hvBat_SoC > 0 && vcan_hvBat_SoC < 101)
    {
      StandardMetrics.ms_v_bat_soc->SetValue(vcan_hvBat_SoC, Percentage);
    }
  }

  // HEVC boot up, suppress invalid values
  if (vcan_hvBat_estimatedRange < 500 && vcan_hvBat_estimatedRange > 0)
  {
    StandardMetrics.ms_v_bat_range_est->SetValue(vcan_hvBat_estimatedRange, Kilometers);
  }

  // Tyre pressure, if over 7000 millibars, BCM values are stale
  if (vcan_tyrePressure_FL < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, vcan_tyrePressure_FL * 0.001, Bar);
  }
  if (vcan_tyrePressure_FR < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, vcan_tyrePressure_FR * 0.001, Bar);
  }
  if (vcan_tyrePressure_RL < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, vcan_tyrePressure_RL * 0.001, Bar);
  }
  if (vcan_tyrePressure_RR < 7000)
  {
    StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, vcan_tyrePressure_RR * 0.001, Bar);
  }

  // Tyre state values to TPMS warning and alerts
  if (vcan_tyreState_FL == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 0);
  }
  else if (vcan_tyreState_FL == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 2);
  }

  if (vcan_tyreState_FR == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 0);
  }
  else if (vcan_tyreState_FR == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 2);
  }

  if (vcan_tyreState_RL == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 0);
  }
  else if (vcan_tyreState_RL == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 2);
  }

  if (vcan_tyreState_RR == 0)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 0);
  }
  else if (vcan_tyreState_RR == 2)
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 1);
  }
  else
  {
    StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 2);
  }

  // Charge state machine from V-CAN
  if (vcan_vehicle_ChargeState == 0)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_door_chargeport->SetValue(false);
    StandardMetrics.ms_v_charge_climit->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 1)
  {
    StandardMetrics.ms_v_charge_state->SetValue("timerwait");
    StandardMetrics.ms_v_charge_substate->SetValue("timerwait");
  }
  if (vcan_vehicle_ChargeState == 2)
  {
    StandardMetrics.ms_v_charge_state->SetValue("done");
    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 3)
  {
    StandardMetrics.ms_v_charge_state->SetValue("charging");
    StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
    StandardMetrics.ms_v_charge_inprogress->SetValue(true);
    StandardMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
  }
  if (vcan_vehicle_ChargeState == 4)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 5)
  {
    StandardMetrics.ms_v_charge_state->SetValue("stopped");
    StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
  if (vcan_vehicle_ChargeState == 6)
  {
    StandardMetrics.ms_v_door_chargeport->SetValue(true);
  }
  if (vcan_vehicle_ChargeState == 8)
  {
    StandardMetrics.ms_v_charge_state->SetValue("prepare");
    StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
    StandardMetrics.ms_v_charge_power->SetValue(0);
  }
}
