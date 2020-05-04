/*
;    Project:       Open Vehicle Monitor System
;    Date:          4th May 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2020       Jarkko Ruoho
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
; Currently support only passive listening of the CAN bus. 
; 
; Testing status: "Works for me", meaning everything is tried just once.  
;
; Most data is reverse engineered from Mercedes Benz B250E (W242), year 2015. 
; vehicle_smarted.cpp was used as an example, and some MsgID's are the same. 
; Some MsgID's matched other Mercedes models. OpenDBC was helpfull here: 
; https://github.com/commaai/opendbc/blob/master/mercedes_benz_e350_2010.dbc
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
  case 0x105: // Motor RPM
    {
      int rpm = ((d[0]&0x3f) << 8) + d[1]; 
      StandardMetrics.ms_v_mot_rpm->SetValue(rpm); // RPM
      StandardMetrics.ms_v_env_throttle->SetValue(d[4]/2.50); // Drive pedal state [%], raw values are from 0 to 250
      break;
    }
  case 0x19F: // Speedo
    {
      float speed = ( ((d[0]&0xf) << 8) + d[1] ) * 0.1; 
      float odo   = ( (d[5] << 16) + (d[6] << 8) + (d[7]) ) * 0.1;
      StandardMetrics.ms_v_pos_speed->SetValue(speed); // speed in km/h
      StandardMetrics.ms_v_pos_odometer->SetValue(odo); // ODO km
      break;
    }
  case 0x203: // Wheel speeds
    {
      /* Using wheel speed for speedo meter, as I have not yet found a better number */
      float fl_speed = ( ((d[0]&0xf) << 8) + d[1] ) * 0.0603504; 
      float fr_speed = ( ((d[2]&0xf) << 8) + d[3] ) * 0.0603504; 
      float rl_speed = ( ((d[4]&0xf) << 8) + d[5] ) * 0.0603504; 
      float rr_speed = ( ((d[5]&0xf) << 8) + d[7] ) * 0.0603504; 
      // Don't post it, as wheels may be spinning. 0x19F is the correct MsgID for speed
      // StandardMetrics.ms_v_pos_speed->SetValue(fl_speed); // 
      ESP_LOGD(TAG, "Speeds: FL %3d, FR %3d, RL %3d, RR %3d",
	       (int)fl_speed, (int)fr_speed, (int)rl_speed, (int)rr_speed);
      break;
    }
  case 0x205: // 12V battery voltage
    {
      StandardMetrics.ms_v_bat_12v_voltage->SetValue(d[1]*0.1); // Volts
      break;
    }	
  case 0x33D: // Momentary power
    {
      StandardMetrics.ms_v_bat_power->SetValue(d[4]-100); // kW 
      break;
    }	
    // case 0x34E: // Distance Today , Distance since reset, scale is 0.1 km
  case 0x34F: // Range
    {
      StandardMetrics.ms_v_bat_range_est->SetValue((float)d[7]); // Car's estimate on remainging range
      /* This is really an average, but let's move this when better is found */
      StandardMetrics.ms_v_bat_consumption->SetValue((float)d[1]*0.5/1.609344); // Consumption per distance
      ESP_LOGD(TAG, "Range from %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
	       p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  case 0x3F2: //Eco display
    {
      mt_ed_eco_accel->SetValue((float)d[0] * 0.5);
      mt_ed_eco_const->SetValue((float)d[1] * 0.5);
      mt_ed_eco_coast->SetValue((float)d[2] * 0.5);
      mt_ed_eco_score->SetValue((float)d[3] * 0.5);
      ESP_LOGD(TAG, "ECO from %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
	       p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  default:
    //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x",
    //p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
    break;
  }  
}
