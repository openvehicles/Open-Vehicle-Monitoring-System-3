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
void OvmsVehicleKiaNiroEv::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t *data, uint8_t length)
{
	bool process_all = false;
	switch (job.moduleid_rec)
	{		
		case 0x778: // IGMP
		case 0x7ed: // OBC
		case 0x7a8: // BCM
		case 0x7bb: // AirCon
		case 0x7d9: // ABS 
		case 0x7ea: // VMCU
		case 0x7eb: // MCU
		case 0x7ec: // BMC
		case 0x7ce: // CM
			process_all = true;
			break;

		default:
			ESP_LOGD(TAG, "Unknown module: %03" PRIx32, job.moduleid_rec);
			break;
	}

	// Enabled above as they are converted.
	if (process_all)
	{
		std::string &rxbuf = obd_rxbuf;

		// Assemble first and following frames to get complete reply

		// init rx buffer on first (it tells us whole length)
		if (job.mlframe == 0)
		{
			obd_module = job.moduleid_rec;
			obd_rxtype = job.type;
			obd_rxpid = job.pid;
			obd_frame = 0;
			rxbuf.clear();
			ESP_LOGV(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Buffer: %d - Start",
					 job.moduleid_rec, job.type, job.pid, length + job.mlremain);
			rxbuf.reserve(length + job.mlremain);
		}
		else
		{
			if (obd_frame == 0xffff)
			{
				return; // Aborted
			}
			if ((obd_rxtype != job.type) || (obd_rxpid != job.pid) || (obd_module != job.moduleid_rec))
			{
				ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Dropped Frame",
						 job.moduleid_rec, job.type, job.pid);
				return;
			}
			++obd_frame;
			if (obd_frame != job.mlframe)
			{
				obd_frame = 0xffff;
				rxbuf.clear();
				ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Skipped Frame: %d",
						 job.moduleid_rec, job.type, job.pid, obd_frame);
				return;
			}
		}
		// Append each piece..
		rxbuf.insert(rxbuf.end(), data, data + length);
		/*
		ESP_LOGV(TAG, "IoniqISOTP: IPR %03x TYPE:%x PID: %03x Frame: %d Append: %d Expected: %d - Append",
		  job.moduleid_rec, job.type, job.pid, m_poll_ml_frame, length, job.mlremain);
		*/
		if (job.mlremain > 0)
		{
			// we need more - return for now.
			return;
		}

		Incoming_Full(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, rxbuf);
		obd_frame = 0xffff; // Received all - drop until we have a new frame 0
		rxbuf.clear();
	}
}

void OvmsVehicleKiaNiroEv::Incoming_Full(uint16_t type, uint32_t module_sent, uint32_t module_rec, uint16_t pid, const std::string &data)
{
	ESP_LOGD(TAG, "IoniqISOTP: IPR %03" PRIx32 " TYPE:%x PID: %03x Message Size: %d",
			 module_rec, type, pid, data.size());
	ESP_BUFFER_LOGV(TAG, data.data(), data.size());

	switch (type)
	{
	case VEHICLE_POLL_TYPE_OBDIIEXTENDED:
	{
		switch (module_rec)
		{
		case 0x7bb:
			IncomingFull_AirCon(type, pid, data);
			break;

		case 0x778:
			IncomingFull_IGMP(type, pid, data);
			break;

		case 0x7ce:
			IncomingFull_CM(type, pid, data);
			break;

		case 0x7a8:
			IncomingFull_BCM(type, pid, data);
			break;

		case 0x7ec:
			IncomingFull_BMC(type, pid, data);
			break;

		case 0x7d9:
			IncomingFull_AbsEsp(type, pid, data);
			break;

		default:
			ESP_LOGD(TAG, "Unkown module_rec");
			break;
		}
	}
	break;

	case VEHICLE_POLL_TYPE_OBDIIGROUP:
	{
		switch (module_rec)
		{
		case 0x7ed:
			IncomingFull_OBC(type, pid, data);
			break;

		case 0x7ea:
			IncomingFull_VMCU(type, pid, data);
			break;

		case 0x7eb:
			IncomingFull_MCU(type, pid, data);
			break;
		}
	}
	break;

	case VEHICLE_POLL_TYPE_OBDII_1A:
	{
		switch (module_rec)
		{
		case 0x7ea:
			IncomingFull_VMCU(type, pid, data);
			break;
		}
	}
	break;

	default:
		ESP_LOGD(TAG, "Unkown type");
		break;
	}
}

/**
 * Handle incoming messages from cluster.
 */
void OvmsVehicleKiaNiroEv::IncomingFull_CM(uint16_t type, uint16_t pid, const std::string &data)
{
	//	ESP_LOGI(TAG, "CM PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, mlframe, data[0], data[1], data[2], data[3],
	//			data[4], data[5], data[6]);
	//	ESP_LOGI(TAG, "---");
	int32_t value;
	switch (pid)
	{
	case 0xb002:
		if (get_buff_int_be<3>(data, 6, value))
		{
			StdMetrics.ms_v_pos_odometer->SetValue(value, GetConsoleUnits());
		}
		break;
	}
}

void OvmsVehicleKiaNiroEv::IncomingFull_AirCon(uint16_t type, uint16_t pid, const std::string &data)
{
	switch (pid)
	{
	case 0x0100:
		// 3 7 7 7 7 7
		//  7e 50 07 | c8 ff 8a 81 8b 00 ef| 10 ff ff f1 ff 90 ff | ff 00 ff ff 00 00 00
		//  7e 50 07 c8 ff 8a 81 8b 00 ef 10 ff ff f1 ff 90 ff ff 00 ff ff 00 00 00
		uint8_t value;
		if (get_uint_buff_be<1>(data, 5, value))
		{
			StdMetrics.ms_v_env_cabintemp->SetValue((value / 2.0) - 40, Celcius);
		}
		if (get_uint_buff_be<1>(data, 6, value))
		{
			StdMetrics.ms_v_env_temp->SetValue((value / 2.0) - 40, Celcius);
		}
		if (get_uint_buff_be<1>(data, 29, value))
		{
			StdMetrics.ms_v_pos_speed->SetValue(value);
			CalculateAcceleration();
		}
		break;
		/*
		case 0x0102:
		if (mlframe == 1)
		{
			// Coolant temp 1 & 2 in byte 1 and 2
		}
		break;
		 */
	}
}

void OvmsVehicleKiaNiroEv::IncomingFull_AbsEsp(uint16_t type, uint16_t pid, const std::string &data)
{
	int16_t value;
	switch (pid)
	{
	// 3 7 7 7 7 7 1
	case 0xC101:
		if (get_uint_buff_be<1>(data, 18, value))
		{
			//	m_v_emergency_lights->SetValue((CAN_BYTE(2)>>6) & 1);
			m_v_emergency_lights->SetValue(get_bit<6>(value));
		}
		if (get_uint_buff_be<1>(data, 19, value))
		{
			m_v_traction_control->SetValue(get_bit<6>(value));
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
void OvmsVehicleKiaNiroEv::IncomingFull_OBC(uint16_t type, uint16_t pid, const std::string &data)
{
	uint32_t value;
	switch (pid)
	{
	case 0x01:
		// 4 7 7 7 7 7 7 7 3
		//  example: ["ff","fe","57","fc","01","8a","f5","82","53","5b","e9","05","f0","9e","02","47","00","bc","05","b8","43","0e","c8","00","98","08","e5","02","13","76","00","00","00","00","01","f4","00","00","7c","86","00","00","9f","32","03","e8","05","c0","03","0d","0e","07","0d","fb","05","f0"]
		//  example: ff fe 57 fc 01 8a f5 82 53 5b e9 05 f0 9e 02 47 00 bc 05 b8 43 0e c8 00 98 08 e5 02 13 76 00 00 00 00 01 f4 00 00 7c 86 00 00 9f 32 03 e8 05 c0 03 0d 0e 07 0d fb 05 f0
		if (get_uint_buff_be<1>(data, 27, value))
		{
			m_obc_pilot_duty->SetValue((float)value); // Untested
		}

		if (get_uint_buff_be<1>(data, 41, value))
		{
			StdMetrics.ms_v_charge_temp->SetValue((float)(value / 2.0) - 40.0, Celcius); // Untested
		}

		if (get_uint_buff_be<1>(data, 45, value))
		{
			kia_obc_ac_voltage = (float)value;
		}

		if (get_uint_buff_be<2>(data, 48, value))
		{
			float main_batt_voltage = value / 10.0;
			ESP_LOGD(TAG, "OBC Main batt: %f", main_batt_voltage);
		}
		break;

		// case 0x02:

		// break;

	case 0x03:
		// 4 7 7 7 7 7 7 2
		// example ["fe","ff","ff","e0","0c","72","20","06","97","06","8e","00","00","0b","60","00","00","48","09","00","0c","02","80","69","04","01","34","01","03","01","39","00","6d","00","6d","00","a6","01","1f","01","1a","00","29","00","2c","00","13","49"]
		// example fe ff ff e0 0c 72 20 06 97 06 8e 00 00 0b 60 00 00 48 09 00 0c 02 80 69 04 01 34 01 03 01 39 00 6d 00 6d 00 a6 01 1f 01 1a 00 29 00 2c 00 13 49
		if (get_uint_buff_be<2>(data, 3, value))
		{
			kia_obc_ac_current = (float)value / 100.0;
		}
		break;
	}
}

void OvmsVehicleKiaNiroEv::IncomingFull_VMCU(uint16_t type, uint16_t pid, const std::string &data)
{
	// ESP_LOGD(TAG, "VMCU TYPE: %02x PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", type, pid, length, mlframe, data[0], data[1], data[2], data[3],
	//		data[4], data[5], data[6]);
	uint16_t value;
	switch (pid)
	{
	// 4 7 7 4
	case 0x01:
		if (get_uint_buff_be<1>(data, 4, value))
		{
			kn_shift_bits.Park = get_bit<0>(value);
			kn_shift_bits.Reverse = get_bit<1>(value);
			kn_shift_bits.Neutral = get_bit<2>(value);
			kn_shift_bits.Drive = get_bit<3>(value);

			// ESP_LOGD(TAG, "VMCU PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, mlframe, data[0], data[1], data[2], data[3],
			//		data[4], data[5], data[6]);

			// ESP_LOGD(TAG, "ABS/ESP %02x %02x", data[1], data[2]);

			if (kn_shift_bits.Reverse)
			{
				StdMetrics.ms_v_env_gear->SetValue(-1);
			}
			else if (kn_shift_bits.Drive)
			{
				StdMetrics.ms_v_env_gear->SetValue(1);
			}
			else
			{
				StdMetrics.ms_v_env_gear->SetValue(0);
			}
		}
		break;

	// 4 7 7 7 7 5
	// Example: ["f8","ff","fc","00","01","01","00","00","00","93","07","a3","7a","80","39","a5","02","87","7f","0d","39","60","80","56","f5","21","70","00","00","01","01","01","00","00","00","07","00"]
	// Example: f8 ff fc 00 01 01 00 00 00 93 07 a3 7a 80 39 a5 02 87 7f 0d 39 60 80 56 f5 21 70 00 00 01 01 01 00 00 00 07 00
	case 0x02:
		if (get_uint_buff_be<2>(data,15,value)){
			StdMetrics.ms_v_charge_12v_current->SetValue((float)value/100,Amps);
			m_ldc_out_current->SetValue(value,Amps);
		}
		
		if (get_uint_buff_be<2>(data, 23, value))
			ESP_LOGD(TAG,"(VCU) Aux SOC: %d", value);
		break;

	case 0x80:
		std::string vin;
		// 4 7 7 7 7 7 7 7 7 7 7 7 7 2
		// Example: 20 20 20 20 20 20 20 20 20 20 1e 09 0d 14 4b 4d 48 43 38 35 31 4a 55 4c 55 30 35 39 38 31 34 33 36 36 30 31 2d 30 45 32 35 35 20 20 20 20 20 20 20 20 20 20 20 1e 09 0d 14 41 45 56 4c 44 43 37 30 45 41 45 45 4b 35 4d 2d 4e 53 33 2d 44 30 30 30 41 45 35 31 30 34 32 31 00 00 00 00 00 00 00 00
		if (get_buff_string(data, 14, 17, vin))
		{
			StandardMetrics.ms_v_vin->SetValue(vin);
		}
		break;
	}
}

/**
 * Handle incoming messages from MCU-poll
 *
 * -
 */
void OvmsVehicleKiaNiroEv::IncomingFull_MCU(uint16_t type, uint16_t pid, const std::string &data)
{
	int8_t value;
	switch (pid)
	{
		// 0x01
		// 4 7 7 7 7 7 7 7 3
		// Example: 07 ff ff ff 00 00 00 00 00 00 1c 00 08 0a 0c 0c fd ff ff ff 61 d7 cd ab c2 cd 0c 01 b7 04 36 00 0e 62 d7 26 a8 2f c1 cd 23 00 b2 33 00 00 00 00 00 00 00 00 00 00 00 00
	case 0x02:
		if (get_uint_buff_be<1>(data, 15, value))
		{
			StdMetrics.ms_v_mot_temp->SetValue(value);
		}

		if (get_uint_buff_be<1>(data, 14, value))
		{
			StdMetrics.ms_v_inv_temp->SetValue(value);
		}
		break;
		
		// 0x03
		// 0x04
		// 0x05
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
void OvmsVehicleKiaNiroEv::IncomingFull_BMC(uint16_t type, uint16_t pid, const std::string &data)
{
	uint8_t sValue = 0;
	uint16_t value = 0;
	uint16_t drive1 = 0, drive2 = 0;
	uint32_t bValue = 0;
	std::string cells;
	float current;
	switch (pid)
	{
		case 0x0101:
			if (get_uint_buff_be<1>(data, 4, sValue))
				m_b_bms_soc->SetValue(sValue / 2.0);
			
			if (get_uint_buff_be<2>(data, 5, value)){
				m_c_power->SetValue((float)value / 100.0, kW);
				ESP_LOGD(TAG, "Max charge: %f ", (float)value / 100.0);
			}

			if (get_uint_buff_be<2>(data, 7, value)) {
				m_c_power->SetValue((float)value / 100.0, kW);
				ESP_LOGD(TAG, "Max power: %f ", (float)value / 100.0);
			}

			/* Bit 0 - BMS relay Bit
			*  Bit 1 - ???
			*  Bit 5 - normal charge,
			*  Bit 6 - rapid charge
			*  Bit 7 - charging
			*/
			if (get_buff_int_be<1>(data, 9, value))
			{
				ESP_LOGD(TAG, "BMS Relay: %d", get_bit<0>(value));
				m_b_bms_relay->SetValue(get_bit<0>(value));
			}

			current = 0;
			int32_t signedValue;
			if (get_buff_int_be<2>(data, 10, signedValue )) {
				ESP_LOGD(TAG, "Bat Amp: %f", ((float)signedValue / 10.0));
				current = (float)signedValue / 10.0;
				StdMetrics.ms_v_bat_current->SetValue(current, Amps);
			}

			if (get_uint_buff_be<2>(data, 12, value )) {
				float voltage = (float)value / 10.0;
				ESP_LOGD(TAG, "Bat Volt: %f", voltage);
				StdMetrics.ms_v_bat_voltage->SetValue(voltage, Volts);
			}

			if (get_buff_int_be<1>(data, 14, value)){
				StdMetrics.ms_v_bat_pack_tmax->SetValue(value);
				m_b_max_temperature->SetValue(value);
			}

			if (get_buff_int_be<1>(data, 15, value)){
				StdMetrics.ms_v_bat_pack_tmin->SetValue((float)value);
				m_b_min_temperature->SetValue(value);
			}

			BmsRestartCellTemperatures();
			int32_t temp;
			if (get_buff_int_be<1>(data, 16, temp))
				BmsSetCellTemperature(0, temp);

				
			if (get_buff_int_be<1>(data, 17, temp))
				BmsSetCellTemperature(1, temp);
				
			if (get_buff_int_be<1>(data, 18, temp))
				BmsSetCellTemperature(2, temp);
				
			if (get_buff_int_be<1>(data, 19, temp))
				BmsSetCellTemperature(3, temp);

			if (get_buff_int_be<1>(data, 20, temp))
				BmsSetCellTemperature(4, temp);
				
			if (get_buff_int_be<1>(data, 22, value))
				m_b_inlet_temperature->SetValue(value);
				// ESP_LOGD(TAG, "Bat Inlet? temp: %d", value);


			if (get_uint_buff_be<1>(data, 23, value)){
				StdMetrics.ms_v_bat_cell_vmax->SetValue((float)value / 50.0, Volts);
				m_b_cell_volt_max->SetValue((float)value / 50.0, Volts);
			}

			if (get_uint_buff_be<1>(data, 18, value))
				m_b_cell_volt_max_no->SetValue(value);
			
			if (get_uint_buff_be<1>(data, 24, value))
				m_b_cell_volt_min_no->SetValue(value);

			if (get_uint_buff_be<1>(data, 25, value)){
				StdMetrics.ms_v_bat_cell_vmin->SetValue((float)value / 50.0, Volts);
				m_b_cell_volt_min->SetValue((float)value / 50.0, Volts);
			}

			if (get_uint_buff_be<1>(data, 26, value))
				m_b_cell_volt_min_no->SetValue(value);

			// TODO: Check Battery fan feedback
			if (get_uint_buff_be<1>(data, 27, value))
				kn_battery_fan_feedback = value;

			// TODO: Check Battery fan speed
			if (get_uint_buff_be<1>(data, 28, value))
				kn_charge_bits.FanStatus = value;
			
			if (get_uint_buff_be<1>(data, 29, sValue))
				ESP_LOGD(TAG, "Aux Batt Voltage (BMS): %f",(float) sValue/10);
				
			// 30 CCC
			if (get_uint_buff_be<4>(data, 30, bValue))
				StdMetrics.ms_v_bat_coulomb_recd_total->SetValue((float)bValue/10);
			
			// 35 CDC
			if (get_uint_buff_be<4>(data, 34, bValue))
				StdMetrics.ms_v_bat_coulomb_used_total->SetValue((float)bValue/10);

			// 38 CEC
			if (get_uint_buff_be<4>(data, 38, bValue)){
				StdMetrics.ms_v_bat_energy_recd_total->SetValue(bValue*100, WattHours);
				kia_battery_cum_charge = value;
			}
			
			// 42 CED
			if (get_uint_buff_be<4>(data, 42, bValue)){
				StdMetrics.ms_v_bat_energy_used_total->SetValue(bValue*100, WattHours);
				kia_battery_cum_discharge = value;
			}
			
			// this could be a standard metric
			if (get_uint_buff_be<4>(data, 46, bValue))
				kia_battery_cum_op_time = (value / 3600);
			

			if (get_buff_int_be<1>(data, 50, value))
				m_b_bms_ignition->SetValue(get_bit<2>(value));

			
			if (get_uint_buff_be<2>(data, 51, value))
				ESP_LOGD(TAG, "BMS Capacitor: %f",(float) sValue);

			if (get_uint_buff_be<2>(data, 53, drive1)
				&& get_uint_buff_be<2>(data, 55, drive2)) {
				uint16_t drive;
				if (drive2 == 0) {
					drive = drive1;
				} else if (drive1 == 0) {
					drive = drive2;
				} else {
					drive = ((drive1 + drive2) + 1) / 2;
				}

				StandardMetrics.ms_v_mot_rpm->SetValue(drive);
			}
			
			if (get_uint_buff_be<2>(data, 57, value))
				ESP_LOGD(TAG, "Surge Resistor: %f",(float) value);
		break;

	case 0x0102:
		BmsRestartCellVoltages();
	case 0x0103:
	case 0x0104:
		int8_t count;
		int8_t offset;

		count = pid != 0x104 ? 36 : 28;
		offset = ((pid - 0x102) * 32);
		for (int32_t base = 4; base < count; base++)
		{
            int32_t cellNo = (base - 4) + offset;
			if (!get_uint_buff_be<1>(data, base, value))
			{
				ESP_LOGE(TAG, "IoniqISOTP.Battery Cell %d", cellNo);
			}
			else
			{
				float voltage = (float)value * 0.02;
				BmsSetCellVoltage(cellNo, voltage);
				ESP_LOGV(TAG, "IoniqISOTP.Battery Cell %d: %f", cellNo, voltage);
			}
		}
		break;

	// 3 7 7 7 7 7 5
	// 00 3f 46 10 00 00 00 00 00 00 00 00 00 14 94 00 25 52 2c 24 00 01 50 1e 9f 03 c4 07 00 4f 03 9f 00 12 03 11 01 03 13 5d 01 00 00
	case 0x0105:
		if(get_uint_buff_be<1>(data, 20, value))
			ESP_LOGD(TAG,"Voltage Diff: %f",(float) value / 50);

		if (get_uint_buff_be<1>(data, 22, value))
			ESP_LOGD(TAG, "Airbag %d", value);

		if (get_uint_buff_be<1>(data, 23, value))
			m_b_heat_1_temperature->SetValue(value);

		if (get_uint_buff_be<2>(data, 25, value))
			StdMetrics.ms_v_bat_soh->SetValue((float)value / 10.0);

		if (get_uint_buff_be<1>(data, 27, value))
			m_b_cell_det_max_no->SetValue(value);

		if (get_uint_buff_be<2>(data, 28, value))
			m_b_cell_det_min->SetValue((float)value / 10.0);

		if (get_uint_buff_be<1>(data, 30, value))
			m_b_cell_det_min_no->SetValue(value);

		if (get_uint_buff_be<1>(data, 31, value)) {
          ESP_LOGD(TAG, "IoniqISOTP.BMC: SOC: %f", value / 2.0);
          StdMetrics.ms_v_bat_soc->SetValue(value / 2.0, Percentage);
        }

		break;

	// not added
	case 0x106:
		// Unvalidated
		if(get_uint_buff_be<1>(data, 4, value))
			ESP_LOGD(TAG, "Coolant Temp: %d", value);

		break;
	}
}

/**
 * Handle incoming messages from BCM-poll
 * Charge Port?
 * Tyre Pressure Monitoring
 */
void OvmsVehicleKiaNiroEv::IncomingFull_BCM(uint16_t type, uint16_t pid, const std::string &data)
{
	uint32_t value;
	switch (pid)
	{
		// 3 5
	case 0xB00E:
	{
		if (get_uint_buff_be<1>(data, 4, value))
		{
			StdMetrics.ms_v_door_chargeport->SetValue(get_bit<5>(value));
		}
	}
	break;

		// 3 5
	case 0xB00C:
	{
		if (get_uint_buff_be<1>(data, 4, value))
		{
			m_v_heated_handle->SetValue(get_bit<5>(value));
		}
	}
	break;

	// 3 7 7 3
	case 0xC002: { // TPMS ID
        uint32_t idPart;
        for (int idx = 0; idx < 4; ++idx) {
          if (get_uint_buff_be<3>(data, 4 + (idx * 4), idPart)) {
            if (idPart != 0) {
              kia_tpms_id[idx] = idPart;
            }
          }
        }
      }
	break;

		// 3 7 7 3
	case 0xC00B:
	{
		uint8_t iPSI;
		uint16_t iTemp;

		for (int idx = 0; idx < 4; ++idx) {
        //   if (get_uint_buff_be<3>(data, 4 + (idx * 4), idPart)) {
			if (get_uint_buff_be<1>(data, 4 + (idx * 4), iPSI)){
			  	if (iPSI > 0)
			  		StdMetrics.ms_v_tpms_pressure->SetElemValue(idx, iPSI / 5.0, PSI);
			}

			if (get_uint_buff_be<2>(data, 5 + (idx * 4), iTemp)){
				if (iTemp > 0)
					StdMetrics.ms_v_tpms_temp->SetElemValue(idx, iTemp / 1000, Celcius);
          	}
        }
	}
	break;
	}
}

/**
 * Handle incoming messages from IGMP-poll
 * IGMP AKA; EPCU
 *
 */
void OvmsVehicleKiaNiroEv::IncomingFull_IGMP(uint16_t type, uint16_t pid, const std::string &data)
{
	if (type == VEHICLE_POLL_TYPE_OBDIIEXTENDED)
	{
		uint32_t value;
		switch (pid)
		{
		case 0xbc03:
			if (get_uint_buff_be<1>(data, 5, value))
			{
				StdMetrics.ms_v_env_on->SetValue((value & 0x60) > 0);

				m_v_seat_belt_driver->SetValue(get_bit<1>(value));
				m_v_seat_belt_passenger->SetValue(get_bit<2>(value));

				StdMetrics.ms_v_door_hood->SetValue(get_bit<0>(value));
			}

			if (get_uint_buff_be<1>(data, 4, value))
			{
				if (IsLHD())
				{
					StdMetrics.ms_v_door_fl->SetValue(get_bit<5>(value));
					StdMetrics.ms_v_door_fr->SetValue(get_bit<4>(value));
					StdMetrics.ms_v_door_rl->SetValue(get_bit<0>(value));
					StdMetrics.ms_v_door_rr->SetValue(get_bit<2>(value));
					m_v_door_lock_rl->SetValue(get_bit<1>(value));
					m_v_door_lock_rr->SetValue(get_bit<3>(value));
				}
				else
				{
					StdMetrics.ms_v_door_fr->SetValue(get_bit<5>(value));
					StdMetrics.ms_v_door_fl->SetValue(get_bit<4>(value));
					StdMetrics.ms_v_door_rr->SetValue(get_bit<0>(value));
					StdMetrics.ms_v_door_rl->SetValue(get_bit<2>(value));
					m_v_door_lock_rr->SetValue(get_bit<1>(value));
					m_v_door_lock_rl->SetValue(get_bit<3>(value));
				}
			}
			break;
		case 0xbc04:
			// b5 3f 74 | ea 01 00 0e 43 -- Back seats
			if (get_uint_buff_be<1>(data, 6, value))
			{
				/*
				 * Car data is by driver, but metrics is left/right
				 */
				if (IsLHD())
				{
					m_v_seat_belt_back_left->SetValue(get_bit<1>(value));
					m_v_seat_belt_back_middle->SetValue(get_bit<3>(value));
					m_v_seat_belt_back_right->SetValue(get_bit<2>(value));
				}
				else
				{
					m_v_seat_belt_back_left->SetValue(get_bit<2>(value));
					m_v_seat_belt_back_middle->SetValue(get_bit<3>(value));
					m_v_seat_belt_back_right->SetValue(get_bit<1>(value));
				}
			}

			if (get_uint_buff_be<1>(data, 4, value))
			{
				if (IsLHD())
				{
					m_v_door_lock_fl->SetValue(get_bit<3>(value));
					m_v_door_lock_fr->SetValue(get_bit<2>(value));
				}
				else
				{
					m_v_door_lock_fr->SetValue(get_bit<3>(value));
					m_v_door_lock_fl->SetValue(get_bit<2>(value));
				}
			}

			if (get_uint_buff_be<1>(data, 7, value))
			{
				StdMetrics.ms_v_env_headlights->SetValue(get_bit<4>(value));
			}
			else
			{
				ESP_LOGE(TAG, "IoniqISOTP.IGMP.FULL: Headlights fail retrieval");
			}
			break;

		case 0xbc07:
			// 08 41 f0 | 00 00 00 09 00 ON or Charging 12V
			// 08 41 f0 | 00 00 00 01 00 Off
			// 08 41 f0 | 00 00 00 05 00 off with Charge port open
			// 08 41 f0 | 00 00 80 05 00 ???
			// 08 41 f0 | 00 00 80 00 00 Charging (and asleep?)
			if (get_uint_buff_be<1>(data, 5, value))
			{
				m_v_rear_defogger->SetValue(get_bit<1>(value));
				// StdMetrics.ms_v_door_chargeport->SetValue(get_bit<0>(value) !=  get_bit<4>(value));
			}

			if (get_uint_buff_be<1>(data, 6, value))
			{
				// ESP_LOGD(TAG, "IoniqISOTP.IGMP.State: %x", value);
				// 05 is off with charge port open
				// 00 is sleep
				// 01 is off (not asleep)
				// 09 is running
			}
			break;
		}
	}
}

/**
 * Determine if this car is a Hyundai Kona
 *
 * Currently from VIN
 */
bool OvmsVehicleKiaNiroEv::IsKona()
{
	if (m_vin[0] == 'K' && m_vin[1] == 'M' && m_vin[2] == 'H')
	{
		return true;
	}
	else
	{
		return false;
	}
}

/**
 * Get console ODO units
 *
 * Currently from configuration
 */
metric_unit_t OvmsVehicleKiaNiroEv::GetConsoleUnits()
{
	return MyConfig.GetParamValueBool("xkn", "consoleKilometers", true) ? Kilometers : Miles;
}

/**
 * Determine if this car is left hand drive
 *
 * Currently from configuration
 */
bool OvmsVehicleKiaNiroEv::IsLHD()
{
	return MyConfig.GetParamValueBool("xkn", "leftDrive", true);
}
