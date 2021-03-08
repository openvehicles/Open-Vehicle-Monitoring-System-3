/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th March 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Shane Hunns
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

#ifndef __VEHICLE_MAXED3_H__
#define __VEHICLE_MAXED3_H__

#include "vehicle.h"
#include "ovms_metrics.h"
#include "freertos/timers.h"
#include <vector>

using namespace std;

class OvmsCommand;

class OvmsVehicleMaxed3 : public OvmsVehicle
  {
  public:
    OvmsVehicleMaxed3();
    ~OvmsVehicleMaxed3();
      
  public:
      void IncomingFrameCan1(CAN_frame_t* p_frame);

  protected:
      virtual void Ticker1(uint32_t ticker);
      string              med_obd_rxbuf;
      
  private:
      void IncomingPollFrame(CAN_frame_t* frame);
   

      void HandleVinMessage(uint8_t* data, uint8_t length, uint16_t remain);
      void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
      /// A temporary store for the VIN
      char m_vin[18];
 
  };
#endif //#ifndef __VEHICLE_MAXED3_H__

