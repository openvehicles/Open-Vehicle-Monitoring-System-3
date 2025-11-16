/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#include "vehicle_renaultzoe_ph2.h"

void OvmsVehicleRenaultZoePh2::IncomingEVC(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  float temp = 0;
  switch (pid)
  {
  case 0x2006:
  { // Odometer (Total Vehicle Distance)
    StandardMetrics.ms_v_pos_odometer->SetValue((float)CAN_UINT24(0), Kilometers);
    // ESP_LOGD(TAG, "2006 EVC ms_v_pos_odometer: %d", CAN_UINT24(0));
    break;
  }
  case 0x2003:
  { // Vehicle Speed
    StandardMetrics.ms_v_pos_speed->SetValue((float)(CAN_UINT(0) * 0.01), KphPS);
    // ESP_LOGD(TAG, "2003 EVC ms_v_pos_speed: %f", CAN_UINT(0) * 0.01);
    break;
  }
  case 0x2005:
  { // 12V Battery Voltage
    StandardMetrics.ms_v_charge_12v_voltage->SetValue((float)(CAN_UINT(0) * 0.01), Volts);
    // ESP_LOGD(TAG, "2005 EVC ms_v_charge_12v_voltage: %f", CAN_UINT(0) * 0.01);
    break;
  }
  case 0x21CF:
  { // Inverter status
    // ESP_LOGD(TAG, "21CF EVC mt_inv_status: %d", CAN_NIBL(0));
    if (CAN_NIBL(0) == 1)
    {
      mt_inv_status->SetValue("Inverter off");
    }
    else if (CAN_NIBL(0) == 2)
    {
      mt_inv_status->SetValue("Inverter on");
    }
    else if (CAN_NIBL(0) == 3)
    {
      mt_inv_status->SetValue("Inverter decharging");
    }
    else if (CAN_NIBL(0) == 4)
    {
      mt_inv_status->SetValue("Inverter alternator mode");
    }
    else if (CAN_NIBL(0) == 5)
    {
      mt_inv_status->SetValue("Inverter ready to sleep");
    }
    else
    {
      mt_inv_status->SetValue("Inverter state unknown");
    }
    break;
  }
  case 0x2218:
  { // Ambient temperature
    temp = (float)(CAN_UINT(0) * 0.1 - 273);
    if ((temp > -39) && (temp < 80))
    {
      StandardMetrics.ms_v_env_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "2218 EVC ms_v_env_temp: %f", (CAN_UINT(0) * 0.1 - 273));
    break;
  }
  case 0x2B85:
  { // Charge plug preset
    // ESP_LOGD(TAG, "2B85 EVC Charge plug present: %d", CAN_NIBL(0));
    if (CAN_NIBL(0) == 1)
    {
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      if (!CarPluggedIn)
      {
        ESP_LOGI(TAG, "Charge cable plugged in");
        CarPluggedIn = true;
      }
    }
    if (CAN_NIBL(0) == 0)
    {
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      if (CarPluggedIn)
      {
        ESP_LOGI(TAG, "Charge cable plugged out");
        CarPluggedIn = false;
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
      }
    }
    break;
  }
  case 0x2B6D:
  { // Charge MMI States
    // ESP_LOGD(TAG, "2B6D Charge MMI States RAW: %d", CAN_NIBL(0));
    if (CAN_NIBL(0) == 0)
    {
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_door_chargeport->SetValue(false);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : No Charge");
    }
    if (CAN_NIBL(0) == 1)
    {
      StandardMetrics.ms_v_charge_state->SetValue("timerwait");
      StandardMetrics.ms_v_charge_substate->SetValue("timerwait");
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Waiting for a planned charge");
    }
    if (CAN_NIBL(0) == 2)
    {
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_power->SetValue(0);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Ended charge");
    }
    if (CAN_NIBL(0) == 3)
    {
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_timestamp->SetValue(StdMetrics.ms_m_timeutc->AsInt());
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Charge in progress");
    }
    if (CAN_NIBL(0) == 4)
    {
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_power->SetValue(0);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Charge failure");
    }
    if (CAN_NIBL(0) == 5)
    {
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_power->SetValue(0);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Waiting for current charge");
    }
    if (CAN_NIBL(0) == 6)
    {
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Chargeport opened");
      ESP_LOGI(TAG, "Chargedoor opened");
    }
    if (CAN_NIBL(0) == 8)
    {
      StandardMetrics.ms_v_charge_state->SetValue("prepare");
      StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_power->SetValue(0);
      // ESP_LOGD(TAG, "2B6D Charge MMI States : Charge preparation");
    }
    if (!mt_bus_awake->AsBool())
    {
      ZoeWakeUp();
    }
    break;
  }
  case 0x2B7A:
  { // Charge type
    // ESP_LOGD(TAG, "2B7A EVC Charge type: %d", (CAN_NIBL(0)));
    if (CAN_NIBL(0) == 0)
    {
      StandardMetrics.ms_v_charge_type->SetValue("undefined");
    }
    if (CAN_NIBL(0) == 1 || CAN_NIBL(0) == 2)
    {
      StandardMetrics.ms_v_charge_type->SetValue("type2");
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      // Set Charge Limit to 32A, in BCB there is a current setting, but its value will be ignored and cannot be set during charging, so its useless
      StandardMetrics.ms_v_charge_climit->SetValue(32);
    }
    if (CAN_NIBL(0) == 3)
    {
      StandardMetrics.ms_v_charge_type->SetValue("chademo");
      StandardMetrics.ms_v_charge_mode->SetValue("performance");
    }
    if (CAN_NIBL(0) == 4)
    {
      StandardMetrics.ms_v_charge_type->SetValue("ccs");
      StandardMetrics.ms_v_charge_mode->SetValue("performance");
      StandardMetrics.ms_v_charge_climit->SetValue(150);
    }
    break;
  }
  case 0x3064:
  { // Motor rpm
    StandardMetrics.ms_v_mot_rpm->SetValue((float)(CAN_UINT(0)));
    // ESP_LOGD(TAG, "3064 EVC ms_v_mot_rpm: %d", (CAN_UINT(0)));
    break;
  }
  case 0x300F:
  { // AC charging power available
    mt_main_power_available->SetValue((float)(CAN_UINT(0) * 0.025), kW);
    // ESP_LOGD(TAG, "300F EVC mt_main_power_available: %f", (CAN_UINT(0) * 0.025));
    break;
  }
  case 0x300D:
  { // AC input current
    StandardMetrics.ms_v_charge_current->SetValue((float)(CAN_UINT(0) * 0.1), Amps);
    // Power factor measured with a Janitza UMG512 Class A power analyser to get more precision
    // Only three phases measurement at the moment
    if (StandardMetrics.ms_v_charge_current->AsFloat() > 19.0f)
    {
      ACInputPowerFactor = 1.0;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 18.0f)
    {
      ACInputPowerFactor = 0.997;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 17.0f)
    {
      ACInputPowerFactor = 0.99;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 16.0f)
    {
      ACInputPowerFactor = 0.978;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 15.0f)
    {
      ACInputPowerFactor = 0.948;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 14.0f)
    {
      ACInputPowerFactor = 0.931;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 13.0f)
    {
      ACInputPowerFactor = 0.916;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 12.0f)
    {
      ACInputPowerFactor = 0.902;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 11.0f)
    {
      ACInputPowerFactor = 0.888;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 10.0f)
    {
      ACInputPowerFactor = 0.905;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 9.0f)
    {
      ACInputPowerFactor = 0.929;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 8.0f)
    {
      ACInputPowerFactor = 0.901;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() > 7.0f)
    {
      ACInputPowerFactor = 0.775;
    }
    else if (StandardMetrics.ms_v_charge_current->AsFloat() < 6.0f && StandardMetrics.ms_v_charge_inprogress->AsBool(false))
    {
      ACInputPowerFactor = 0.5;
    }
    if (StandardMetrics.ms_v_charge_type->AsString() == "type2" && mt_main_phases_num->AsFloat() == 3 && StandardMetrics.ms_v_charge_inprogress->AsBool(false))
    {
      StandardMetrics.ms_v_charge_power->SetValue((StandardMetrics.ms_v_charge_current->AsFloat() * StandardMetrics.ms_v_charge_voltage->AsFloat() * ACInputPowerFactor * 1.732f) * 0.001, kW);
    }
    else if (StandardMetrics.ms_v_charge_type->AsString() == "type2" && (mt_main_phases_num->AsFloat() == 2 || mt_main_phases_num->AsFloat() == 1))
    {
      StandardMetrics.ms_v_charge_power->SetValue((StandardMetrics.ms_v_charge_current->AsFloat() * StandardMetrics.ms_v_charge_voltage->AsFloat() * ACInputPowerFactor) * 0.001, kW);
    }
    else if (StandardMetrics.ms_v_charge_type->AsString() == "type2")
    {
      StandardMetrics.ms_v_charge_power->SetValue(0);
    }
    // ESP_LOGD(TAG, "300D EVC mt_main_current: %f", (CAN_UINT(0) * 0.1));
    break;
  }
  case 0x300B:
  { // AC phases used
    // ESP_LOGD(TAG, "300B EVC mt_main_phases: %d", (CAN_NIBL(0)));
    if (CAN_NIBL(0) == 0)
    {
      mt_main_phases->SetValue("one phase");
      mt_main_phases_num->SetValue(1);
    }
    if (CAN_NIBL(0) == 1)
    {
      mt_main_phases->SetValue("two phase");
      mt_main_phases_num->SetValue(2);
    }
    if (CAN_NIBL(0) == 2)
    {
      mt_main_phases->SetValue("three phase");
      mt_main_phases_num->SetValue(3);
    }
    if (CAN_NIBL(0) == 3)
    {
      mt_main_phases->SetValue("not detected");
      mt_main_phases_num->SetValue(0);
    }
    break;
  }
  case 0x2B8A:
  { // AC mains voltage
    StandardMetrics.ms_v_charge_voltage->SetValue((float)(CAN_UINT(0) * 0.5), Volts);
    // ESP_LOGD(TAG, "2B8A EVC ms_v_charge_voltage: %f", (CAN_UINT(0) * 0.5));
    break;
  }

  default:
  {
    char *buf = NULL;
    size_t rlen = len, offset = 0;
    do
    {
      rlen = FormatHexDump(&buf, data + offset, rlen, 16);
      offset += 16;
      ESP_LOGW(TAG, "OBD2: unhandled reply from EVC [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}