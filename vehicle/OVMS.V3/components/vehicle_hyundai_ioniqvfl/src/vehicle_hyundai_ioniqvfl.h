/*
;    Project:       Open Vehicle Monitor System
;    Module:        Vehicle Hyundai Ioniq vFL
;    Date:          2021-03-12
;
;    Changes:
;    1.0  Initial release
;
;    (c) 2021       Michael Balzer <dexter@dexters-web.de>
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

#ifndef __VEHICLE_HYUNDAI_IONIQVFL_H__
#define __VEHICLE_HYUNDAI_IONIQVFL_H__

#include "vehicle.h"

using namespace std;

class OvmsVehicleHyundaiVFL : public OvmsVehicle
{
  public:
    OvmsVehicleHyundaiVFL();
    ~OvmsVehicleHyundaiVFL();

  protected:
    void PollerStateTicker();
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

  protected:
    std::string         m_rxbuf;
    OvmsMetricInt       *m_xhi_charge_state = NULL;
    OvmsMetricFloat     *m_xhi_bat_soc_bms = NULL;

};

#endif // __VEHICLE_HYUNDAI_IONIQVFL_H__
