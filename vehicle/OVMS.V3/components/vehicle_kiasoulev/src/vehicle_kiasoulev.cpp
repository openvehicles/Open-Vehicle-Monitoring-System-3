/*
;    Project:       Open Vehicle Monitor System
;    Date:          22th October 2017
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2017        Geir Øyvind Vælidalo <geir@validalo.net>
;;
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

#include "esp_log.h"
static const char *TAG = "v-kiasoulev";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_kiasoulev.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

#define VERSION "0.1.0"

static const OvmsVehicle::poll_pid_t vehicle_kiasoulev_polls[]
  =
  {
    { 0x7e2, 0, 	    VEHICLE_POLL_TYPE_OBDIIVEHICLE,  0x02, { 0, 120, 120 } }, // VIN
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x01, { 30, 10, 10 } }, 	// Diag page 01
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x02, { 0, 30, 10 } }, 	// Diag page 02
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x03, { 0, 30, 10 } }, 	// Diag page 03
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x04, { 0, 30, 10 } }, 	// Diag page 04
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x05, { 120, 10, 10 } }, // Diag page 05
    { 0x794, 0x79c, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x02, { 0, 0, 10 } }, 	// On board charger
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x00, { 30, 10, 10 } }, 	// VMCU  Shift-stick
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x02, { 30, 10, 0 } }, 	// Motor temp++
    { 0x7df, 0x7de, VEHICLE_POLL_TYPE_OBDIIGROUP, 	0x06, { 30, 10, 0 } }, 	// TMPS
    { 0, 0, 0, 0, { 0, 0, 0 } }
  };

OvmsVehicleKiaSoulEv::OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Kia Soul EV v3.0 vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(ks_battery_cell_voltage,0,sizeof(ks_battery_cell_voltage));
  memset(ks_battery_module_temp,0,sizeof(ks_battery_module_temp));
  memset(ks_battery_cum_op_time,0,sizeof(ks_battery_cum_op_time));
  memset(ks_tpms_id,0,sizeof(ks_tpms_id));
  ks_battery_max_detoriation = 0;
  ks_battery_min_detoriation = 0;
  ks_maxrange = CFG_DEFAULT_MAXRANGE;

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);

  MyConfig.RegisterParam("x.ks", "Kia Soul EV", true, true);
  ConfigChanged(NULL);

  // init metrics:
  m_version = MyMetrics.InitString("x.ks.m.version", 0, VERSION " " __DATE__ " " __TIME__);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicleKiaSoulEv::Ticker1, this, _1, _2));
  PollSetPidList(m_can1,vehicle_kiasoulev_polls);
  PollSetState(0);
  }

OvmsVehicleKiaSoulEv::~OvmsVehicleKiaSoulEv()
  {
  ESP_LOGI(TAG, "Shutdown Kia Soul EV vehicle module");
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */

void OvmsVehicleKiaSoulEv::ConfigChanged(OvmsConfigParam* param)
{
  ESP_LOGD(TAG, "Kia Soul EV reload configuration");

  // Instances:
  //
  //  suffsoc           Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange         Sufficient range [km] (Default: 0=disabled)
  //  maxrange          Maximum ideal range at 20 °C [km] (Default: 80)
  //
  //  cap_act_prc       Battery actual capacity level [%] (Default: 100.0)
  //  cap_nom_ah        Battery nominal capacity [Ah] (Default: 108.0)
  //
  //  chargelevel       Charge power level [1-7] (Default: 0=unlimited)
  //  chargemode        Charge mode: 0=notify, 1=stop at sufficient SOC/range (Default: 0)
  //
  //  canwrite          Bool: CAN write enabled (Default: no)
  //  autoreset         Bool: SEVCON reset on error (Default: yes)
  //  kickdown          Bool: SEVCON automatic kickdown (Default: yes)
  //  autopower         Bool: SEVCON automatic power level adjustment (Default: yes)
  //  console           Bool: SimpleConsole inputs enabled (Default: no)
  //

  //TODO ks_battery_capacity = MyConfig.GetParamValueInt("x.ks", "batteryCapacity", CGF_DEFAULT_BATTERY_CAPACITY);

  ks_maxrange = MyConfig.GetParamValueInt("x.ks", "maxrange", CFG_DEFAULT_MAXRANGE);
  if (ks_maxrange <= 0)
    ks_maxrange = CFG_DEFAULT_MAXRANGE;

  *StdMetrics.ms_v_charge_limit_soc = (float) MyConfig.GetParamValueInt("x.ks", "suffsoc");
  *StdMetrics.ms_v_charge_limit_range = (float) MyConfig.GetParamValueInt("x.ks", "suffrange");
/*
  cfg_chargemode = MyConfig.GetParamValueInt("x.ks", "chargemode");
  cfg_chargelevel = MyConfig.GetParamValueInt("x.ks", "chargelevel");
  twizy_flags.EnableWrite = MyConfig.GetParamValueBool("x.ks", "canwrite", false);
*/
}

////////////////////////////////////////////////////////////////////////
// vehicle_kiasoulev_car_on()
// Takes care of setting all the state appropriate when the car is on
// or off. Centralized so we can more easily make on and off mirror
// images.
//

void OvmsVehicleKiaSoulEv::vehicle_kiasoulev_car_on(bool isOn)
  {

  if (isOn)
    {
	// Car is ON
	if (!StdMetrics.ms_v_env_on->AsBool())
	  {
	  StdMetrics.ms_v_env_on->SetValue(isOn);
	  StdMetrics.ms_v_env_awake->SetValue(isOn);

	  // Start trip, save current state
	  //ODO at Start of trip
	  ks_trip_start_odo = POS_ODO;
	  ks_start_cdc = ks_battery_cum_discharge; 	//Cumulated discharge
	  ks_start_cc = ks_battery_cum_charge; 		//Cumulated charge
	  }
	if ( StdMetrics.ms_v_env_parktime->AsInt() > 0)
	  {
	  StdMetrics.ms_v_env_parktime->SetValue(0); // No longer parking
	  //TODO net_req_notification(NET_NOTIFY_ENV);
	  }
  } else {
    // Car is OFF
	StdMetrics.ms_v_env_on->SetValue(isOn);
	StdMetrics.ms_v_env_awake->SetValue(isOn);

	StdMetrics.ms_v_pos_speed->SetValue(0);

	if (StdMetrics.ms_v_env_parktime->AsInt() == 0) {
	  StdMetrics.ms_v_env_parktime->SetValue((int) (time(NULL) - 1));
	  //TODO net_req_notification(NET_NOTIFY_ENV);
	  }
    }
  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO- ks_trip_start_odo );
  }

/**
 * Handles incoming CAN-frames on bus 1
 */
void OvmsVehicleKiaSoulEv::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
  	  case 0x018:
  	   {
       // Doors status Byte 0:
  	   StdMetrics.ms_v_door_chargeport->SetValue((d[0] & 0x01) > 0);
  	   StdMetrics.ms_v_door_fl->SetValue((d[0] & 0x10) > 0);
  	   StdMetrics.ms_v_door_fr->SetValue((d[0] & 0x80) > 0);
  	   StdMetrics.ms_v_door_rl->SetValue(false); //TODO
  	   StdMetrics.ms_v_door_rr->SetValue(false); //TODO
  	   // Light status Byte 3 & 5
  	   // Seat belt status Byte 7
  	   }
       break;

  	  case 0x120: //Locks - TODO Not working correctly
  	    {
  	    	  if( d[2]==0x20 ){
  	        if( d[3]==0x20){
  	          //TODO ks_doors2.CarLocked = 0x01;
  	        }else if( d[3]==0x10){
  	          //TODO ks_doors2.CarLocked = 0x00;
  	        }
  	       }
         }
  	     break;

      case 0x200:
        {
          // Estimated range:
          uint16_t estRange = d[2] + ((d[1] & 1)<<9);
          if (estRange > 0){
        	    StdMetrics.ms_v_bat_range_est->SetValue((float)(estRange<<1), Kilometers );
          }
        }
        break;

      case 0x433:
        {
          // Parking brake status
        	  StdMetrics.ms_v_env_handbrake->SetValue((d[2] & 0x10) > 0);
        }
        break;

      case 0x4f0:
        {
          // Odometer:
          if (/*length = 8 &&*/ (d[7] || d[6] || d[5])) { //TODO can_datalength was needed in V2
            StdMetrics.ms_v_pos_odometer->SetValue((d[7]<<16) + (d[6]<<8) + d[5], Kilometers);
          }
        }
        break;

      case 0x4f2:
        {
          // Speed:
        	  StdMetrics.ms_v_pos_speed->SetValue( d[1] >> 1, Kph ); // kph

          // 4f2 is one of the few can-messages that are sent while car is both on and off.
          // Byte 2 and 7 are some sort of counters which runs while the car is on.
          if (d[2] > 0 || d[7] > 0){
        	    vehicle_kiasoulev_car_on(true);
          } else if (d[2] == 0 && d[7] == 0 && StdMetrics.ms_v_pos_speed->AsFloat(Kph) == 0 && StdMetrics.ms_v_env_handbrake->AsBool()) {
            // Boths 2 and 7 and speed is 0, so we assumes the car is off.
      	    vehicle_kiasoulev_car_on(false);
          }
        }
        break;

      case 0x542:
        {
        // SOC:
        if (d[0] > 0)
        	  StdMetrics.ms_v_bat_soc->SetValue( d[0] >> 1 ); //In V2 car_SOC
        }
        break;

      case 0x581:
        {
          // Car is CHARGING:
        	  StdMetrics.ms_v_bat_power->SetValue((float)(((uint16_t) d[7] << 8) | d[6])/256.0, kW);
        }
        break;

      case 0x594:
        {
          // BMS SOC:
          uint16_t val = (uint16_t) d[5] * 5 + d[6];
          if (val > 0){
            ks_bms_soc = (uint8_t) (val / 10);
          }
        }
        break;

      case 0x653:
        {
          // AMBIENT TEMPERATURE:
          StdMetrics.ms_v_env_temp->SetValue( TO_CELCIUS(d[5]/2.0), Celcius);
        }
        break;
    }

  	// Check if response is from synchronous can message
	if(p_frame->MsgID==(ks_send_can.id+0x08) && ks_send_can.status==0xff){
		//Store message bytes so that the async method can continue
		ks_send_can.status=3;
		ks_send_can.byte[0]=d[0];
		ks_send_can.byte[1]=d[1];
		ks_send_can.byte[2]=d[2];
		ks_send_can.byte[3]=d[3];
		ks_send_can.byte[4]=d[4];
		ks_send_can.byte[5]=d[5];
		ks_send_can.byte[6]=d[6];
		ks_send_can.byte[7]=d[7];
	}

  }

void OvmsVehicleKiaSoulEv::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

/**
 * Incoming poll reply messages
 */
void OvmsVehicleKiaSoulEv::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
	  uint8_t i;
	  UINT base;
	  uint32_t val;

	  switch (m_poll_moduleid_low) { //TODO Is this correct?
	    case 0x7de: //TPMS
	      switch (pid) {
	        case 0x06:
	          if (m_poll_ml_frame == 0) {
	            val = CAN_UINT32(4);
	            if (val > 0) ks_tpms_id[0] = val;

	          } else if (m_poll_ml_frame == 1) {
	            i = CAN_BYTE(0);
	            if (i > 0) StdMetrics.ms_v_tpms_fl_p->SetValue( TO_PSI(i), PSI);
	            StdMetrics.ms_v_tpms_fl_t->SetValue( TO_CELCIUS(CAN_BYTE(1)), Celcius);
	            val = (ks_tpms_id[1] & 0x000000ff) | (CAN_UINT32(4) & 0xffffff00);
	            if (val > 0) ks_tpms_id[1] = val;

	          } else if (m_poll_ml_frame == 2) {
	            val = (uint32_t) CAN_BYTE(0) | (ks_tpms_id[1] & 0xffffff00);
	            if (val > 0) ks_tpms_id[1] = val;
	            i = CAN_BYTE(1);
	            if (i > 0) StdMetrics.ms_v_tpms_fr_p->SetValue( TO_PSI(i), PSI);
	            StdMetrics.ms_v_tpms_fr_t->SetValue( TO_CELCIUS(CAN_BYTE(2)), Celcius);
	            val = (ks_tpms_id[2] & 0x0000ffff) | (CAN_UINT32(5) & 0xffff0000);
	            if (val > 0) ks_tpms_id[2] = val;

	          } else if (m_poll_ml_frame == 3) {
	            val = ((uint32_t) CAN_UINT(0)) | (ks_tpms_id[2] & 0xffff0000);
	            if (val > 0) ks_tpms_id[2] = val;
	            i = CAN_BYTE(2);
	            if (i > 0) StdMetrics.ms_v_tpms_rl_p->SetValue( TO_PSI(i), PSI);
	            StdMetrics.ms_v_tpms_rl_t->SetValue( TO_CELCIUS(CAN_BYTE(3)), Celcius);
	            val = (ks_tpms_id[3] & 0x00ffffff) | ((uint32_t) CAN_BYTE(6) << 24);
	            if (val > 0) ks_tpms_id[3] = val;

	          } else if (m_poll_ml_frame == 4) {
	            val = (CAN_UINT24(0)) | (ks_tpms_id[3] & 0xff000000);
	            if (val > 0) ks_tpms_id[3] = val;
	            i = CAN_BYTE(3);
	            if (i > 0) StdMetrics.ms_v_tpms_rr_p->SetValue( TO_PSI(i), PSI);
	            StdMetrics.ms_v_tpms_rr_t->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
	          }
	          break;
	      }
	      break;

	    case 0x79c: //OBC
	      switch (pid) {
	        case 0x02:
	          if (m_poll_ml_frame == 1) {
	            ks_obc_volt = (float) CAN_UINT(2) / 10.0;
	            //} else if (vehicle_poll_ml_frame == 2) {
	            //ks_obc_ampere = ((UINT) can_databuffer[4 + CAN_ADJ] << 8)
	            //        | (UINT) can_databuffer[5 + CAN_ADJ];
	          }
	          break;
	      }
	      break;

	    case 0x7ea:
	      switch (pid) {
	        case 0x00:
	          if (m_poll_ml_frame == 1) {
	            ks_shift_bits.value = CAN_BYTE(3);
	          }
	          break;

	        case 0x02:
	          // VIN (multi-line response):
	          // VIN length is 20 on Kia => skip first frame (3 bytes):
	          if (m_poll_ml_frame > 0 && type == VEHICLE_POLL_TYPE_OBDIIVEHICLE) {
	        	  	base = m_poll_ml_offset - length - 3;
	            for (i = 0; (i < length) && ((base + i)<(sizeof (m_vin) - 1)); i++)
	              m_vin[base + i] = CAN_BYTE(i);
	            if (m_poll_ml_remain == 0)
	            	m_vin[base + i] = 0;
	          }
	          if (type == VEHICLE_POLL_TYPE_OBDIIGROUP) {
	            if (m_poll_ml_frame == 3) {
	            	  StdMetrics.ms_v_mot_temp->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
	            	  StdMetrics.ms_v_inv_temp->SetValue( TO_CELCIUS(CAN_BYTE(5)), Celcius );
	            	  StdMetrics.ms_v_charge_temp->SetValue( TO_CELCIUS(CAN_BYTE(6)), Celcius );
	            }
	          }
	          break;
	      }
	      break;

	    case 0x7ec:
	      switch (pid) {
	        case 0x01:
	          // diag page 01: skip first frame (no data)
	          if (m_poll_ml_frame > 0) {
	            if (m_poll_ml_frame == 1) // 02 21 01 - 21
	            {
	              ks_battery_avail_charge = CAN_UINT(1);
	              //ks_battery_avail_discharge = (UINT8)(((UINT) can_databuffer[5 + CAN_ADJ]
	              //        | ((UINT) can_databuffer[4 + CAN_ADJ] << 8))>>2);
	              i = CAN_BYTE(5);
	              StdMetrics.ms_v_charge_pilot->SetValue((i & 0x80) > 0);
	              ks_charge_bits.ChargingChademo = ((i & 0x40) > 0);
	              ks_charge_bits.ChargingJ1772 = ((i & 0x20) > 0);
	              ks_battery_current = (ks_battery_current & 0x00FF)
	                      | ((UINT) CAN_BYTE(6) << 8);
	            }
	            if (m_poll_ml_frame == 2) // 02 21 01 - 22
	            {
	              ks_battery_current = (ks_battery_current & 0xFF00)
	                      | (UINT) CAN_BYTE(0);
	              ks_battery_DC_voltage = (float)CAN_UINT(1)/10.0;
	              ks_battery_module_temp[0] = CAN_BYTE(3);
	              ks_battery_module_temp[1] = CAN_BYTE(4);
	              ks_battery_module_temp[2] = CAN_BYTE(5);
	              ks_battery_module_temp[3] = CAN_BYTE(6);

	            } else if (m_poll_ml_frame == 3) // 02 21 01 - 23
	            {
	              ks_battery_module_temp[4] = CAN_BYTE(0);
	              ks_battery_module_temp[5] = CAN_BYTE(1);
	              ks_battery_module_temp[6] = CAN_BYTE(2);
	              ks_battery_module_temp[7] = CAN_BYTE(3);
	              //TODO What about the 30kWh-version?

	              ks_battery_max_cell_voltage = CAN_BYTE(5);
	              ks_battery_max_cell_voltage_no = CAN_BYTE(6);

	            } else if (m_poll_ml_frame == 4) // 02 21 01 - 24
	            {
	              ks_battery_min_cell_voltage = CAN_BYTE(0);
	              ks_battery_min_cell_voltage_no = CAN_BYTE(1);
	              ks_battery_fan_feedback = CAN_BYTE(2);
	              ks_charge_bits.FanStatus = CAN_BYTE(3) & 0xF;
	              StdMetrics.ms_v_bat_12v_voltage->SetValue (CAN_BYTE(4)/10.0 , Volts);
	              ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0x0000FFFF) | ((uint32_t) CAN_UINT(5) << 16);

	            } else if (m_poll_ml_frame == 5) // 02 21 01 - 25
	            {
	              ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0xFFFF0000) | ((uint32_t) CAN_UINT(0));
	              ks_battery_cum_discharge_current = CAN_UINT32(2);
	              ks_battery_cum_charge = (ks_battery_cum_charge & 0x00FFFFFF) | ((uint32_t) CAN_BYTE(6)) << 24;
	            } else if (m_poll_ml_frame == 6) // 02 21 01 - 26
	            {
	              ks_battery_cum_charge = (ks_battery_cum_charge & 0xFF000000) | ((uint32_t) CAN_UINT24(0));
	              ks_battery_cum_discharge = CAN_UINT32(3);
	            } else if (m_poll_ml_frame == 7) // 02 21 01 - 27
	            {
	              val = CAN_UINT32(0) / 3600;
	              ks_battery_cum_op_time[0] = (val >> 16) & 0xff;
	              ks_battery_cum_op_time[1] = (val >> 8) & 0xff;
	              ks_battery_cum_op_time[2] = val & 0xff;
	            }
	          }
	          break;

	        case 0x02: //Cell voltages
	        case 0x03:
	        case 0x04:
	          // diag page 02-04: skip first frame (no data)
			  base = ((pid-2)<<5) + m_poll_ml_offset - (length - 3);
			  for (i = 0; i < length && ((base + i)<sizeof (ks_battery_cell_voltage)); i++)
				ks_battery_cell_voltage[base + i] = CAN_BYTE(i);
	          break;

	        case 0x05:
	          if (m_poll_ml_frame == 1) {
	            ks_battery_inlet_temperature = CAN_BYTE(5);
	            ks_battery_min_temperature = CAN_BYTE(6);
	          } else if (m_poll_ml_frame == 2) {
	            ks_battery_max_temperature = CAN_BYTE(0);
	          } else if (m_poll_ml_frame == 3) {
	            //ks_air_bag_hwire_duty = can_databuffer[5 + CAN_ADJ];
	            ks_battery_heat_1_temperature = CAN_BYTE(5);
	            ks_battery_heat_2_temperature = CAN_BYTE(6);
	          } else if (m_poll_ml_frame == 4) {
	            ks_battery_max_detoriation = CAN_UINT(0);
	            ks_battery_max_detoriation_cell_no = CAN_BYTE(2);
	            ks_battery_min_detoriation = CAN_UINT(3);
	            ks_battery_min_detoriation_cell_no = CAN_BYTE(5);
	          }
	          break;
	      }
	      break;

	  }
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleKiaSoulEv::Ticker1(std::string event, void* data)
  {
	  // ---------------------------------------------------------------------------
	  //
	  // Check CAN bus activity timeout:
	  //
	  //TODO
	  /*
	  if (ks_busactive > 0)
	    ks_busactive--;

	  if (ks_busactive == 0) {
	    // CAN bus is inactive, switch OFF:
	    ks_doors1.CarON = 0;
	    ks_doors1.Charging = 0;
	    ks_chargepower = 0;
	    car_speed = 0; //Clear speed
	  }
	   */
	  /***************************************************************************
	   * Read feature configuration:
	   */

	  UpdateMaxRange();

	  //
	  // Convert KS status data to OVMS framework:
	  //
	  *StdMetrics.ms_m_timeutc = (int) time(NULL); // → framework? roadster fetches from CAN…

	  if (FULL_RANGE > 0)
		StdMetrics.ms_v_bat_range_ideal->SetValue( FULL_RANGE * BAT_SOC / 100.0, Kilometers);


	  StdMetrics.ms_v_bat_temp->SetValue(((float) ks_battery_module_temp[0] + (float) ks_battery_module_temp[1] +
	          (float) ks_battery_module_temp[2] + (float) ks_battery_module_temp[3] +
	          (float) ks_battery_module_temp[4] + (float) ks_battery_module_temp[5] +
	          (float) ks_battery_module_temp[6] + (float) ks_battery_module_temp[7]) / 8, Celcius);

	  // Update trip
	  if (StdMetrics.ms_v_env_on->AsBool()){
		  StdMetrics.ms_v_pos_trip->SetValue( POS_ODO - ks_trip_start_odo , Kilometers);
		  StdMetrics.ms_v_bat_energy_used->SetValue( ks_battery_cum_discharge - ks_start_cdc - (ks_battery_cum_charge - ks_start_cc), kWh );
		  StdMetrics.ms_v_bat_energy_recd->SetValue( ks_battery_cum_charge - ks_start_cc, kWh );
	  }

	  //
	  // Check for car status changes:
	  //
/*	  if( car_doors1bits.FrontLeftDoor!=ks_doors1.FrontLeftDoor ||
	      car_doors1bits.FrontRightDoor!=ks_doors1.FrontRightDoor ||
	      car_doors1bits.ChargePort != ks_doors1.ChargePort ||
	      car_doors1bits.HandBrake != ks_doors1.HandBrake ||
	      car_doors2bits.CarLocked != ks_doors2.CarLocked
	      ){
	    car_doors1bits.FrontLeftDoor = ks_doors1.FrontLeftDoor;
	    car_doors1bits.FrontRightDoor = ks_doors1.FrontRightDoor;
	    car_doors1bits.ChargePort = ks_doors1.ChargePort;
	    car_doors1bits.HandBrake = ks_doors1.HandBrake;
	    car_doors2bits.CarLocked = ks_doors2.CarLocked;
	    net_req_notification(NET_NOTIFY_ENV);
	  }

*/
	  //Update charging information
	  if (ks_charge_bits.ChargingJ1772) { // J1772 - Type 1  charging
		  SetChargeMetrics(ks_obc_volt,
				StdMetrics.ms_v_bat_power->AsFloat(kW) * 1000 / CHARGE_VOLTAGE,
				"standard",
				6600 / CHARGE_VOLTAGE,
				"type1");
	  } else if (ks_charge_bits.ChargingChademo) { //ChaDeMo charging
		  SetChargeMetrics(ks_battery_DC_voltage,
				-ks_battery_current / 10.0,
				"performance",
				(float) ks_battery_avail_charge * 10 / ks_battery_DC_voltage,
				"chademo");
	  }

	  //
	  // Check for charging status changes:
	  bool isCharging = StdMetrics.ms_v_charge_pilot->AsBool()
				  && (ks_charge_bits.ChargingChademo || ks_charge_bits.ChargingJ1772)
		          && (CHARGE_CURRENT > 0);

	  if (isCharging) {
	    if (!StdMetrics.ms_v_charge_inprogress->AsBool() ) {
	      // ******* Charging started: **********
	    	  StdMetrics.ms_v_charge_duration_full->SetValue( 60*24, Minutes ); //Lets start at 24H to full.
	    	  StdMetrics.ms_v_charge_minutes->SetValue( 0, Minutes );
	      StdMetrics.ms_v_charge_state->SetValue( "charging" );
	      StdMetrics.ms_v_charge_kwh->SetValue( 0, kWh );  // kWh charged
	      //TODO ks_sms_bits.NotifyCharge = 1; // Send SMS when charging is fully initiated
	      ks_cum_charge_start = ks_battery_cum_charge;
	      ks_chargeduration = 0;

	      StdMetrics.ms_v_charge_inprogress->SetValue( isCharging );

	      //TODO net_req_notification(NET_NOTIFY_CHARGE);
	      //TODO net_req_notification(NET_NOTIFY_STAT);
	    } else {
	      // ******* Charging continues: *******
	    	  ks_chargeduration++;
	      StdMetrics.ms_v_charge_minutes->SetValue( (int) ks_chargeduration / 60, Minutes );

	      if (((BAT_SOC > 0) && (LIMIT_SOC > 0)
	    		  && (BAT_SOC >= LIMIT_SOC) && (ks_last_soc < LIMIT_SOC)) ||
	              ((EST_RANGE > 0) && (LIMIT_RANGE > 0)
	            		  && (IDEAL_RANGE >= LIMIT_RANGE )
	            		  && (ks_last_ideal_range < LIMIT_RANGE ))) {
	        // ...enter state 2=topping off:
		    StdMetrics.ms_v_charge_state->SetValue("topoff");

	        // ...send charge alert:
	        //TODO net_req_notification(NET_NOTIFY_CHARGE);
		    //TODO net_req_notification(NET_NOTIFY_STAT);
	      }        // ...else set "topping off" from 94% SOC:
	      else if (BAT_SOC >= 95) {
		    StdMetrics.ms_v_charge_state->SetValue("topoff");
	        //TODO net_req_notification(NET_NOTIFY_ENV);
	      }
	    }

	    // Check if we have what is needed to calculate remaining minutes
	    if (CHARGE_VOLTAGE > 0 && CHARGE_CURRENT > 0) {
	    	  //Calculate remaining charge time
	    	  float chargeTarget_full = ks_battery_capacity;
	    	  float chargeTarget_soc = ks_battery_capacity;
	    	  float chargeTarget_range = ks_battery_capacity;

	      if (LIMIT_SOC > 0) {
	        chargeTarget_soc = ks_battery_capacity * LIMIT_SOC / 100.0;
	      } else if (LIMIT_RANGE > 0) {
	        chargeTarget_range = LIMIT_RANGE * ks_battery_capacity / FULL_RANGE;
	      }
	      if (ks_charge_bits.ChargingChademo) { //ChaDeMo charging
	        chargeTarget_full = MIN(chargeTarget_full, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	        chargeTarget_soc = MIN(chargeTarget_soc, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	        chargeTarget_range = MIN(chargeTarget_range, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	      }

	      StdMetrics.ms_v_charge_duration_full->SetValue( calcMinutesRemaining(chargeTarget_full), Minutes);
	      StdMetrics.ms_v_charge_duration_soc->SetValue( calcMinutesRemaining(chargeTarget_soc), Minutes);
	      StdMetrics.ms_v_charge_duration_range->SetValue( calcMinutesRemaining(chargeTarget_range), Minutes);

	      //TODO if (ks_sms_bits.NotifyCharge == 1) { //Send Charge SMS after we have initialized the voltage and current settings
	      //  ks_sms_bits.NotifyCharge = 0;
	        //TODO net_req_notification(NET_NOTIFY_CHARGE);
	        //TODO net_req_notification(NET_NOTIFY_STAT);
	      //}
	    } else {
	      StdMetrics.ms_v_charge_state->SetValue("heating");
	    }
	    StdMetrics.ms_v_charge_kwh->SetValue((float)(ks_battery_cum_charge - ks_cum_charge_start) / 10.0, kWh); // kWh charged
	    ks_last_soc = BAT_SOC;
	    ks_last_ideal_range = IDEAL_RANGE;

	  } else if (!isCharging && StdMetrics.ms_v_charge_inprogress->AsBool()) {
	    // ** Charge completed or interrupted: **
		StdMetrics.ms_v_charge_current->SetValue( 0 );
	    StdMetrics.ms_v_charge_climit->SetValue( 0 );
	    if (BAT_SOC == 100)
		    StdMetrics.ms_v_charge_state->SetValue("done");
	    else
		    StdMetrics.ms_v_charge_state->SetValue("stopped");

	    StdMetrics.ms_v_charge_kwh->SetValue( (float)(ks_battery_cum_charge - ks_cum_charge_start) / 10, kWh );  // kWh charged

	    ks_cum_charge_start = 0;
	    StdMetrics.ms_v_charge_inprogress->SetValue( false );

	    //TODO net_req_notification(NET_NOTIFY_CHARGE);
	    //TODO net_req_notification(NET_NOTIFY_STAT);
	  }

	  //Check aux battery and send alert
	  if( StdMetrics.ms_v_bat_12v_voltage->AsFloat()<12.2 ){
		  //TODO Send SMS-varsel.
	  }
  }

  /**
   *  Sets the charge metrics
   */
  void OvmsVehicleKiaSoulEv::SetChargeMetrics(float voltage, float current, const char* mode, float climit, const char* type){
	  StdMetrics.ms_v_charge_voltage->SetValue( voltage, Volts );
	  StdMetrics.ms_v_charge_current->SetValue( current, Amps );
	  StdMetrics.ms_v_charge_mode->SetValue( mode );
	  StdMetrics.ms_v_charge_climit->SetValue( climit, Amps);
	  StdMetrics.ms_v_charge_type->SetValue( type );
  }

  /**
   * Calculates minutes remaining before target is reached. Based on current charge speed.
   * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
   */
  uint16_t OvmsVehicleKiaSoulEv::calcMinutesRemaining(float target)
  {
	  return MIN( 1440, (uint16_t)
              (((target - (ks_battery_capacity * BAT_SOC) / 100.0)*60.0) /
              (CHARGE_VOLTAGE * CHARGE_CURRENT)));
  }

/**
 * Updates the maximum real world range at current temperature.
 * Also updates the State of Health
 */
void OvmsVehicleKiaSoulEv::UpdateMaxRange(void)
{
	//Update State of Health using following assumption: 10% buffer
	StdMetrics.ms_v_bat_soh->SetValue(110-(ks_battery_max_detoriation+ks_battery_min_detoriation)/2);

	StdMetrics.ms_v_bat_cac->SetValue( (ks_battery_capacity * BAT_SOH * BAT_SOC/10000.0) / 400, AmpHours);

	float maxRange = ks_maxrange * BAT_SOH / 100.0;
	float amb_temp = StdMetrics.ms_v_env_temp->AsFloat(Celcius);
	float bat_temp = StdMetrics.ms_v_bat_temp->AsFloat(Celcius);

	// Temperature compensation:
	//   - Assumes standard maxRange specified at 20 degrees C
	//   - Range halved at -20C.
	if (maxRange != 0) {
		maxRange = (maxRange * (100.0 - (int) (ABS(20.0 - (amb_temp+bat_temp)/2)* 1.25))) / 100.0;
	}
	StdMetrics.ms_v_bat_range_full->SetValue(maxRange, Kilometers);
}

/**
 * Send a can message and wait for the response before continuing.
 * Time out after 1 second.
 */
bool OvmsVehicleKiaSoulEv::SendCanMessage_sync(uint16_t id, uint8_t count,
		uint8_t serviceId, uint8_t b1, uint8_t b2, uint8_t b3, uint8_t b4,
		uint8_t b5, uint8_t b6)
{
	uint16_t timeout;

	ks_send_can.id=serviceId;

	CAN_frame_t frame;
	memset(&frame,0,sizeof(frame));

	frame.origin = m_can1;
	frame.FIR.U = 0;
	frame.FIR.B.DLC = 8;
	frame.FIR.B.FF = CAN_frame_std;
	frame.MsgID = id;
	frame.data.u8[0] = count;
	frame.data.u8[1] = serviceId;
	frame.data.u8[2] = b1;
	frame.data.u8[3] = b2;
	frame.data.u8[4] = b3;
	frame.data.u8[5] = b4;
	frame.data.u8[6] = b5;
	frame.data.u8[7] = b6;

	//Send can message
	m_can1->Write(&frame);

	//Now wait for the response
	timeout = 300; // ~1500 ms
	do {
		 /* Block for 50ms. */
		 vTaskDelay( xDelay );
	} while (ks_send_can.status == 0xff && --timeout);

	//Did we get the response?
	if(timeout != 0 ){
		if( ks_send_can.byte[1]==serviceId+0x40 ){
			return true;
		}else{
			ks_send_can.status = 0x2;
		}
	}else{
		ks_send_can.status = 0x1; //Timeout
	}
	return false;
 }

/**
 * Send a can message to set ECU in Diagnostic session mode
 */
bool OvmsVehicleKiaSoulEv::SetTemporarySessionMode(uint16_t id, uint8_t mode){
  return SendCanMessage_sync(id, 2,VEHICLE_POLL_TYPE_OBDIISESSION, mode,0,0,0,0,0);
}

/**
 * Open or lock the doors
 */
bool OvmsVehicleKiaSoulEv::SetDoorLock(bool open, const char* password){
  if( ks_shift_bits.Park ){
    //TODO if( IsPasswordOk(password) ){
	if( SetTemporarySessionMode(SMART_JUNCTION_BOX, 3)){ //0x771 0x02 0x10 0x03
		  // 0x771 0x04 0x2F 0xBC 0x1[0,1] 0x03
	  return SendCanMessage_sync(SMART_JUNCTION_BOX, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, 0xbc, open?0x10:0x11, 0x03, 0,0,0 );
	}
    //}
  }
  return false;
}

//(04) 2F BC 09 03 --> Open the trunk lock
bool OvmsVehicleKiaSoulEv::OpenTrunk(const char* password){
  if( ks_shift_bits.Park ){
    //TODO if( IsPasswordOk(password) ){
	if( SetTemporarySessionMode(SMART_JUNCTION_BOX, 3)){ //0x771 0x02 0x10 0x03
		  // 0x771 0x04 0x2F 0xBC 0x09 0x03
	  return SendCanMessage_sync(SMART_JUNCTION_BOX, 4,VEHICLE_POLL_TYPE_OBDII_IOCTRL_BY_ID, 0xbc, 0x09, 0x03, 0,0,0 );
	}
    //}
  }
  return false;
}

/**
 * Check if the password is ok
 */
bool OvmsVehicleKiaSoulEv::IsPasswordOk(const char *password)
  {
  std::string p = MyConfig.GetParamValue("password","module");
  return ((p.empty())||(p.compare(password)==0));
  }


OvmsVehicle::vehicle_command_t OvmsVehicleKiaSoulEv::CommandLock(const char* pin)
  {
  SetDoorLock(false,pin);
  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleKiaSoulEv::CommandUnlock(const char* pin)
  {
  SetDoorLock(true, pin);
  return Success;
  }


class OvmsVehicleKiaSoulEvInit
  {
  public: OvmsVehicleKiaSoulEvInit();
  } MyOvmsVehicleKiaSoulEvInit  __attribute__ ((init_priority (9000)));

OvmsVehicleKiaSoulEvInit::OvmsVehicleKiaSoulEvInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Kia Soul EV (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleKiaSoulEv>("KS","Kia Soul EV");
  }

