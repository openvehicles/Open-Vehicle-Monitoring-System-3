/*
;    Project:       Open Vehicle Monitor System
;	 Subproject:    Integrate VW e-Golf
;
;    (C) 2023  Jared Greco <jaredgreco@gmail.com>
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

#ifndef __VEHICLE_VWEG_H__
#define __VEHICLE_VWEG_H__

#include <atomic>

#include "can.h"
#include "vehicle.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"

// #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
// #endif

// Car (poll) states
#define VWEGOLF_OFF         0           // All systems sleeping
#define VWEGOLF_AWAKE       1           // Base systems online
#define VWEGOLF_CHARGING    2           // Base systems online & car is charging the main battery
#define VWEGOLF_ON          3           // All systems online & car is drivable

class OvmsVehicleVWeGolf : public OvmsVehicle
{
public:
  OvmsVehicleVWeGolf();
  ~OvmsVehicleVWeGolf();

  void IncomingFrameCan1(CAN_frame_t* p_frame);
  void IncomingFrameCan2(CAN_frame_t* p_frame);
  void IncomingFrameCan3(CAN_frame_t* p_frame);

  vehicle_command_t CommandWakeup() override;
  void SendOcuHeartbeat();

protected:
  void Ticker1(uint32_t ticker) override;
  void Ticker10(uint32_t ticker) override;
  void Ticker60(uint32_t ticker) override;

public:
  // bool IsOff() {
  //   return m_poll_state == VWEGOLF_OFF;
  // }
  // bool IsAwake() {
  //   return m_poll_state == VWEGOLF_AWAKE;
  // }
  // bool IsCharging() {
  //   return m_poll_state == VWEGOLF_CHARGING;
  // }
  // bool IsOn() {
  //   return m_poll_state == VWEGOLF_ON;
  // }

  // --------------------------------------------------------------------------
  // Web UI Subsystem
  //  - implementation: vweup_web.(h,cpp)
  //

  #ifdef CONFIG_OVMS_COMP_WEBSERVER
  protected:
    void WebInit();
    void WebDeInit();
  
    // static void WebCfgFeatures(PageEntry_t &p, PageContext_t &c);
    static void WebCfgClimate(PageEntry_t &p, PageContext_t &c);
    static void WebCfgTesting(PageEntry_t &p, PageContext_t &c);
    static void WebDispChgMetrics(PageEntry_t &p, PageContext_t &c);
    // static void WebDispBattHealth(PageEntry_t &p, PageContext_t &c);
  #endif
  
  public:
    void GetDashboardConfig(DashboardConfig& cfg);
private:
  bool m_is_car_online = true;
  uint8_t m_last_message_received = 255;
  uint8_t m_temperature_web = 19;
  bool m_is_charging_on_battery_b = false;
  bool m_mirror_fold_in_requested = false;
  bool m_web_horn_requested = false;
  bool m_web_indicators_requested = false;
  bool m_web_panic_mode_requested = false;
  bool m_web_unlock_requested = false;
  bool m_web_lock_requested = false;
  bool m_is_control_active = false;
};

#endif //#ifndef __VEHICLE_VWEG_H__
