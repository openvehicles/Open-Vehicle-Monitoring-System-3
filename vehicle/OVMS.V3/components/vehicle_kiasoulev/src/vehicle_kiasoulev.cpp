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

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_100KBPS);

  memset(m_vin,0,sizeof(m_vin));

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
	if (!StandardMetrics.ms_v_env_on->AsBool())
	  {
	  StandardMetrics.ms_v_env_on->SetValue(isOn);
	  StandardMetrics.ms_v_env_awake->SetValue(isOn);

	  // Start trip, save current state
	  //ODO at Start of trip
	  ks_trip_start_odo = StandardMetrics.ms_v_pos_odometer->AsFloat();
	  ks_start_cdc = ks_battery_cum_discharge; 	//Cumulated discharge
	  ks_start_cc = ks_battery_cum_charge; 		//Cumulated charge
	  }
	if ( StandardMetrics.ms_v_env_parktime->AsInt() > 0)
	  {
	  StandardMetrics.ms_v_env_parktime->SetValue(0); // No longer parking
	  //TODO net_req_notification(NET_NOTIFY_ENV);
	  }
  } else {
    // Car is OFF
	StandardMetrics.ms_v_env_on->SetValue(isOn);
	StandardMetrics.ms_v_env_awake->SetValue(isOn);

	StandardMetrics.ms_v_pos_speed->SetValue(0);

	if (StandardMetrics.ms_v_env_parktime->AsInt() == 0) {
	  StandardMetrics.ms_v_env_parktime->SetValue((int) (time(NULL) - 1));
	  //TODO net_req_notification(NET_NOTIFY_ENV);
	  }
    }
  StandardMetrics.ms_v_pos_trip->SetValue( StandardMetrics.ms_v_pos_odometer->AsFloat() - ks_trip_start_odo );
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
  	   StandardMetrics.ms_v_door_chargeport->SetValue((d[0] & 0x01) > 0);
  	   StandardMetrics.ms_v_door_fl->SetValue((d[0] & 0x10) > 0);
  	   StandardMetrics.ms_v_door_fr->SetValue((d[0] & 0x80) > 0);
  	   StandardMetrics.ms_v_door_rl->SetValue(false); //TODO
  	   StandardMetrics.ms_v_door_rr->SetValue(false); //TODO
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
        	    StandardMetrics.ms_v_bat_range_est->SetValue(estRange<<1);
          }
        }
        break;

      case 0x433:
        {
          // Parking brake status
        	  StandardMetrics.ms_v_env_handbrake->SetValue((d[2] & 0x10) > 0);
        }
        break;

      case 0x4f0:
        {
          // Odometer:
          if (/*can_datalength = 8 &&*/ (d[7] || d[6] || d[5])) { //TODO can_datalength was needed in V2
            StandardMetrics.ms_v_pos_odometer->SetValue((d[7]<<16) + (d[6]<<8) + d[5]);
          }
        }
        break;

      case 0x4f2:
        {
          // Speed:
          //if (can_mileskm == 'M')
        	  //  StandardMetrics.ms_v_pos_speed->SetValue( MiFromKm(d[1] >> 1) ); // mph
          //else
        	    StandardMetrics.ms_v_pos_speed->SetValue( d[1] >> 1 ); // kph

          // 4f2 is one of the few can-messages that are sent while car is both on and off.
          // Byte 2 and 7 are some sort of counters which runs while the car is on.
          if (d[2] > 0 || d[7] > 0){
        	    vehicle_kiasoulev_car_on(true);
          } else if (d[2] == 0 && d[7] == 0 && StandardMetrics.ms_v_pos_speed == 0 && StandardMetrics.ms_v_env_handbrake) {
            // Boths 2 and 7 and speed is 0, so we assumes the car is off.
      	    vehicle_kiasoulev_car_on(false);
          }
        }
        break;

      case 0x542:
        {
        // SOC:
        if (d[0] > 0)
        	  StandardMetrics.ms_v_bat_soc->SetValue( d[0] >> 1 ); //In V2 car_SOC
        }
        break;

      case 0x581:
        {
          // Car is CHARGING:
        	  ks_chargepower = (((uint16_t) d[7] << 8) | d[6]);
          //StandardMetrics.ms_v_charge_kwh->SetValue( ((uint16_t) d[7] << 8) | d[6]); // TODO probably wrong
        }
        break;

      case 0x594:
        {
          // BMS SOC:
          uint16_t val = (uint16_t) d[5] * 5 + d[6];
          if (val > 0){
            //ks_bms_soc = (uint8_t) (val / 10);
          }
        }
        break;

      case 0x653:
        {
          // AMBIENT TEMPERATURE:
          StandardMetrics.ms_v_env_temp->SetValue( (((signed int) d[5]) / 2) - 40 );
        }
        break;
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
	            i = CAN_BYTE(0);//data[1 + CAN_ADJ];
	            if (i > 0) StandardMetrics.ms_v_tpms_fl_p->SetValue(i*0.68875);
	            StandardMetrics.ms_v_tpms_fl_p->SetValue( CAN_BYTE(1) -15);
	            val = (ks_tpms_id[1] & 0x000000ff) | (CAN_UINT32(4) & 0xffffff00);
	            if (val > 0) ks_tpms_id[1] = val;

	          } else if (m_poll_ml_frame == 2) {
	            val = (uint32_t) CAN_BYTE(0) | (ks_tpms_id[1] & 0xffffff00);
	            if (val > 0) ks_tpms_id[1] = val;
	            i = CAN_BYTE(1); //data[2 + CAN_ADJ];
	            if (i > 0) StandardMetrics.ms_v_tpms_fr_p->SetValue(i*0.68875);
	            StandardMetrics.ms_v_tpms_fr_p->SetValue( CAN_BYTE(2) -15);
	            val = (ks_tpms_id[2] & 0x0000ffff) | (CAN_UINT32(5) & 0xffff0000);
	            if (val > 0) ks_tpms_id[2] = val;

	          } else if (m_poll_ml_frame == 3) {
	            val = ((uint32_t) CAN_UINT(0)) | (ks_tpms_id[2] & 0xffff0000);
	            if (val > 0) ks_tpms_id[2] = val;
	            i = CAN_BYTE(2);//[3 + CAN_ADJ];
	            if (i > 0) StandardMetrics.ms_v_tpms_rl_p->SetValue(i*0.68875);
	            StandardMetrics.ms_v_tpms_rl_p->SetValue( CAN_BYTE(3) -15);
	            val = (ks_tpms_id[3] & 0x00ffffff) | ((uint32_t) CAN_BYTE(6) << 24);
	            if (val > 0) ks_tpms_id[3] = val;

	          } else if (m_poll_ml_frame == 4) {
	            val = (CAN_UINT24(0)) | (ks_tpms_id[3] & 0xff000000);
	            if (val > 0) ks_tpms_id[3] = val;
	            i = CAN_BYTE(3); //data[4 + CAN_ADJ];
	            if (i > 0) StandardMetrics.ms_v_tpms_rr_p->SetValue(i*0.68875);
	            StandardMetrics.ms_v_tpms_rr_p->SetValue( CAN_BYTE(4) -15);
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
	            	  StandardMetrics.ms_v_mot_temp->SetValue( (float)CAN_BYTE(4) - 40 );
	            	  StandardMetrics.ms_v_inv_temp->SetValue( (float)CAN_BYTE(5) - 40 );
	            	  StandardMetrics.ms_v_charge_temp->SetValue( (float)CAN_BYTE(6) - 40 );
	              //TODO car_stale_temps = 120;
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
	              StandardMetrics.ms_v_charge_pilot->SetValue((i & 0x80) > 0);
	              ks_charge_bits.ChargingChademo = ((i & 0x40) > 0);
	              ks_charge_bits.ChargingJ1772 = ((i & 0x20) > 0);
	              ks_battery_current = (ks_battery_current & 0x00FF)
	                      | ((UINT) CAN_BYTE(6) << 8);
	            }
	            if (m_poll_ml_frame == 2) // 02 21 01 - 22
	            {
	              ks_battery_current = (ks_battery_current & 0xFF00)
	                      | (UINT) CAN_BYTE(0);
	              ks_battery_DC_voltage = CAN_UINT(1);
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
	              //TODO car_stale_temps = 120; // Reset stale indicator

	              ks_battery_max_cell_voltage = CAN_BYTE(5);
	              ks_battery_max_cell_voltage_no = CAN_BYTE(6);

	            } else if (m_poll_ml_frame == 4) // 02 21 01 - 24
	            {
	              ks_battery_min_cell_voltage = CAN_BYTE(0);
	              ks_battery_min_cell_voltage_no = CAN_BYTE(1);
	              ks_battery_fan_feedback = CAN_BYTE(2);
	              ks_charge_bits.FanStatus = CAN_BYTE(3) & 0xF;
	              ks_auxVoltage = CAN_BYTE(4);
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
	          /*if (!ks_sms_bits.BCV_BlockFetched) {
	            if (ks_sms_bits.BCV_BlockToFetch == vehicle_poll_pid - 2) {
	              if (vehicle_poll_ml_frame >= 0) {
	                base = vehicle_poll_ml_offset - can_datalength - 3;
	                for (i = 0; i < can_datalength && ((base + i)<sizeof (ks_battery_cell_voltage)); i++)
	                  ks_battery_cell_voltage[base + i] = can_databuffer[i];
	                ks_sms_bits.BCV_BlockFetched = 1;
	                ks_sms_bits.BCV_BlockSent = 0;
	              }
	            }
	          }*/
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
	  ks_maxRange = vehicle_kiasoulev_get_maxrange();

	  //
	  // Convert KS status data to OVMS framework:
	  //
	  *StdMetrics.ms_m_timeutc = (int) time(NULL); // → framework? roadster fetches from CAN…

	  if (ks_maxRange > 0)
		StandardMetrics.ms_v_bat_range_ideal->SetValue(ks_maxRange * StandardMetrics.ms_v_bat_soc->AsFloat() / 100.0);
	  else
	    StandardMetrics.ms_v_bat_range_ideal->SetValue(StandardMetrics.ms_v_bat_range_ideal->AsFloat());


	  StandardMetrics.ms_v_bat_temp->SetValue(((signed int) ks_battery_module_temp[0] + (signed int) ks_battery_module_temp[1] +
	          (signed int) ks_battery_module_temp[2] + (signed int) ks_battery_module_temp[3] +
	          (signed int) ks_battery_module_temp[4] + (signed int) ks_battery_module_temp[5] +
	          (signed int) ks_battery_module_temp[6] + (signed int) ks_battery_module_temp[7]) / 8);

	  // Update trip
	  if (StandardMetrics.ms_v_env_on->AsBool()){
		  StandardMetrics.ms_v_pos_trip->SetValue( StandardMetrics.ms_v_pos_odometer->AsFloat() - ks_trip_start_odo );
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
		StandardMetrics.ms_v_charge_voltage->SetValue( ks_obc_volt );
		StandardMetrics.ms_v_charge_current->SetValue( (((long) ks_chargepower * 1000) >> 8) / StandardMetrics.ms_v_charge_voltage->AsFloat() );
	    StandardMetrics.ms_v_charge_mode->SetValue("standard");
	    StandardMetrics.ms_v_charge_climit->SetValue( 6600 / StandardMetrics.ms_v_charge_voltage->AsFloat() ); // 6.6kW
	    StandardMetrics.ms_v_charge_type->SetValue("type1");
	  } else if (ks_charge_bits.ChargingChademo) { //ChaDeMo charging
		StandardMetrics.ms_v_charge_voltage->SetValue( ks_battery_DC_voltage / 10.0 );
		StandardMetrics.ms_v_charge_current->SetValue( -ks_battery_current / 10.0 );
	    StandardMetrics.ms_v_charge_mode->SetValue("performance");
	    StandardMetrics.ms_v_charge_climit->SetValue( (UINT) ((long) ks_battery_avail_charge * 100 / ks_battery_DC_voltage)); // 250A - 100kW/400V
	    StandardMetrics.ms_v_charge_type->SetValue("chademo");
	  }

	  //
	  // Check for charging status changes:

	  bool isCharging = StandardMetrics.ms_v_charge_pilot->AsBool()
				  && (ks_charge_bits.ChargingChademo || ks_charge_bits.ChargingJ1772)
		          && (StandardMetrics.ms_v_charge_current->AsFloat() > 0);
//	  StandardMetrics.ms_v_charge_inprogress->SetValue( isCharging );

	  if (isCharging) {
	    if (!StandardMetrics.ms_v_charge_inprogress->AsBool() ) {
	      // ******* Charging started: **********
	      //car_doors1bits.PilotSignal = 1;
	      //car_doors1bits.ChargePort = 1;
	      //car_doors1bits.Charging = 1;
	      //car_doors5bits.Charging12V = 1;
	    	  StandardMetrics.ms_v_charge_duration_full->SetValue(60*24);
	    	  StandardMetrics.ms_v_charge_minutes->SetValue(0);
	      StandardMetrics.ms_v_charge_state->SetValue("charging");
	      StandardMetrics.ms_v_charge_kwh->SetValue(0);  // kWh charged
	      //ks_sms_bits.NotifyCharge = 1; // Send SMS when charging is fully initiated
	      ks_cum_charge_start = ks_battery_cum_charge;
	      ks_chargeduration = 0;

	      StandardMetrics.ms_v_charge_inprogress->SetValue( isCharging );

	      //TODO net_req_notification(NET_NOTIFY_CHARGE);
	      //TODO net_req_notification(NET_NOTIFY_STAT);
	    } else {
	      // ******* Charging continues: *******
	    	  ks_chargeduration++;
	      StandardMetrics.ms_v_charge_minutes->SetValue( (int) ks_chargeduration / 60 );

	      /* TODO Handle TOPPING OFF m.m.
	      if (((StandardMetrics.ms_v_bat_soc->AsFloat() > 0) && (suffSOC > 0) && (StandardMetrics.ms_v_bat_soc->AsFloat() >= suffSOC) && (ks_last_soc < suffSOC)) ||
	              ((ks_estrange > 0) && (suffRange > 0) && (car_idealrange >= suffRange) && (ks_last_ideal_range < suffRange))) {
	        // ...enter state 2=topping off:
		    StandardMetrics.ms_v_charge_state->SetValue("topoff");

	        // ...send charge alert:
	        //TODO net_req_notification(NET_NOTIFY_CHARGE);
		    //TODO net_req_notification(NET_NOTIFY_STAT);
	      }        // ...else set "topping off" from 94% SOC:
	      else*/ if (StandardMetrics.ms_v_bat_soc->AsFloat() >= 95) {
		    StandardMetrics.ms_v_charge_state->SetValue("topoff");
	        //TODO net_req_notification(NET_NOTIFY_ENV);
	      }
	    }

	    // Check if we have what is needed to calculate remaining minutes
	    if (StandardMetrics.ms_v_charge_voltage->AsFloat() > 0 && StandardMetrics.ms_v_charge_current->AsFloat() > 0) {
	    	  //Calculate remaining charge time
	    	  float chargeTarget_full = ks_battery_capacity;
	    	  float chargeTarget_soc = ks_battery_capacity;
	    	  float chargeTarget_range = ks_battery_capacity;

	      if (StandardMetrics.ms_v_charge_limit_soc->AsFloat() > 0) {
	        chargeTarget_soc = ks_battery_capacity * StandardMetrics.ms_v_charge_limit_soc->AsFloat() / 100.0;
	      } else if (StandardMetrics.ms_v_charge_limit_range->AsFloat() > 0) {
	        chargeTarget_range = StandardMetrics.ms_v_charge_limit_range->AsFloat() * ks_battery_capacity / ks_maxRange;
	      }
	      if (ks_charge_bits.ChargingChademo) { //ChaDeMo charging
	        chargeTarget_full = MIN(chargeTarget_full, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	        chargeTarget_soc = MIN(chargeTarget_soc, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	        chargeTarget_range = MIN(chargeTarget_range, ks_battery_capacity*0.83); //Limit charge target to 83% when using ChaDeMo
	      }

	      StandardMetrics.ms_v_charge_duration_full->SetValue( calcMinutesRemaining(chargeTarget_full));
	      StandardMetrics.ms_v_charge_duration_soc->SetValue( calcMinutesRemaining(chargeTarget_soc));
	      StandardMetrics.ms_v_charge_duration_range->SetValue( calcMinutesRemaining(chargeTarget_range));

	      //TODO if (ks_sms_bits.NotifyCharge == 1) { //Send Charge SMS after we have initialized the voltage and current settings
	      //  ks_sms_bits.NotifyCharge = 0;
	        //TODO net_req_notification(NET_NOTIFY_CHARGE);
	        //TODO net_req_notification(NET_NOTIFY_STAT);
	      //}
	    } else {
	      StandardMetrics.ms_v_charge_state->SetValue("heating");
	    }
	    StandardMetrics.ms_v_charge_kwh->SetValue((ks_battery_cum_charge - ks_cum_charge_start) / 10.0);
	    ks_last_soc = StandardMetrics.ms_v_charge_kwh->AsFloat();
	    ks_last_ideal_range = StandardMetrics.ms_v_bat_range_ideal->AsFloat();

	  } else if (!isCharging && StandardMetrics.ms_v_charge_inprogress->AsBool()) {
	    // ** Charge completed or interrupted: **
		StandardMetrics.ms_v_charge_current->SetValue( 0 );
	    StandardMetrics.ms_v_charge_climit->SetValue( 0 );
	    if (StandardMetrics.ms_v_bat_soc->AsFloat() == 100)
		    StandardMetrics.ms_v_charge_state->SetValue("done");
	    else
		    StandardMetrics.ms_v_charge_state->SetValue("stopped");

	    StandardMetrics.ms_v_charge_kwh->SetValue( (uint8_t) (ks_battery_cum_charge - ks_cum_charge_start) / 10 );  // kWh charged

	    ks_cum_charge_start = 0;
	    StandardMetrics.ms_v_charge_inprogress->SetValue( false );

	    //TODO net_req_notification(NET_NOTIFY_CHARGE);
	    //TODO net_req_notification(NET_NOTIFY_STAT);
	  }

/*
	  // Check if we have a can-message as a response to a QRY sms
	  if (ks_sms_bits.QRY_Read && !ks_sms_bits.QRY_Sent) {
	    //Only second MSbit is sat, so we have data and are ready to send
	    vehicle_kiasoul_query_sms_response();
	  }

	  // Check if we have to send a response to BCV
	  if (ks_sms_bits.BCV_BlockFetched && !ks_sms_bits.BCV_BlockSent) {
	    vehicle_kiasoul_bcv_sms_response();
	  }
	  */
  }

  uint16_t OvmsVehicleKiaSoulEv::calcMinutesRemaining(float target)
  {
	  return MIN( 1440, (uint16_t)
              (((target - (ks_battery_capacity * StandardMetrics.ms_v_bat_soc->AsFloat()) / 100.0)*60.0) /
              (StandardMetrics.ms_v_charge_voltage->AsFloat() * StandardMetrics.ms_v_charge_current->AsFloat())));
  }

/**
 * Returns the maximum real world range at current temperature
 */
float OvmsVehicleKiaSoulEv::vehicle_kiasoulev_get_maxrange(void)
{
  float maxRange = ks_battery_capacity*(160/27000); //TODO sys_features[FEATURE_MAXRANGE];

  // Temperature compensation:
  //   - assumes standard maxRange specified at 20∞C
  //   - Temperature halved at -20C.
  if (maxRange != 0) {
    maxRange = (maxRange * (100.0 - (int) (ABS(20.0 - StandardMetrics.ms_v_env_temp->AsFloat())* 1.25))) / 100.0;
  }
  return maxRange;
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

