/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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

#ifndef __VEHICLE_NISSANLEAF_H__
#define __VEHICLE_NISSANLEAF_H__

#include "vehicle.h"

#define GEN_1_NEW_CAR_GIDS 281l
#define GEN_1_NEW_CAR_GIDS_S "281"
#define GEN_1_NEW_CAR_RANGE_MILES 84

using namespace std;

class OvmsVehicleNissanLeaf : public OvmsVehicle
  {
  public:
    OvmsVehicleNissanLeaf();
    ~OvmsVehicleNissanLeaf();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void Ticker1(std::string event, void* data);

  protected:

  public:
    const std::string VehicleName();
  };

#endif //#ifndef __VEHICLE_NISSANLEAF_H__
