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

#ifndef __OTA_H__
#define __OTA_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ovms_events.h"
#include "ovms_mutex.h"

struct ota_info
  {
  std::string version_firmware;
  std::string version_server;
  bool update_available;
  std::string partition_running;
  std::string partition_boot;
  std::string changelog_server;
  };

class OvmsOTA
  {
  public:
    OvmsOTA();
    ~OvmsOTA();

  public:
    static void GetStatus(ota_info& info, bool check_update=true);

  public:
    void LaunchAutoFlash(bool force=false);
    bool AutoFlash(bool force=false);
    void Ticker600(std::string event, void* data);

  public:
    OvmsMutex m_flashing;
    TaskHandle_t m_autotask;
    int m_lastcheckday;
    std::string m_lastnotifyversion;

#ifdef CONFIG_OVMS_COMP_SDCARD
  protected:
    void AutoFlashSD(std::string event, void* data);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  };

extern OvmsOTA MyOTA;

#endif //#ifndef __OTA_H__
