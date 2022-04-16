/*
;    Project:       Open Vehicle Monitor System for Toyota RAV4 EV (2012-2014)
;    Date:          22nd June 2021
;
;    Changes:
;    0.1.0  Trial code
;    0.2.0  Code initially submitted to master branch
;    0.3.0  Implemented v_on and v_charging states with some metrics only set/cleared depending on these states
;    0.4.0  Calc Ideal Range from Avail kWh, Changed v_on to be gated on BMS_status instead of BMS_CtrSet to 
;           eliminate the overlap with AC charging state. Implemented DC charging detection and v_dc_charging.
;           Dropped SOC_ui. Added gear letter metric. Added odometer decode and display.
;
;    (C) 2021-2022 Mike Iimura
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
  v_on = false;
  v_charging = false;
  v_dc_charging = false;
  iFrameCountCan1 = 0;
  iFrameCountCan2 = 0;
  iNegPowerCount = 0;
  // m_charge_w = 0;

  // m_v_bat_socui = MyMetrics.InitFloat("xr4.v.b.socui", SM_STALE_MIN, 0, Percentage);
  m_v_bat_cool_in_temp = MyMetrics.InitFloat("xr4.v.b.t.cltin", SM_STALE_MIN, 0, Celcius);
  m_v_bat_cool_out_temp = MyMetrics.InitFloat("xr4.v.b.t.cltout", SM_STALE_MIN, 0, Celcius);
  m_v_bat_cool_pump_1  = MyMetrics.InitInt("xr4.v.b.p.one", SM_STALE_HIGH, 0, Percentage);
  m_v_bat_cool_pump_2  = MyMetrics.InitInt("xr4.v.b.p.two", SM_STALE_HIGH, 0, Percentage);
  m_v_bat_energy_avail = MyMetrics.InitFloat("xr4.v.b.en.avail", SM_STALE_MIN, 0, kWh);
  m_v_env_gear_letter = MyMetrics.InitString("xr4.v.e.gear.letter", SM_STALE_NONE, 0);
  m_v_mot_cool_in_temp = MyMetrics.InitFloat("xr4.v.m.t.cltin", SM_STALE_MIN, 0, Celcius);
  // m_v_mot_cool_out_temp = MyMetrics.InitFloat("xr4.v.m.t.cltout", SM_STALE_MIN, 0, Celcius);
  // it turns out this is value is not populated by the RAV4 Thermal Control ECU, it came from a Model S DBC
  m_v_mot_cool_pump = MyMetrics.InitInt("xr4.v.m.pump", SM_STALE_HIGH, 0, Percentage);
  m_v_chg_pilot_cur = MyMetrics.InitInt("xr4.v.c.pilot.current", SM_STALE_MIN, 0, Amps);

/*
#ifdef CONFIG_OVMS_COMP_TPMS
  m_candata_timer = TS_CANDATA_TIMEOUT;
  m_tpms_cmd = Idle;
  m_tpms_pos = 0;
#endif // #ifdef CONFIG_OVMS_COMP_TPMS
*/
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Tesla Bus
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Toyota Bus 

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
    if (v_on)
    {     // check for negative pack power while in Park - must be DC charging.
      if ((StandardMetrics.ms_v_bat_power->AsFloat() < 0) && (m_v_env_gear_letter->AsString()=="P") && (StandardMetrics.ms_v_charge_pilot->AsBool()==false))
      {
        iNegPowerCount++;
        if (!v_dc_charging && iNegPowerCount > 2)
        {   // set DC Charging state
          StdMetrics.ms_v_door_chargeport->SetValue(true);
          StandardMetrics.ms_v_charge_state->SetValue("charging");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          StandardMetrics.ms_v_charge_type->SetValue("chademo");
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_inprogress->SetValue(true);
          v_dc_charging = true;
          ESP_LOGD(TAG,"DC Charging Started");
        }
      } else {
        if (StandardMetrics.ms_v_bat_power->AsFloat() > 0) 
        {
          iNegPowerCount = 0;
          if ((StandardMetrics.ms_v_env_gear->AsInt()==0) && v_dc_charging)
          {   // clear DC charging state
            StdMetrics.ms_v_door_chargeport->SetValue(false);
            StandardMetrics.ms_v_charge_state->SetValue("stopped");
            StandardMetrics.ms_v_charge_substate->SetValue("stopped");
            StandardMetrics.ms_v_charge_type->SetValue("");
            StandardMetrics.ms_v_charge_mode->SetValue("");
            StandardMetrics.ms_v_charge_inprogress->SetValue(false);
            v_dc_charging = false;
            ESP_LOGD(TAG,"DC Charging Stopped");
          }
        }
      }
    } 
    ESP_LOGV(TAG,"CAN1 frames: %d", iFrameCountCan1);   // Verbose logging of frames per second on each bus
    ESP_LOGV(TAG,"CAN2 frames: %d", iFrameCountCan2);
    iFrameCountCan1=0;
    iFrameCountCan2=0;
  }

void OvmsVehicleToyotaRav4Ev::Ticker60(uint32_t ticker)
{
  //  StandardMetrics.ms_v_bat_temp->SetValue(StandardMetrics.ms_v_bat_cell_tmax->AsFloat());   // moved to direct decode of 0x312
  if (StandardMetrics.ms_v_env_on->AsBool())
  {
//    ESP_LOGD(TAG,"SOC_MIN: %.1f, SOC_UI: %.1f, Pack_NRG: %.1f", StandardMetrics.ms_v_bat_soc->AsFloat(), m_v_bat_socui->AsFloat(), m_v_bat_energy_avail->AsFloat());
    ESP_LOGD(TAG,"SOC_MIN: %.1f, Pack_NRG: %.1f", StandardMetrics.ms_v_bat_soc->AsFloat(), m_v_bat_energy_avail->AsFloat());
  }
    
}

// IncomingFrameCan1 (Tesla)
void OvmsVehicleToyotaRav4Ev::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  
  uint8_t *d = p_frame->data.u8;

  iFrameCountCan1++;
  switch (p_frame->MsgID)
    {
    case 0x102: // BMS_hvBusStatus  *** BMS current and voltage (about 72 frame/sec) ***
      {
      // Don't update battery voltage too quickly (as it jumps around like crazy) 
      if (StandardMetrics.ms_v_bat_voltage->Age() > 1)
      {
//        StandardMetrics.ms_v_bat_voltage->SetValue(((float)((int)(d[1]<<8)+d[0]))/100);
        fPackVolts = ((float)((int)d[1]<<8)+d[0])/100;
        iPackAmps = ((d[3]&0x3F)<<8) + d[2];
        if (d[3] & 0x40) fPackAmps = -0.1 * float(iPackAmps - 16384);
        else fPackAmps = -0.1 * float(iPackAmps);
        if (v_on || v_charging)
        {
          StandardMetrics.ms_v_bat_voltage->SetValue(fPackVolts);
          StandardMetrics.ms_v_bat_current->SetValue(fPackAmps);
          StandardMetrics.ms_v_bat_power->SetValue(fPackVolts * fPackAmps / 1000);
        }
      }
//      StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      // This signal is from the Model S code. RAV4 EV is only 6 bytes in Frame 0x102, so it doesn't have this signal.
      break;
      }
    case 0x116: // DI_torque2   *** Set Gear selector ***
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          m_v_env_gear_letter->SetValue("P");
//          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
//          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          m_v_env_gear_letter->SetValue("R");
//          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
//          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 3: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          m_v_env_gear_letter->SetValue("N");
//          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
//          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 4: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          m_v_env_gear_letter->SetValue("D");
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          break;
        case 5: // B Mode
          StandardMetrics.ms_v_env_gear->SetValue(1);
          m_v_env_gear_letter->SetValue("B");
//          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
//          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        default:
          break;
        }
      break;
      }
    case 0x20C:   // CHG_lineStatus
      {
        if (v_charging)
        {
          StandardMetrics.ms_v_charge_voltage->SetValue(((d[2]&0x01)<<8) + d[1]);
          StandardMetrics.ms_v_charge_power->SetValue(float((d[3]<<3)+((d[2]&0xE0)>>5))*0.01);
          StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_charge_power->AsFloat()*1000/StandardMetrics.ms_v_charge_voltage->AsFloat());
        }
        break;
      }
    case 0x210:   // DCDC_status
      {
        if (v_charging || v_on)
        {
          StandardMetrics.ms_v_charge_12v_current->SetValue(float(d[4]));
          StandardMetrics.ms_v_bat_12v_current->SetValue(float(d[4]));
          StandardMetrics.ms_v_charge_12v_voltage->SetValue(float(d[5])*0.1);
          StandardMetrics.ms_v_charge_12v_power->SetValue(float(d[4])*float(d[5])*0.1);
        }
        break;
      }  
    case 0x212:   // BMS_status
      {
//        if (d[2] & BMS_CTRSET_CLOSED)   // Contactor is closed, passing HVDC to the car. This is a proxy for READY status.
        if ((d[2]>>4) == BMS_DRIVE)      // BMS indicates DRIVE status. This is a proxy for vehicle READY status.
        {                               // BMS status is BMS_CHARGER when contactors are closed for AC charging.
          if (!v_on)
          {
            StandardMetrics.ms_v_env_on->SetValue(true);
            v_on = true;
            ESP_LOGD(TAG,"Transition to Vehicle ON");
          }
        } else {
          if (v_on)
          {
            StandardMetrics.ms_v_env_on->SetValue(false);
            v_on = false;
            ESP_LOGD(TAG,"Transition to Vehicle OFF");
            StandardMetrics.ms_v_bat_voltage->SetValue(0);
            StandardMetrics.ms_v_bat_current->SetValue(0);
            StandardMetrics.ms_v_bat_power->SetValue(0);
            StandardMetrics.ms_v_bat_12v_current->SetValue(0);
            StandardMetrics.ms_v_charge_12v_current->SetValue(0);
            StandardMetrics.ms_v_charge_12v_voltage->SetValue(0);
            StandardMetrics.ms_v_charge_12v_power->SetValue(0);
          }
        }
        break;
      }
    case 0x21C:   // CHG_status
      {
        switch (d[0]>>4)
        {
          case 0x00:  // Standby
            if (v_charging)
            {
              StandardMetrics.ms_v_door_chargeport->SetValue(false);
              StandardMetrics.ms_v_charge_state->SetValue("stopped");
              StandardMetrics.ms_v_charge_substate->SetValue("stopped");
              StandardMetrics.ms_v_charge_type->SetValue("");
              StandardMetrics.ms_v_charge_mode->SetValue("");
              StandardMetrics.ms_v_charge_inprogress->SetValue(false);
              v_charging = false;
              ESP_LOGD(TAG,"Charging Stopped");
              StandardMetrics.ms_v_charge_voltage->SetValue(0);
              StandardMetrics.ms_v_charge_power->SetValue(0);
              StandardMetrics.ms_v_charge_current->SetValue(0);
            }
            break;
          case 0x01:  // Clear Fault
            break;
          case 0x02:  // Fault
            ESP_LOGD(TAG,"Charging Fault");
            break;
          case 0x03:  // Precharge
            break;
          case 0x04:  // Enable
            if (!v_charging)
            {
              StandardMetrics.ms_v_door_chargeport->SetValue(true);
              StandardMetrics.ms_v_charge_state->SetValue("charging");
              StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
              StandardMetrics.ms_v_charge_type->SetValue("type1");
              StandardMetrics.ms_v_charge_mode->SetValue("standard");
              StandardMetrics.ms_v_charge_inprogress->SetValue(true);
              v_charging = true;
              ESP_LOGD(TAG,"Charging Started");
            }
            break;
        }
        switch ((d[1]&0x18)>>3)   // bits 11, 12
        {
          case 0x00:  // CHG_PROXIMITY_SNA
          case 0x01:  // CHG_PROXIMITY_DISCONNECTED
          case 0x02:  // CHG_PROXIMITY_UNLATCHED
            StandardMetrics.ms_v_charge_pilot->SetValue(false);
            break;
          case 0x03:  // CHG_PROXIMITY_LATCHED  
            StandardMetrics.ms_v_charge_pilot->SetValue(true);
            break;
        }
        m_v_chg_pilot_cur->SetValue(d[2]/2);
        if (m_v_chg_pilot_cur == 0 && StandardMetrics.ms_v_charge_pilot->AsBool())
        {
          StandardMetrics.ms_v_charge_substate->SetValue("powerwait");
        }
        break;
      }

    case 0x256:   // DI_state
      {
      StandardMetrics.ms_v_pos_speed->SetValue( ((((int)d[3]&0x0f)<<8) + (int)d[2])/10, (d[3]&0x80)?Kph:Mph );
      break;
      }

    case 0x302: // BMS_socStatus
      {
//        m_v_bat_socui->SetValue( float((uint16_t(d[1]>>2)) + ((uint16_t(d[2] & 0x0f))<<6))/10 );           // BMS_socUI
        StandardMetrics.ms_v_bat_soc->SetValue( ((((int)d[1] & 0x03)<<8) + d[0])/10 );      // BMS_socMin 
        m_v_bat_energy_avail->SetValue( float((((int)d[4]&0x0F)<<6)+((d[3]&0xFC)>>2))/10 ); // BMS_socNrg
        StandardMetrics.ms_v_bat_range_ideal->SetValue(m_v_bat_energy_avail->AsFloat()*5.635);  // Ideal Range = avail kWh * 5.635km/kWh 
      break;
      }
    case 0x306: // DI_temperature
      {
        StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
        StandardMetrics.ms_v_mot_temp->SetValue((int)d[2]-40);
      break;
      }
    case 0x312: // BMS_thermalStatus
      {
        StandardMetrics.ms_v_bat_temp->SetValue(((int8_t)d[6])+40);
        break;
      }
    case 0x31A: // THC_coolantTemp
      {
        m_v_bat_cool_in_temp->SetValue(float(((int)(d[1]&0x03)<<8)+d[0])*0.125f-40.0f);
        m_v_bat_cool_out_temp->SetValue(float(((int)(d[3]&0x03)<<8)+d[2])*0.125f-40.0f);
        m_v_mot_cool_in_temp->SetValue(float(((int)(d[5]&0x07)<<8)+d[4])*0.125f-40.0f);
//        m_v_mot_cool_out_temp->SetValue(float(((int)(d[7]&0x07)<<8)+d[6])*0.125f-40.0f);  // value not present
      break;
      }
    case 0x32A: // THC_status1
      {
        m_v_bat_cool_pump_1->SetValue(d[0]&0x7F);
        m_v_bat_cool_pump_2->SetValue(d[1]&0x7F);
        m_v_mot_cool_pump->SetValue(d[2]&0x7F);
      break;
      }
    case 0x3D2: // BMS_kwhCounter
      {
        uint32_t iDischCntr = (d[3]<<24)+(d[2]<<16)+(d[1]<<8)+d[0];
        uint32_t iChgCntr = (d[7]<<24)+(d[6]<<16)+(d[5]<<8)+d[4];
        StandardMetrics.ms_v_bat_energy_used_total->SetValue(float(iDischCntr)/1000);
        StandardMetrics.ms_v_charge_kwh_grid_total->SetValue(float(iChgCntr)/1000);
        break;
      }
    case 0x562: // BMS_OdometerStatus
      {
        StandardMetrics.ms_v_pos_odometer->SetValue(float((d[3]<<24)+(d[2]<<16)+(d[1]<<8)+d[0])*0.001, Miles);
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
    iFrameCountCan2++;

  }

/*
void OvmsVehicleToyotaRav4Ev::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  // Toyota RAV4 EV CAN3 Not Used
  }
*/

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

/**
 * GetDashboardConfig: RAV4 EV specific dashboard setup
 
void OvmsVehicleToyotaRav4Ev::GetDashboardConfig(DashboardConfig& cfg)
{
  cfg.gaugeset1 =
    "yAxis: [{"
      // Speed:
      "min: 0, max: 120,"
      "plotBands: ["
        "{ from: 0, to: 70, className: 'green-band' },"
        "{ from: 70, to: 100, className: 'yellow-band' },"
        "{ from: 100, to: 120, className: 'red-band' }]"
    "},{"
      // Voltage:
      "min: 45, max: 60,"
      "plotBands: ["
        "{ from: 45, to: 47.5, className: 'red-band' },"
        "{ from: 47.5, to: 50, className: 'yellow-band' },"
        "{ from: 50, to: 60, className: 'green-band' }]"
    "},{"
      // SOC:
      "min: 0, max: 100,"
      "plotBands: ["
        "{ from: 0, to: 12.5, className: 'red-band' },"
        "{ from: 12.5, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 100, className: 'green-band' }]"
    "},{"
      // Efficiency:
      "min: 0, max: 300,"
      "plotBands: ["
        "{ from: 0, to: 150, className: 'green-band' },"
        "{ from: 150, to: 250, className: 'yellow-band' },"
        "{ from: 250, to: 300, className: 'red-band' }]"
    "},{"
      // Power:
      "min: -10, max: 30,"
      "plotBands: ["
        "{ from: -10, to: 0, className: 'violet-band' },"
        "{ from: 0, to: 15, className: 'green-band' },"
        "{ from: 15, to: 25, className: 'yellow-band' },"
        "{ from: 25, to: 30, className: 'red-band' }]"
    "},{"
      // Charger temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 65, className: 'normal-band border' },"
        "{ from: 65, to: 80, className: 'red-band border' }]"
    "},{"
      // Battery temperature:
      "min: -15, max: 65, tickInterval: 25,"
      "plotBands: ["
        "{ from: -15, to: 0, className: 'red-band border' },"
        "{ from: 0, to: 50, className: 'normal-band border' },"
        "{ from: 50, to: 65, className: 'red-band border' }]"
    "},{"
      // Inverter temperature:
      "min: 20, max: 80, tickInterval: 20,"
      "plotBands: ["
        "{ from: 20, to: 70, className: 'normal-band border' },"
        "{ from: 70, to: 80, className: 'red-band border' }]"
    "},{"
      // Motor temperature:
      "min: 50, max: 125, tickInterval: 25,"
      "plotBands: ["
        "{ from: 50, to: 110, className: 'normal-band border' },"
        "{ from: 110, to: 125, className: 'red-band border' }]"
    "}]";
}
*/