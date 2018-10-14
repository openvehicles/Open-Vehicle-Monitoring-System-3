/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th Oct 2018
;
;    Changes:
;    0.1.0  Initial release
;           - fork of vehicle_demo
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo 
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

#ifndef __VEHICLE_RENAULTZOE_H__
#define __VEHICLE_RENAULTZOE_H__

#include "vehicle.h"

// CAN buffer access macros: b=byte# 0..7 / n=nibble# 0..15
#define CAN_BYTE(b)     data[b]
#define CAN_UINT(b)     (((UINT)CAN_BYTE(b) << 8) | CAN_BYTE(b+1))
#define CAN_UINT24(b)   (((uint32_t)CAN_BYTE(b) << 16) | ((UINT)CAN_BYTE(b+1) << 8) | CAN_BYTE(b+2))
#define CAN_UINT32(b)   (((uint32_t)CAN_BYTE(b) << 24) | ((uint32_t)CAN_BYTE(b+1) << 16)  | ((UINT)CAN_BYTE(b+2) << 8) | CAN_BYTE(b+3))
#define CAN_NIBL(b)     (can_databuffer[b] & 0x0f)
#define CAN_NIBH(b)     (can_databuffer[b] >> 4)
#define CAN_NIB(n)      (((n)&1) ? CAN_NIBL((n)>>1) : CAN_NIBH((n)>>1))

#define POLLSTATE_OFF					PollSetState(0);
#define POLLSTATE_RUNNING			PollSetState(1);
#define POLLSTATE_CHARGING		PollSetState(2);

using namespace std;

class OvmsVehicleRenaultZoe : public OvmsVehicle
  {
  public:
    OvmsVehicleRenaultZoe();
    ~OvmsVehicleRenaultZoe();
		void IncomingFrameCan1(CAN_frame_t* p_frame);
		void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);

	protected:
		void IncomingEVC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain);
		void car_on(bool isOn);
  };

#endif //#ifndef __VEHICLE_RENAUTZOE_H__
