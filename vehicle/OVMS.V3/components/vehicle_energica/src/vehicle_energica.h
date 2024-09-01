/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
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

#ifndef __VEHICLE_ENERGICA_H__
#define __VEHICLE_ENERGICA_H__

#include "vehicle.h"

#include <esp_timer.h>
typedef uint64_t timestamp;
inline timestamp time_us() { return (timestamp)esp_timer_get_time(); }
inline timestamp time_ms() { return time_us() / 1000; }

constexpr timestamp CHARGE_NOTIF_MS = 5000; // Notify charge status every 'CHARGE_NOTIF_MS'ms

class kWhMeasure {
	bool ongoing{false};
	timestamp t_start{0}, t_last{0};
	double power_last{0}, sum_power{0}; // 'sum_power' in W.ms

public:

	kWhMeasure() = default;

	explicit operator bool () const { return ongoing; }
	bool     has_measures  () const { return (t_last > 0); } // Measure available

	timestamp start();
	void      stop ();

	timestamp duration_ms() const { return (ongoing ? time_ms() - t_start : t_start); }
	double    current_kWh() const { constexpr double i = 1e-9 / 3.6; return sum_power * i; }

	bool push(float pwr);
	inline timestamp last_push() const { return t_last; }

};

class OvmsVehicleEnergica : public OvmsVehicle {

public:
	 OvmsVehicleEnergica();
	~OvmsVehicleEnergica();

	void IncomingFrameCan1(CAN_frame_t* p_frame) override;

protected:
	// void Ticker1(uint32_t ticker) override;

	bool stand_down, stats_printed;

	// Custom metrics
	OvmsMetricFloat* m_v_cell_balance;

	kWhMeasure charge_session;
	timestamp last_charge_notif{0};

};

#endif //#ifndef __VEHICLE_ENERGICA_H__
