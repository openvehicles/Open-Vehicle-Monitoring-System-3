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
  std::string hardware_info;
  std::string version_firmware;
  std::string version_server;
  bool update_available;
  std::string partition_running;
  std::string partition_boot;
  std::string changelog_server;
  };

// Flash task mode configuration:
typedef enum
  {
  OTA_FlashCfg_Default,         // standard OTA update
  OTA_FlashCfg_Force,           // force OTA update (skip version check)
  OTA_FlashCfg_FromSD,          // perform update from SD card (/sd/ovms3.bin)
  OTA_FlashCfg_HTTP,            // manual OTA flash via HTTP
  OTA_FlashCfg_SD,              // manual download to SD and flash
  OTA_FlashCfg_VFS              // manual OTA flash from VFS file
  } ota_flashcfg_t;

struct ota_flash_params_t
  {
  ota_flashcfg_t cfg;
  std::string url;
  bool reboot;
  };

class OvmsOTA
  {
  public:
    OvmsOTA();
    ~OvmsOTA();

  public:
    static void GetStatus(ota_info& info, bool check_update=true);

  public:
    void LaunchOTAFlash(ota_flashcfg_t cfg=OTA_FlashCfg_Default, const std::string& url="", bool reboot=false);
    bool AutoFlash(bool force=false);
    bool FlashHTTP(const std::string& url);
    bool FlashVFS(const std::string& path);
    void Ticker600(std::string event, void* data);

  public:
    bool IsFlashStatus();
    void SetFlashStatus(const char* status, int perc=0, bool dolog=false);
    void SetFlashPerc(int perc);
    void ClearFlashStatus();
    const char* GetFlashStatus();
    int GetFlashPerc();

  public:
    OvmsMutex m_flashing;
    const char *m_flashstatus;
    int m_flashperc;
    TaskHandle_t m_autotask;
    int m_lastcheckday;
    std::string m_lastnotifyversion;

#ifdef CONFIG_OVMS_COMP_SDCARD
  protected:
    void CheckFlashSD(std::string event, void* data);
  public:
    bool AutoFlashSD();
    bool FlashSD(const std::string& url);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  };

extern OvmsOTA MyOTA;

#endif //#ifndef __OTA_H__
