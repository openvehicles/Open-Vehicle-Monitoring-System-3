/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th May 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Michael Stegen / Stegen Electronics
;    (C) 2019       Mark Webb-Johnson
;    (C) 2020 Craig Leres
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

#ifndef __VEHICLE_CADILLAC_C2_CTS_H__
#define __VEHICLE_CADILLAC_C2_CTS_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleCadillaccC2CTS : public OvmsVehicle
  {
  public:
    OvmsVehicleCadillaccC2CTS();
    ~OvmsVehicleCadillaccC2CTS();

  protected:
      void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid,
        uint8_t* data, uint8_t length, uint16_t mlremain);

  public:
    void IncomingFrameCan2(CAN_frame_t* p_frame);

  public:
    virtual vehicle_command_t CommandWakeup(void);
    virtual vehicle_command_t CommandLock(const char* pin);
    virtual vehicle_command_t CommandUnlock(const char* pin);

  private:
    void Ticker10(uint32_t ticker);

  protected:
    char m_vin[18];
  };

#endif //#ifndef __VEHICLE_CADILLAC_C2_CTS_H__
