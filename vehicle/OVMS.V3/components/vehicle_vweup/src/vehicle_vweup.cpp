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
;    Subproject:    Integration of support for the VW e-UP
;    Date:          27th February 2020
;
;    Changes:
;    0.1.0  Initial code
:           Code frame with correct TAG and vehicle/can registration
;
;    0.1.1  Started with some SOC capture demo code
;
;    0.1.2  Added VIN, speed, 12 volt battery detection
;
;    0.1.3  Added ODO, fixed speed
;
;    0.1.4  Added WLTP based ideal range, uncertain outdoor temperature
;
;    (C) 2020       Chris van der Meijden
;
;    Big thanx to sharkcow and Dimitrie78.
*/

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#define VERSION "0.1.4"

#include <stdio.h>
#include "vehicle_vweup.h"
#include "metrics_standard.h"

OvmsVehicleVWeUP::OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Start VW e-Up vehicle module");
  memset (m_vin,0,sizeof(m_vin));

  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);
  }

OvmsVehicleVWeUP::~OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Stop VW e-Up vehicle module");
  }

void OvmsVehicleVWeUP::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
    
  switch (p_frame->MsgID) {

    case 0x61A: // SOC - Calculation needs still to be corrected. 61A uncertain too. Could also be 5AC d[3].
      StandardMetrics.ms_v_bat_soc->SetValue(d[7]/2.55);
      StandardMetrics.ms_v_bat_range_ideal->SetValue((265 * (d[7]/2.55)) / 100.0); // This is dirty. Based on WLTP only. Division by 2.55 is wrong too.
      break;

    case 0x65F: // VIN
      switch (d[0]) {
          case 0x00:
            // Part 1
	    m_vin[0] = d[5];
	    m_vin[1] = d[6];
	    m_vin[2] = d[7];
            break;
          case 0x01:
            // Part 2
	    m_vin[3] = d[1];
	    m_vin[4] = d[2];
	    m_vin[5] = d[3];
	    m_vin[6] = d[4];
	    m_vin[7] = d[5];
	    m_vin[8] = d[6];
	    m_vin[9] = d[7];
            break;
	  case 0x02:
            // Part 3 - VIN complete
	    m_vin[10] = d[1];
	    m_vin[11] = d[2];
	    m_vin[12] = d[3];
	    m_vin[13] = d[4];
	    m_vin[14] = d[5];
	    m_vin[15] = d[6];
	    m_vin[16] = d[7];
	    StandardMetrics.ms_v_vin->SetValue(m_vin);
            break;
      }
      break;

    case 0x65D: // ODO
      StandardMetrics.ms_v_pos_odometer->SetValue( ((uint32_t)(d[3] & 0xf) << 12) | ((UINT)d[2] << 8) | d[1] );
      break;

    case 0x320: // Speed
      StandardMetrics.ms_v_env_awake->SetValue(true);
      StandardMetrics.ms_v_pos_speed->SetValue(((d[4] << 8) + d[3]-1)/190);
      break;

    case 0x571: // 12 Volt
      StandardMetrics.ms_v_bat_12v_voltage->SetValue(5 + (0.05 * d[0]));
      break;

    case 0x527: // Outdoor temperature - untested. Wrong ID? If right, d[4] or d[5]?
      StandardMetrics.ms_v_env_temp->SetValue((d[4]/2)-50);
      break;
    
    default:
      //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  }

void OvmsVehicleVWeUP::Ticker1(uint32_t ticker)
  {
  if (StandardMetrics.ms_v_env_awake->IsStale())
    {
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }
  }

class OvmsVehicleVWeUPInit
  {
  public: OvmsVehicleVWeUPInit();
} OvmsVehicleVWeUPInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVWeUPInit::OvmsVehicleVWeUPInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: VW e-Up (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVWeUP>("VWUP","VW e-Up");
  }


