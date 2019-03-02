/*
;    Project:       Open Vehicle Monitor System
;    Date:          21th January 2019
;
;    (C) 2019       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kianiroev.h"

static const char *TAG = "v-kianiroev";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleKiaNiroEv::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
	//ESP_LOGD(TAG, "IPR %03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", m_poll_moduleid_low, type, pid, length, data[0], data[1], data[2], data[3],
	//	data[4], data[5], data[6], data[7]);
	switch (m_poll_moduleid_low)
		{
		// ****** IGMP *****
		case 0x778:
			IncomingIGMP(bus, type, pid, data, length, mlremain);
			break;

		// ****** SJB *****
		case 0x779:
			IncomingSJB(bus, type, pid, data, length, mlremain);
			break;

		// ****** OBC ******
		case 0x79c:
			IncomingOBC(bus, type, pid, data, length, mlremain);
			break;

		// ****** BCM ******
		case 0x7a8:
			IncomingBCM(bus, type, pid, data, length, mlremain);
			break;

	  // ****** AirCon ******
	  case 0x7bb:
			IncomingAirCon(bus, type, pid, data, length, mlremain);
			break;

	  // ****** ABS ESP ******
	  case 0x7d9:
			IncomingAbsEsp(bus, type, pid, data, length, mlremain);
			break;

		// ******* VMCU ******
		case 0x7ea:
			IncomingVMCU(bus, type, pid, data, length, mlremain);
			break;

		// ******* MCU ******
		case 0x7eb:
			IncomingMCU(bus, type, pid, data, length, mlremain);
			break;

		// ***** BMC ****
		case 0x7ec:
			IncomingBMC(bus, type, pid, data, length, mlremain);
			break;

		// ***** LDC ****
		case 0x7cd:
			IncomingLDC(bus, type, pid, data, length, mlremain);
			break;

		default:
			ESP_LOGD(TAG, "Unknown module: %03x", m_poll_moduleid_low);
			break;
	  }
  }

/**
 * Handle incoming messages from Aircon poll.
 */
void OvmsVehicleKiaNiroEv::IncomingAirCon(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	//ESP_LOGD(TAG, "AirCon PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
	//		data[4], data[5], data[6]);
	switch (pid)
		{
		case 0x0100:
			if (m_poll_ml_frame == 1)
				{
				StdMetrics.ms_v_env_temp->SetValue((data[3]/2.0)-40, Celcius);
				StdMetrics.ms_v_env_cabintemp->SetValue((data[2]/2.0)-40, Celcius);
				}
			break;

		case 0x0102:
			if (m_poll_ml_frame == 1)
				{
			  //Coolant temp 1 & 2 in byte 1 and 2
				}
			break;
		}
	}

/**
 * Handle incoming messages from ABS ESP poll.
 */
void OvmsVehicleKiaNiroEv::IncomingAbsEsp(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	//ESP_LOGD(TAG, "ABS/ESP PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
	//		data[4], data[5], data[6]);
	switch (pid)
		{
		case 0xC101:
			if( m_poll_ml_frame == 3)
				{
				m_v_emergency_lights->SetValue((CAN_BYTE(2)>>6) & 1);
				}
			else if( m_poll_ml_frame == 4)
				{
				//TODO Check byte 3 & 5 for parkbrake
				//StdMetrics.ms_v_env_handbrake->SetValue();
				}
			else if (m_poll_ml_frame == 5)
				{
//				// Park 	0000 0000 0
//				// Drive 0011 0011 3
//				// Rev   0010 1011 2
//				uint8_t shift = (data[1]>>4) & 3;
//				kn_shift_bits.Park = shift==0;
//				kn_shift_bits.Drive = shift==3;
//				kn_shift_bits.Reverse = shift==2; //Not working...
//
//				//ESP_LOGD(TAG, "ABS/ESP %02x %02x", data[1], shift);
//
//				if( shift == 0)
//					{
//					StdMetrics.ms_v_env_gear->SetValue(0);
//					}
//				else if( shift==2 )
//					{
//					StdMetrics.ms_v_env_gear->SetValue(-1);
//					}
//				else
//					StdMetrics.ms_v_env_gear->SetValue(1);
				}
			break;
		}
	}


/**
 * Handle incoming messages from On Board Charger-poll
 *
 * - OBC-voltage
 * - Pilot signal duty cycle
 * - Charger temperature
 */
void OvmsVehicleKiaNiroEv::IncomingOBC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x02:
			if (m_poll_ml_frame == 1)
				{
				kn_obc_volt = (float) CAN_UINT(2) / 10.0;
				//} else if (vehicle_poll_ml_frame == 2) {
				//kn_obc_ampere = ((UINT) can_databuffer[4 + CAN_ADJ] << 8)
				//        | (UINT) can_databuffer[5 + CAN_ADJ];
				}
			else if (m_poll_ml_frame == 2)
				{
				m_obc_pilot_duty->SetValue( (float) CAN_BYTE(6) / 3.0 );
				}
			else if (m_poll_ml_frame == 3)
				{
				//TODO Virker dette?
				StdMetrics.ms_v_charge_temp->SetValue( (float) (CAN_BYTE(0)+CAN_BYTE(1)+CAN_BYTE(2))/3, Celcius );
				}
			break;
		}
	}

/**
 * Handle incoming messages from VMCU-poll
 *
 * - Aux battery SOC, Voltage and current
 */
void OvmsVehicleKiaNiroEv::IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	INT base;
	uint8_t bVal;
	switch (pid)
		{
		case 0x01:
			if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
				{
				if (m_poll_ml_frame == 1)
					{
					uint8_t shift_status = data[1] & 0xF;
					kn_shift_bits.Park = shift_status==1;
					kn_shift_bits.Reverse = shift_status==2;
					kn_shift_bits.Neutral = shift_status==4;
					kn_shift_bits.Drive = shift_status==8;

					//ESP_LOGD(TAG, "VMCU PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
					//		data[4], data[5], data[6]);

					ESP_LOGD(TAG, "ABS/ESP %02x %02x", data[1], data[2]);

					if( kn_shift_bits.Reverse)
						{
						StdMetrics.ms_v_env_gear->SetValue(-1);
						}
					else if( kn_shift_bits.Drive)
						{
						StdMetrics.ms_v_env_gear->SetValue(1);
						}
					else
						{
						StdMetrics.ms_v_env_gear->SetValue(0);
						}
					}
				}
			break;

		case 0x02:
			if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
				{
				if (m_poll_ml_frame == 3)
					{
					StdMetrics.ms_v_bat_12v_voltage->SetValue(((CAN_BYTE(2)<<8)+CAN_BYTE(1))/1000.0, Volts);
					StdMetrics.ms_v_bat_12v_current->SetValue( (int16_t)((CAN_BYTE(4)<<8)+CAN_BYTE(3))/1000.0, Amps);
					m_b_aux_soc->SetValue( CAN_BYTE(5), Percentage);
					}
				}
			break;

		case 0x80:
			if( type==VEHICLE_POLL_TYPE_OBDII_SERVICE1A )
				{
				if( m_poll_ml_frame==2)
					{
					m_vin[0]=CAN_BYTE(3);
					m_vin[1]=CAN_BYTE(4);
					m_vin[2]=CAN_BYTE(5);
					m_vin[3]=CAN_BYTE(6);
					}
				else if( m_poll_ml_frame==3 || m_poll_ml_frame==4)
					{
					m_vin[4]=CAN_BYTE(0);
					m_vin[5]=CAN_BYTE(1);
					m_vin[6]=CAN_BYTE(2);
					m_vin[7]=CAN_BYTE(3);
					m_vin[8]=CAN_BYTE(4);
					m_vin[9]=CAN_BYTE(5);
					m_vin[10]=CAN_BYTE(6);
					}
				else if( m_poll_ml_frame==4)
					{
					m_vin[11]=CAN_BYTE(0);
					m_vin[12]=CAN_BYTE(1);
					m_vin[13]=CAN_BYTE(2);
					m_vin[14]=CAN_BYTE(3);
					m_vin[15]=CAN_BYTE(4);
					m_vin[16]=CAN_BYTE(5);
					//m_vin[10]=CAN_BYTE(6);
					StandardMetrics.ms_v_vin->SetValue(m_vin);
					}
				}
			break;
		}
	}

/**
 * Handle incoming messages from VMCU-poll
 *
 * - Aux battery SOC, Voltage and current
 */
void OvmsVehicleKiaNiroEv::IncomingMCU(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x02:
			if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
				{
				if (m_poll_ml_frame == 2)
					{
					//ESP_LOGD(TAG, "VMCU PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
					//		data[4], data[5], data[6]);
						StdMetrics.ms_v_mot_temp->SetValue((int8_t)CAN_BYTE(4)); //TODO Correct?
						StdMetrics.ms_v_inv_temp->SetValue((int8_t)CAN_BYTE(3)); //TODO Correct?
					}
				else if (m_poll_ml_frame == 3)
					{
					ESP_LOGD(TAG, "VMCU PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
							data[4], data[5], data[6]);
					StdMetrics.ms_v_pos_odometer->SetValue(CAN_UINT24(1)/2.0); //ODO=
					}
				}
			break;

		}
	}


/**
 * Handle incoming messages from BMC-poll
 *
 * - Pilot signal available
 * - CCS / Type2
 * - Battery current
 * - Battery voltage
 * - Battery module temp 1-8
 * - Cell voltage max / min + cell #
 * + more
 */
void OvmsVehicleKiaNiroEv::IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	uint8_t bVal;
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
		{
		switch (pid)
			{
			case 0x0101:
				// diag page 01: skip first frame (no data)
				//ESP_LOGD(TAG, "Frame number %x",m_poll_ml_frame);
				if (m_poll_ml_frame == 1)
					{
					//ESP_LOGD(TAG, "BMC PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, m_poll_ml_frame, data[0], data[1], data[2], data[3],
					//		data[4], data[5], data[6]);

					m_b_bms_soc->SetValue(CAN_BYTE(1)/2.0);
					//MAx regen: ((CAN_BYTE(1)<<8) + CAN_BYTE(2))/100
					m_c_power->SetValue( (float)CAN_UINT(2)/100.0, kW);
					//m_c_power->SetValue( (float)CAN_UINT(2)/100.0, kW);
					}
				else if (m_poll_ml_frame == 2)
					{
					StdMetrics.ms_v_bat_current->SetValue((float)CAN_INT(0)/10.0, Amps);
					StdMetrics.ms_v_bat_voltage->SetValue((float)CAN_UINT(2)/10.0, Volts);

					m_b_min_temperature->SetValue( CAN_BYTE(5) );
					m_b_max_temperature->SetValue( CAN_BYTE(4) );
					BmsRestartCellTemperatures();
					BmsSetCellTemperature(0, CAN_BYTE(6));
					StdMetrics.ms_v_bat_temp->SetValue((float)CAN_BYTE(5), Celcius); //TODO Should we use Min temp or Max Temp?
					}
				else if (m_poll_ml_frame == 3)
					{
					BmsSetCellTemperature(1, CAN_BYTE(0));
					BmsSetCellTemperature(2, CAN_BYTE(1));
					BmsSetCellTemperature(3, CAN_BYTE(2));
					m_b_inlet_temperature->SetValue( CAN_BYTE(5) );
					m_b_cell_volt_max->SetValue((float)CAN_BYTE(6)/50.0, Volts);
					}
				else if (m_poll_ml_frame == 4)
					{
					m_b_cell_volt_max_no->SetValue(CAN_BYTE(0));
					m_b_cell_volt_min->SetValue((float)CAN_BYTE(1)/50.0, Volts);
					m_b_cell_volt_min_no->SetValue(CAN_BYTE(2));
					kn_battery_fan_feedback = CAN_BYTE(3); //TODO Battery fan feedback
					kn_charge_bits.FanStatus = CAN_BYTE(4) & 0xF;  //TODO Battery fan speed
					kn_battery_cum_charge_current = (kn_battery_cum_charge_current & 0x00FFFFFF) | ((uint32_t) CAN_UINT(6) << 24);
					}
				else if (m_poll_ml_frame == 5)
					{
					kn_battery_cum_charge_current = (kn_battery_cum_charge_current & 0xFF000000) | ((uint32_t) CAN_UINT24(0));
					kn_battery_cum_discharge_current = CAN_UINT32(3);

					}
				else if (m_poll_ml_frame == 6)
					{
					kn_battery_cum_charge = CAN_UINT32(0);
					kn_battery_cum_discharge = (kn_battery_cum_discharge & 0xFF) | ((uint32_t) CAN_UINT24(4) << 8);
					}
				else if (m_poll_ml_frame == 7)
					{
					kn_battery_cum_discharge = (kn_battery_cum_discharge & 0xFFFFFF00) | ((uint32_t) CAN_BYTE(0));
					kn_battery_cum_op_time = CAN_UINT32(1) / 3600;
					}
				break;

			case 0x0102: //Cell voltages
			case 0x0103:
			case 0x0104:
				// diag page 02-04: skip first frame (no data)
				if(m_poll_ml_frame==0 && pid==0x0102)
					{
					BmsRestartCellVoltages();
					}
				else if(m_poll_ml_frame>0)
					{
					int8_t base = ((pid-0x102)*32) - 1 + (m_poll_ml_frame-1) * 7;
					bVal = (m_poll_ml_frame==1)? 1 : 0;
					for (;bVal < length && ((base + bVal)<96); bVal++)
						{
						BmsSetCellVoltage((uint8_t)(base + bVal), (float)CAN_BYTE(bVal) * 0.02);
						}
					}
				break;
			case 0x0105:
				if (m_poll_ml_frame == 3)
					{
					m_b_heat_1_temperature->SetValue( CAN_BYTE(6) );
					}
				else if (m_poll_ml_frame == 4)
					{
					m_b_heat_2_temperature->SetValue( CAN_BYTE(0) );
					StdMetrics.ms_v_bat_soh->SetValue( (float)CAN_UINT(1)/10.0 );
					m_b_cell_det_max_no->SetValue( CAN_BYTE(3) );
					m_b_cell_det_min->SetValue( (float)CAN_UINT(4)/10.0 );
					m_b_cell_det_min_no->SetValue( CAN_BYTE(6) );
					}
				else if (m_poll_ml_frame == 5)
					{
					StdMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(0)/2.0);
					BmsSetCellVoltage(96, (float)CAN_BYTE(3) * 0.02);
					BmsSetCellVoltage(97, (float)CAN_BYTE(4) * 0.02);
					}
				break;
			}
		}
	}

/**
 * Handle incoming messages from BCM-poll
 *
 *
 */
void OvmsVehicleKiaNiroEv::IncomingBCM(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	uint8_t bVal;
	uint32_t lVal;

	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
		{
		switch (pid)
			{
			case 0xB00E:
				if (m_poll_ml_frame == 1)
					{
					StdMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(1)>>4) & 1);
					}
				break;

			case 0xC002:
				if (m_poll_ml_frame == 1)
					{
					SET_TPMS_ID(0, CAN_UINT32(1));
					lVal = (kn_tpms_id[1] & 0x0000ffff) | ((uint32_t)CAN_UINT(5)<<16);
					SET_TPMS_ID(1, lVal);
					}
				else if (m_poll_ml_frame == 2)
					{
					lVal = (uint32_t) CAN_UINT(0) | (kn_tpms_id[1] & 0xffff0000);
					SET_TPMS_ID(1, lVal);
					SET_TPMS_ID(2, CAN_UINT32(2));
					lVal = (kn_tpms_id[1] & 0x00ffffff) | ((uint32_t)CAN_BYTE(5)<<24);
					SET_TPMS_ID(3, lVal);
					}
				else if (m_poll_ml_frame == 3)
					{
					lVal = (uint32_t) CAN_UINT24(0) | (kn_tpms_id[3] & 0xff000000);
					SET_TPMS_ID(3, lVal);
					}
				break;

			case 0xC006:
				if (m_poll_ml_frame == 1)
					{
					bVal = CAN_BYTE(1);
					if (bVal > 0) StdMetrics.ms_v_tpms_fl_p->SetValue( bVal/5.0, PSI);
					bVal = CAN_BYTE(2);
					if (bVal > 0) StdMetrics.ms_v_tpms_fl_t->SetValue( bVal-50.0, Celcius);
					bVal = CAN_BYTE(5);
					if (bVal > 0) StdMetrics.ms_v_tpms_fr_p->SetValue( bVal/5.0, PSI);
					bVal = CAN_BYTE(6);
					if (bVal > 0) StdMetrics.ms_v_tpms_fr_t->SetValue( bVal-50.0, Celcius);
					}
				else if (m_poll_ml_frame == 2)
					{
					bVal = CAN_BYTE(2);
					if (bVal > 0) StdMetrics.ms_v_tpms_rr_p->SetValue( bVal/5.0, PSI);
					bVal = CAN_BYTE(3);
					if (bVal > 0) StdMetrics.ms_v_tpms_rr_t->SetValue( bVal-50.0, Celcius);
					bVal = CAN_BYTE(6);
					if (bVal > 0) StdMetrics.ms_v_tpms_rl_p->SetValue( bVal/5.0, PSI);
					}
				else if (m_poll_ml_frame == 3)
					{
					bVal = CAN_BYTE(0);
					if (bVal > 0) StdMetrics.ms_v_tpms_rl_t->SetValue( bVal-50.0, Celcius);
					}
				break;
			}
		}
	}

/**
 * Handle incoming messages from IGMP-poll
 *
 *
 */
void OvmsVehicleKiaNiroEv::IncomingIGMP(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
		{
		switch (pid)
			{
			case 0xbc03:
				if (m_poll_ml_frame == 1)
					{
					//StdMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(1)>>4) & 1);
					StdMetrics.ms_v_door_trunk->SetValue((CAN_BYTE(1)>>7) & 1);
					StdMetrics.ms_v_door_fl->SetValue((CAN_BYTE(1)>>5) & 1);
					StdMetrics.ms_v_door_fr->SetValue((CAN_BYTE(1)>>4) & 1);
					StdMetrics.ms_v_door_rl->SetValue(CAN_BYTE(1) & 1);
					StdMetrics.ms_v_door_rr->SetValue((CAN_BYTE(1)>>2) & 1);

					StdMetrics.ms_v_door_hood->SetValue(CAN_BYTE(2) & 1);

					m_v_door_lock_rl->SetValue((CAN_BYTE(1)>>1) & 1);
					m_v_door_lock_rr->SetValue((CAN_BYTE(1)>>3) & 1);

					StdMetrics.ms_v_env_on->SetValue((CAN_BYTE(2) & 0x60)>0);

					//TODO StdMetrics.ms_v_door_trunk->SetValue((d[5] & 0x80) > 0);
					//TODO StdMetrics.ms_v_env_locked->SetValue(false);
					}
				break;

			case 0xbc04:
				if (m_poll_ml_frame == 1)
					{
					m_v_door_lock_fl->SetValue((CAN_BYTE(1)>>3) & 1);
					m_v_door_lock_fr->SetValue((CAN_BYTE(1)>>2) & 1);
					}
				break;

			}
		}
	}


/**
 * Handle incoming messages from LDC-poll
 *
 * - LDC out voltage
 * - LDC out current
 * - LDC in voltage
 * - LDC temperature
 */
void OvmsVehicleKiaNiroEv::IncomingLDC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x01:
			ESP_LOGI(TAG, "LDC %04x %02x %02x %02x %02x %02x %02x %02x %02x", pid, m_poll_ml_frame, CAN_BYTE(0), CAN_BYTE(1), CAN_BYTE(2), CAN_BYTE(3), CAN_BYTE(4), CAN_BYTE(5), CAN_BYTE(6));

			// 12V system
			kn_ldc_enabled = (CAN_BYTE(0) & 0x07) != 0;
			if( CAN_BYTE(0)==0x07 )
				{
				m_ldc_out_voltage->SetValue( CAN_BYTE(1) / 10.0 );
				m_ldc_out_current->SetValue( CAN_BYTE(2) );
				m_ldc_in_voltage->SetValue( CAN_BYTE(3) * 2 );
				m_ldc_temperature->SetValue( CAN_BYTE(4) - 100 );
				}
			break;
		}
	}

/**
 * Handle incoming messages from SJB-poll
 */
void OvmsVehicleKiaNiroEv::IncomingSJB(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	//ESP_LOGW(TAG, "779X %02x %02x %02x %02x %02x %02x %02x %02x %02x", pid, m_poll_ml_frame, CAN_BYTE(0), CAN_BYTE(1), CAN_BYTE(2), CAN_BYTE(3), CAN_BYTE(4), CAN_BYTE(5), CAN_BYTE(6));
	switch (pid)
		{
		case 0x04:
			if (m_poll_ml_frame == 1)
				{
				//ESP_LOGW(TAG, "779 8 21 %02x %02x %02x %02x %02x %02x %02x", CAN_BYTE(0), CAN_BYTE(1), CAN_BYTE(2), CAN_BYTE(3), CAN_BYTE(4), CAN_BYTE(5), CAN_BYTE(6));
				}
			break;
		}
	}
