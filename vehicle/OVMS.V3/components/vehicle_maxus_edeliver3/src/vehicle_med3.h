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
#include "ovms_webserver.h"


using namespace std;

class OvmsCommand;

class OvmsVehicleMaxed3 : public OvmsVehicle
  {
  public:
    OvmsVehicleMaxed3();
    ~OvmsVehicleMaxed3();

  protected:
      std::string         m_rxbuf;

      
  protected:
    void PollerStateTicker();
    void IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain);

  private:
      void IncomingFrameCan1(CAN_frame_t* p_frame);
      void IncomingPollFrame(CAN_frame_t* frame);
      /// A temporary store for the VIN
      char m_vin[17];
      float med3_cum_energy_charge_wh;


#ifdef CONFIG_OVMS_COMP_WEBSERVER
    // --------------------------------------------------------------------------
    // Webserver subsystem
    //
  public:
    void WebInit();
    //static void WebCfgFeatures(PageEntry_t& p, PageContext_t& c);
    void GetDashboardConfig(DashboardConfig& cfg);
    static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);
#endif //CONFIG_OVMS_COMP_WEBSERVER
      
       };

#endif //#ifndef __VEHICLE_MED3_H__

