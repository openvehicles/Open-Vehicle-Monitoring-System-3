/*
;    Project:       Open Vehicle Monitor System
;    Date:          17 July 2018
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2018	    Paul Rensel / Reho Engineering
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

#ifndef __VEHICLE_ZEVA_H__
#define __VEHICLE_ZEVA_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleZeva : public OvmsVehicle
  {
  public:
    OvmsVehicleZeva();
    ~OvmsVehicleZeva();

  public:
    void Ticker1(uint32_t ticker) override;
    void Ticker10(uint32_t ticker) override;

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame) override;
    vehicle_command_t CommandSetChargeMode(vehicle_mode_t mode) override;
    vehicle_command_t CommandSetChargeCurrent(uint16_t limit) override;
    vehicle_command_t CommandStartCharge() override;
    vehicle_command_t CommandStopCharge() override;
    vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart) override;
    vehicle_command_t CommandCooldown(bool cooldownon) override;
    vehicle_command_t CommandWakeup() override;
    vehicle_command_t CommandLock(const char* pin) override;
    vehicle_command_t CommandUnlock(const char* pin) override;
    vehicle_command_t CommandActivateValet(const char* pin) override;
    vehicle_command_t CommandDeactivateValet(const char* pin) override;
    vehicle_command_t CommandHomelink(int button, int durationms=1000) override;
  };

#endif //#ifndef __VEHICLE_ZEVA_H__
