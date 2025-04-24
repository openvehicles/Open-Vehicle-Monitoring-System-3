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

void OvmsVehicleRenaultZoePh2::IncomingINV(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  switch (pid)
  {
  case 0x700C:
  { // Inverter temperature
    StandardMetrics.ms_v_inv_temp->SetValue(float((CAN_UINT24(0) * 0.001953125) - 40), Celcius);
    // Zoe has no dedicated charger, it uses chameleon charger which uses motor and inverter to charge the battery, so we can use inverter temperature as charger temperature
    StandardMetrics.ms_v_charge_temp->SetValue(float((CAN_UINT24(0) * 0.001953125) - 40), Celcius);
    // ESP_LOGD(TAG, "700C INV ms_v_inv_temp RAW: %f", float(CAN_UINT24(0)));
    // ESP_LOGD(TAG, "700C INV ms_v_inv_temp: %f", float((CAN_UINT24(0) * 0.001953125) - 40));
    break;
  }
  case 0x700F:
  { // Motor, Stator1 temperature
    StandardMetrics.ms_v_mot_temp->SetValue(float((CAN_UINT24(0) * 0.001953125) - 40), Celcius);
    mt_mot_temp_stator1->SetValue(float((CAN_UINT24(0) * 0.001953125) - 40), Celcius);
    // ESP_LOGD(TAG, "700F INV ms_v_mot_temp: %f", float((CAN_UINT24(0) * 0.001953125) - 40));
    break;
  }
  case 0x7010:
  { // Stator 2 temperature
    mt_mot_temp_stator2->SetValue(float((CAN_UINT24(0) * 0.001953125) - 40), Celcius);
    // ESP_LOGD(TAG, "7010 INV mt_mot_temp_stator2: %f", float((CAN_UINT24(0) * 0.001953125) - 40));
    break;
  }
  case 0x2004:
  { // Battery voltage sense
    mt_inv_hv_voltage->SetValue(float(CAN_UINT(0) * 0.03125), Volts);
    // ESP_LOGD(TAG, "2004 INV mt_inv_hv_voltage: %f", float(CAN_UINT(0) * 0.03125));
    StandardMetrics.ms_v_inv_power->SetValue(float(mt_inv_hv_voltage->AsFloat() * mt_inv_hv_current->AsFloat()) * 0.001);
    break;
  }
  case 0x7049:
  { // Battery current sense
    mt_inv_hv_current->SetValue(float((CAN_UINT(0) * 0.03125) - 500), Amps);
    // ESP_LOGD(TAG, "7049 INV mt_inv_hv_current: %f", float(CAN_UINT(0) * 0.003125) - 500);
    StandardMetrics.ms_v_inv_power->SetValue(float(mt_inv_hv_current->AsFloat() * mt_inv_hv_voltage->AsFloat()) * 0.001);
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
      ESP_LOGW(TAG, "OBD2: unhandled reply from INV [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}