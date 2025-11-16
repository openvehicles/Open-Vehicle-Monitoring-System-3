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

void OvmsVehicleRenaultZoePh2::IncomingUCM(uint16_t type, uint16_t pid, const char *data, uint16_t len)
{
  switch (pid)
  {
  case 0x6079:
  { // 12V Battery Current
    StandardMetrics.ms_v_charge_12v_current->SetValue((float)(CAN_UINT(0) * 0.1), Amps);
    StandardMetrics.ms_v_bat_12v_current->SetValue((float)(CAN_UINT(0) * 0.1), Amps);
    // ESP_LOGD(TAG, "6079 UCM ms_v_charge_12v_current: %f", CAN_UINT(0) * 0.1);
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
      ESP_LOGW(TAG, "OBD2: unhandled reply from UCM [%02x %02x]: %s", type, pid, buf ? buf : "-");
    } while (rlen);
    if (buf)
      free(buf);
    break;
  }
  }
}