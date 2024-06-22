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

#if 0
#include <sys/time.h>
inline uint64_t time_us(void)
{
	struct timeval tv;
	::gettimeofday(&tv, nullptr);
	return int64_t(tv.tv_sec) * 1000000 + tv.tv_usec;
}
#else
#include <esp_timer.h>
inline int64_t time_us() { return esp_timer_get_time(); }
#endif
inline int64_t time_ms() { return time_us() / 1000; }


class kWhMeasure {
	bool ongoing;
	int64_t t_start, t_last;
	double power_last, sum_power; // 'sum_power' in W.ms

public:

	kWhMeasure();

	explicit operator bool () const { return (ongoing && t_last > 0); } // Measure available

	void start();
	void stop ();

	int64_t duration_ms() const { return (ongoing ? time_ms() - t_start : t_start); }
	double  current_kWh() const { constexpr double i = 1.0 / 3600000; return sum_power * i; }

	bool push(float volt, float amp);

};

class OvmsVehicleEnergica : public OvmsVehicle {
public:
	OvmsVehicleEnergica();
	~OvmsVehicleEnergica();

	void IncomingFrameCan1(CAN_frame_t* p_frame) override;

protected:
#if 0
	void Ticker1(uint32_t ticker) override;
	void Ticker60(uint32_t ticker) override;
#endif

	// Custom metrics
	OvmsMetricFloat* m_v_cell_balance;

	kWhMeasure charge_session;

};

#endif //#ifndef __VEHICLE_ENERGICA_H__
