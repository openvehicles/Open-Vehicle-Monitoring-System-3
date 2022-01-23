/*
;    Project:       Open Vehicle Monitor System
;    Date:          5th July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
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

#ifndef __VEHICLE_FIAT500E_H__
#define __VEHICLE_FIAT500E_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleFiat500e : public OvmsVehicle
  {
  public:
    OvmsVehicleFiat500e();
    ~OvmsVehicleFiat500e();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandStartCharge();
    virtual vehicle_command_t CommandStopCharge();
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms);

  //public:
    //static void xse_drivemode(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  protected:
    virtual void Ticker1(uint32_t ticker);

    OvmsMetricFloat *mt_mb_trip_reset;        // Distance since reset
    OvmsMetricFloat *mt_mb_trip_start;        // Distance since default trip started
    OvmsMetricFloat *mt_mb_consumption_start; //  since default trip started
    OvmsMetricFloat *mt_mb_eco_accel;         // eco score on acceleration over last 6 hours
    OvmsMetricFloat *mt_mb_eco_const;         // eco score on constant driving over last 6 hours
    OvmsMetricFloat *mt_mb_eco_coast;         // eco score on coasting over last 6 hours
    OvmsMetricFloat *mt_mb_eco_score;         // eco score shown on dashboard over last 6 hours
    
    OvmsMetricFloat *mt_mb_fl_speed;          // Front Left  wheel speed, km/h
    OvmsMetricFloat *mt_mb_fr_speed;          // Front Right wheel speed, km/h
    OvmsMetricFloat *mt_mb_rl_speed;          // Rear  Left  wheel speed, km/h
    OvmsMetricFloat *mt_mb_rr_speed;          // Rear  Right wheel speed, km/h
    
    OvmsMetricFloat *ft_v_acelec_pwr;        // Aircondition electric power
    OvmsMetricFloat *ft_v_htrelec_pwr;       // Heater electric power
  };

#endif //#ifndef __VEHICLE_FIAT500E_H__
