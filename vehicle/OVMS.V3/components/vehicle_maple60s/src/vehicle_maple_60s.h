/*
;    Project:       Open Vehicle Monitor System
;    Date:          16th August 2024
;
;    Changes:
;    1.0  Initial stub
;
;    (C) 2024        Jaime Middleton / Tucar
;    (C) 2024        Axel Troncoso   / Tucar
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

#ifndef __VEHICLE_MAPLE60S_H__
#define __VEHICLE_MAPLE60S_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleMaple60S : public OvmsVehicle
{
public:
  OvmsVehicleMaple60S();
  ~OvmsVehicleMaple60S();

public:
  void IncomingFrameCan1(CAN_frame_t *p_frame) override;
  void Ticker1(uint32_t ticker) override;

private:
  struct {
    uint8_t byte[8];
    uint8_t status;
    uint16_t id;
  } send_can_buffer;

  std::array<bool, 4> m_door_lock_status;
};

#endif // #ifndef __VEHICLE_MAPLE60S_H__