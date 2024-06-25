/*
;    Project:       Open Vehicle Monitor System
;    Date:          19 June 2024
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2024       Matthieu Labas
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

/*
; CREDIT
*/

/** TODO:
 * = StandardMetrics:
 *   - ms_v_env_parktime, ms_v_env_drivetime, ms_v_env_drivemode
 *   - ms_v_door_chargeport, ms_v_inv_temp (IGBT?)
 *   - ms_v_bat_consumption (from last speed and last power), ms_v_bat_energy_used, ms_v_bat_energy_recd
 * = Metrics ms_v_env_aux12v, ms_v_env_charging12v and nms_v_env_awake are always 'on'
 * = Charge stats:
 *   - ms_v_charge_climit, ms_v_charge_type, ms_v_charge_limit_soc
 * = Trip
 * = Additionnal Energica-specific metrics
 */

#include "ovms_log.h"
static const char *TAG = "v-energica";

#include "vehicle_energica.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle_energica_pids.h"


timestamp kWhMeasure::start()
{
	t_start = time_ms();
	t_last  = 0;

	power_last = 0;
	sum_power  = 0;

	ongoing = true;

	return t_start;
}

void kWhMeasure::stop()
{
	t_start = duration_ms();
	ongoing = false;
}

bool kWhMeasure::push(float volt, float amp)
{
	if (!ongoing) return false;

	int64_t ts = time_ms();
	float pwr = volt * amp;

	bool ret = (t_last > 0);
	if (ret) {
		sum_power += (ts - t_last) * (pwr + power_last) / 2; // Integral (area) between 't_last' and 'ts'
	}
	t_last = ts;
	power_last = pwr;

	return ret;
}


OvmsVehicleEnergica::OvmsVehicleEnergica()
{
	ESP_LOGI(TAG, "Energica vehicle module");

	RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

	// Custom metrics
	m_v_cell_balance = MyMetrics.InitFloat("nrjk.v.b.c.balance", SM_STALE_MIN, 0, Volts);
}

OvmsVehicleEnergica::~OvmsVehicleEnergica()
{
	ESP_LOGI(TAG, "Shutdown Energica vehicle module");
}

void OvmsVehicleEnergica::IncomingFrameCan1(CAN_frame_t* p_frame)
{
	uint8_t *d = p_frame->data.u8;

	switch (p_frame->MsgID) {
		// Motor temperatures
		case 0x20: {
			const pid_20* val = reinterpret_cast<const pid_20*>(d);

			*StandardMetrics.ms_v_mot_temp = val->motor_temp_C();
			break;
		}

		// Buttons and indicators
		case 0x102: {
			const pid_102* val = reinterpret_cast<const pid_102*>(d);

			*StandardMetrics.ms_v_env_on         = val->ignition;
			*StandardMetrics.ms_v_env_footbrake  = (val->rear_brake ? 1.0f : 0.0f);
			*StandardMetrics.ms_v_env_handbrake  = val->front_brake; // TODO: side stand?
			*StandardMetrics.ms_v_env_headlights = val->high_beam;

			*StandardMetrics.ms_v_charge_inprogress = val->charging; // TODO: val->charge_port_unlocked?

			if (val->charging) { // Charging, start charge session if not running
				if (!charge_session) {
					ESP_LOGI(TAG, "Charge started");
					charge_session.start();
				}
			} else { // Not charging, stop charge session if running
				if (charge_session) {
					ESP_LOGI(TAG, "Charge stopped");
					charge_session.stop();
				}
			}

			if (charge_session && val->blink_push) {
				ESP_LOGI(TAG, "Charge duration %llu seconds, %.3f kWh", charge_session.duration_ms() / 1000, (float)charge_session.current_kWh());
			}
			break;
		}

		// Engine speeds
		case 0x104: {
			const pid_104* val = reinterpret_cast<const pid_104*>(d);

			*StandardMetrics.ms_v_mot_rpm      = val->rpm;
			*StandardMetrics.ms_v_pos_speed    = val->speed_kmh();
			*StandardMetrics.ms_v_pos_odometer = val->odometer_km();
			*StandardMetrics.ms_v_env_gear     = val->reverse ? -1 : 1;
			break;
		}

		// Buttons and indicators
		case 0x109: {
			const pid_109* val = reinterpret_cast<const pid_109*>(d);

			*StandardMetrics.ms_v_env_throttle = val->throttle_pc();
			break;
		}

		// Pack status
		case 0x200: {
			const pid_200* val = reinterpret_cast<const pid_200*>(d);

			float volts = val->pack_voltage();
			float amps = val->pack_current();

			*StandardMetrics.ms_v_bat_soc     = val->soc();
			*StandardMetrics.ms_v_bat_soh     = val->soh();
			*StandardMetrics.ms_v_bat_voltage = volts;
			*StandardMetrics.ms_v_bat_current = amps;
			*StandardMetrics.ms_v_bat_power   = val->power_W();
			*StandardMetrics.ms_v_bat_temp    = val->temp_cell_max(); // Battery temperature is cell max

			*StandardMetrics.ms_v_bat_pack_tmin = val->temp_cell_min();
			*StandardMetrics.ms_v_bat_pack_tmax = val->temp_cell_max();

			if (charge_session.push(volts, amps)) {
				float kwh = (float)charge_session.current_kWh();
				timestamp duration = charge_session.duration_ms() / 1000;
				*StandardMetrics.ms_v_charge_current = amps;
				*StandardMetrics.ms_v_charge_voltage = volts;
				*StandardMetrics.ms_v_charge_power   = (volts * amps) / 1000;
				*StandardMetrics.ms_v_charge_time    = duration;
				*StandardMetrics.ms_v_charge_kwh     = kwh;
				ESP_LOGI(TAG, "Charge duration %llu seconds, %.3f kWh", duration, kwh);
			}
			break;
		}

		// Pack cells
		case 0x203: {
			const pid_203* val = reinterpret_cast<const pid_203*>(d);
			float vmin = val->min_cell_voltage();
			float vmax = val->max_cell_voltage();

			*StandardMetrics.ms_v_bat_pack_vmin = vmin * 0.001f; // mV to V
			*StandardMetrics.ms_v_bat_pack_vmax = vmax * 0.001f;
			*StandardMetrics.ms_v_bat_pack_vavg = val->avg_cell_voltage() * 0.001f;

			*m_v_cell_balance = (vmax - vmin) * 0.001f;

			// TODO: Test this
			// *StandardMetrics.ms_v_bat_cell_vmin->SetElemValue(val->min_cell_num(), vmin, Volts);
			// *StandardMetrics.ms_v_bat_cell_vmax->SetElemValue(val->max_cell_num(), vmax, Volts);
			break;
		}

		// GPS
		case 0x410: {
			uint16_t msg = reinterpret_cast<_pid_410*>(d)->msg();
			switch (msg) {
				case msg_gps_latspeed: {
					const pid_410_gps_latspdcourse* val = reinterpret_cast<const pid_410_gps_latspdcourse*>(d);

					*StandardMetrics.ms_v_pos_latitude  = val->latitude();
					*StandardMetrics.ms_v_pos_direction = val->course();
					*StandardMetrics.ms_v_pos_gpsspeed  = val->speed();
					break;
				}

				case msg_gps_longalt: {
					const pid_410_gps_longalt* val = reinterpret_cast<const pid_410_gps_longalt*>(d);

					*StandardMetrics.ms_v_pos_longitude = val->longitude();
					*StandardMetrics.ms_v_pos_altitude  = val->altitude();
					*StandardMetrics.ms_v_pos_gpslock   = (val->fix() > 0);
					break;
				}

				case msg_gps_datetime: {
					const pid_410_gps_date* val = reinterpret_cast<const pid_410_gps_date*>(d);

					*StandardMetrics.ms_v_pos_gpstime  = val->time_utc(); // FIXME: do not convert for each message: keep last one and convert only when required (by IncomingPollReply()?)
					*StandardMetrics.ms_v_pos_satcount = val->n_satellites();
					break;
				}
			}

			break;
		}
	}

}

#if 0
void OvmsVehicleEnergica::Ticker1(uint32_t ticker)
{
}

void OvmsVehicleEnergica::Ticker60(uint32_t ticker)
{
	if (charge_session) {
	}
}
#endif



class OvmsVehicleEnergicaInit
{
	public: OvmsVehicleEnergicaInit()
	{
		ESP_LOGI(TAG, "Registering Vehicle: Energica (9000)");

		MyVehicleFactory.RegisterVehicle<OvmsVehicleEnergica>("NRJK", "Energica");
	}
} MyOvmsVehicleEnergicaInit  __attribute__ ((init_priority (9000)));
