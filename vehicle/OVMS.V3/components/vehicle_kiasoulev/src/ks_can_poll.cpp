/*
;    Project:       Open Vehicle Monitor System
;    Date:          29th December 2017
;
;    (C) 2017       Geir Øyvind Vælidalo <geir@validalo.net>
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
#include "vehicle_kiasoulev.h"

static const char *TAG = "v-kiasoulev";

/**
 * Incoming poll reply messages
 */
void OvmsVehicleKiaSoulEv::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
	//ESP_LOGW(TAG, "%03x TYPE:%x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", m_poll_moduleid_low, type, pid, length, data[0], data[1], data[2], data[3],
	//	data[4], data[5], data[6], data[7]);
	switch (m_poll_moduleid_low)
		{
		// ****** SJB *****
		case 0x779:
			IncomingSJB(bus, type, pid, data, length, mlremain);
			break;

		// ****** OBC ******
		case 0x79c:
			IncomingOBC(bus, type, pid, data, length, mlremain);
			break;

		// ****** TPMS ******
		case 0x7de:
			IncomingTPMS(bus, type, pid, data, length, mlremain);
			break;

		// ******* VMCU ******
		case 0x7ea:
			IncomingVMCU(bus, type, pid, data, length, mlremain);
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
 * Handle incoming messages from TPMS poll.
 */
void OvmsVehicleKiaSoulEv::IncomingTPMS(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	uint8_t bVal;
	uint32_t lVal;

	switch (pid)
		{
		case 0x06:
			if (m_poll_ml_frame == 0)
				{
				lVal = CAN_UINT32(4);
				SET_TPMS_ID(0, lVal);
				}
			else if (m_poll_ml_frame == 1)
				{
				bVal = CAN_BYTE(0);
				if (bVal > 0) StdMetrics.ms_v_tpms_fl_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_fl_t->SetValue( TO_CELCIUS(CAN_BYTE(1)), Celcius);
				lVal = (ks_tpms_id[1] & 0x000000ff) | (CAN_UINT32(4) & 0xffffff00);
				SET_TPMS_ID(1, lVal);
				}
			else if (m_poll_ml_frame == 2)
				{
				lVal = (uint32_t) CAN_BYTE(0) | (ks_tpms_id[1] & 0xffffff00);
				SET_TPMS_ID(1, lVal);
				bVal = CAN_BYTE(1);
				if (bVal > 0) StdMetrics.ms_v_tpms_fr_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_fr_t->SetValue( TO_CELCIUS(CAN_BYTE(2)), Celcius);
				lVal = (ks_tpms_id[2] & 0x0000ffff) | (CAN_UINT32(5) & 0xffff0000);
				SET_TPMS_ID(2, lVal);

				}
			else if (m_poll_ml_frame == 3)
				{
				lVal = ((uint32_t) CAN_UINT(0)) | (ks_tpms_id[2] & 0xffff0000);
				SET_TPMS_ID(2, lVal);
				bVal = CAN_BYTE(2);
				if (bVal > 0) StdMetrics.ms_v_tpms_rl_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_rl_t->SetValue( TO_CELCIUS(CAN_BYTE(3)), Celcius);
				lVal = (ks_tpms_id[3] & 0x00ffffff) | ((uint32_t) CAN_BYTE(6) << 24);
				SET_TPMS_ID(3, lVal);

			} else if (m_poll_ml_frame == 4) {
				lVal = (CAN_UINT24(0)) | (ks_tpms_id[3] & 0xff000000);
				SET_TPMS_ID(3, lVal);
				bVal = CAN_BYTE(3);
				if (bVal > 0) StdMetrics.ms_v_tpms_rr_p->SetValue( TO_PSI(bVal), PSI);
				StdMetrics.ms_v_tpms_rr_t->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
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
void OvmsVehicleKiaSoulEv::IncomingOBC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x02:
			if (m_poll_ml_frame == 1)
				{
				ks_obc_volt = (float) CAN_UINT(2) / 10.0;
				//} else if (vehicle_poll_ml_frame == 2) {
				//ks_obc_ampere = ((UINT) can_databuffer[4 + CAN_ADJ] << 8)
				//        | (UINT) can_databuffer[5 + CAN_ADJ];
				}
			else if (m_poll_ml_frame == 2)
				{
				m_obc_pilot_duty->SetValue( (float) CAN_BYTE(6) / 3.0 );
				}
			else if (m_poll_ml_frame == 3)
				{
				StdMetrics.ms_v_charge_temp->SetValue( (float) (CAN_BYTE(0)+CAN_BYTE(1)+CAN_BYTE(2))/3, Celcius );
				}
			break;
		}
	}

/**
 * Handle incoming messages from VMCU-poll
 *
 * - Gear shifter position
 * - VIN
 * - RPM
 * - Motor temperature
 * - Inverter temperature
 */
void OvmsVehicleKiaSoulEv::IncomingVMCU(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	INT base;
	uint8_t bVal;

	switch (pid)
		{
		case 0x00:
			if (m_poll_ml_frame == 1)
				{
				ks_shift_bits.value = CAN_BYTE(3);
				}
			break;

		case 0x02:
			// VIN (multi-line response):
			if (type == VEHICLE_POLL_TYPE_OBDIIVEHICLE)
				{
				base = m_poll_ml_offset - length;

				for (bVal = 0; (bVal < length) && ((base + bVal)<sizeof (m_vin)); bVal++)
					if(base+bVal>0) m_vin[base + bVal-1] = CAN_BYTE(bVal);
				if (m_poll_ml_remain == 0 && base+bVal>=0) m_vin[base + bVal] = 0;

				//Set VIN
				StandardMetrics.ms_v_vin->SetValue(m_vin);
				}
			if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
				{
				if (m_poll_ml_frame == 1)
					{
					StdMetrics.ms_v_mot_rpm->SetValue( ((uint16_t)CAN_BYTE(5)<<8) | (uint16_t)CAN_BYTE(6) );
					}
				else if (m_poll_ml_frame == 3)
					{
					StdMetrics.ms_v_mot_temp->SetValue( TO_CELCIUS(CAN_BYTE(4)), Celcius);
					StdMetrics.ms_v_inv_temp->SetValue( TO_CELCIUS(CAN_BYTE(5)), Celcius );
					}
				}
			break;
		}
	}

/**
 * Handle incoming messages from BMC-poll
 *
 * - Pilot signal available
 * - ChaDeMo / J1772
 * - Battery current
 * - Battery voltage
 * - Battery module temp 1-8
 * - Cell voltage max / min + cell #
 * + more
 */
void OvmsVehicleKiaSoulEv::IncomingBMC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	UINT base;
	uint8_t bVal;

	switch (pid)
		{
		case 0x01:
			// diag page 01: skip first frame (no data)
			if (m_poll_ml_frame > 0) {
				if (m_poll_ml_frame == 1) // 02 21 01 - 21
					{
					m_c_power->SetValue( (float)CAN_UINT(1)/100.0, kW);
					bVal = CAN_BYTE(5);
					StdMetrics.ms_v_charge_pilot->SetValue((bVal & 0x80) > 0);
					ks_charge_bits.ChargingChademo = ((bVal & 0x40) > 0);
					ks_charge_bits.ChargingJ1772 = ((bVal & 0x20) > 0);
					ks_battery_current = (ks_battery_current & 0x00FF)
									| ((UINT) CAN_BYTE(6) << 8);
					}
				else if (m_poll_ml_frame == 2) // 02 21 01 - 22
					{
					ks_battery_current = (ks_battery_current & 0xFF00) | (UINT) CAN_BYTE(0);
					StdMetrics.ms_v_bat_current->SetValue((float)ks_battery_current/10.0, Amps);
					StdMetrics.ms_v_bat_voltage->SetValue((float)CAN_UINT(1)/10.0, Volts);
					//TODO  BmsSetCellTemperature(0, CAN_BYTE(3));
					//TODO  BmsSetCellTemperature(1, CAN_BYTE(4));
					//TODO BmsSetCellTemperature(2, CAN_BYTE(5));
					//TODO BmsSetCellTemperature(3, CAN_BYTE(6));
         	 StdMetrics.ms_v_bat_temp->SetValue(((float)CAN_BYTE(3) + CAN_BYTE(4) +
         			CAN_BYTE(5) + CAN_BYTE(6)) / 4, Celcius);

					}
				else if (m_poll_ml_frame == 3) // 02 21 01 - 23
					{
           //TODO BmsSetCellTemperature(4, CAN_BYTE(0));
					//TODO BmsSetCellTemperature(5, CAN_BYTE(1));
					//TODO BmsSetCellTemperature(6, CAN_BYTE(2));
					//TODO BmsSetCellTemperature(7, CAN_BYTE(3));
					//TODO What about the 30kWh-version?

					m_b_cell_volt_max->SetValue((float)CAN_BYTE(5)/50.0, Volts);
					m_b_cell_volt_max_no->SetValue(CAN_BYTE(6));

					}
				else if (m_poll_ml_frame == 4) // 02 21 01 - 24
					{
					m_b_cell_volt_min->SetValue((float)CAN_BYTE(0)/50.0, Volts);
					m_b_cell_volt_min_no->SetValue(CAN_BYTE(1));
					ks_battery_fan_feedback = CAN_BYTE(2);
					ks_charge_bits.FanStatus = CAN_BYTE(3) & 0xF;
					StdMetrics.ms_v_bat_12v_voltage->SetValue ((float)CAN_BYTE(4)/10.0 , Volts);
					ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0x0000FFFF) | ((uint32_t) CAN_UINT(5) << 16);

					}
				else if (m_poll_ml_frame == 5) // 02 21 01 - 25
					{
					ks_battery_cum_charge_current = (ks_battery_cum_charge_current & 0xFFFF0000) | ((uint32_t) CAN_UINT(0));
					ks_battery_cum_discharge_current = CAN_UINT32(2);
					ks_battery_cum_charge = (ks_battery_cum_charge & 0x00FFFFFF) | ((uint32_t) CAN_BYTE(6)) << 24;
					}
				else if (m_poll_ml_frame == 6) // 02 21 01 - 26
					{
					ks_battery_cum_charge = (ks_battery_cum_charge & 0xFF000000) | ((uint32_t) CAN_UINT24(0));
					ks_battery_cum_discharge = CAN_UINT32(3);
					}
				else if (m_poll_ml_frame == 7) // 02 21 01 - 27
					{
					ks_battery_cum_op_time = CAN_UINT32(0) / 3600;
					}
				}
			break;

		case 0x02: //Cell voltages
		case 0x03:
		case 0x04:
			// diag page 02-04: skip first frame (no data)
			base = ((pid-2)<<5) + m_poll_ml_offset - (length - 3);
			for (bVal = 0; bVal < length && ((base + bVal)<101); bVal++)
				{
				//TODO BmsSetCellVoltage(base + bVal, CAN_BYTE(bVal) * 0.02);
				//ks_battery_cell_voltage[base + bVal] = CAN_BYTE(bVal);
			  }
			break;

		case 0x05:
			if (m_poll_ml_frame == 1)
				{
				//TODO Untested.
				base = ((pid-2)<<5) + m_poll_ml_offset - (length - 3);
				for (bVal = 0; bVal < length && ((base + bVal)<101); bVal++){
					//ks_battery_cell_voltage[base + bVal] = CAN_BYTE(bVal);
           // TODO BmsSetCellVoltage(base + bVal, CAN_BYTE(bVal) * 0.02);
					}

				m_b_inlet_temperature->SetValue( CAN_BYTE(5) );
				m_b_min_temperature->SetValue( CAN_BYTE(6) );
				}
			else if (m_poll_ml_frame == 2)
				{
				m_b_max_temperature->SetValue( CAN_BYTE(0) );
				}
			else if (m_poll_ml_frame == 3)
				{
				//ks_air_bag_hwire_duty = can_databuffer[5 + CAN_ADJ];
				m_b_heat_1_temperature->SetValue( CAN_BYTE(5) );
				m_b_heat_2_temperature->SetValue( CAN_BYTE(6) );
				}
			else if (m_poll_ml_frame == 4)
				{
				m_b_cell_det_max->SetValue( (float)CAN_UINT(0)/10.0 );
				m_b_cell_det_max_no->SetValue( CAN_BYTE(2) );
				m_b_cell_det_min->SetValue( (float)CAN_UINT(3)/10.0 );
				m_b_cell_det_min_no->SetValue( CAN_BYTE(5) );
				}
			break;
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
void OvmsVehicleKiaSoulEv::IncomingLDC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
	{
	switch (pid)
		{
		case 0x01:
			// 12V system
			ks_ldc_enabled = (CAN_BYTE(0) & 0x07) != 0;
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
void OvmsVehicleKiaSoulEv::IncomingSJB(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
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
