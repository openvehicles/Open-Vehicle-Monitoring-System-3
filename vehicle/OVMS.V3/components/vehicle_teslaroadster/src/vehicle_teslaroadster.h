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

#ifndef __VEHICLE_TESLAROADSTER_H__
#define __VEHICLE_TESLAROADSTER_H__

#include "freertos/timers.h"
#include "vehicle.h"

using namespace std;

class OvmsVehicleTeslaRoadster : public OvmsVehicle
  {
  public:
    OvmsVehicleTeslaRoadster();
    ~OvmsVehicleTeslaRoadster();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  public:
    virtual vehicle_command_t CommandSetChargeMode(vehicle_mode_t mode);
    virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
    virtual vehicle_command_t CommandStartCharge();
    virtual vehicle_command_t CommandStopCharge();
    virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart);
    virtual vehicle_command_t CommandCooldown(bool cooldownon);
    virtual vehicle_command_t CommandWakeup();
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);
    virtual vehicle_command_t CommandActivateValet(const char* pin);
    virtual vehicle_command_t CommandDeactivateValet(const char* pin);
    virtual vehicle_command_t CommandHomelink(int button, int durationms=1000);

  public:
    TimerHandle_t m_homelink_timer;
    int m_homelink_timerbutton;
    void DoHomelinkStop();

  public:
    bool m_cooldown_running; // True if cooldown procedure is currently running
    vehicle_mode_t m_cooldown_prev_chargemode; // Charge Mode prior to cooldown
    int m_cooldown_prev_chargelimit;           // Charge Limit prior to cooldown
    bool m_cooldown_prev_charging;             // Charging prior to cooldown
    int m_cooldown_limit_temp;                 // Cooldown battery temp limit
    int m_cooldown_limit_minutes;              // Cooldown time limit
    int m_cooldown_current;                    // Cooldown current
    int m_cooldown_cycles_done;                // Cooldown cycles done
    int m_cooldown_recycle_ticker;             // Cooldown recycle ticker

  public:
    bool m_speedo_running; // True if digital speedo feature is running
    CAN_frame_t m_speedo_frame;                // Current digital speedo frame
    int m_speedo_rawspeed;                     // Current raw speed
    int m_speedo_ticker;                       // Digital speedo ticker
    int m_speedo_ticker_max;                   // Max value for ticker
    int m_speedo_ticker_count;                 // Count for speed ticker
    TimerHandle_t m_speedo_timer;              // Timer for digital speedo

  public:
    virtual void Status(int verbosity, OvmsWriter* writer);

  protected:
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();

  protected:
    virtual int GetNotifyChargeStateDelay(const char* state);
    virtual void NotifiedVehicleChargeStart();
    virtual void NotifiedVehicleOn();
    virtual void NotifiedVehicleOff();
    virtual void Ticker1(uint32_t ticker);
    virtual void Ticker60(uint32_t ticker);

  protected:
    void RequestStreamStartCAC();
    void RequestStreamStartHVAC();
    void RequestStreamStopCAC();
    void ChargeTimePredictor();

  protected:
    char m_vin[18];
    char m_type[5];
    bool m_awake; // True if car is awake
    bool m_requesting_cac;
  };

#endif //#ifndef __VEHICLE_TESLAROADSTER_H__
