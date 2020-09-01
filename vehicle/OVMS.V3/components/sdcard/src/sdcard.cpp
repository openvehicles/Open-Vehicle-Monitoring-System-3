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

#include "ovms_log.h"
static const char *TAG = "sdcard";

#include <string>
#include <string.h>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "esp_intr_alloc.h"
#include "driver/gpio.h"
#include "sdcard.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "ovms_boot.h"

static int insertcount = 0;
static int mountcount = 0;

void sdcard::Ticker1(std::string event, void* data)
  {
  if (insertcount > 0)
    {
    insertcount--;
    if (insertcount == 0)
      {
      CheckCardState();
      }
    }
  if (mountcount > 0)
    {
    mountcount--;
    if (mountcount == 0)
      {
      if (m_mounted)
        MyEvents.SignalEvent("sd.mounted", NULL);
      else
        {
        MyEvents.SignalEvent("sd.unmounted", NULL);
        if (MyBoot.IsShuttingDown())
          MyBoot.RestartReady(TAG);
        }
      }
    }
  }

void sdcard::EventSystemShutDown(std::string event, void* data)
  {
  if (m_mounted && event == "system.shuttingdown")
    {
    MyBoot.RestartPending(TAG);
    ESP_LOGI(TAG,"Unmounting SDCARD for reset");
    unmount();
    }
  else if (m_mounted && event == "system.shutdown")
    {
    ESP_LOGI(TAG,"Hard SD unmount for reset");
    unmount(true);
    }
  }

static void IRAM_ATTR sdcard_isr_handler(void* arg)
  {
  insertcount = 2;
  }

sdcard::sdcard(const char* name, bool mode1bit, bool autoformat, int cdpin)
  : pcp(name)
  {
  m_host = SDMMC_HOST_DEFAULT();
  if (mode1bit)
    {
    m_host.flags = SDMMC_HOST_FLAG_1BIT;
    }

  m_slot = SDMMC_SLOT_CONFIG_DEFAULT();
// Disable driver-level CD pin, as we do this ourselves
//  if (cdpin)
//    {
//    m_slot.gpio_cd = (gpio_num_t)cdpin;
//    }
  m_slot.width = 1;

  memset(&m_mount,0,sizeof(esp_vfs_fat_sdmmc_mount_config_t));
  m_mount.format_if_mount_failed = autoformat;
  m_mount.max_files = 5;

  m_mounted = false;
  m_unmounting = false;
  m_cd = false;
  m_cdpin = cdpin;
  insertcount = 5;

  // Register our events
  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&sdcard::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown", std::bind(&sdcard::EventSystemShutDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shutdown", std::bind(&sdcard::EventSystemShutDown, this, _1, _2));

  gpio_pullup_en((gpio_num_t)cdpin);
  gpio_set_intr_type((gpio_num_t)cdpin, GPIO_INTR_ANYEDGE);
  gpio_isr_handler_add((gpio_num_t)cdpin, sdcard_isr_handler, (void*) this);
  }

sdcard::~sdcard()
  {
  if (m_mounted)
    {
    unmount(true);
    }
  }

esp_err_t sdcard::mount()
  {
  if (m_unmounting)
    {
    ESP_LOGE(TAG, "mount failed: unmount in progress");
    return ESP_FAIL;
    }
  else if (m_mounted)
    {
    ESP_LOGE(TAG, "mount failed: already mounted");
    return ESP_FAIL;
    }

  m_host.max_freq_khz = MyConfig.GetParamValueInt("sdcard", "maxfreq.khz", 16000);
  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sd", &m_host, &m_slot, &m_mount, &m_card);
  if (ret == ESP_OK)
    {
    ESP_LOGI(TAG, "mount done");
    m_mounted = true;
    m_unmounting = false;
    mountcount = 3;
    }
  else
    {
    ESP_LOGE(TAG, "mount failed: %s", esp_err_to_name(ret));
    }

  return ret;
  }

static void sdcard_unmounting_done(const char* event, void* data)
  {
  ESP_LOGD(TAG, "unmount: preparation done");
  MyPeripherals->m_sdcard->unmount(true);
  }

esp_err_t sdcard::unmount(bool hard /*=false*/)
  {
  if (!m_mounted)
    return ESP_OK;

  if (!hard)
    {
    if (!m_unmounting)
      {
      m_unmounting = true;
      ESP_LOGD(TAG, "unmount: preparing");
      MyEvents.SignalEvent("sd.unmounting", NULL, sdcard_unmounting_done);
      }
    return ESP_FAIL;
    }
  else
    {
    esp_err_t ret = esp_vfs_fat_sdmmc_unmount();
    if (ret == ESP_OK)
      {
      ESP_LOGI(TAG, "unmount done");
      m_mounted = false;
      m_unmounting = false;
      mountcount = 3;
      }
    else
      {
      ESP_LOGE(TAG, "unmount failed: %s", esp_err_to_name(ret));
      }
    return ret;
    }
  }

bool sdcard::isavailable()
  {
  return m_mounted && !m_unmounting;
  }

bool sdcard::isunmounting()
  {
  return m_unmounting;
  }

bool sdcard::ismounted()
  {
  return m_mounted;
  }

bool sdcard::isinserted()
  {
  return m_cd;
  }

void sdcard::CheckCardState()
  {
  bool cd = (gpio_get_level((gpio_num_t)m_cdpin)==0)?true:false;

  if (cd != m_cd)
    {
    m_cd = cd;
    if (m_cd)
      {
      // SD CARD has been inserted. Let's auto-mount
      ESP_LOGI(TAG, "SD CARD has been inserted");
      MyEvents.SignalEvent("sd.insert", NULL);
      if (MyConfig.GetParamValueBool("sdcard", "automount", true))
        {
        mount();
        }
      }
    else
      {
      // SD CARD has been removed. A bit late, but let's dismount
      ESP_LOGI(TAG, "SD CARD has been removed");
      if (m_mounted) unmount(true);
      MyEvents.SignalEvent("sd.remove", NULL);
      }
    }
  }

void sdcard_printinfo(int verbosity, OvmsWriter* writer)
  {
  sdmmc_card_t* card = MyPeripherals->m_sdcard->m_card;
  FATFS *fs;
  DWORD fre_clust, fre_sect, tot_sect;

  writer->printf("Name: %s\n", card->cid.name);

  // Get volume information and free clusters
  // Note: assuming drive "1:" = /sd ("0:" = /store)
  if (f_getfree("1:", &fre_clust, &fs) == FR_OK)
    {
    tot_sect = (fs->n_fatent - 2) * fs->csize;
    fre_sect = fre_clust * fs->csize;
    writer->printf("Size: %6llu MB\nFree: %6llu MB\n",
      ((uint64_t) tot_sect) * card->csd.sector_size / (1024 * 1024),
      ((uint64_t) fre_sect) * card->csd.sector_size / (1024 * 1024));
    }

  if (verbosity > COMMAND_RESULT_MINIMAL)
    {
    writer->printf("\nCard type: %s\n", (card->ocr & SD_OCR_SDHC_CAP)?"SDHC/SDXC":"SDSC");
    writer->printf("Max speed: %d kHz\n", card->csd.tr_speed/1000);
    writer->printf("Capacity: %llu MB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
    writer->printf("CSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d\n",
                    card->csd.csd_ver,
                    card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
    writer->printf("SCR: sd_spec=%d, bus_width=%d\n", card->scr.sd_spec, card->scr.bus_width);
    }
  }

void sdcard_mount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_sdcard->isunmounting())
    {
    writer->puts("Error: SD CARD is unmounting");
    return;
    }
  if (MyPeripherals->m_sdcard->ismounted())
    {
    writer->puts("SD CARD is already mounted");
    return;
    }

  esp_err_t res = MyPeripherals->m_sdcard->mount();
  if (res == ESP_OK)
    writer->puts("Mounted SD CARD");
  else
    writer->printf("Error: SD mount failed: %s\n", esp_err_to_name(res));
  }

void sdcard_unmount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyPeripherals->m_sdcard->ismounted())
    {
    writer->puts("SD CARD not mounted");
    return;
    }

  esp_err_t res = ESP_OK;
  int maxwait_seconds = 5;
  if (argc >= 1)
    maxwait_seconds = atoi(argv[0]);

  if (maxwait_seconds == 0)
    {
    MyPeripherals->m_sdcard->unmount();
    writer->puts("Unmounting SD CARD");
    return;
    }

  for (int i=maxwait_seconds; i; i--)
    {
    res = MyPeripherals->m_sdcard->unmount();
    if (res == ESP_OK) break;
    if (i > 1)
      vTaskDelay(pdMS_TO_TICKS(1000));
    }
  if (res == ESP_OK)
    writer->puts("Unmounted SD CARD");
  else
    writer->printf("Error: SD unmount failed: %s\n", esp_err_to_name(res));
  }

void sdcard_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_sdcard->isinserted())
    {
    writer->puts("SD CARD is inserted");
    }
  else
    {
    writer->puts("SD CARD is not inserted");
    }
  if (MyPeripherals->m_sdcard->ismounted())
    {
    if (MyPeripherals->m_sdcard->isavailable())
      writer->puts("Status: available");
    else
      writer->puts("Status: unmounting");
    sdcard_printinfo(verbosity, writer);
    }
  }

class SDCardInit
  {
  public: SDCardInit();
} MySDCardInit  __attribute__ ((init_priority (4400)));

SDCardInit::SDCardInit()
  {
  ESP_LOGI(TAG, "Initialising SD CARD (4400)");

  MyConfig.RegisterParam("sdcard", "SD CARD configuration", true, true);

  OvmsCommand* cmd_sd = MyCommandApp.RegisterCommand("sd","SD CARD framework", sdcard_status, "", 0, 0, false);
  cmd_sd->RegisterCommand("mount","Mount SD CARD",sdcard_mount);
  cmd_sd->RegisterCommand("unmount","Unmount SD CARD",sdcard_unmount,"[<maxwait_seconds>]",0,1);
  cmd_sd->RegisterCommand("status","Show SD CARD status",sdcard_status);
  }
