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

#include <string>
#include <string.h>
#include "sdcard.h"
#include "command.h"
#include "peripherals.h"

sdcard::sdcard(std::string name, bool mode1bit, bool autoformat, int cdpin)
  : pcp(name)
  {
  m_host = SDMMC_HOST_DEFAULT();
  if (mode1bit)
    {
    m_host.flags = SDMMC_HOST_FLAG_1BIT;
    }

  m_slot = SDMMC_SLOT_CONFIG_DEFAULT();
  if (cdpin)
    {
    m_slot.gpio_cd = (gpio_num_t)cdpin;
    }

  memset(&m_mount,0,sizeof(esp_vfs_fat_sdmmc_mount_config_t));
  m_mount.format_if_mount_failed = autoformat;
  m_mount.max_files = 5;

  m_mounted = false;
  }

sdcard::~sdcard()
  {
  if (m_mounted)
    {
    unmount();
    }
  }

esp_err_t sdcard::mount()
  {
  if (m_mounted)
    {
    unmount();
    }

  esp_err_t ret = esp_vfs_fat_sdmmc_mount("/sdcard", &m_host, &m_slot, &m_mount, &m_card);
  if (ret == ESP_OK)
    {
    m_mounted = true;
    }

  return ret;
  }

esp_err_t sdcard::unmount()
  {
  esp_err_t ret = esp_vfs_fat_sdmmc_unmount();
  if (ret == ESP_OK)
    {
    m_mounted = false;
    }
  return ret;
  }

bool sdcard::ismounted()
  {
  return m_mounted;
  }

void sdcard_mount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPeripherals.m_sdcard->mount();
  if (MyPeripherals.m_sdcard->ismounted())
    {
    if (verbosity > COMMAND_RESULT_MINIMAL)
      {
      sdmmc_card_t* card = MyPeripherals.m_sdcard->m_card;
      writer->printf("Name: %s\n", card->cid.name);
      writer->printf("Type: %s\n", (card->ocr & SD_OCR_SDHC_CAP)?"SDHC/SDXC":"SDSC");
      writer->printf("Speed: %s\n", (card->csd.tr_speed > 25000000)?"high speed":"default speed");
      writer->printf("Size: %lluMB\n", ((uint64_t) card->csd.capacity) * card->csd.sector_size / (1024 * 1024));
      writer->printf("CSD: ver=%d, sector_size=%d, capacity=%d read_bl_len=%d\n",
                     card->csd.csd_ver,
                     card->csd.sector_size, card->csd.capacity, card->csd.read_block_len);
      writer->printf("SCR: sd_spec=%d, bus_width=%d\n", card->scr.sd_spec, card->scr.bus_width);
      }
    writer->puts("Mounted SD CARD");
    }
  else
    {
    writer->puts("Error: SD CARD could not be mounted");
    }
  }

void sdcard_unmount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPeripherals.m_sdcard->unmount();
  writer->puts("Unmounted SD CARD");
  }

class SDCardInit
  {
  public: SDCardInit();
} MySDCardInit  __attribute__ ((init_priority (6000)));

SDCardInit::SDCardInit()
  {
  puts("Initialising SD CARD Framework");
  OvmsCommand* cmd_sd = MyCommandApp.RegisterCommand("sd","SD CARD framework",NULL,"<$C>",1,1);
  cmd_sd->RegisterCommand("mount","Mount SD CARD",sdcard_mount,"",0,0);
  cmd_sd->RegisterCommand("unmount","Unmount SD CARD",sdcard_unmount,"",0,0);
  }
