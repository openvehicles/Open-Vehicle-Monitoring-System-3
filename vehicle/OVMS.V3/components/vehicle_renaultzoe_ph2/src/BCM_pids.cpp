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

void OvmsVehicleRenaultZoePh2::IncomingBCM(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  switch (pid)
  {
  case 0x6300:
  { // TPMS pressure - front left
    if ((CAN_UINT(0) * 7.5) < 7672)
    {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, (float)CAN_UINT(0) * 7.5 / 10, kPa);
    }
    // ESP_LOGD(TAG, "6300 BCM tpms pressure FL: %f", CAN_UINT(0) * 7.5);
    break;
  }
  case 0x6301:
  { // TPMS pressure - front right
    if ((CAN_UINT(0) * 7.5) < 7672)
    {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, (float)CAN_UINT(0) * 7.5 / 10, kPa);
    }
    // ESP_LOGD(TAG, "6301 BCM tpms pressure FR: %f", CAN_UINT(0) * 7.5);
    break;
  }
  case 0x6302:
  { // TPMS pressure - rear left
    if ((CAN_UINT(0) * 7.5) < 7672)
    {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, (float)CAN_UINT(0) * 7.5 / 10, kPa);
    }
    // ESP_LOGD(TAG, "6302 BCM tpms pressure RL: %f", CAN_UINT(0) * 7.5);
    break;
  }
  case 0x6303:
  { // TPMS pressure - rear right
    if ((CAN_UINT(0) * 7.5) < 7672)
    {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, (float)CAN_UINT(0) * 7.5 / 10, kPa);
    }
    // ESP_LOGD(TAG, "6303 BCM tpms pressure RR: %f", CAN_UINT(0) * 7.5);
    break;
  }
  case 0x6310:
  { // TPMS temp - front left
    if (CAN_BYTE(0) < 127)
    {
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, CAN_BYTE(0) - 30, Celcius);
    }
    // ESP_LOGD(TAG, "6310 BCM tpms temp FL: %d", CAN_NIB(0));
    break;
  }
  case 0x6311:
  { // TPMS temp - front right
    if (CAN_BYTE(0) < 127)
    {
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, CAN_BYTE(0) - 30, Celcius);
    }
    // ESP_LOGD(TAG, "6311 BCM tpms temp FR: %d", CAN_NIBL(0));
    break;
  }
  case 0x6312:
  { // TPMS temp - rear left
    if (CAN_BYTE(0) < 127)
    {
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, CAN_BYTE(0) - 30, Celcius);
    }
    // ESP_LOGD(TAG, "6312 BCM tpms temp RL: %d", CAN_NIBH(0));
    break;
  }
  case 0x6313:
  { // TPMS temp - rear right
    if (CAN_BYTE(0) < 127)
    {
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, CAN_BYTE(0) - 30, Celcius);
    }
    // ESP_LOGD(TAG, "6313 BCM tpms temp RR: %d", CAN_BYTE(0));
    break;
  }
  case 0x4109:
  { // TPMS alert - front left
    if (CAN_UINT(0) == 0)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 0);
    }
    if (CAN_UINT(0) == 1 || CAN_UINT(0) == 3 || CAN_UINT(0) == 5 || CAN_UINT(0) == 7)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 2);
    }
    if (CAN_UINT(0) == 2 || CAN_UINT(0) == 4 || CAN_UINT(0) == 6)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FL, 1);
    }
    // ESP_LOGD(TAG, "40FF BCM tpms alert FL: %d", CAN_UINT(0));
    break;
  }
  case 0x410A:
  { // TPMS alert - front right
    if (CAN_UINT(0) == 0)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 0);
    }
    if (CAN_UINT(0) == 1 || CAN_UINT(0) == 3 || CAN_UINT(0) == 5 || CAN_UINT(0) == 7)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 2);
    }
    if (CAN_UINT(0) == 2 || CAN_UINT(0) == 4 || CAN_UINT(0) == 6)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_FR, 1);
    }
    // ESP_LOGD(TAG, "40FF BCM tpms alert FR: %d", CAN_UINT(0));
    break;
  }
  case 0x410B:
  { // TPMS alert - rear left
    if (CAN_UINT(0) == 0)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 0);
    }
    if (CAN_UINT(0) == 1 || CAN_UINT(0) == 3 || CAN_UINT(0) == 5 || CAN_UINT(0) == 7)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 2);
    }
    if (CAN_UINT(0) == 2 || CAN_UINT(0) == 4 || CAN_UINT(0) == 6)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RL, 1);
    }
    // ESP_LOGD(TAG, "40FF BCM tpms alert RL: %d", CAN_UINT(0));
    break;
  }
  case 0x410C:
  { // TPMS alert - rear right
    if (CAN_UINT(0) == 0)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 0);
    }
    if (CAN_UINT(0) == 1 || CAN_UINT(0) == 3 || CAN_UINT(0) == 5 || CAN_UINT(0) == 7)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 2);
    }
    if (CAN_UINT(0) == 2 || CAN_UINT(0) == 4 || CAN_UINT(0) == 6)
    {
      StandardMetrics.ms_v_tpms_alert->SetElemValue(MS_V_TPMS_IDX_RR, 1);
    }
    // ESP_LOGD(TAG, "40FF BCM tpms alert RR: %d", CAN_UINT(0));
    break;
  }
  case 0x8004:
  { // Car secure aka vehicle locked
    StandardMetrics.ms_v_env_locked->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "8004 BCM Car Secure S: %d", CAN_UINT(0));
    break;
  }
  case 0x6026:
  { // Front left door
    StandardMetrics.ms_v_door_fl->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "6026 BCM Front left door: %d", CAN_UINT(0));
    break;
  }
  case 0x6027:
  { // Front right door
    StandardMetrics.ms_v_door_fr->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "6027 BCM Front right door: %d", CAN_UINT(0));
    break;
  }
  case 0x61B2:
  { // Rear left door
    StandardMetrics.ms_v_door_rl->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "61B2 BCM Rear left door: %d", CAN_UINT(0));
    break;
  }
  case 0x61B3:
  { // Rear right door
    StandardMetrics.ms_v_door_rr->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "61B3 Rear right door: %d", CAN_UINT(0));
    break;
  }
  case 0x609B:
  { // Tailgate
    StandardMetrics.ms_v_door_trunk->SetValue((bool)CAN_UINT(0));
    // ESP_LOGD(TAG, "609B Tailgate: %d", CAN_UINT(0));
    break;
  }
  case 0x4186:
  { // Low beam lights
    StandardMetrics.ms_v_env_headlights->SetValue((bool)CAN_UINT(0));
    /*if ((bool)CAN_UINT(0)) {
      ESP_LOGD(TAG, "4186 Low beam lights: active");
    } else {
      ESP_LOGD(TAG, "4186 Low beam lights: inactive");
    }*/
    break;
  }
  case 0x60C6:
  { // Ignition relay (switch)
    /*if ((bool)CAN_UINT(0)) {
      ESP_LOGD(TAG, "60C6 Ignition relay: active");
    } else {
      ESP_LOGD(TAG, "60C6 Ignition relay: inactive");
    }*/
    if (!CarIsCharging)
    { // Ignore Igniton while charging
      StandardMetrics.ms_v_env_on->SetValue((bool)CAN_UINT(0));
    }
    StandardMetrics.ms_v_env_awake->SetValue((bool)CAN_UINT(0));
    break;
  }
  case 0x4060:
  { // Vehicle identificaftion number
    zoe_vin[0] = CAN_BYTE(0);
    zoe_vin[1] = CAN_BYTE(1);
    zoe_vin[2] = CAN_BYTE(2);
    zoe_vin[3] = CAN_BYTE(3);
    zoe_vin[4] = CAN_BYTE(4);
    zoe_vin[5] = CAN_BYTE(5);
    zoe_vin[6] = CAN_BYTE(6);
    zoe_vin[7] = CAN_BYTE(7);
    zoe_vin[8] = CAN_BYTE(8);
    zoe_vin[9] = CAN_BYTE(9);
    zoe_vin[10] = CAN_BYTE(10);
    zoe_vin[11] = CAN_BYTE(11);
    zoe_vin[12] = CAN_BYTE(12);
    zoe_vin[13] = CAN_BYTE(13);
    zoe_vin[14] = CAN_BYTE(14);
    zoe_vin[15] = CAN_BYTE(15);
    zoe_vin[16] = CAN_BYTE(16);
    zoe_vin[17] = 0;
    StandardMetrics.ms_v_vin->SetValue((string)zoe_vin);
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
      ESP_LOGW(TAG, "OBD2: unhandled reply from BCM [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}