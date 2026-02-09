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
			m_obc_pilot_duty->SetValue((float)value / 10.0); // Untested
		}

		if (get_uint_buff_be<1>(data, 42, value))
		{
			StdMetrics.ms_v_charge_temp->SetValue((float)(value / 2.0) - 40.0, Celcius); // Untested
		}

		if (get_uint_buff_be<1>(data, 45, value))
		{
			kia_obc_ac_voltage = (float)value;
		}

		if (get_uint_buff_be<2>(data, 49, value))
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
		if (get_uint_buff_be<2>(data, 4, value))
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
		if (get_uint_buff_be<2>(data, 16, value))
		{
			// This is the closest appoximation i can find to the 12V Amperage
			StdMetrics.ms_v_bat_12v_current->SetValue(value / 100.0, Amps);
		}

		/* if (get_uint_buff_be<2>(data, 19, value))
		{
			StdMetrics.ms_v_bat_12v_voltage->SetValue(value / 1000.0, Volts);
		} */

		/* if (get_uint_buff_be<1>(data, 23, value))
		{
			m_b_aux_soc->SetValue(value, Percentage);
		} */
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
 * Handle incoming messages from VMCU-poll
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
	uint32_t value;
	switch (pid)
	{
		// 3 7 7 7 7 7 7 7 7
		// ff fd e7 ff 30 0e c8 00 8e 03 ff 78 0c 67 0f 0d 0d 0d 0e 0e 0f 00 12 b4 02 b4 1f 00 00 98 00 00 b3 61 00 00 ae 4e 00 00 3b 5e 00 00 37 00 00 49 69 5c 09 01 3d 00 00 00 00 0b b8
	case 0x0101:
		// diag page 01: skip first frame (no data)
		// ESP_LOGD(TAG, "Frame number %x",mlframe);
		// ESP_LOGD(TAG, "BMC PID:%02x %x %02x %02x %02x %02x %02x %02x %02x %02x", pid, length, mlframe, data[0], data[1], data[2], data[3],
		// 		data[4], data[5], data[6]);

		// --------------"Stolen" from HIF_can_poll 346------------------------------
		          /* m_b_bms_relay->SetValue(get_bit<0>(value));
          m_b_bms_soc->SetValue(value / 2.0);
          StdMetrics.ms_v_bat_current->SetValue(current, Amps);
          StdMetrics.ms_v_bat_voltage->SetValue(voltage, Volts);
            m_c_power->SetValue( current * voltage, Watts);
            StdMetrics.ms_v_charge_power->SetValue(-current * voltage, Watts);
            StdMetrics.ms_v_charge_power->Clear();
          m_b_min_temperature->SetValue( value );
          m_b_max_temperature->SetValue( value );
        BmsRestartCellTemperatures();
          BmsSetCellTemperature(i, temp);
          m_b_inlet_temperature->SetValue( value, Celcius );
          StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(value * 0.1f, AmpHours);
          StdMetrics.ms_v_bat_coulomb_used_total->SetValue(value/10, AmpHours);
          StdMetrics.ms_v_bat_energy_recd_total->SetValue(value*100, WattHours);
          StdMetrics.ms_v_bat_energy_used_total->SetValue(value*100, WattHours);
          m_v_accum_op_time->SetValue(value, Seconds);
          m_b_bms_ignition->SetValue(get_bit<2>(value));
          // StandardMetrics.ms_v_mot_rpm->SetValue(drive); (Unable to verify)
           */
		// --------------------------------------------------------------------------


		if (get_buff_int_be<1>(data, 4, value))
			m_b_bms_soc->SetValue((float)value / 2.0);

		// MAx regen: ((CAN_BYTE(1)<<8) + CAN_BYTE(2))/100
		if (get_buff_int_be<2>(data, 5, bVal))
			ESP_LOGD(TAG, "C-Power: %f", ((float)bVal / 100.0));
		// m_c_power->SetValue((float)bVal / 100.0, kW);

		// m_c_power->SetValue( (float)CAN_UINT(2)/100.0, kW);
		if (get_buff_int_be<1>(data, 9, value))
		{
			ESP_LOGD(TAG, "BMS Relay: %d", get_bit<0>(value));
			m_b_bms_relay->SetValue(get_bit<0>(value));
		}

		float current = 0;
        int32_t signedValue;
        if (!get_buff_int_be<2>(data, 10, signedValue )) {
        	ESP_LOGE(TAGP, "IoniqISOTP.BMC: BMS Current: Bad Buffer");
        }
        else {
			ESP_LOGD(TAG, "Bat Amp: %f", ((float)bVal / 10.0));
        	current = (float)signedValue / 10.0;
        	StdMetrics.ms_v_bat_current->SetValue(current, Amps);
        }

        if (!get_uint_buff_be<2>(data, 12, value )) {
          	ESP_LOGE(TAGP, "IoniqISOTP.BMC: BMS Voltage: Bad Buffer");
        } else {
			ESP_LOGD(TAG, "Bat Volt: %f", ((float)bVal / 10.0));
			float voltage = (float)value / 10.0;
			StdMetrics.ms_v_bat_voltage->SetValue(voltage, Volts);

			if (current != 0) {
				m_c_power->SetValue( current * voltage, Watts);
			}

			if (current < 0) {
				StdMetrics.ms_v_charge_power->SetValue(-current * voltage, Watts);
			} else {
				StdMetrics.ms_v_charge_power->Clear();
			}
        }

		if (get_buff_int_be<1>(data, 14, value))
			ESP_LOGD(TAG, "Bat temp max: %d", value);
		// m_b_max_temperature->SetValue(value);

		if (get_buff_int_be<1>(data, 15, value))
			ESP_LOGD(TAG, "Bat temp min: %d", value);
		// m_b_min_temperature->SetValue(value);

		BmsRestartCellTemperatures();
		if (get_buff_int_be<1>(data, 16, value))
			ESP_LOGD(TAG, "Bat Cell temp 0: %d", value);
		// BmsSetCellTemperature(0, value);

		if (get_buff_int_be<1>(data, 16, value))
			ESP_LOGD(TAG, "Bat overall? temp: %d", value);
		// StdMetrics.ms_v_bat_temp->SetValue((float)value, Celcius); // TODO Should we use Min temp or Max Temp?

		if (get_buff_int_be<1>(data, 17, value))
			BmsSetCellTemperature(1, value);

		if (get_buff_int_be<1>(data, 18, value))
			BmsSetCellTemperature(2, value);

		if (get_buff_int_be<1>(data, 19, value))
			BmsSetCellTemperature(3, value);

		if (get_buff_int_be<1>(data, 22, value))
			ESP_LOGD(TAG, "Bat Inlet? temp: %d", value);
		// m_b_inlet_temperature->SetValue(value);

		if (get_buff_int_be<1>(data, 23, value))
			ESP_LOGD(TAG, "Bat Max V: %d", value);
		// m_b_cell_volt_max->SetValue((float)value / 50.0, Volts);

		// This doesn't make sense given 879:0
		if (get_buff_int_be<1>(data, 18, value))
			ESP_LOGD(TAG, "Bat Max(?) V: %d", value);
		// StandardMetrics.ms_v_bat_cell_vmax->SetValue((float)value / 50.0, Volts);

		if (get_buff_int_be<1>(data, 24, value))
			ESP_LOGD(TAG, "Number of cells at max: %d", value);
		// m_b_cell_volt_max_no->SetValue(value);

		if (get_buff_int_be<1>(data, 25, value))
		{
			ESP_LOGD(TAG, "Bat Min V: %d", value);
			// m_b_cell_volt_min->SetValue((float)value / 50.0, Volts);
			// StandardMetrics.ms_v_bat_cell_vmin->SetValue((float)value / 50.0, Volts);
		}

		if (get_buff_int_be<1>(data, 26, value))
			ESP_LOGD(TAG, "Number of cells at min: %d", value);
		// m_b_cell_volt_min_no->SetValue(value);

		// TODO Battery fan feedback
		/* if (get_buff_int_be<1>(data, 27, value))
			kn_battery_fan_feedback = value;

		// TODO Battery fan speed
		if (get_buff_int_be<1>(data, 28, value))
				kn_charge_bits.FanStatus = value & 0xF; */

		if (get_uint_buff_be<4>(data, 30, bVal)){
          	StdMetrics.ms_v_bat_coulomb_recd_total->SetValue(bVal * 0.1f, AmpHours);
			ESP_LOGD(TAG, "CCC: %d", bVal);
		}

		if (get_uint_buff_be<4>(data, 34, bVal)){
			StdMetrics.ms_v_bat_coulomb_used_total->SetValue(bVal/10, AmpHours);
			ESP_LOGD(TAG, "CDC: %d", bVal);
		}

		if (get_uint_buff_be<4>(data, 38, bVal)){
			StdMetrics.ms_v_bat_energy_recd_total->SetValue(bVal*100, WattHours);
			kia_battery_cum_charge = bVal;
			ESP_LOGD(TAG, "CCP: %d", bVal);
		}

		if (get_uint_buff_be<4>(data, 42, bVal)){
          	StdMetrics.ms_v_bat_energy_used_total->SetValue(value*100, WattHours);
			ESP_LOGD(TAG, "CDP: %d", bVal);
			kia_battery_cum_discharge = bVal;
		}

		if (get_uint_buff_be<4>(data, 46, bVal)){
			// this could be a standard metric
			kia_battery_cum_op_time = (bVal / 3600);
		}

		if (get_buff_int_be<1>(data, 50, value))
		{
			ESP_LOGD(TAG, "BMS Ignition: %d", value);
			m_b_bms_ignition->SetValue(get_bit<2>(value));
		}

		break;

	// 3 7 7 7 7 5
	case 0x0102:
	// 3 7 7 7 7 5
	case 0x0103:
	// 3 7 7 7 7 7 5
	case 0x0104:65
		int8_t count;
		int8_t offset;
		if (pid == 0x0102)
		{
			BmsRestartCellVoltages();
		}

		count = pid != 0x104 ? 36 : 28;
		offset = ((pid - 0x102) * 30);

		for (int8_t base = 4; base > count; base++)
		{
			if (!get_buff_int_be<1>(data, base, value))
			{
				ESP_LOGE(TAG, "IoniqISOTP.Battery Cell %d", (base + offset));
			}
			else
			{
				// BmsSetCellVoltage((uint8_t)(base + offset), (float)value * 2);
				ESP_LOGD(TAG, " Cell %d: %f", (base + offset), (float)value * 2);
			}
		}
		// 104-32:4
		break;

	// 3 7 7 7 7 7 5
	// 00 3f 46 10 00 00 00 00 00 00 00 00 00 14 94 00 25 52 2c 24 00 01 50 1e 9f 03 c4 07 00 4f 03 9f 00 12 03 11 01 03 13 5d 01 00 00
	case 0x0105:
		// if (get_buff_int_be<1>(data, 23, value))
		//	m_b_heat_1_temperature->SetValue(value);

		if (get_uint_buff_be<2>(data, 25, bVal))
			ESP_LOGD(TAG, "SOH: %f", ((float)bVal / 10.0));
		// StdMetrics.ms_v_bat_soh->SetValue((float)bVal / 10.0);

		/* if (get_buff_int_be<1>(data, 27, value))
			m_b_cell_det_max_no->SetValue(value);

		if (get_uint_buff_be<2>(data, 28, bVal))
			m_b_cell_det_min->SetValue((float)bVal / 10.0);

		if (get_buff_int_be<1>(data, 30, value))
			m_b_cell_det_min_no->SetValue(value); */

		if (get_buff_int_be<1>(data, 31, value))
			StdMetrics.ms_v_bat_soc->SetValue(value / 2.0);

		if (get_buff_int_be<1>(data, 32, value))
			ESP_LOGD(TAG, "Cell 96: %f", (float)value * 0.02);
		// BmsSetCellVoltage(96, (float)value * 0.02);

		if (get_buff_int_be<1>(data, 33, value))
			ESP_LOGD(TAG, "Cell 97: %f", (float)value * 0.02);
		// BmsSetCellVoltage(97, (float)value * 0.02);
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
	case 0xC002:
	{
		uint32_t lVal;
		if (get_uint_buff_be<3>(data, 2, value))
		{
			SET_TPMS_ID(0, value);
		}

		if (get_uint_buff_be<2>(data, 6, value))
		{
			lVal = (kia_tpms_id[1] & 0x0000ffff) | (value << 16);
			SET_TPMS_ID(1, lVal);
		}

		if (get_uint_buff_be<2>(data, 10, value))
		{
			lVal = value | (kia_tpms_id[1] & 0xffff0000);
			SET_TPMS_ID(1, lVal);
		}
		if (get_uint_buff_be<3>(data, 12, value))
		{
			SET_TPMS_ID(2, value);
		}

		if (get_uint_buff_be<1>(data, 15, value))
		{
			lVal = (kia_tpms_id[1] & 0x00ffffff) | (value << 24);
			SET_TPMS_ID(3, lVal);
		}

		if (get_uint_buff_be<3>(data, 17, value))
		{
			lVal = value | (kia_tpms_id[3] & 0xff000000);
			SET_TPMS_ID(3, lVal);
		}
	}
	break;

		// 3 7 7 3
	case 0xC00B:
	{
		uint32_t iPSI, iTemp;
		if (get_uint_buff_be<1>(data, 4, iPSI) && iPSI > 0)
		{
			StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, iPSI / 5.0, PSI);
		}
		if (get_uint_buff_be<1>(data, 5, iTemp) && iTemp > 0)
		{
			StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, iTemp - 50.0, Celcius);
		}

		if (get_uint_buff_be<1>(data, 9, iPSI) && iPSI > 0)
		{
			StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, iPSI / 5.0, PSI);
		}
		if (get_uint_buff_be<1>(data, 10, iTemp) && iTemp > 0)
		{
			StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, iTemp - 50.0, Celcius);
		}

		if (get_uint_buff_be<1>(data, 12, iPSI) && iPSI > 0)
		{
			StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, iPSI / 5.0, PSI);
		}
		if (get_uint_buff_be<1>(data, 13, iTemp) && iTemp > 0)
		{
			StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, iTemp - 50.0, Celcius);
		}

		if (get_uint_buff_be<1>(data, 16, iPSI) && iPSI > 0)
		{
			StdMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, iPSI / 5.0, PSI);
		}
		if (get_uint_buff_be<1>(data, 17, iTemp) && iTemp > 0)
		{
			StdMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, iTemp - 50.0, Celcius);
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
