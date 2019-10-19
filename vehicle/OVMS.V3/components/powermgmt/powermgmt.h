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
#define POWERMGMT_MODEMOFF_DELAY      96 // hours
#define POWERMGMT_WIFIOFF_DELAY       24 // hours
#define POWERMGMT_12V_SHUTDOWN_DELAY  30 // minutes

class powermgmt
  {
  public:
    powermgmt();
    virtual ~powermgmt();

  public:
    void Ticker1(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);

  private:
    bool m_enabled;
    unsigned int m_notcharging_timer;
    unsigned int m_12v_alert_timer;

    unsigned int m_modemoff_delay;
    unsigned int m_wifioff_delay;
    unsigned int m_12v_shutdown_delay;
    bool m_charging;
    bool m_modem_off, m_wifi_off;

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
