/*
;    Project:       Open Vehicle Monitor System
;    Module:        Power Management Webserver
;    Date:          3rd September 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019        Marko Juhanne
;;
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

#ifndef __POWERMGMT_H__
#define __POWERMGMT_H__

#include <sys/types.h>
#include <string>
#include <map>
#include "ovms_command.h"

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

// Defaults
#define POWERMGMT_MODEMOFF_DELAY          96 // hours (4 days)
#define POWERMGMT_MODEMOFF_WIFI_DELAY     0  // minutes (0 = disabled)
#define POWERMGMT_WIFIOFF_DELAY           24 // hours
#define POWERMGMT_12V_SHUTDOWN_DELAY      15 // minutes
#define POWERMGMT_POWERSAVING_DAYS        4  // days exceeding which periodic power saving is activated
#define POWERMGMT_POWERSAVING_DEEPSLEEP_DAYS 9 // days (0 = unlimited), days exceeding + deep sleep days = 13 days default
#define POWERMGMT_POWERSAVING_INTERVAL    6  // hours
#define POWERMGMT_POWERSAVING_DURATION    15 // minutes
#define POWERMGMT_POWERSAVING_TARGET      "both" // modem & wifi

// power consumption estimates (mA) for each component in active state
#define POWERMGMT_EST_BASE                28.0f // base system
#define POWERMGMT_EST_WIFI_AP             25.0f // wifi AP
#define POWERMGMT_EST_WIFI_CLIENT         2.0f  // wifi client
#define POWERMGMT_EST_MODEM               8.0f  // modem
#define POWERMGMT_EST_MODEM_SLEEP         5.0f  // modem in sleep mode (active GPS antenna attached)
#define POWERMGMT_EST_GPS                 23.0f // gps
  

struct powermgmt_consumption_t
  {
  float base_ma;
  float wifi_ma;
  float modem_ma;
  float gps_ma;
  float total_ma;
  };

class powermgmt
  {
  public:
    powermgmt();
    virtual ~powermgmt();

  public:
    void Ticker1(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);
    void GetEstimatedConsumption(powermgmt_consumption_t& out);
    float GetEstimatedCurrent();
    void PowerOffWifi();
    void PowerOffModem(bool wifi_triggered = false);
    void PowerOnWifi();
    void PowerOnModem();
    void CommandStatus(int verbosity, OvmsWriter* writer);
    void CommandConfig(int verbosity, OvmsWriter* writer);
    void CommandPowerOff(int verbosity, OvmsWriter* writer, const char* target);
    void CommandPowerOn(int verbosity, OvmsWriter* writer, const char* target);
    void CommandPowerSaving(int verbosity, OvmsWriter* writer, const char* action, const char* target = nullptr, const char* days_str = nullptr);
    void EventVehicleOn(std::string event, void* data);
    void EventVehicleAwake(std::string event, void* data);
    void EventCharge12VStart(std::string event, void* data);
    void EventCharge12VStop(std::string event, void* data);
    unsigned int GetNotChargingTimer() const { return m_notcharging_timer; }
    unsigned int GetModemOffWifiTimer() const { return m_modemoff_wifi_timer; }
    unsigned int Get12VAlertTimer() const { return m_12v_alert_timer; }
    unsigned int GetPowerSavingTimer() const { return m_powersaving_timer; }
    bool IsPowerSavingActive() const { return m_powersaving_active; }
    bool IsModemOff() const { return m_modem_off; }
    bool IsModemOffWifi() const { return m_modem_off_wifi; }
    bool IsWifiOff() const { return m_wifi_off; }
    bool IsWifiOffIfDisconnected() const { return m_wifioff_if_disconnected; }

  private:
    bool m_enabled;
    unsigned int m_notcharging_timer;
    unsigned int m_modemoff_wifi_timer;
    unsigned int m_12v_alert_timer;
    unsigned int m_powersaving_timer;

    unsigned int m_modemoff_delay;
    unsigned int m_modemoff_wifi_delay;
    bool m_modem_wifi_fallback;
    unsigned int m_wifioff_delay;
    bool m_wifioff_if_disconnected;
    unsigned int m_12v_shutdown_delay;
    bool m_powersaving_enabled;
    unsigned int m_powersaving_days;
    unsigned int m_powersaving_deepsleep_days;
    std::string m_powersaving_target;
    unsigned int m_powersaving_interval;
    unsigned int m_powersaving_duration;
    bool m_powersaving_active;
    bool m_powersaving_deepsleep_fallback;
    unsigned int m_powersaving_cycle_timer;
    bool m_powersaving_awake_phase;

    bool m_charging;
    bool m_modem_off, m_modem_off_wifi, m_wifi_off;

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  // --------------------------------------------------------------------------
  // Webserver subsystem
  //  - implementation: powermgmt_web.(h,cpp)
  // 
  
  public:
    void WebInit();
    void WebCleanup();
    static void WebCfgPowerManagement(PageEntry_t& p, PageContext_t& c);

#endif //CONFIG_OVMS_COMP_WEBSERVER

  };

extern powermgmt MyPowerMgmt;

#endif //#ifndef __POWERMGMT_H__
