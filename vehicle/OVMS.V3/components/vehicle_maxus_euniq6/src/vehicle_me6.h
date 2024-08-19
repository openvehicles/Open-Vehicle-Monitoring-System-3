/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th August 2024
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Jaime Middleton / Tucar
;    (C) 2021       Axel Troncoso   / Tucar
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

#ifndef __VEHICLE_ME6_H__
#define __VEHICLE_ME6_H__

#include "vehicle.h"
#include "metrics_standard.h"

#include "freertos/timers.h"

#include <vector>


using namespace std;

class OvmsVehicleMaxe6 : public OvmsVehicle
{
public:

  OvmsVehicleMaxe6();
  ~OvmsVehicleMaxe6();

protected:
  void Ticker1(uint32_t ticker) override;
    
private:
  void IncomingFrameCan1(CAN_frame_t* p_frame) override;

  struct {
    uint8_t byte[8];
    uint8_t status;
    uint16_t id;
  } send_can_buffer;
};

#endif //#ifndef __VEHICLE_ME6_H__

