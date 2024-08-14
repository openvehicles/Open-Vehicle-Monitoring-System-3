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

typedef struct{
       int fromPercent;
       int toPercent;
       float maxChargeSpeed;
}charging_profile;

class OvmsCommand;

class OvmsVehicleMaxe6 : public OvmsVehicle
  {
  public:
    OvmsVehicleMaxe6();
    ~OvmsVehicleMaxe6();
      
    OvmsMetricInt *m_poll_state_metric; // Kept equal to m_poll_state for debugging purposes
    OvmsMetricFloat* m_soc_raw;
    OvmsMetricFloat* m_watt_hour_raw;
    OvmsMetricFloat* m_consump_raw;
    OvmsMetricFloat* m_consumprange_raw;
    OvmsMetricFloat* m_poll_bmsstate;

  protected:
      std::string         m_rxbuf;

      
  protected:
      void ConfigChanged(OvmsConfigParam* param) override;
      void PollerStateTicker(canbus *bus);
      void Ticker1(uint32_t ticker) override;
      void IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length) override;
      void processEnergy();
      float consumpRange;
      float hvpower;
      char m_vin[17];
      
  private:
      void IncomingFrameCan1(CAN_frame_t* p_frame) override;
      void IncomingPollFrame(CAN_frame_t* frame);
      void SetBmsStatus(uint8_t status);
      /// A temporary store for the VIN
     
      int calcMinutesRemaining(int toSoc, charging_profile charge_steps[]);
      float me56_cum_energy_charge_wh;
      bool soc_limit_reached;
      bool range_limit_reached;
      bool vanIsOn;
      bool ccschargeon;
      bool acchargeon;
      
      virtual void calculateEfficiency();
      
  };

#endif //#ifndef __VEHICLE_ME6_H__

