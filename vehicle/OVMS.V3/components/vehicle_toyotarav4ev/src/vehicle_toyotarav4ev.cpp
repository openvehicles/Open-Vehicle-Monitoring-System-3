/*
;    Project:       Open Vehicle Monitor System for RAV4 EV (2012-2014)
;    Date:          22nd June 2021
;
;    Changes:
;    0.1.0  Trial code
;
;   Adapted from Tesla Model S by:
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
static const char *TAG = "v-toyotarav4ev";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_toyotarav4ev.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

OvmsVehicleToyotaRav4Ev::OvmsVehicleToyotaRav4Ev()
  {
  ESP_LOGI(TAG, "Toyota RAV4 EV vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  m_charge_w = 0;

  m_v_bat_cool_in_temp = MyMetrics.InitFloat("xr4.v.b.t.cltin", SM_STALE_MIN, 0, Celcius);
  m_v_bat_cool_out_temp = MyMetrics.InitFloat("xr4.v.b.t.cltout", SM_STALE_MIN, 0, Celcius);
  m_v_bat_cool_pump_1  = MyMetrics.InitInt("xr4.v.b.p.one", SM_STALE_HIGH, 0, Percentage);
  m_v_bat_cool_pump_2  = MyMetrics.InitInt("xr4.v.b.p.two", SM_STALE_HIGH, 0, Percentage);
  m_v_bat_energy_avail = MyMetrics.InitFloat("xr4.v.b.en.avail", SM_STALE_MIN, 0, kWh);
  m_v_mot_cool_in_temp = MyMetrics.InitFloat("xr4.v.m.t.cltin", SM_STALE_MIN, 0, Celcius);
  // m_v_mot_cool_out_temp = MyMetrics.InitFloat("xr4.v.m.t.cltout", SM_STALE_MIN, 0, Celcius);
  // it turns out this is value is not populated by the RAV4 Thermal Control ECU, it came from a Model S DBC
  m_v_mot_cool_pump = MyMetrics.InitInt("xr4.v.m.pump", SM_STALE_HIGH, 0, Percentage);

/*
#ifdef CONFIG_OVMS_COMP_TPMS
  m_candata_timer = TS_CANDATA_TIMEOUT;
  m_tpms_cmd = Idle;
  m_tpms_pos = 0;
#endif // #ifdef CONFIG_OVMS_COMP_TPMS
*/
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Tesla Bus at vehicle rear
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Toyota Bus under dash

  BmsSetCellArrangementVoltage(92, 6);
  BmsSetCellArrangementTemperature(32, 2);
  BmsSetCellLimitsVoltage(1,4.9);
  BmsSetCellLimitsTemperature(-90,90);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
#endif
  }

OvmsVehicleToyotaRav4Ev::~OvmsVehicleToyotaRav4Ev()
  {
  ESP_LOGI(TAG, "Shutdown Toyota RAV4 EV vehicle module");
  MyCommandApp.UnregisterCommand("xr4");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  }

void OvmsVehicleToyotaRav4Ev::Ticker1(uint32_t ticker)
  {
  if ((m_candata_timer > 0)&&(--m_candata_timer == 0))
    {
    // Car has gone to sleep
    ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }
  }

void OvmsVehicleToyotaRav4Ev::Ticker60(uint32_t ticker)
{
  StandardMetrics.ms_v_bat_temp->SetValue(StandardMetrics.ms_v_bat_cell_temp->AsFloat());
}

// IncomingFrameCan1 (Tesla)
void OvmsVehicleToyotaRav4Ev::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  
  uint8_t *d = p_frame->data.u8;
  static float fPackVolts;
  static float fPackAmps;
  static uint16_t iPackAmps;

  switch (p_frame->MsgID)
    {
    case 0x102: // BMS current and voltage
      {
      // Don't update battery voltage too quickly (as it jumps around like crazy)
      if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
      {
        fPackVolts = ((float)((int)d[1]<<8)+d[0])/100;
        iPackAmps = ((d[3]&0x3F)<<8) + d[2];
        if (d[3] & 0x40) fPackAmps = -0.1 * float(iPackAmps - 16384);
        else fPackAmps = -0.1 * float(iPackAmps);
        StandardMetrics.ms_v_bat_voltage->SetValue(fPackVolts);
        StandardMetrics.ms_v_bat_current->SetValue(fPackAmps);
        StandardMetrics.ms_v_bat_power->SetValue(fPackVolts * fPackAmps / 1000);
      }
//      StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      // This signal is from the Model S code. RAV4 EV is only 6 bytes in Frame 0x102, so it doesn't have this signal.
      break;
      }
    case 0x116: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 3: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 4: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        default:
          break;
        }
      break;
      }
    case 0x210:
      {
        StandardMetrics.ms_v_charge_12v_current->SetValue(float(d[4]));
        StandardMetrics.ms_v_charge_12v_voltage->SetValue(float(d[5])*0.1);
        StandardMetrics.ms_v_charge_12v_power->SetValue(float(d[4])*float(d[5])*0.1);
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
        // BMS_socUI from Tesla Model S
      //  StandardMetrics.ms_v_bat_soc->SetValue( (((int)d[1]>>2) + (((int)d[2] & 0x0f)<<6))/10 );
        // BMS_socMin 
        StandardMetrics.ms_v_bat_soc->SetValue( ((((int)d[1] & 0x03)<<8) + d[0])/10 );
        m_v_bat_energy_avail->SetValue( float((((int)d[4]&0x0F)<<6)+((d[3]&0xFC)>>2))/10 );
      break;
      }
    case 0x306: // Temperatures
      {
        StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
        StandardMetrics.ms_v_mot_temp->SetValue((int)d[2]-40);
      break;
      }
    case 0x31A: // Coolant Temperatures
      {
        m_v_bat_cool_in_temp->SetValue(float(((int)(d[1]&0x03)<<8)+d[0])*0.125f-40.0f);
        m_v_bat_cool_out_temp->SetValue(float(((int)(d[3]&0x03)<<8)+d[2])*0.125f-40.0f);
        m_v_mot_cool_in_temp->SetValue(float(((int)(d[5]&0x07)<<8)+d[4])*0.125f-40.0f);
//        m_v_mot_cool_out_temp->SetValue(float(((int)(d[7]&0x07)<<8)+d[6])*0.125f-40.0f);
      break;
      }
    case 0x32A: // Coolant Pumps
      {
        m_v_bat_cool_pump_1->SetValue(d[0]&0x7F);
        m_v_bat_cool_pump_2->SetValue(d[1]&0x7F);
        m_v_mot_cool_pump->SetValue(d[2]&0x7F);
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

// IncomingFrameCan2 (Toyota)
void OvmsVehicleToyotaRav4Ev::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  // Toyota RAV4 EV Toyota Bus Not Implemented Yet
  }

void OvmsVehicleToyotaRav4Ev::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  // Toyota RAV4 EV CAN3 Not Used
  }

void OvmsVehicleToyotaRav4Ev::Notify12vCritical()
  { // Not supported on RAV4 EV
  }

void OvmsVehicleToyotaRav4Ev::Notify12vRecovered()
  { // Not supported on RAV4 EV
  }

void OvmsVehicleToyotaRav4Ev::NotifyBmsAlerts()
  { // Not supported on RAV4 EV
  }


class OvmsVehicleToyotaRav4EvInit
  {
  public: OvmsVehicleToyotaRav4EvInit();
} MyOvmsVehicleToyotaRav4EvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleToyotaRav4EvInit::OvmsVehicleToyotaRav4EvInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Toyota RAV4 EV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleToyotaRav4Ev>("TYR4","Toyota RAV4 EV");
  }
