/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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
; Thanks to Scott451 for figuring out many of the Roadster CAN bus messages used by the OVMS,
; and the pinout of the CAN bus socket in the Roadster.
; http://www.teslamotorsclub.com/showthread.php/4388-iPhone-app?p=49456&viewfull=1#post49456"]iPhone app
; Thanks to fuzzylogic for further analysis and messages such as door status, unlock/lock, speed, VIN, etc.
; Thanks to markwj for further analysis and messages such as Trip, Odometer, TPMS, etc.
*/

#include "ovms_log.h"
static const char *TAG = "v-energica";

#include <stdio.h>
#include <string.h>
#include "vehicle_energica.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "vehicle_energica_pids.h"

// TODO: Detect charge start/stop (PID 0x102 bit 9) and build charge stats

OvmsVehicleEnergica::OvmsVehicleEnergica()
  {
  ESP_LOGI(TAG, "Energica vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
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
			*StandardMetrics.ms_v_env_handbrake  = val->front_brake;
			*StandardMetrics.ms_v_env_headlights = val->high_beam;
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

			*StandardMetrics.ms_v_bat_soc     = val->soc();
			*StandardMetrics.ms_v_bat_soh     = val->soh();
			*StandardMetrics.ms_v_bat_voltage = val->pack_voltage();
			*StandardMetrics.ms_v_bat_current = val->pack_current();
			*StandardMetrics.ms_v_bat_power   = val->power_W() * 0.001f;
			*StandardMetrics.ms_v_bat_temp    = val->temp_cell_max();

			*StandardMetrics.ms_v_bat_pack_tmin = val->temp_cell_min();
			*StandardMetrics.ms_v_bat_pack_tmax = val->temp_cell_max();
			break;
		}

		// Pack cells
		case 0x203: {
			const pid_203* val = reinterpret_cast<const pid_203*>(d);

			*StandardMetrics.ms_v_bat_pack_vmin = val->min_cell_voltage() * 0.001f; // mV to V
			*StandardMetrics.ms_v_bat_pack_vmax = val->max_cell_voltage() * 0.001f;
			*StandardMetrics.ms_v_bat_pack_vavg = val->avg_cell_voltage() * 0.001f;
			break;
		}

		// GPS
		case 0x410: {
			uint16_t msg = *reinterpret_cast<uint16_t*>(d); // PID 0x410 is little-endian
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
					*StandardMetrics.ms_v_pos_gpslock   = val->fix() != 0;
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

void OvmsVehicleEnergica::Ticker1(uint32_t ticker)
  {
  }

void OvmsVehicleEnergica::Ticker60(uint32_t ticker)
  {
	// TODO: Display charge statistics, etc. (when relevant)
  if (StandardMetrics.ms_v_charge_inprogress->AsBool())
	{
	}
  else
	{
	// Vehicle is not charging
	}
  }
