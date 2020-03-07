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
;    Date:          7th March 2020
;
;    Changes:
;    0.1.0  Initial code
:           Code frame with correct TAG and vehicle/can registration
;
;    0.1.1  Started with some SOC capture demo code
;
;    0.1.2  Added VIN, speed, 12 volt battery detection
;
;    0.1.3  Added ODO (Dimitrie78), fixed speed
;
;    0.1.4  Added WLTP based ideal range, uncertain outdoor temperature
;
;    0.1.5  Finalized SOC calculation (sharkcow), added estimated range
;
;    0.1.6  Created a climate control first try, removed 12 volt battery status
;
;    0.1.7  Added status of doors
;
;    0.1.8  Added config page for the webfrontend. Features canwrite and modelyear.
;           Differentiation between model years is now possible.
;
;    (C) 2020       Chris van der Meijden
;
;    Big thanx to sharkcow and Dimitrie78.
*/

#include "ovms_log.h"
static const char *TAG = "v-vweup";

#define VERSION "0.1.7"

#include <stdio.h>
#include "vehicle_vweup.h"
#include "metrics_standard.h"
#include "ovms_webserver.h"
#include "ovms_events.h"
#include "ovms_metrics.h"


OvmsVehicleVWeUP::OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Start VW e-Up vehicle module");
  memset (m_vin,0,sizeof(m_vin));

  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);

  MyConfig.RegisterParam("xnl", "VW e-Up", true, true);
  ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
  }

OvmsVehicleVWeUP::~OvmsVehicleVWeUP()
  {
  ESP_LOGI(TAG, "Stop VW e-Up vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
  }

bool OvmsVehicleVWeUP::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xnl", "canwrite",  (bits& 1)!=0);
      return true;
    }
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}

const std::string OvmsVehicleVWeUP::GetFeature(int key)
{
  switch (key)
  {
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("xnl", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}

void OvmsVehicleVWeUP::ConfigChanged(OvmsConfigParam* param)
{
  ESP_LOGD(TAG, "Nissan Leaf reload configuration");
}

void OvmsVehicleVWeUP::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID) {

    case 0x61A: // SOC. Is this different for > 2019 models? 
      StandardMetrics.ms_v_bat_soc->SetValue(d[7]/2);
      if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2020)
        {
          StandardMetrics.ms_v_bat_range_ideal->SetValue((260 * (d[7]/2)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        } else {
          StandardMetrics.ms_v_bat_range_ideal->SetValue((160 * (d[7]/2)) / 100.0); // This is dirty. Based on WLTP only. Should be based on SOH.
        }
      break;

    case 0x52D: // KM range left (estimated).
      if( d[0] != 0xFE )
        StandardMetrics.ms_v_bat_range_est->SetValue(d[0]);
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

    case 0x527: // Outdoor temperature - untested. Wrong ID? If right, d[4] or d[5]?
      StandardMetrics.ms_v_env_temp->SetValue((d[4]/2)-50);
      break;

    case 0x3E3: // Cabin temperature
      // We should use:
      //
      // StandardMetrics.ms_v_env_cabintemp->SetValue((d[2]-100)/2);
      //
      // which is not implemented in the app.
      //
      // So instead we use as a quick workaround the PEM temperature:
      //
      StandardMetrics.ms_v_inv_temp->SetValue((d[2]-100)/2);
      break;

    case 0x470: // Doors
      StandardMetrics.ms_v_door_fl->SetValue((d[1] & 0x01) > 0);
      StandardMetrics.ms_v_door_fr->SetValue((d[1] & 0x02) > 0);
      break;

    default:
      //ESP_LOGD(TAG, "IFC %03x 8 %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
//
// Does nothing if @command is out of range


// Begin of remote climate control template
//
// This is just a template and does nothing!
// We are missing the CAN ID's yet to make it work

void OvmsVehicleVWeUP::SendCommand(RemoteCommand command)
  {
  // OVMS crashes on signal 26 when not connected to the CAN bus. Needs to be resolved.
  unsigned char data[8];

  uint8_t length;
  length = 8;

  canbus *comfBus;
  comfBus = m_can3;

  switch (command)
    {
    // data values are wrong. We need to find the right ID's.
    case ENABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Enable Climate Control");
      // 0x767 04 2F 09 B5 02 55 55 55 climate on?
      data[0] = 0x04;
      data[1] = 0x2F;
      data[2] = 0x09;
      data[3] = 0xB5;
      data[4] = 0x02;
      data[5] = 0x55;
      data[6] = 0x55;
      data[7] = 0x55;
      break;
    case DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Disable Climate Control");
      // 0x767 04 2F 09 B5 00 55 55 55 climate off?
      data[0] = 0x04;
      data[1] = 0x2F;
      data[2] = 0x09;
      data[3] = 0xB5;
      data[4] = 0x00;
      data[5] = 0x55;
      data[6] = 0x55;
      data[7] = 0x55;
      break;
    case AUTO_DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Auto Disable Climate Control");
      // 0x767 04 2F 09 B5 00 55 55 55 climate off?
      data[0] = 0x04;
      data[1] = 0x2F;
      data[2] = 0x09;
      data[3] = 0xB5;
      data[4] = 0x00;
      data[5] = 0x55;
      data[6] = 0x55;
      data[7] = 0x55;
      break;
    default:
      return;
    }
    // Does this work?
    comfBus->WriteStandard(0x767, length, data);
  }

////////////////////////////////////////////////////////////////////////
// implements the repeated sending of remote commands and releases
// EV SYSTEM ACTIVATION REQUEST after an appropriate amount of time

void OvmsVehicleVWeUP::RemoteCommandTimer()
  {
  ESP_LOGI(TAG, "RemoteCommandTimer %d", nl_remote_command_ticker);
  if (nl_remote_command_ticker > 0)
    {
    nl_remote_command_ticker--;
    if (nl_remote_command != AUTO_DISABLE_CLIMATE_CONTROL)
      {
      SendCommand(nl_remote_command);
      }
    if (nl_remote_command_ticker == 1 && nl_remote_command == ENABLE_CLIMATE_CONTROL)
      {
      xTimerStart(m_ccDisableTimer, 0);
      }

    // nl_remote_command_ticker is set to REMOTE_COMMAND_REPEAT_COUNT in
    // RemoteCommandHandler() and we decrement it every 10th of a
    // second, hence the following if statement evaluates to true
    // ACTIVATION_REQUEST_TIME tenths after we start
    }
    else
    {
      xTimerStop(m_remoteCommandTimer, 0);
    }
  }

void OvmsVehicleVWeUP::CcDisableTimer()
  {
  ESP_LOGI(TAG, "CcDisableTimer");
  SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUP::RemoteCommandHandler(RemoteCommand command)
  {
  ESP_LOGI(TAG, "RemoteCommandHandler");

  nl_remote_command = command;
  nl_remote_command_ticker = REMOTE_COMMAND_REPEAT_COUNT;
  xTimerStart(m_remoteCommandTimer, 0);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUP::CommandHomelink(int button, int durationms)
  {
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0)
    {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
    }
  if (button == 1)
    {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
    }
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVWeUP::CommandClimateControl(bool climatecontrolon)
  {
  ESP_LOGI(TAG, "CommandClimateControl");
  return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
  }

// End of remote climate contol template

void OvmsVehicleVWeUP::Ticker1(uint32_t ticker)
  {
  // Needs to be replaced with a CAN signal. This has a delay of 120 sec.
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
