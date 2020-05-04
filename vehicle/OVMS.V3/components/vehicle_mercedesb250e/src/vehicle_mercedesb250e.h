/*
;    Project:       Open Vehicle Monitor System
;    Date:          4th May 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2020       Jarkko Ruoho
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

#ifndef __VEHICLE_MERCEDESB250E_H__
#define __VEHICLE_MERCEDESB250E_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleMercedesB250e : public OvmsVehicle
  {
  public:
    OvmsVehicleMercedesB250e();
    ~OvmsVehicleMercedesB250e();

  public:
    void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
    OvmsMetricFloat *mt_ed_eco_accel;             // eco score on acceleration over last 6 hours
    OvmsMetricFloat *mt_ed_eco_const;             // eco score on constant driving over last 6 hours
    OvmsMetricFloat *mt_ed_eco_coast;             // eco score on coasting over last 6 hours
    OvmsMetricFloat *mt_ed_eco_score;             // eco score shown on dashboard over last 6 hours
    
  };

#endif //#ifndef __VEHICLE_MERCEDESB250E_H__
