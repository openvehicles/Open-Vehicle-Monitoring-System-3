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

#ifndef __VEHICLE_TESLAMODELS_H__
#define __VEHICLE_TESLAMODELS_H__

#include "vehicle.h"
#include "ovms_command.h"
#include "ovms_metrics.h"

using namespace std;

class OvmsVehicleTeslaModelS: public OvmsVehicle
  {
  public:
    OvmsVehicleTeslaModelS();
    ~OvmsVehicleTeslaModelS();

  public:
    void CommandBMS(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    void IncomingFrameCan1(CAN_frame_t* p_frame);
    void IncomingFrameCan2(CAN_frame_t* p_frame);
    void IncomingFrameCan3(CAN_frame_t* p_frame);

  protected:
    virtual void Notify12vCritical();
    virtual void Notify12vRecovered();

  protected:
    char m_vin[18];
    char m_type[5];
    float m_brick_v[96];
    float m_module_t1[16];
    float m_module_t2[16];
    uint32_t m_bmvt;
    OvmsMetricVector<float>* m_bms_v;
    OvmsMetricVector<float>* m_bms_t1;
    OvmsMetricVector<float>* m_bms_t2;
  };

#endif //#ifndef __VEHICLE_TESLAMODELS_H__
