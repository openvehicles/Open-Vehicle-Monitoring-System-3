/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th March 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Shane Hunns
;    (C) 2021       Peter Harry
;    (C) 2021       Michael Balzer
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

#ifndef __VEHICLE_MED3_H__
#define __VEHICLE_MED3_H__

#include "vehicle.h"
#include "metrics_standard.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif
#include "med3_pids.h"

#include "freertos/timers.h"

#include <vector>


using namespace std;

typedef struct{
       int fromPercent;
       int toPercent;
       float maxChargeSpeed;
}charging_profile;

class OvmsCommand;

class OvmsVehicleMaxed3 : public OvmsVehicle
  {
  public:
    OvmsVehicleMaxed3();
    ~OvmsVehicleMaxed3();
      
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
      void PollerStateTicker();
      void Ticker1(uint32_t ticker);
      void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);
      void processEnergy();
      float consumpRange;
      float hvpower;
      char m_vin[17];
      
  private:
      void IncomingFrameCan1(CAN_frame_t* p_frame);
      void IncomingPollFrame(CAN_frame_t* frame);
      void SetBmsStatus(uint8_t status);
      /// A temporary store for the VIN
     
      int calcMinutesRemaining(int toSoc, charging_profile charge_steps[]);
      float med3_cum_energy_charge_wh;
      bool soc_limit_reached;
      bool range_limit_reached;
      bool vanIsOn;
      bool ccschargeon;
      bool acchargeon;
      
      virtual void calculateEfficiency();
      
      


#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //
  public:
    void WebInit();
    void WebDeInit();
    //static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    void GetDashboardConfig(DashboardConfig& cfg);
    static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);
    static void WebCfgBattery(PageEntry_t& p, PageContext_t& c);
#endif //CONFIG_OVMS_COMP_WEBSERVER
      
       };

#endif //#ifndef __VEHICLE_MED3_H__

