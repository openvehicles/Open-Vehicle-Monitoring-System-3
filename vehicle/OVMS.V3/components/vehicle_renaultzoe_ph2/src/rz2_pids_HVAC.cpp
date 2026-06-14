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

void OvmsVehicleRenaultZoePh2::IncomingHVAC(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  float temp = 0;
  switch (pid)
  {
  case 0x4009:
  { // Cabin temperature
    StandardMetrics.ms_v_env_cabintemp->SetValue(float((CAN_INT(0) - 400) / 10.0f), Celcius);
    // ESP_LOGD(TAG, "4361 HVAC ms_v_env_cabintemp: %f", float((CAN_UINT(0) - 400) / 10));
    break;
  }
  case 0x43D8:
  { // Compressor speed
    mt_hvac_compressor_speed->SetValue(float(CAN_UINT(0)));
    // ESP_LOGD(TAG, "43D8 HVAC mt_hvac_compressor_speed: %d", CAN_UINT(0));
    break;
  }
  case 0x4404:
  { // Compressor speed request - only in heat pumpe mode, if ac mode the compressor ECU regulate itself
    mt_hvac_compressor_speed_req->SetValue(float(CAN_UINT(0)));
    // ESP_LOGD(TAG, "4404 HVAC mt_hvac_compressor_speed_req: %d", CAN_UINT(0));
    break;
  }
  case 0x440C:
  { // Compressor discharge temperature - invalid values on ECU boot-up
    temp = float(((CAN_UINT(0) * 5) - 300) / 10);
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_compressor_discharge_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "440C HVAC mt_hvac_compressor_discharge_temp: %d", temp);
    break;
  }
  case 0x440D:
  { // Compressor suction temperature - invalid values on ECU boot-up
    temp = float(((CAN_UINT(0) * 5) - 300) / 10);
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_compressor_suction_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "440D HVAC mt_hvac_compressor_suction_temp: %d", CAN_UINT(0));
    break;
  }
  case 0x4368:
  { // Cooling evap setpoint temp - invalid values on ECU boot-up
    temp = float((CAN_UINT(0) - 400) / 10);
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_cooling_evap_setpoint->SetValue(temp);
    }
    // ESP_LOGD(TAG, "4368 HVAC mt_hvac_cooling_evap_setpoint: %d", CAN_UINT(0));
    break;
  }
  case 0x400A:
  { // Cooling evap temp - invalid values on ECU boot-up
    temp = float((CAN_UINT(0) - 400) / 10);
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_cooling_evap_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "400A HVAC mt_hvac_cooling_evap_temp: %d", CAN_UINT(0));
    break;
  }
  case 0x43A5:
  { // Heating cond setpoint temp - invalid values on ECU boot-up
    temp = float((CAN_UINT(0) * 0.01953125) - 400) / 10;
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_heating_cond_setpoint->SetValue(temp);
    }
    // ESP_LOGD(TAG, "43A5 HVAC mt_hvac_heating_cond_setpoint: %d", CAN_UINT(0));
    break;
  }
  case 0x4429:
  { // Heating cond temp - invalid values on ECU boot-up
    temp = float((CAN_UINT(0) * 0.39063) - 4000) / 100;
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_heating_cond_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "4429 HVAC mt_hvac_heating_cond_temp: %d", CAN_UINT(0));
    break;
  }
  case 0x43A4:
  { // Heating temp - invalid values on ECU boot-up
    temp = float((CAN_UINT(0) * 0.39063) - 4000) / 100;
    if ((temp > 0) && (temp < 80))
    {
      mt_hvac_heating_temp->SetValue(temp);
    }
    // ESP_LOGD(TAG, "43A4 HVAC mt_hvac_heating_temp: %d", CAN_UINT(0));
    break;
  }
  case 0x4341:
  { // Heating ptc request (0-5)
    mt_hvac_heating_ptc_req->SetValue(float((CAN_NIBL(0) & 0x0F) % 6));
    // ESP_LOGD(TAG, "4341 HVAC mt_hvac_heating_ptc_req: %d", CAN_UINT(0));
    break;
  }
  case 0x4402:
  { // Compressor state
    if (CAN_NIBL(0) == 1)
    {
      mt_hvac_compressor_mode->SetValue("AC mode");
    }
    if (CAN_NIBL(0) == 2)
    {
      mt_hvac_compressor_mode->SetValue("De-ICE mode");
    }
    if (CAN_NIBL(0) == 4)
    {
      mt_hvac_compressor_mode->SetValue("Heat pump mode");
    }
    if (CAN_NIBL(0) == 6)
    {
      mt_hvac_compressor_mode->SetValue("Demisting mode");
    }
    if (CAN_NIBL(0) == 7)
    {
      mt_hvac_compressor_mode->SetValue("idle");
    }
    // ESP_LOGD(TAG, "%d HVAC mt_hvac_compressor_mode: %d", pid,  CAN_UINT(0));
    break;
  }
  case 0x4369:
  { // Compressor pressure
    mt_hvac_compressor_pressure->SetValue(float(CAN_UINT(0) * 0.1));
    // ESP_LOGD(TAG, "%d HVAC mt_hvac_compressor_pressure: %f", pid,  CAN_UINT(0) * 0.1);
    break;
  }
  case 0x4436:
  { // Compressor power
    mt_hvac_compressor_power->SetValue(float(CAN_UINT(0) * 25.0 / 100.0));
    // ESP_LOGD(TAG, "%d HVAC mt_hvac_compressor_power: %f", pid,  CAN_UINT(0) * 25.0 / 100.0);
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
      ESP_LOGW(TAG, "OBD2: unhandled reply from HVAC [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}