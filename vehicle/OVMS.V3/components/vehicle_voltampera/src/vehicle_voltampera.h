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

#ifndef __VEHICLE_VOLTAMPERA_H__
#define __VEHICLE_VOLTAMPERA_H__

#include "vehicle.h"

using namespace std;

#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
#include "swcan.h"
#endif

class OvmsVehicleVoltAmpera : public OvmsVehicle
  {
  public:
    OvmsVehicleVoltAmpera();
    ~OvmsVehicleVoltAmpera();

  public:
    void Status(int verbosity, OvmsWriter* writer);

    vehicle_command_t CommandWakeup();
    vehicle_command_t CommandClimateControl(bool enable);
    vehicle_command_t CommandLock(const char* pin);
    vehicle_command_t CommandUnlock(const char* pin);

  protected:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan3(CAN_frame_t* p_frame);
    void IncomingFrameSWCan(CAN_frame_t* p_frame);
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
    void TxCallback(const CAN_frame_t* p_frame, bool success);
    void CommandWakeupComplete(const CAN_frame_t* p_frame, bool success);
    void ClimateControlWatchdog();
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker10(uint32_t ticker);

  protected:
    char m_vin[18];
    char m_type[6];
    int m_modelyear;
    unsigned int m_charge_timer;
    unsigned int m_charge_wm;
    unsigned int m_candata_timer;
    unsigned int m_range_rated_km;
    unsigned int m_range_estimated_km;
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
    swcan* p_swcan;  
#endif
    CanFrameCallback wakeup_frame_sent;
    unsigned int m_tx_retry_counter;
    bool m_preheat_override_BCM;        // Enable functionality to imitate BCM when preheating (instead of using Onstar commands)
    bool m_preheat_BCM_overridden;       // Imitate BCM this preheating session (this is disabled when preheat initiated using key fob)
    unsigned int m_preheat_max_time;
    OvmsMetricInt * mt_preheat_status;
    OvmsMetricInt *  mt_preheat_timer;
    OvmsMetricBool * mt_ac_active;
    OvmsMetricInt *  mt_ac_front_blower_fan_speed;  // %
    OvmsMetricFloat *  mt_coolant_heater_pwr;       // kW
    OvmsMetricInt *  mt_coolant_temp;
  };

#endif //#ifndef __VEHICLE_VOLTAMPERA_H__
