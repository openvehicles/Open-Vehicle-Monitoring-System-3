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


#include "ovms_log.h"

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"

#include "vehicle_renaultzoe_ph2_obd.h"
#include "ph2_poller.h"

OvmsVehicleRenaultZoePh2OBD::OvmsVehicleRenaultZoePh2OBD() {
  ESP_LOGI(TAG, "Start Renault Zoe Ph2 (OBD) vehicle module");

  //Init variables supressing push on boot up
  StandardMetrics.ms_v_type->SetValue("RZ2");
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);
  StandardMetrics.ms_v_charge_pilot->SetValue(false);
  StandardMetrics.ms_v_charge_state->SetValue("stopped");
  StandardMetrics.ms_v_charge_substate->SetValue("stopped");
  StandardMetrics.ms_v_env_on->SetValue(false);
  
  MyConfig.RegisterParam("xrz2o", "Renault Zoe Ph2 (OBD) configuration", true, true);
  ConfigChanged(NULL);

  // Init Zoe Ph2 OBD Connection (CAN Gateway)
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  POLLSTATE_OFF;
  ESP_LOGI(TAG, "Pollstate switched to OFF");
  
  PollSetPidList(m_can1, renault_zoe_polls);
  PollSetThrottling(10);
  PollSetResponseSeparationTime(20);
  
  // Renault ZOE specific metrics
  mt_bus_awake                = MyMetrics.InitBool("zph2.v.bus.awake", SM_STALE_NONE, false);
  mt_pos_odometer_start       = MyMetrics.InitFloat("zph2.v.pos.odometer.start", SM_STALE_MID, 0, Kilometers, true);
  mt_bat_used_start           = MyMetrics.InitFloat("zph2.b.used.start", SM_STALE_MID, 0, kWh, true);
  mt_bat_recd_start           = MyMetrics.InitFloat("zph2.b.recd.start", SM_STALE_MID, 0, kWh, true);
  mt_bat_chg_start            = MyMetrics.InitFloat("zph2.b.chg.start", SM_STALE_MID, 0, kWh, true);
  mt_bat_available_energy     = MyMetrics.InitFloat("zph2.b.avail.energy", SM_STALE_NONE, 0, kWh, true);
  mt_bat_aux_power_consumer   = MyMetrics.InitFloat("zph2.b.aux.power.consumer", SM_STALE_MID, 0, Watts);
	mt_bat_aux_power_ptc        = MyMetrics.InitFloat("zph2.b.aux.power.ptc", SM_STALE_MID, 0, Watts);
  mt_bat_max_charge_power     = MyMetrics.InitFloat("zph2.b.max.charge.power", SM_STALE_MID, 0, kW, true);
  mt_bat_cycles               = MyMetrics.InitFloat("zph2.b.cycles", SM_STALE_MID, 0, Other, true);
  mt_main_power_available     = MyMetrics.InitFloat("zph2.c.main.power.available", SM_STALE_MIN, 0, kW);
  mt_main_phases              = MyMetrics.InitString("zph2.c.main.phases", SM_STALE_MIN, 0);
  mt_main_phases_num          = MyMetrics.InitFloat("zph2.c.main.phases.num", SM_STALE_MIN, 0);
  mt_inv_status               = MyMetrics.InitString("zph2.m.inverter.status", SM_STALE_NONE, 0);
  mt_inv_hv_voltage           = MyMetrics.InitFloat("zph2.i.voltage", SM_STALE_MID, 0, Volts, true);
  mt_inv_hv_current           = MyMetrics.InitFloat("zph2.i.current", SM_STALE_MID, 0, Amps);
  mt_mot_temp_stator1         = MyMetrics.InitFloat("zph2.m.temp.stator1", SM_STALE_MID, 0, Celcius);
  mt_mot_temp_stator2         = MyMetrics.InitFloat("zph2.m.temp.stator2", SM_STALE_MID, 0, Celcius);
  mt_hvac_compressor_speed    = MyMetrics.InitFloat("zph2.h.compressor.speed", SM_STALE_MID, 0);
  mt_hvac_compressor_pressure = MyMetrics.InitFloat("zph2.h.compressor.pressure", SM_STALE_MID, 0);
  mt_hvac_compressor_power    = MyMetrics.InitFloat("zph2.h.compressor.power", SM_STALE_MID, 0, Watts);
  mt_hvac_compressor_mode     = MyMetrics.InitString("zph2.h.compressor.mode", SM_STALE_MID, 0, Other);

  // BMS configuration:
  BmsSetCellArrangementVoltage(96, 1);
  BmsSetCellArrangementTemperature(12, 1);
  BmsSetCellLimitsVoltage(2.0, 5.0);
  BmsSetCellLimitsTemperature(-39, 200);
  BmsSetCellDefaultThresholdsVoltage(0.030, 0.050);
  BmsSetCellDefaultThresholdsTemperature(4.0, 5.0);  

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
  #endif
}

OvmsVehicleRenaultZoePh2OBD::~OvmsVehicleRenaultZoePh2OBD() {
  ESP_LOGI(TAG, "Stop Renault Zoe Ph2 (OBD) vehicle module");
}

void OvmsVehicleRenaultZoePh2OBD::ConfigChanged(OvmsConfigParam* param) {
  if (param && param->GetName() != "xrz2o")
    return;
 
  // get values from config store
  m_range_ideal               = MyConfig.GetParamValueInt("xrz2o", "rangeideal", 350);
  m_battery_capacity          = MyConfig.GetParamValueInt("xrz2o", "battcapacity", 52000);
  m_UseBMScalculation         = MyConfig.GetParamValueBool("xrz2o", "UseBMScalculation", false);
  StandardMetrics.ms_v_bat_range_ideal->SetValue(m_range_ideal, Kilometers);
  if (m_battery_capacity == 52000) {
    Bat_cell_capacity = 78.0 * 2 * (StandardMetrics.ms_v_bat_soh->AsFloat() / 100.0);
  }
  if (m_battery_capacity == 41000) {
    Bat_cell_capacity = 65.6 * 2 * (StandardMetrics.ms_v_bat_soh->AsFloat() / 100.0);
  }
  if (m_battery_capacity == 22000) {
    Bat_cell_capacity = 36.0 * 2 * (StandardMetrics.ms_v_bat_soh->AsFloat() / 100.0);
  }
  ESP_LOGI(TAG, "Renault Zoe Ph2 (OBD) reload configuration: Range ideal: %d, Battery capacity: %d, Use BMS as energy counter: %s", m_range_ideal, m_battery_capacity, m_UseBMScalculation ? "yes" : "no");
}

void OvmsVehicleRenaultZoePh2OBD::ZoeWakeUp() {
  ESP_LOGI(TAG,"Zoe woke up (CAN Bus activity detected)");
      mt_bus_awake->SetValue(true);;
      StandardMetrics.ms_v_env_charging12v->SetValue(true);
      POLLSTATE_ON;
      ESP_LOGI(TAG, "Pollstate switched to ON");
}

/**
 * Handles incoming CAN-frames on bus 1
 */
void OvmsVehicleRenaultZoePh2OBD::IncomingFrameCan1(CAN_frame_t* p_frame) {
	uint8_t *data = p_frame->data.u8;
	//ESP_LOGI(TAG, "PID:%x DATA: %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  //ESP_LOGD(TAG, "Status CAN Bus: %s", mt_bus_awake->AsBool() ? "true" : "false");

  // Poll reply gives 0x83 0xc0 that means zoe is sleeping and CAN gateway does not respond to anything
   if (mt_bus_awake->AsBool() && data[0] == 0x83 && data[1] == 0xc0) {
      ESP_LOGI(TAG,"Zoe has gone to sleep (CAN Gateway NAK response)");
      mt_bus_awake->SetValue(false);
		  StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
      StandardMetrics.ms_v_pos_speed->SetValue( 0 );
      StandardMetrics.ms_v_bat_12v_current->SetValue( 0 );
      StandardMetrics.ms_v_charge_12v_current->SetValue( 0 );
      StandardMetrics.ms_v_bat_current->SetValue( 0 );
      POLLSTATE_OFF;
      ESP_LOGI(TAG, "Pollstate switched to OFF");
      //Check if car is locked, if not send notify
      if (!StandardMetrics.ms_v_env_locked->AsBool()) {
        MyNotify.NotifyString("alert", "vehicle.lock", "Vehicle is not locked");
      }
    } else if (!mt_bus_awake->AsBool()) {
      ZoeWakeUp();
    }
  //There are some free frames on wakeup and start and stop charging.... I try to use them to see if Car is awake and charging, see /Reference/ZOE_Ph2_xxx text files
  //if (!mt_bus_awake->AsBool() && (data[0] == 0x05 || data[0] == 0x06 || data[0] == 0x07)) { //listen for SingleFrames (0x0) with length of 5-7
  //    ZoeWakeUp();
  //}
}

/**
 * Handles incoming poll results
 */
void OvmsVehicleRenaultZoePh2OBD::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	string& rxbuf = zoe_obd_rxbuf;
  
  //ESP_LOGV(TAG, "pid: %04x length: %d m_poll_ml_remain: %d m_poll_ml_frame: %d", pid, length, m_poll_ml_remain, m_poll_ml_frame);

  // init / fill rx buffer:
  if (m_poll_ml_frame == 0) {
    rxbuf.clear();
    rxbuf.reserve(length + remain);
  }
  rxbuf.append((char*)data, length);
  
  if (remain)
    return;
  
	switch (m_poll_moduleid_low) {
		// ****** INV *****
		case 0x18daf1df:
			IncomingINV(type, pid, rxbuf.data(), rxbuf.size());
			break;
    // ****** EVC *****
		case 0x18daf1da:
			IncomingEVC(type, pid, rxbuf.data(), rxbuf.size());
			break;
    // ****** BCM *****
    case 0x765:
      IncomingBCM(type, pid, rxbuf.data(), rxbuf.size());
      break;
    // ****** LBC *****
    case 0x18daf1db:
      IncomingLBC(type, pid, rxbuf.data(), rxbuf.size());
      break;
    // ****** HVAC *****
    case 0x764:
      IncomingHVAC(type, pid, rxbuf.data(), rxbuf.size());
      break;
    // ****** UCM *****
    case 0x76D:
      IncomingUCM(type, pid, rxbuf.data(), rxbuf.size());
      break;
    // ****** CLUSTER *****
    //case 0x763:
    //  IncomingCLUSTER(type, pid, rxbuf.data(), rxbuf.size());
    //  break;
	}
}

int OvmsVehicleRenaultZoePh2OBD::calcMinutesRemaining(float charge_voltage, float charge_current) {
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);

  float remaining_wh    = m_battery_capacity - (m_battery_capacity * bat_soc / 100.0);
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0;
  //ESP_LOGD(TAG, "SOC: %f, BattCap:%d, Current: %f, Voltage: %f, RemainWH: %f, RemainHour: %f, RemainMin: %f", bat_soc, m_battery_capacity, charge_current, charge_voltage, remaining_wh, remaining_hours, remaining_mins);
  return MIN( 1440, (int)remaining_mins );
}

//Handle Charging values
void OvmsVehicleRenaultZoePh2OBD::ChargeStatistics() {
  float charge_current  = fabs(StandardMetrics.ms_v_bat_current->AsFloat(0, Amps));
  float charge_voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float battery_power   = fabs(StandardMetrics.ms_v_bat_power->AsFloat(0, kW));
  float charger_power   = StandardMetrics.ms_v_charge_power->AsFloat(0, kW);
  float ac_current      = StandardMetrics.ms_v_charge_current->AsFloat(0, Amps);
  string ac_phases      = mt_main_phases->AsString();
  
  if (CarIsCharging != CarLastCharging) {
        if (CarIsCharging) {
          StandardMetrics.ms_v_charge_kwh->SetValue(0);
          StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
          if (m_UseBMScalculation) {
            mt_bat_chg_start->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat());
          }
        } else {
          StandardMetrics.ms_v_charge_inprogress->SetValue(CarIsCharging);
        }
      }
  CarLastCharging = CarIsCharging;

  if (!StandardMetrics.ms_v_charge_pilot->AsBool() || !StandardMetrics.ms_v_charge_inprogress->AsBool() || (charge_current <= 0.0) ) {
    return;
  }
  
  if (charge_voltage > 0 && charge_current > 0) {
    float power  = charge_voltage * charge_current / 1000.0;
    float energy = power / 3600.0 * 10.0;
    float c_efficiency;

    if (m_UseBMScalculation) {
      StandardMetrics.ms_v_charge_kwh->SetValue(StandardMetrics.ms_v_charge_kwh_grid_total->AsFloat() - mt_bat_chg_start->AsFloat());
    } else {
      StandardMetrics.ms_v_charge_kwh->SetValue( StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);
    }
    
    int minsremaining = calcMinutesRemaining(charge_voltage, charge_current);
    StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining, Minutes);

    if (StandardMetrics.ms_v_charge_type->AsString() == "type2") {
      c_efficiency = (battery_power / charger_power) * 100.0;
      if (c_efficiency < 100.0) {
        StandardMetrics.ms_v_charge_efficiency->SetValue(c_efficiency);
      }
      ESP_LOGI(TAG, "Charge time remaining: %d mins, AC Charge at %.2f kW with %.1f amps, %s at %.1f efficiency, %.2f powerfactor", minsremaining, charger_power, ac_current, ac_phases.c_str(), StandardMetrics.ms_v_charge_efficiency->AsFloat(100), ACInputPowerFactor);
    } else if (StandardMetrics.ms_v_charge_type->AsString() == "ccs" || StandardMetrics.ms_v_charge_type->AsString() == "chademo") {
      StandardMetrics.ms_v_charge_power->SetValue(battery_power);
      ESP_LOGI(TAG, "Charge time remaining: %d mins, DC Charge at %.2f kW", minsremaining, battery_power);
    }
  }
}

void OvmsVehicleRenaultZoePh2OBD::EnergyStatisticsOVMS() {
  float voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  float power = voltage * current / 1000.0;
  
  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool()) {
    float energy = power / 3600.0 * 10.0;
    if (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue( StandardMetrics.ms_v_bat_energy_used->AsFloat() + energy);
    else
      StandardMetrics.ms_v_bat_energy_recd->SetValue( StandardMetrics.ms_v_bat_energy_recd->AsFloat() - energy);
  }
}

void OvmsVehicleRenaultZoePh2OBD::EnergyStatisticsBMS() {
  StandardMetrics.ms_v_bat_energy_used->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat() - mt_bat_used_start->AsFloat());
  StandardMetrics.ms_v_bat_energy_recd->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat() - mt_bat_recd_start->AsFloat());
}

void OvmsVehicleRenaultZoePh2OBD::Ticker10(uint32_t ticker) {
  if (StandardMetrics.ms_v_charge_pilot->AsBool() && !StandardMetrics.ms_v_env_on->AsBool()) {
    ChargeStatistics();
  } else {
    if (m_UseBMScalculation) {
      EnergyStatisticsBMS();
    } else {
      EnergyStatisticsOVMS();
    }
  }
}

void OvmsVehicleRenaultZoePh2OBD::Ticker1(uint32_t ticker) {
  if (StandardMetrics.ms_v_env_on->AsBool() && !CarIsDriving) {
    CarIsDriving = true;
    //Start trip after power on
    StandardMetrics.ms_v_pos_trip->SetValue(0);
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0));
    StandardMetrics.ms_v_bat_energy_used->SetValue(0);
    StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
    if (m_UseBMScalculation) {
      mt_bat_used_start->SetValue(StandardMetrics.ms_v_bat_energy_used_total->AsFloat());
      mt_bat_recd_start->SetValue(StandardMetrics.ms_v_bat_energy_recd_total->AsFloat());
    }
  }
  if (CarIsDriving && !CarIsDrivingInit) {
    CarIsDrivingInit = true;
    ESP_LOGI(TAG, "Pollstate switched to DRIVING");
    POLLSTATE_DRIVING;
  }
  if (!StandardMetrics.ms_v_env_on->AsBool() && CarIsDriving) {
    CarIsDriving = false;
    CarIsDrivingInit = false;
    ESP_LOGI(TAG, "Pollstate switched to ON");
    POLLSTATE_ON;
  }
  if (StandardMetrics.ms_v_charge_pilot->AsBool() && StandardMetrics.ms_v_charge_inprogress->AsBool() && !CarIsCharging) {
    CarIsCharging = true;
    ESP_LOGI(TAG, "Pollstate switched to CHARGING");
    POLLSTATE_CHARGING;
  } else if (!StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging) {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    ESP_LOGI(TAG, "Pollstate switched to ON");
    POLLSTATE_ON;
  } else if (StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging && StandardMetrics.ms_v_charge_substate->AsString() == "powerwait") {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    ESP_LOGI(TAG, "Pollstate switched to OFF, Wait for power...");
    POLLSTATE_OFF;
  } else if (StandardMetrics.ms_v_charge_pilot->AsBool() && CarIsCharging && StandardMetrics.ms_v_charge_state->AsString() == "done") {
    CarIsCharging = false;
    StandardMetrics.ms_v_charge_duration_full->SetValue(0);
    ESP_LOGI(TAG, "Pollstate switched to OFF, done charging...");
    POLLSTATE_OFF;
  }

  if (StandardMetrics.ms_v_env_on->AsBool()) {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }
  StandardMetrics.ms_v_bat_range_est->SetValue((m_range_ideal * (StandardMetrics.ms_v_bat_soc->AsFloat(1) * 0.01)), Kilometers);

}

class OvmsVehicleRenaultZoePh2OBDInit {
  public: OvmsVehicleRenaultZoePh2OBDInit();
} MyOvmsVehicleRenaultZoePh2OBDInit  __attribute__ ((init_priority (9000)));


OvmsVehicleRenaultZoePh2OBDInit::OvmsVehicleRenaultZoePh2OBDInit() 
{
  ESP_LOGI(TAG, "Registering Vehicle: Renault Zoe Ph2 (OBD) (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultZoePh2OBD>("RZ2","Renault Zoe Ph2 (OBD)");
}