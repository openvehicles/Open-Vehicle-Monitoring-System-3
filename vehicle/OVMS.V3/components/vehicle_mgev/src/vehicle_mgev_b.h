/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

#ifndef __VEHICLE_MGEV_B_H__
#define __VEHICLE_MGEV_B_H__

#include "vehicle_mgev.h"

class OvmsVehicleMgEvB : public OvmsVehicleMgEv
{
    public:
        OvmsVehicleMgEvB();
        ~OvmsVehicleMgEvB();

    protected:
        void Ticker1(uint32_t ticker) override;
        vehicle_command_t CommandWakeup() override;        

    private:
        void MainStateMachine(canbus* currentBus, uint32_t ticker);
        void GWMAwake(canbus* currentBus);
        void GWMUnlocked();	
        void RetryCheckState();
        void GWMUnknown();

        enum class GWMStates
        {
            Unknown,				//Unknown state
            CheckingState,			//Checking what state GWM is in
            Awake, 					//Awake but don't know if locked or unlocked. Currently unused.
            Unlocked,				//Awake and unlocked - can get parameters
            WaitToRetryCheckState	//Waiting, doing nothing for a set amount of time before retrying check state
        }; 

        // Counter for waiting to change GWM state back to Unknown to restart state check process
        uint8_t m_RetryCheckStateWaitCount = 0;
        // Remember if we are active so when we go to sleep, we can do certain tasks once
        bool m_OVMSActive = true;
};

#endif  // __VEHICLE_MGEV_B_H__
