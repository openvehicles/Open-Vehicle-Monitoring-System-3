/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th February 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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
static const char *TAG = "ota";

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#if ESP_IDF_VERSION_MAJOR < 4
#include "strverscmp.h"
#endif
#if ESP_IDF_VERSION_MAJOR >= 5
#include <spi_flash_mmap.h>
#endif
#include "ovms_ota.h"
#include "ovms_command.h"
#include "ovms_boot.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_peripherals.h"
#include "ovms_notify.h"
#include "metrics_standard.h"
#include "ovms_http.h"
#include "ovms_buffer.h"
#include "ovms_boot.h"
#include "ovms_netmanager.h"
#include "ovms_version.h"
#include "crypt_md5.h"
#include "ovms_vfs.h"

#define OTA_BUF_SIZE 8192  // buffer size for OTA download to SD card
OvmsOTA MyOTA __attribute__ ((init_priority (4400)));

////////////////////////////////////////////////////////////////////////////////
// Utility functions

#ifdef CONFIG_OVMS_COMP_SDCARD
/**
 * CheckSDFreeSpace: check if SD card has enough free space.
 * Includes a configurable spare reserve (ota > min.sd.space, in MB, default 50)
 * to leave headroom for logs, CAN traces and other data on the SD card.
 * @param needed    minimum bytes required for the download
 * @param writer    optional OvmsWriter for user output (may be NULL)
 * @return true if enough space available (or check not possible), false if not
 */
static bool CheckSDFreeSpace(size_t needed, OvmsWriter* writer)
  {
  FATFS *fs;
  DWORD fre_clust;
  if (f_getfree("1:", &fre_clust, &fs) == FR_OK)
    {
    int sector_size = (MyPeripherals->m_sdcard && MyPeripherals->m_sdcard->m_card)
                    ? MyPeripherals->m_sdcard->m_card->csd.sector_size : 512;
    uint64_t free_bytes = (uint64_t)fre_clust * fs->csize * sector_size;
    int spare_mb = MyConfig.GetParamValueInt("ota", "min.sd.space", 50);
    uint64_t spare_bytes = (uint64_t)spare_mb * 1024 * 1024;
    uint64_t required = (uint64_t)needed + spare_bytes;
    if (writer)
      writer->printf("SD card free space: %llu MB (reserve: %d MB)\n",
                     free_bytes / (1024*1024), spare_mb);
    if (free_bytes < required)
      {
      if (writer)
        writer->printf("Error: Not enough space on SD card (%llu MB free, %llu MB needed incl. %d MB reserve)\n",
                       free_bytes / (1024*1024), required / (1024*1024), spare_mb);
      return false;
      }
    return true;
    }
  else
    {
    if (writer)
      writer->puts("Warning: Cannot determine SD card free space, proceeding anyway");
    return true;
    }
  }
#endif // CONFIG_OVMS_COMP_SDCARD

int buildverscmp(std::string v1, std::string v2)
  {
  // compare canonical versions & check dirty state:
  // - cut off "-dirty[…]" and/or "/<partition>/<tag>[…]"
  // - if versions are equal and one of them is dirty, always return 1 (→update)
  int dirtycnt = 0;
  auto canonicalize = [&dirtycnt](std::string &v)
    {
    std::string::size_type p;
    if ((p = v.find("-dirty")) != std::string::npos)
      {
      v.resize(p);
      dirtycnt++;
      }
    else if ((p = v.find_first_of('/')) != std::string::npos)
      v.resize(p);
    };
  canonicalize(v1);
  canonicalize(v2);
  int cmp = strverscmp(v1.c_str(), v2.c_str());
  if (cmp == 0 && dirtycnt == 1)
    return 1;
  return cmp;
  }

////////////////////////////////////////////////////////////////////////////////
// Commands

void ota_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  ota_info info;
  std::string version;
  int len = 0;
  bool check_update = (strcmp(cmd->GetName(), "status")==0);
  MyOTA.GetStatus(info, check_update);

  if (info.hardware_info != "")
    len += writer->printf("Hardware:          %s\n", info.hardware_info.c_str());
  if (info.version_firmware != "")
    len += writer->printf("Firmware:          %s\n", info.version_firmware.c_str());
  if (info.partition_running != "")
    len += writer->printf("Running partition: %s\n", info.partition_running.c_str());
  if (info.partition_boot != "")
    len += writer->printf("Boot partition:    %s\n", info.partition_boot.c_str());

  if (MyOTA.IsFlashStatus())
    {
    if (MyOTA.GetFlashPerc()>0)
      len += writer->printf("Status:            %s (%d%%)\n", MyOTA.GetFlashStatus(), MyOTA.GetFlashPerc());
    else
      len += writer->printf("Status:            %s\n", MyOTA.GetFlashStatus());
    }

  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_FACTORY);
  if (version != "")
      len += writer->printf("Factory image:     %s\n", version.c_str());
  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_OTA_0);
  if (version != "")
      len += writer->printf("OTA_O image:       %s\n", version.c_str());
  version = GetOVMSPartitionVersion(ESP_PARTITION_SUBTYPE_APP_OTA_1);
  if (version != "")
      len += writer->printf("OTA_1 image:       %s\n", version.c_str());
  if (info.version_server != "")
    {
    len += writer->printf("Server Available:  %s%s\n", info.version_server.c_str(),
      (buildverscmp(info.version_server,info.version_firmware) > 0) ? " (is newer)" : " (no update required)");
    if (!info.changelog_server.empty())
      {
      writer->puts("");
      writer->puts(info.changelog_server.c_str());
      }
    }

  if (len == 0)
    writer->puts("OTA status unknown");
  }

void ota_flash_vfs(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOTA.m_autotask != NULL)
    {
    writer->puts("Error: Flash operation already in progress");
    return;
    }
  bool reboot = MyConfig.GetParamValueBool("ota", "auto.reboot", false);
  writer->printf("Launching OTA flash from %s...\n", argv[0]);
  MyOTA.LaunchOTAFlash(OTA_FlashCfg_VFS, argv[0], reboot);
  }

#ifdef CONFIG_OVMS_COMP_SDCARD
void ota_flash_sd(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOTA.m_autotask != NULL)
    {
    writer->puts("Error: Flash operation already in progress");
    return;
    }

  std::string url;
  if (argc > 0)
    {
    url = argv[0];
    }
  else
    {
    url = MyConfig.GetParamValue("ota","http.mru");
    if (url.empty())
      {
      std::string tag = MyConfig.GetParamValue("ota","tag");
      url = MyConfig.GetParamValue("ota","server");
      if (url.empty())
        url = "api.openvehicles.com/firmware/ota";
      url.append("/");
      url.append(GetOVMSProduct());
      url.append("/");
      if (tag.empty())
        url.append(CONFIG_OVMS_VERSION_TAG);
      else
        url.append(tag);
      url.append("/ovms3.bin");
      }
    }

  bool reboot = MyConfig.GetParamValueBool("ota", "auto.reboot", false);
  writer->printf("Launching OTA download to SD and flash from %s...\n", url.c_str());
  MyOTA.LaunchOTAFlash(OTA_FlashCfg_SD, url, reboot);
  }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

void ota_flash_http(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOTA.m_autotask != NULL)
    {
    writer->puts("Error: Flash operation already in progress");
    return;
    }

  std::string url;
  if (argc == 0)
    {
    std::string tag = MyConfig.GetParamValue("ota","tag");
    url = MyConfig.GetParamValue("ota","server");
    if (url.empty())
      url = "api.openvehicles.com/firmware/ota";
    url.append("/");
    url.append(GetOVMSProduct());
    url.append("/");
    if (tag.empty())
      url.append(CONFIG_OVMS_VERSION_TAG);
    else
      url.append(tag);
    url.append("/ovms3.bin");
    }
  else
    {
    url = argv[0];
    }

  bool reboot = MyConfig.GetParamValueBool("ota", "auto.reboot", false);

#ifdef CONFIG_OVMS_COMP_SDCARD
  if (MyConfig.GetParamValueBool("ota", "sd.buffer", false) && path_exists("/sd"))
    {
    writer->printf("SD buffer enabled: routing download via SD card from %s...\n", url.c_str());
    MyOTA.LaunchOTAFlash(OTA_FlashCfg_SD, url, reboot);
    return;
    }
#endif

  writer->printf("Launching OTA flash from %s...\n", url.c_str());
  MyOTA.LaunchOTAFlash(OTA_FlashCfg_HTTP, url, reboot);
  }

void ota_flash_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOTA.IsFlashStatus())
    {
    writer->printf("Flash: %s (%d%%)\n", MyOTA.GetFlashStatus(), MyOTA.GetFlashPerc());
    // Clear stale status if task has already ended (e.g. reboot disabled):
    if (MyOTA.m_autotask == NULL)
      MyOTA.ClearFlashStatus();
    }
  else if (MyOTA.m_autotask != NULL)
    writer->puts("Flash: running");
  else
    writer->puts("Flash: idle");
  }

void ota_flash_auto(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool force = (strcmp(cmd->GetName(), "force")==0);
  bool reboot = true; // reboot always for auto update (MyConfig.GetParamValueBool("ota", "auto.reboot", false);)
  writer->puts("Triggering automatic firmware update...");
  MyOTA.LaunchOTAFlash(force ? OTA_FlashCfg_Force : OTA_FlashCfg_Default, "", reboot);
  }

void ota_boot(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string tn = cmd->GetName();

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    writer->puts("Error: Flash operation already in progress - cannot flash again");
    return;
    }

  esp_partition_subtype_t subtype = ESP_PARTITION_SUBTYPE_ANY;
  if (tn.compare("factory")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_FACTORY; }
  else if (tn.compare("ota_0")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0; }
  else if (tn.compare("ota_1")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1; }
  if (subtype == ESP_PARTITION_SUBTYPE_ANY) return;

  const esp_partition_t* p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
  if (p != NULL)
    {
    esp_err_t err = esp_ota_set_boot_partition(p);
    switch (err)
      {
      case ESP_OK:
        writer->printf("Boot from %s at 0x%08" PRIx32 " (size 0x%08" PRIx32 ")\n",p->label,p->address,p->size);
        break;
      case ESP_ERR_INVALID_ARG:
        writer->puts("Error: partition argument didn't point to a valid OTA partition of type 'app'");
        break;
      case ESP_ERR_OTA_VALIDATE_FAILED:
        writer->puts("Error: partition contained invalid app image, or signature validation failed");
        break;
      case ESP_ERR_NOT_FOUND:
        writer->puts("Error: OTA partition not found");
        break;
      case ESP_ERR_FLASH_OP_TIMEOUT:
      case ESP_ERR_FLASH_OP_FAIL:
        writer->puts("Error: Flash erase/write failed");
        break;
      default:
        writer->printf("Error: cannot set OTA boot partition (error %d)\n",err);
        break;
      }
    }
  }

void ota_erase(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string tn = cmd->GetName();

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    writer->puts("Error: Flash operation already in progress - cannot flash again");
    return;
    }

  esp_partition_subtype_t subtype = ESP_PARTITION_SUBTYPE_ANY;
  if (tn.compare("factory")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_FACTORY; }
  else if (tn.compare("ota_0")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0; }
  else if (tn.compare("ota_1")==0)
    { subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1; }
  if (subtype == ESP_PARTITION_SUBTYPE_ANY) return;

  const esp_partition_t *p = esp_ota_get_running_partition();
  if ((p != NULL) && (p->subtype == subtype))
    {
    writer->puts("Error: Cannot erase currently running partition");
    return;
    }
  p = esp_ota_get_boot_partition();
  if ((p != NULL) && (p->subtype == subtype))
    {
    writer->puts("Error: Cannot erase boot partition");
    return;
    }

  p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, subtype, NULL);
  if (p != NULL)
    {
    MyOTA.SetFlashStatus("OTA Erase: Erasing partition...");
    writer->puts(MyOTA.GetFlashStatus());
    esp_ota_handle_t otah;
    esp_err_t err = esp_ota_begin(p, OTA_SIZE_UNKNOWN, &otah);
    MyOTA.ClearFlashStatus();
    if (err != ESP_OK)
      {
      writer->printf("Error: ESP32 error #%d starting OTA operation\n",err);
      return;
      }
    esp_ota_end(otah);
    writer->puts("Partition erase complete");
    }
  else
    {
    writer->puts("Error: Cannot find specified partition");
    }
  }

void ota_copy(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string fn = cmd->GetParent()->GetName();
  std::string tn = cmd->GetName();

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    writer->puts("Error: Flash operation already in progress - cannot flash again");
    return;
    }

  esp_partition_subtype_t from = ESP_PARTITION_SUBTYPE_ANY;
  if (fn.compare("factory")==0)
    { from = ESP_PARTITION_SUBTYPE_APP_FACTORY; }
  else if (fn.compare("ota_0")==0)
    { from = ESP_PARTITION_SUBTYPE_APP_OTA_0; }
  else if (fn.compare("ota_1")==0)
    { from = ESP_PARTITION_SUBTYPE_APP_OTA_1; }
  else return;

  esp_partition_subtype_t to = ESP_PARTITION_SUBTYPE_ANY;
  if (tn.compare("factory")==0)
    { to = ESP_PARTITION_SUBTYPE_APP_FACTORY; }
  else if (tn.compare("ota_0")==0)
    { to = ESP_PARTITION_SUBTYPE_APP_OTA_0; }
  else if (tn.compare("ota_1")==0)
    { to = ESP_PARTITION_SUBTYPE_APP_OTA_1; }
  else return;

  const esp_partition_t *p = esp_ota_get_running_partition();
  if ((p != NULL) && (p->subtype == to))
    {
    writer->puts("Error: Cannot copy to currently running partition");
    return;
    }
  p = esp_ota_get_boot_partition();
  if ((p != NULL) && (p->subtype == to))
    {
    writer->puts("Error: Cannot copy to boot partition");
    return;
    }

  const esp_partition_t *from_p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, from, NULL);
  if (from_p == NULL)
    {
    writer->puts("Error: Could not find partition to copy from");
    return;
    }
  const esp_partition_t *to_p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, to, NULL);
  if (to_p == NULL)
    {
    writer->puts("Error: Could not find partition to copy to");
    return;
    }
  if (from_p->size != to_p->size)
    {
    writer->puts("Error: The two specified partitions are not the same size");
    return;
    }

  writer->printf("OTA copy %s (%08" PRIu32 ") -> %s (%08" PRIx32 ") size %" PRIu32 "\n",
    fn.c_str(), from_p->address,
    tn.c_str(), to_p->address, to_p->size);

  MyOTA.SetFlashStatus("OTA Copy: Preparing flash partition...");
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(to_p, OTA_SIZE_UNKNOWN, &otah);
  if (err != ESP_OK)
    {
    MyOTA.ClearFlashStatus();
    writer->printf("Error: ESP32 error #%d starting OTA operation\n",err);
    return;
    }

  MyOTA.SetFlashStatus("OTA Copy: Copying flash image...");
  size_t offset = 0;
  while (offset < to_p->size)
    {
    char buf[512];
    size_t todo = to_p->size - offset;
    if (todo > sizeof(buf)) todo=sizeof(buf);
    esp_err_t err = esp_partition_read(from_p, offset, buf, todo);
    if (err != ESP_OK)
      {
      MyOTA.ClearFlashStatus();
      writer->printf("Error: ESP32 error #%d reading source at offset %d",err,offset);
      esp_ota_end(otah);
      return;
      }
    err = esp_ota_write(otah, buf, todo);
    if (err != ESP_OK)
      {
      MyOTA.ClearFlashStatus();
      writer->printf("Error: ESP32 error #%d writing destinatio at offset %d",err,offset);
      esp_ota_end(otah);
      return;
      }
    offset += todo;
    MyOTA.SetFlashPerc((offset*100)/to_p->size);
    }

  MyOTA.SetFlashStatus("OTA Copy: Finalising copy...");
  esp_ota_end(otah);
  MyOTA.ClearFlashStatus();
  writer->puts("OTA copy complete");
  }

////////////////////////////////////////////////////////////////////////////////
// OvmsOTA
//
// The main OTA functionality

#ifdef CONFIG_OVMS_COMP_SDCARD
void OvmsOTA::CheckFlashSD(std::string event, void* data)
  {
  if (path_exists("/sd/ovms3.bin"))
    {
    bool reboot = true; // reboot always for auto update (MyConfig.GetParamValueBool("ota", "auto.reboot", false);)
    LaunchOTAFlash(OTA_FlashCfg_FromSD, "", reboot);
    }
  }

bool OvmsOTA::AutoFlashSD()
  {
  FILE* f = fopen("/sd/ovms3.bin", "r");
  if (f == NULL) return false;
  setvbuf(f, NULL, _IOFBF, 4096);

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "AutoFlashSD: Flash operation already in progress - cannot auto flash");
    fclose(f);
    return false;
    }

  if (running==NULL)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Current running image cannot be determined - aborting");
    fclose(f);
    return false;
    }
  ESP_LOGW(TAG, "AutoFlashSD Current running partition is: %s",running->label);

  if (target==NULL)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Target partition cannot be determined - aborting");
    fclose(f);
    return false;
    }
  ESP_LOGW(TAG, "AutoFlashSD Target partition is: %s",target->label);

  if (running == target)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Cannot flash to running image partition");
    fclose(f);
    return false;
    }

  struct stat ds;
  if (stat("/sd/ovms3.bin", &ds) != 0)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Cannot stat file");
    fclose(f);
    return false;
    }
  ESP_LOGW(TAG, "AutoFlashSD Source image is %d bytes in size",(int)ds.st_size);

  SetFlashStatus("OTA Auto Flash SD: Preparing flash partition...",0,true);
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d when starting OTA operation",err);
    fclose(f);
    return false;
    }

  SetFlashStatus("OTA Auto Flash SD: Flashing image paritition...",0,true);
  char* buf = (char*)malloc(OTA_BUF_SIZE);
  if (buf == NULL)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "AutoFlashSD Error: Cannot allocate flash read buffer");
    esp_ota_end(otah);
    fclose(f);
    return false;
    }
  size_t done = 0;
  while(size_t n = fread(buf, 1, OTA_BUF_SIZE, f))
    {
    err = esp_ota_write(otah, buf, n);
    if (err != ESP_OK)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d when writing to flash - state is inconsistent",err);
      esp_ota_end(otah);
      free(buf);
      fclose(f);
      return false;
      }
    done += n;
    SetFlashPerc((done*100)/ds.st_size);
    }

  free(buf);
  fclose(f);

  SetFlashStatus("OTA Auto Flash SD: Finalising flash image...",0,true);
  err = esp_ota_end(otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d finalising OTA operation - state is inconsistent",err);
    return false;
    }

  SetFlashStatus("OTA Auto Flash SD: Setting boot partition...",0,true);
  err = esp_ota_set_boot_partition(target);
  ClearFlashStatus();
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d setting boot partition - check before rebooting",err);
    return false;
    }

  char done_path[40];
  snprintf(done_path, sizeof(done_path), "/sd/ovms3.%s.done", target->label);
  remove(done_path);
  if (rename("/sd/ovms3.bin", done_path) != 0)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ovms3.bin could not be renamed to %s - check before rebooting", done_path);
    return false;
    }

  ESP_LOGW(TAG, "AutoFlashSD OTA flash successful: Flashed %d bytes, and booting from '%s'",
                 (int)ds.st_size,target->label);
  return true;
  }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

OvmsOTA::OvmsOTA()
  {
  ESP_LOGI(TAG, "Initialising OTA (4400)");

  m_autotask = NULL;
  m_lastcheckday = -1;
  m_flashstatus = NULL;
  m_flashperc = 0;

  MyConfig.RegisterParam("ota", "OTA setup and status", true, true);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.600", std::bind(&OvmsOTA::Ticker600, this, _1, _2));

#ifdef CONFIG_OVMS_COMP_SDCARD
  MyEvents.RegisterEvent(TAG,"sd.mounted", std::bind(&OvmsOTA::CheckFlashSD, this, _1, _2));
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  OvmsCommand* cmd_ota = MyCommandApp.RegisterCommand("ota","OTA framework", ota_status, "", 0, 0, false);

  OvmsCommand* cmd_otastatus = cmd_ota->RegisterCommand("status","Show OTA status",ota_status);
  cmd_otastatus->RegisterCommand("nocheck","…skip check for available update",ota_status);

  OvmsCommand* cmd_otaflash = cmd_ota->RegisterCommand("flash","OTA flash");
  cmd_otaflash->RegisterCommand("status","Show flash progress",ota_flash_status);
  cmd_otaflash->RegisterCommand("vfs","OTA flash vfs",ota_flash_vfs,"<file>",1,1, true, vfs_file_validate);
  cmd_otaflash->RegisterCommand("http","OTA flash http",ota_flash_http,"[<url>]",0,1);
#ifdef CONFIG_OVMS_COMP_SDCARD
  cmd_otaflash->RegisterCommand("sd","OTA download to SD and flash",ota_flash_sd,"[<url>]",0,1);
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD
  OvmsCommand* cmd_otaflash_auto = cmd_otaflash->RegisterCommand("auto","Automatic regular OTA flash (over web)",ota_flash_auto);
  cmd_otaflash_auto->RegisterCommand("force","…force update (even if server version older)",ota_flash_auto);

  OvmsCommand* cmd_otaboot = cmd_ota->RegisterCommand("boot","OTA boot");
  cmd_otaboot->RegisterCommand("factory","Boot from factory image",ota_boot);
  cmd_otaboot->RegisterCommand("ota_0","Boot from ota_0 image",ota_boot);
  cmd_otaboot->RegisterCommand("ota_1","Boot from ota_1 image",ota_boot);

  OvmsCommand* cmd_otaerase = cmd_ota->RegisterCommand("erase","OTA erase");
  cmd_otaerase->RegisterCommand("factory","Erase factory image",ota_erase);
  cmd_otaerase->RegisterCommand("ota_0","Erase ota_0 image",ota_erase);
  cmd_otaerase->RegisterCommand("ota_1","Erase ota_1 image",ota_erase);

  OvmsCommand* cmd_otacopy = cmd_ota->RegisterCommand("copy","OTA copy");
  OvmsCommand* cmd_otacopyf = cmd_otacopy->RegisterCommand("factory","OTA copy factory <to>");
  cmd_otacopyf->RegisterCommand("ota_0","Copy factory to ota_0 image",ota_copy);
  cmd_otacopyf->RegisterCommand("ota_1","Copy factory to ota_1 image",ota_copy);
  OvmsCommand* cmd_otacopy0 = cmd_otacopy->RegisterCommand("ota_0","OTA copy ota_0 <to>");
  cmd_otacopy0->RegisterCommand("factory","Copy ota_0 to factory image",ota_copy);
  cmd_otacopy0->RegisterCommand("ota_1","Copy ota_0 to ota_1 image",ota_copy);
  OvmsCommand* cmd_otacopy1 = cmd_otacopy->RegisterCommand("ota_1","OTA copy ota_1 <to>");
  cmd_otacopy1->RegisterCommand("factory","Copy ota_1 to factory image",ota_copy);
  cmd_otacopy1->RegisterCommand("ota_0","Copy ota_1 to ota_0 image",ota_copy);
  }

OvmsOTA::~OvmsOTA()
  {
  }

void OvmsOTA::GetStatus(ota_info& info, bool check_update /*=true*/)
  {
  info.hardware_info = "";
  info.version_firmware = "";
  info.version_server = "";
  info.update_available = false;
  info.partition_running = "";
  info.partition_boot = "";
  info.changelog_server = "";

  if (StdMetrics.ms_m_hardware)
    info.hardware_info = StdMetrics.ms_m_hardware->AsString();

  OvmsMetricString* m = StandardMetrics.ms_m_version;
  if (m != NULL)
    {
    info.version_firmware = m->AsString();

    if (check_update && MyNetManager.m_connected_any)
      {
      // We have a wifi connection, so let's try to find out the version the server has
      std::string tag = MyConfig.GetParamValue("ota","tag");
      std::string url = MyConfig.GetParamValue("ota","server");
      if (url.empty())
        url = "api.openvehicles.com/firmware/ota";
      url.append("/");
      url.append(GetOVMSProduct());
      url.append("/");
      if (tag.empty())
        url.append(CONFIG_OVMS_VERSION_TAG);
      else
        url.append(tag);
      url.append("/ovms3.ver");

      OvmsHttpClient http(url);
      if (http.IsOpen() && http.ResponseCode() == 200 && http.BodyHasLine())
        {
        info.version_server = http.BodyReadLine();
        char rbuf[512];
        ssize_t k;
        while ((k = http.BodyRead(rbuf,512)) > 0)
          {
          info.changelog_server.append(rbuf,k);
          }
        http.Disconnect();
        }
      }

    if (buildverscmp(info.version_server, info.version_firmware) > 0)
      info.update_available = true;

    const esp_partition_t *p = esp_ota_get_running_partition();
    if (p != NULL)
      {
      info.partition_running = p->label;
      }
    p = esp_ota_get_boot_partition();
    if (p != NULL)
      {
      info.partition_boot = p->label;
      }
    }
  }

void OvmsOTA::Ticker600(std::string event, void* data)
  {
  if (MyConfig.GetParamValueBool("auto", "ota", true) == false)
    return;

  time_t rawtime;
  time ( &rawtime );
  struct tm tmu;
  localtime_r(&rawtime, &tmu);

  if ((tmu.tm_hour == MyConfig.GetParamValueInt("ota","auto.hour",2)) &&
      (tmu.tm_mday != m_lastcheckday))
    {
    m_lastcheckday = tmu.tm_mday;  // So we only try once a day (unless cleared due to a temporary fault)
    bool reboot = true; // reboot always for auto update (MyConfig.GetParamValueBool("ota", "auto.reboot", false);)
    LaunchOTAFlash(OTA_FlashCfg_Default, "", reboot);
    }
  }

bool OvmsOTA::IsFlashStatus()
  {
  return (m_flashstatus != NULL);
  }

void OvmsOTA::SetFlashStatus(const char* status, int perc, bool dolog)
  {
  m_flashstatus = status;
  m_flashperc = perc;
  if (dolog)
    { ESP_LOGI(TAG, "%s", status); }
  }

void OvmsOTA::SetFlashPerc(int perc)
  {
  if (m_flashperc/10 != perc/10)
    { ESP_LOGI(TAG, "%s %d%%", m_flashstatus, perc); }
  m_flashperc = perc;
  }

void OvmsOTA::ClearFlashStatus()
  {
  m_flashstatus = NULL;
  m_flashperc = 0;
  }

const char* OvmsOTA::GetFlashStatus()
  {
  return m_flashstatus;
  }

int OvmsOTA::GetFlashPerc()
  {
  return m_flashperc;
  }

////////////////////////////////////////////////////////////////////////////////
// Flash methods for task-based execution
// These are called from OTAFlashTask and use ESP_LOG* instead of OvmsWriter

bool OvmsOTA::FlashVFS(const std::string& path)
  {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "FlashVFS: Flash operation already in progress");
    return false;
    }

  if (running==NULL)
    {
    ESP_LOGE(TAG, "FlashVFS: Current running image cannot be determined");
    return false;
    }
  if (target==NULL)
    {
    ESP_LOGE(TAG, "FlashVFS: Target partition cannot be determined");
    return false;
    }
  if (running == target)
    {
    ESP_LOGE(TAG, "FlashVFS: Cannot flash to running image partition");
    return false;
    }

  struct stat ds;
  if (stat(path.c_str(), &ds) != 0)
    {
    ESP_LOGE(TAG, "FlashVFS: Cannot stat %s", path.c_str());
    return false;
    }
  ESP_LOGI(TAG, "FlashVFS: Source image %s is %d bytes", path.c_str(), (int)ds.st_size);

  FILE* f = fopen(path.c_str(), "r");
  if (f == NULL)
    {
    ESP_LOGE(TAG, "FlashVFS: Cannot open %s", path.c_str());
    return false;
    }

  SetFlashStatus("OTA VFS Flash: Preparing flash partition...",0,true);
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashVFS: ESP32 error #%d starting OTA", err);
    fclose(f);
    return false;
    }

  SetFlashStatus("OTA VFS Flash: Writing flash partition...");
  char buf[512];
  size_t done = 0;
  while (size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    err = esp_ota_write(otah, buf, n);
    if (err != ESP_OK)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "FlashVFS: ESP32 error #%d writing flash - state is inconsistent", err);
      esp_ota_end(otah);
      fclose(f);
      return false;
      }
    done += n;
    SetFlashPerc((done*100)/ds.st_size);
    }
  fclose(f);

  SetFlashStatus("OTA VFS Flash: Finalising flash partition...");
  err = esp_ota_end(otah);
  ClearFlashStatus();
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashVFS: ESP32 error #%d finalising - state is inconsistent", err);
    return false;
    }

  ESP_LOGI(TAG, "FlashVFS: Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashVFS: ESP32 error #%d setting boot partition", err);
    return false;
    }

  ESP_LOGI(TAG, "FlashVFS: Success - flashed %d bytes from %s", (int)ds.st_size, path.c_str());
  MyConfig.SetParamValue("ota", "http.mru", path);
  return true;
  }

bool OvmsOTA::FlashHTTP(const std::string& url)
  {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "FlashHTTP: Flash operation already in progress");
    return false;
    }

  if (running==NULL)
    {
    ESP_LOGE(TAG, "FlashHTTP: Current running image cannot be determined");
    return false;
    }
  if (target==NULL)
    {
    ESP_LOGE(TAG, "FlashHTTP: Target partition cannot be determined");
    return false;
    }
  if (running == target)
    {
    ESP_LOGE(TAG, "FlashHTTP: Cannot flash to running image partition");
    return false;
    }

  SetFlashStatus("OTA HTTP Flash: Downloading...",0,true);
  OvmsHttpClient http(url);
  if (!http.IsOpen())
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashHTTP: http://%s request failed", url.c_str());
    return false;
    }

  size_t expected = http.BodySize();
  if (expected < 32)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashHTTP: Expected download size (%d) is invalid", expected);
    return false;
    }
  ESP_LOGI(TAG, "FlashHTTP: Downloading %d bytes from %s", expected, url.c_str());

  SetFlashStatus("OTA HTTP Flash: Preparing flash partition...");
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, expected, &otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashHTTP: ESP32 error #%d starting OTA", err);
    http.Disconnect();
    return false;
    }

  SetFlashStatus("OTA HTTP Flash: Downloading OTA image...");
  uint8_t rbuf[512];
  size_t filesize = 0;
  ssize_t k;
  while ((k = http.BodyRead(rbuf,512)) > 0)
    {
    filesize += k;
    SetFlashPerc((filesize*100)/expected);
    if (filesize > target->size)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "FlashHTTP: Firmware bigger than partition - state is inconsistent");
      esp_ota_end(otah);
      http.Disconnect();
      return false;
      }
    err = esp_ota_write(otah, rbuf, k);
    if (err != ESP_OK)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "FlashHTTP: ESP32 error #%d writing flash - state is inconsistent", err);
      esp_ota_end(otah);
      http.Disconnect();
      return false;
      }
    }
  http.Disconnect();
  ESP_LOGI(TAG, "FlashHTTP: Download %s at %d bytes", (k<0)?"failed":"complete", filesize);

  if (filesize != expected)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashHTTP: Download size (%d) does not match expected (%d)", filesize, expected);
    esp_ota_end(otah);
    return false;
    }

  SetFlashStatus("OTA HTTP Flash: Finalising flash partition...");
  err = esp_ota_end(otah);
  ClearFlashStatus();
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashHTTP: ESP32 error #%d finalising - state is inconsistent", err);
    return false;
    }

  ESP_LOGI(TAG, "FlashHTTP: Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashHTTP: ESP32 error #%d setting boot partition", err);
    return false;
    }

  ESP_LOGI(TAG, "FlashHTTP: Success - flashed %d bytes from %s", expected, url.c_str());
  MyConfig.SetParamValue("ota", "http.mru", url);
  return true;
  }

#ifdef CONFIG_OVMS_COMP_SDCARD
bool OvmsOTA::FlashSD(const std::string& url)
  {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "FlashSD: Flash operation already in progress");
    return false;
    }

  if (running==NULL)
    {
    ESP_LOGE(TAG, "FlashSD: Current running image cannot be determined");
    return false;
    }
  if (target==NULL)
    {
    ESP_LOGE(TAG, "FlashSD: Target partition cannot be determined");
    return false;
    }
  if (running == target)
    {
    ESP_LOGE(TAG, "FlashSD: Cannot flash to running image partition");
    return false;
    }

  if (!path_exists("/sd"))
    {
    ESP_LOGE(TAG, "FlashSD: SD card not mounted");
    return false;
    }

  // Phase 1: Download to SD card
  SetFlashStatus("OTA SD Flash: Downloading...",0,true);
  OvmsHttpClient http(url);
  if (!http.IsOpen())
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: http://%s request failed", url.c_str());
    return false;
    }

  size_t expected = http.BodySize();
  if (expected < 32)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Expected download size (%d) is invalid", expected);
    http.Disconnect();
    return false;
    }

  if (!CheckSDFreeSpace(expected, NULL))
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Not enough space on SD card");
    http.Disconnect();
    return false;
    }

  ESP_LOGI(TAG, "FlashSD: Downloading %d bytes from %s to SD", expected, url.c_str());
  remove("/sd/ovms3.bin.dl");
  FILE *f = fopen("/sd/ovms3.bin.dl", "w");
  if (f == NULL)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Cannot create /sd/ovms3.bin.dl");
    http.Disconnect();
    return false;
    }
  setvbuf(f, NULL, _IOFBF, 4096);

  uint8_t* rbuf = (uint8_t*)malloc(OTA_BUF_SIZE);
  if (rbuf == NULL)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Cannot allocate download buffer");
    fclose(f);
    http.Disconnect();
    return false;
    }
  size_t filesize = 0;
  ssize_t k;
  bool dl_ok = true;
  while ((k = http.BodyRead(rbuf,OTA_BUF_SIZE)) > 0)
    {
    filesize += k;
    SetFlashPerc((filesize*50)/expected);
    if (fwrite(rbuf, 1, k, f) != (size_t)k)
      {
      ESP_LOGW(TAG, "FlashSD: SD write error at %d bytes", filesize);
      dl_ok = false;
      break;
      }
    }
  free(rbuf);
  fclose(f);
  http.Disconnect();

  if (!dl_ok || k < 0 || filesize != expected)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Download failed (%d/%d bytes)", filesize, expected);
    remove("/sd/ovms3.bin.dl");
    return false;
    }

  remove("/sd/ovms3.bin");
  if (rename("/sd/ovms3.bin.dl", "/sd/ovms3.bin") != 0)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Cannot rename dl file");
    remove("/sd/ovms3.bin.dl");
    return false;
    }

  ESP_LOGI(TAG, "FlashSD: Download complete (%d bytes), flashing from SD...", filesize);

  // Phase 2: Flash from SD file
  struct stat ds;
  if (stat("/sd/ovms3.bin", &ds) != 0)
    {
    ESP_LOGE(TAG, "FlashSD: Cannot stat /sd/ovms3.bin");
    ClearFlashStatus();
    return false;
    }

  f = fopen("/sd/ovms3.bin", "r");
  if (f == NULL)
    {
    ESP_LOGE(TAG, "FlashSD: Cannot open /sd/ovms3.bin for reading");
    ClearFlashStatus();
    return false;
    }
  setvbuf(f, NULL, _IOFBF, 4096);

  SetFlashStatus("OTA SD Flash: Preparing flash partition...");
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: ESP32 error #%d starting OTA", err);
    fclose(f);
    return false;
    }

  char* fbuf = (char*)malloc(OTA_BUF_SIZE);
  if (fbuf == NULL)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "FlashSD: Cannot allocate flash read buffer");
    esp_ota_end(otah);
    fclose(f);
    return false;
    }

  SetFlashStatus("OTA SD Flash: Flashing from SD card...");
  size_t done = 0;
  while (size_t n = fread(fbuf, 1, OTA_BUF_SIZE, f))
    {
    err = esp_ota_write(otah, fbuf, n);
    if (err != ESP_OK)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "FlashSD: ESP32 error #%d writing flash - state is inconsistent", err);
      esp_ota_end(otah);
      free(fbuf);
      fclose(f);
      return false;
      }
    done += n;
    SetFlashPerc(50 + (done*50)/ds.st_size);
    }
  free(fbuf);
  fclose(f);

  SetFlashStatus("OTA SD Flash: Finalising flash partition...");
  err = esp_ota_end(otah);
  ClearFlashStatus();
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashSD: ESP32 error #%d finalising - state is inconsistent", err);
    return false;
    }

  ESP_LOGI(TAG, "FlashSD: Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "FlashSD: ESP32 error #%d setting boot partition", err);
    return false;
    }

  char done_path[40];
  snprintf(done_path, sizeof(done_path), "/sd/ovms3.%s.done", target->label);
  remove(done_path);
  rename("/sd/ovms3.bin", done_path);

  ESP_LOGI(TAG, "FlashSD: Success - flashed %d bytes from %s to %s", (int)ds.st_size, url.c_str(), target->label);
  MyConfig.SetParamValue("ota", "http.mru", url);
  return true;
  }
#endif // CONFIG_OVMS_COMP_SDCARD

static void OTAFlashTask(void *pvParameters)
  {
  ota_flash_params_t* params = (ota_flash_params_t*)pvParameters;
  ota_flashcfg_t cfg = params->cfg;
  std::string url = params->url;
  bool reboot = params->reboot;
  delete params;

  bool success = false;

  switch (cfg)
    {
    case OTA_FlashCfg_Default:
      ESP_LOGI(TAG, "OTAFlashTask: Auto OTA update");
      success = MyOTA.AutoFlash(false);
      break;
    case OTA_FlashCfg_Force:
      ESP_LOGI(TAG, "OTAFlashTask: Forced OTA update");
      success = MyOTA.AutoFlash(true);
      break;
    case OTA_FlashCfg_FromSD:
      ESP_LOGI(TAG, "OTAFlashTask: Flash from SD card");
#ifdef CONFIG_OVMS_COMP_SDCARD
      success = MyOTA.AutoFlashSD();
#else
      ESP_LOGE(TAG, "OTAFlashTask: SD card support not compiled in");
#endif
      break;
    case OTA_FlashCfg_HTTP:
      ESP_LOGI(TAG, "OTAFlashTask: HTTP flash from %s", url.c_str());
      success = MyOTA.FlashHTTP(url);
      break;
    case OTA_FlashCfg_SD:
      ESP_LOGI(TAG, "OTAFlashTask: SD download+flash from %s", url.c_str());
#ifdef CONFIG_OVMS_COMP_SDCARD
      success = MyOTA.FlashSD(url);
#else
      ESP_LOGE(TAG, "OTAFlashTask: SD card support not compiled in");
#endif
      break;
    case OTA_FlashCfg_VFS:
      ESP_LOGI(TAG, "OTAFlashTask: VFS flash from %s", url.c_str());
      success = MyOTA.FlashVFS(url);
      break;
    }

  if (success)
    {
    if (reboot)
      {
      MyNotify.NotifyString("info", "ota.update", "OTA flash complete, rebooting to activate");
      ESP_LOGI(TAG, "OTAFlashTask: Complete. Requesting restart...");
      // Flash has completed. We now need to reboot
      vTaskDelay(pdMS_TO_TICKS(5000));
      MyBoot.SetFirmwareUpdate();
      MyBoot.Restart();
      }
    else
      {
      MyNotify.NotifyString("info", "ota.update", "OTA flash complete (reboot disabled, use 'module reset' to activate)");
      ESP_LOGI(TAG, "OTAFlashTask: Flash complete (reboot disabled, use 'module reset' to activate)");
      MyOTA.SetFlashStatus("Flash complete (reboot disabled)");
      }
    }

  MyOTA.m_autotask = NULL;
  vTaskDelete(NULL);
  }

void OvmsOTA::LaunchOTAFlash(ota_flashcfg_t cfg /*=OTA_FlashCfg_Default*/, const std::string& url /*=""*/, bool reboot /*=false*/)
  {
  if (m_autotask != NULL)
    {
    ESP_LOGW(TAG, "LaunchOTAFlash: Task already running (cannot launch new)");
    return;
    }

  ota_flash_params_t* params = new ota_flash_params_t;
  params->cfg = cfg;
  params->url = url;
  params->reboot = reboot;

  xTaskCreatePinnedToCore(OTAFlashTask, "OVMS OTA Flash",
    6144, (void*)params, 5, &m_autotask, CORE(1));
  }

bool OvmsOTA::AutoFlash(bool force)
  {
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  if (running==NULL)
    {
    ESP_LOGW(TAG, "AutoFlash: Current running image cannot be determined - aborting");
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  if (target==NULL)
    {
    ESP_LOGW(TAG, "AutoFlash: Target partition cannot be determined - aborting");
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  if (running == target)
    {
    ESP_LOGW(TAG, "AutoFlash: Cannot flash to running image partition");
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  ota_info info;
  MyOTA.GetStatus(info, true);

  if (info.version_server.length() == 0)
    {
    ESP_LOGW(TAG,"AutoFlash: Server version could not be determined");
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  if ((!force) && (buildverscmp(info.version_server,info.version_firmware) <= 0))
    {
    ESP_LOGI(TAG,"AutoFlash: No new firmware available (%s is current)",info.version_server.c_str());
    return false;
    }

  if ((!MyNetManager.m_connected_wifi) && (!MyConfig.GetParamValueBool("ota", "auto.allow.modem", false)))
    {
    ESP_LOGW(TAG, "AutoFlash: Cannot flash without wifi connection, or auto.allow.modem");
    if (info.version_server.compare(m_lastnotifyversion) != 0)
      {
      MyNotify.NotifyStringf("info", "ota.update", "New firmware %s is available. Connect to WIFI to update", info.version_server.c_str());
      m_lastnotifyversion = info.version_server;
      }
    return false;
    }

  OvmsMutexLock m_lock(&m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "AutoFlash: Flash operation already in progress - cannot auto flash");
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  std::string tag = MyConfig.GetParamValue("ota","tag");
  std::string url = MyConfig.GetParamValue("ota","server");
  if (url.empty())
    url = "api.openvehicles.com/firmware/ota";

  url.append("/");
  url.append(GetOVMSProduct());
  url.append("/");

  if (tag.empty())
    url.append(CONFIG_OVMS_VERSION_TAG);
  else
    url.append(tag);
  url.append("/ovms3.bin");

  ESP_LOGI(TAG, "AutoFlash: Update %s to %s (%s)",
    target->label,
    info.version_server.c_str(),
    url.c_str());
  MyNotify.NotifyStringf("info", "ota.update", "New OTA firmware %s is now being downloaded", info.version_server.c_str());

  // HTTP client request...
  OvmsHttpClient http(url);
  if (!http.IsOpen())
    {
    ESP_LOGE(TAG, "AutoFlash: http://%s request failed", url.c_str());
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  size_t expected = http.BodySize();
  if (expected < 32)
    {
    ESP_LOGE(TAG, "AutoFlash: Expected download file size (%d) is invalid", expected);
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  // Try SD-buffered download if configured (auto.buffer)
#ifdef CONFIG_OVMS_COMP_SDCARD
  if (MyConfig.GetParamValueBool("ota", "sd.buffer", false)
      && path_exists("/sd") && CheckSDFreeSpace(expected, NULL))
    {
    ESP_LOGI(TAG, "AutoFlash: Buffering download to SD card (auto.buffer)...");
    SetFlashStatus("OTA Auto Flash: Downloading to SD card...",0,true);
    remove("/sd/ovms3.bin.dl");
    FILE *f = fopen("/sd/ovms3.bin.dl", "w");
    if (f != NULL)
      {
      setvbuf(f, NULL, _IOFBF, 4096);
      uint8_t* rbuf = (uint8_t*)malloc(OTA_BUF_SIZE);
      if (rbuf == NULL)
        {
        ESP_LOGE(TAG, "AutoFlash: Cannot allocate download buffer");
        fclose(f);
        remove("/sd/ovms3.bin.dl");
        ClearFlashStatus();
        m_lastcheckday = -1;
        return false;
        }
      size_t filesize = 0;
      ssize_t k;
      bool dl_ok = true;
      while ((k = http.BodyRead(rbuf,OTA_BUF_SIZE)) > 0)
        {
        filesize += k;
        SetFlashPerc((filesize*50)/expected);
        if (fwrite(rbuf, 1, k, f) != (size_t)k)
          {
          ESP_LOGW(TAG, "AutoFlash: SD write error at %d bytes", filesize);
          dl_ok = false;
          break;
          }
        }
      free(rbuf);
      fclose(f);
      http.Disconnect();

      if (dl_ok && k >= 0 && filesize == expected)
        {
        remove("/sd/ovms3.bin");
        if (rename("/sd/ovms3.bin.dl", "/sd/ovms3.bin") == 0)
          {
          ESP_LOGI(TAG, "AutoFlash: SD download complete (%d bytes), flashing from SD...", filesize);

          // Flash from SD file (mutex already held)
          struct stat ds;
          if (stat("/sd/ovms3.bin", &ds) != 0)
            {
            ESP_LOGE(TAG, "AutoFlash: Cannot stat /sd/ovms3.bin");
            ClearFlashStatus();
            m_lastcheckday = -1;
            return false;
            }

          f = fopen("/sd/ovms3.bin", "r");
          if (f == NULL)
            {
            ESP_LOGE(TAG, "AutoFlash: Cannot open /sd/ovms3.bin for reading");
            ClearFlashStatus();
            m_lastcheckday = -1;
            return false;
            }
          setvbuf(f, NULL, _IOFBF, 4096);

          SetFlashStatus("OTA Auto Flash: Preparing flash partition...");
          esp_ota_handle_t otah;
          esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
          if (err != ESP_OK)
            {
            ClearFlashStatus();
            ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d starting OTA operation", err);
            fclose(f);
            return false;
            }

          char* fbuf = (char*)malloc(OTA_BUF_SIZE);
          if (fbuf == NULL)
            {
            ClearFlashStatus();
            ESP_LOGE(TAG, "AutoFlash: Cannot allocate flash read buffer");
            esp_ota_end(otah);
            fclose(f);
            return false;
            }

          SetFlashStatus("OTA Auto Flash: Flashing from SD card...");
          size_t done = 0;
          while (size_t n = fread(fbuf, 1, OTA_BUF_SIZE, f))
            {
            err = esp_ota_write(otah, fbuf, n);
            if (err != ESP_OK)
              {
              ClearFlashStatus();
              ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d writing flash - state is inconsistent", err);
              esp_ota_end(otah);
              free(fbuf);
              fclose(f);
              return false;
              }
            done += n;
            SetFlashPerc(50 + (done*50)/ds.st_size);
            }
          free(fbuf);
          fclose(f);

          SetFlashStatus("OTA Auto Flash: Finalising flash partition...");
          err = esp_ota_end(otah);
          ClearFlashStatus();
          if (err != ESP_OK)
            {
            ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d finalising OTA - state is inconsistent", err);
            return false;
            }

          ESP_LOGI(TAG, "AutoFlash: Setting boot partition...");
          err = esp_ota_set_boot_partition(target);
          if (err != ESP_OK)
            {
            ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d setting boot partition", err);
            return false;
            }

          char done_path[40];
          snprintf(done_path, sizeof(done_path), "/sd/ovms3.%s.done", target->label);
          remove(done_path);
          rename("/sd/ovms3.bin", done_path);

          ESP_LOGI(TAG, "AutoFlash: SD-buffered flash of %d bytes from %s to %s", (int)ds.st_size, url.c_str(), target->label);
          MyConfig.SetParamValue("ota", "http.mru", url);
          return true;
          }
        }

      // SD download or rename failed
      ESP_LOGW(TAG, "AutoFlash: SD buffer failed (%d/%d bytes), will retry next cycle",
               filesize, expected);
      remove("/sd/ovms3.bin.dl");
      ClearFlashStatus();
      m_lastcheckday = -1;
      return false;
      }
    else
      {
      // Cannot create temp file on SD, fall through to direct flash
      ESP_LOGW(TAG, "AutoFlash: Cannot create SD temp file, using direct flash");
      }
    }
#endif // CONFIG_OVMS_COMP_SDCARD

  SetFlashStatus("OTA Auto Flash: Preparing flash partition...",0,true);
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, expected, &otah);
  if (err != ESP_OK)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d when starting OTA operation", err);
    http.Disconnect();
    return false;
    }

  // Now, process the body
  SetFlashStatus("OTA Auto Flash: Downloading OTA image...");
  uint8_t rbuf[512];
  size_t filesize = 0;
  ssize_t k;
  while ((k = http.BodyRead(rbuf,512)) > 0)
    {
    filesize += k;
    SetFlashPerc((filesize*100)/expected);
    if (filesize > target->size)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "AutoFlash: Download firmware is bigger than available partition space - state is inconsistent");
      esp_ota_end(otah);
      http.Disconnect();
      return false;
      }
    err = esp_ota_write(otah, rbuf, k);
    if (err != ESP_OK)
      {
      ClearFlashStatus();
      ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d when writing to flash - state is inconsistent", err);
      esp_ota_end(otah);
      http.Disconnect();
      return false;
      }
    }
  http.Disconnect();
  ESP_LOGI(TAG, "AutoFlash: Download %s (at %d bytes)", (k<0) ? "failed" : "complete", filesize);

  if (filesize != expected)
    {
    ClearFlashStatus();
    ESP_LOGE(TAG, "AutoFlash: Download file size (%d) does not match expected (%d)", filesize, expected);
    esp_ota_end(otah);
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  SetFlashStatus("OTA Auto Flash: Finalising flash partition...");
  err = esp_ota_end(otah);
  ClearFlashStatus();
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d finalising OTA operation - state is inconsistent", err);
    return false;
    }

  // All done
  ESP_LOGI(TAG, "AutoFlash: Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d setting boot partition - check before rebooting", err);
    return false;
    }

  ESP_LOGI(TAG, "AutoFlash: Success flash of %d bytes from %s", expected, url.c_str());
  MyConfig.SetParamValue("ota", "http.mru", url);

  return true;
  }
