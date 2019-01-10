/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 */

#ifndef __VEHICLE_SMARTED_H__
#define __VEHICLE_SMARTED_H__

#include "vehicle.h"
#include "freertos/timers.h"
#include "ovms_webserver.h"

using namespace std;

class OvmsVehicleSmartED : public OvmsVehicle
{
  public:
    OvmsVehicleSmartED();
    ~OvmsVehicleSmartED();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
	
  public:
	virtual vehicle_command_t CommandSetChargeCurrent(uint16_t limit);
	virtual vehicle_command_t CommandSetChargeTimer(bool timeron, uint16_t timerstart);
	virtual vehicle_command_t CommandClimateControl(bool enable);
	virtual vehicle_command_t CommandLock();
    virtual vehicle_command_t CommandUnlock();
	
  protected:
    virtual void Ticker60(uint32_t ticker);
	TimerHandle_t m_locking_timer;

};

#endif //#ifndef __VEHICLE_SMARTED_H__
