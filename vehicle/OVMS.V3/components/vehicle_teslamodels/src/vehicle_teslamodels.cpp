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
static const char *TAG = "v-teslamodels";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslamodels.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

OvmsVehicleTeslaModelS::OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Tesla Model S vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  m_charge_w = 0;

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_125KBPS);

  BmsSetCellArrangementVoltage(96, 6);
  BmsSetCellArrangementTemperature(32, 2);
  BmsSetCellLimitsVoltage(1,4.9);
  BmsSetCellLimitsTemperature(-90,90);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
#endif
  }

OvmsVehicleTeslaModelS::~OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Model S vehicle module");
  MyCommandApp.UnregisterCommand("xts");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x102: // BMS current and voltage
      {
      // Don't update battery voltage too quickly (as it jumps around like crazy)
      if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
        StandardMetrics.ms_v_bat_voltage->SetValue(((float)((int)d[1]<<8)+d[0])/100);
      StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      break;
      }
    case 0x116: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_awake->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 3: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 4: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_awake->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        default:
          break;
        }
      break;
      }
    case 0x222: // Charging related
      {
      m_charge_w = uint16_t(d[1]<<8)+d[0];
      break;
      }
    case 0x256: // Speed
      {
      StandardMetrics.ms_v_pos_speed->SetValue( ((((int)d[3]&0x0f)<<8) + (int)d[2])/10, (d[3]&0x80)?Kph:Mph );
      break;
      }
    case 0x28C: // Charging related
      {
      float charge_v = (float)(uint16_t(d[1]<<8) + d[0])/256;
      if ((charge_v != 0)&&(m_charge_w != 0))
        { // Charging
        StandardMetrics.ms_v_charge_current->SetValue((unsigned int)(m_charge_w/charge_v));
        StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue((unsigned int)(m_charge_w/charge_v));
        }
      else
        { // Not charging
        StandardMetrics.ms_v_charge_current->SetValue(0);
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue(0);
        }
      break;
      }
    case 0x302: // SOC
      {
      StandardMetrics.ms_v_bat_soc->SetValue( (((int)d[1]>>2) + (((int)d[2] & 0x0f)<<6))/10 );
      break;
      }
    case 0x306: // Temperatures
      {
      StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
      StandardMetrics.ms_v_mot_temp->SetValue((int)d[2]-40);
      break;
      }
    case 0x398: // Country
      {
      m_type[0] = 'T';
      m_type[1] = 'S';
      m_type[2] = d[0];
      m_type[3] = d[1];
      StandardMetrics.ms_v_type->SetValue(m_type);
      break;
      }
    case 0x508: // VIN
      {
      switch(d[0])
        {
        case 0:
          memcpy(m_vin,d+1,7);
          break;
        case 1:
          memcpy(m_vin+7,d+1,7);
          break;
        case 2:
          memcpy(m_vin+14,d+1,3);
          m_vin[17] = 0;
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          break;
        }
      break;
      }
    case 0x5d8: // Odometer (0x562 is battery, so this is motor or car?)
      {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[3]<<24)
                                                + ((uint32_t)d[2]<<16)
                                                + ((uint32_t)d[1]<<8)
                                                + d[0])/1000, Miles);
      break;
      }
    case 0x6f2: // BMS brick voltage and module temperatures
      {
      int v1 = ((int)(d[2]&0x3f)<<8) + d[1];
      int v2 = (((int)d[4]&0x0f)<<10) + (((int)d[3])<<2) + (d[2]>>6);
      int v3 = (((int)d[6]&0x03)<<12) + (((int)d[5])<<4) + (d[4]>>4);
      int v4 = (((int)d[7])<<6) + (((int)d[6]>>2));
      if ((v1 != 0x3fff)&&(v2 != 0x3fff)&&(v3 != 0x3fff)&&(v4 != 0x3fff)&&
          (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
        {
        if (d[0]<24)
          {
          // Voltages
          int k = d[0]*4;
          BmsSetCellVoltage(k,   0.000305 * v1);
          BmsSetCellVoltage(k+1, 0.000305 * v2);
          BmsSetCellVoltage(k+2, 0.000305 * v3);
          BmsSetCellVoltage(k+3, 0.000305 * v4);
          }
        else
          {
          // Temperatures
          int k = (d[0]-24)*4;
          BmsSetCellTemperature(k,   0.0122 * ((v1 & 0x1FFF) - (v1 & 0x2000)));
          BmsSetCellTemperature(k+1, 0.0122 * ((v2 & 0x1FFF) - (v2 & 0x2000)));
          BmsSetCellTemperature(k+2, 0.0122 * ((v3 & 0x1FFF) - (v3 & 0x2000)));
          BmsSetCellTemperature(k+3, 0.0122 * ((v4 & 0x1FFF) - (v4 & 0x2000)));
          }
        }
      break;
      }
    default:
      break;
    }
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x2f8: // MCU GPS speed/heading
      StandardMetrics.ms_v_pos_gpshdop->SetValue((float)d[0] / 10);
      StandardMetrics.ms_v_pos_direction->SetValue((float)(((uint32_t)d[2]<<8)+d[1])/128.0);
      StandardMetrics.ms_v_pos_gpsspeed->SetValue((float)(((uint32_t)d[4]<<8)+d[3])/256.0,Kph);
      if (d[0] < 255)
        {
        StandardMetrics.ms_v_pos_gpslock->SetValue(true);
        StandardMetrics.ms_v_pos_gpsmode->SetValue("TESLA");
        }
      break;
    case 0x3d8: // MCU GPS latitude / longitude
      StandardMetrics.ms_v_pos_latitude->SetValue((double)(((uint32_t)(d[3]&0x0f) << 24) +
                                                          ((uint32_t)d[2] << 16) +
                                                          ((uint32_t)d[1] << 8) +
                                                          (uint32_t)d[0]) / 1000000.0);
      StandardMetrics.ms_v_pos_longitude->SetValue((double)(((uint32_t)d[6] << 20) +
                                                           ((uint32_t)d[5] << 12) +
                                                           ((uint32_t)d[4] << 4) +
                                                           ((uint32_t)(d[3]&0xf0) >> 4)) / 1000000.0);
      break;
    default:
      break;
    }
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleTeslaModelS::Notify12vCritical()
  { // Not supported on Model S
  }

void OvmsVehicleTeslaModelS::Notify12vRecovered()
  { // Not supported on Model S
  }

void OvmsVehicleTeslaModelS::NotifyBmsAlerts()
  { // Not supported on Model S
  }

class OvmsVehicleTeslaModelSInit
  {
  public: OvmsVehicleTeslaModelSInit();
} MyOvmsVehicleTeslaModelSInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaModelSInit::OvmsVehicleTeslaModelSInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Model S (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaModelS>("TS","Tesla Model S");
  }
