/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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

#include "ovms_log.h"
static const char *TAG = "v-mercedesb250e";

#define VERSION "1.0.0"

#include <stdio.h>
#include "metrics_standard.h"
#include "ovms_metrics.h"


#include "vehicle_mercedesb250e.h"

OvmsVehicleMercedesB250e::OvmsVehicleMercedesB250e()
{
  ESP_LOGI(TAG, "Start Mercedes-Benz B250E vehicle module");

  mt_ed_eco_accel          = MyMetrics.InitFloat("xse.v.display.accel", SM_STALE_MIN, 0, Percentage);
  mt_ed_eco_const          = MyMetrics.InitFloat("xse.v.display.const", SM_STALE_MIN, 0, Percentage);
  mt_ed_eco_coast          = MyMetrics.InitFloat("xse.v.display.coast", SM_STALE_MIN, 0, Percentage);
  mt_ed_eco_score          = MyMetrics.InitFloat("xse.v.display.ecoscore", SM_STALE_MIN, 0, Percentage);

  
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
}

OvmsVehicleMercedesB250e::~OvmsVehicleMercedesB250e()
{
  ESP_LOGI(TAG, "Shutdown MERCEDESB250E vehicle module");
}

class OvmsVehicleMercedesB250eInit
  {
  public: OvmsVehicleMercedesB250eInit();
} MyOvmsVehicleMercedesB250eInit  __attribute__ ((init_priority (9000)));

OvmsVehicleMercedesB250eInit::OvmsVehicleMercedesB250eInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: MERCEDESB250E (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleMercedesB250e>("MB", "Mercedes-Benz B250E, W242");
}

void OvmsVehicleMercedesB250e::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  uint8_t *d = p_frame->data.u8;
   
  switch (p_frame->MsgID) {
  case 0x203: // Wheel speeds
    {
      /* Using wheel speed for speedo meter, as I have not yet found a better number */
      float fl_speed = ( ((d[0]&0xf) << 8) + d[1] ) * 0.0603504; 
      float fr_speed = ( ((d[2]&0xf) << 8) + d[3] ) * 0.0603504; 
      float rl_speed = ( ((d[4]&0xf) << 8) + d[5] ) * 0.0603504; 
      float rr_speed = ( ((d[5]&0xf) << 8) + d[7] ) * 0.0603504; 
      StandardMetrics.ms_v_pos_speed->SetValue(fl_speed); // HV Voltage
      ESP_LOGD(TAG, "Speeds: FL %3d, FR %3d, RL %3d, RR %3d", (int)fl_speed, (int)fr_speed, (int)rl_speed, (int)rr_speed);
      break;
    }
  case 0x34F: // Range
    {
      StandardMetrics.ms_v_bat_range_est->SetValue((float)d[7]); // HV Voltage
      ESP_LOGD(TAG, "Range from %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  case 0x3F2: //Eco display
    {
      mt_ed_eco_accel->SetValue((float)d[0] * 0.5);
      mt_ed_eco_const->SetValue((float)d[1] * 0.5);
      mt_ed_eco_coast->SetValue((float)d[2] * 0.5);
      mt_ed_eco_score->SetValue((float)d[3] * 0.5);
      ESP_LOGD(TAG, "ECO from %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  default:
    //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    break;
  }  
}
