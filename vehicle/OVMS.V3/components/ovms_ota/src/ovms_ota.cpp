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
static const char *TAG = "ota";

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <string>
#include <string.h>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include "strverscmp.h"
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

OvmsOTA MyOTA __attribute__ ((init_priority (4400)));

////////////////////////////////////////////////////////////////////////////////
// OvmsOTAWriter
//
// Used to write OTA firmware to flash

class OvmsOTAWriter : public OvmsSyncHttpClient
  {
  public:
    OvmsOTAWriter(const esp_partition_t* target,
                  OvmsWriter* writer);
    ~OvmsOTAWriter();

  public:
    virtual void ConnectionBodyStart(struct mg_connection *nc);
    virtual void ConnectionBodyData(struct mg_connection *nc, uint8_t* data, size_t len);
    virtual void ConnectionBodyFinish(struct mg_connection *nc);

  public:
    const esp_partition_t* m_target;
    OvmsWriter* m_writer;
    esp_ota_handle_t m_otah;
    size_t m_sofar;
    size_t m_filesize;
  };

OvmsOTAWriter::OvmsOTAWriter(const esp_partition_t* target,
              OvmsWriter* writer)
  : OvmsSyncHttpClient(false) // Don't buffer body
  {
  m_target = target;
  m_writer = writer;
  m_sofar = 0;
  m_filesize = 0;
  }

OvmsOTAWriter::~OvmsOTAWriter()
  {
  }

void OvmsOTAWriter::ConnectionBodyStart(struct mg_connection *nc)
  {
  size_t expected = m_bodysize;

  if (expected < 32)
    {
    if (m_writer)
      m_writer->printf("Error: Invalid expected file size %d\n",expected);
    else
      ESP_LOGE(TAG,"Error: Invalid expected file size %d",expected);
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_error = std::string("Invalid expected file size for firmware");
    if (m_waitcompletion != NULL) xSemaphoreGive(m_waitcompletion);
    return;
    }
  else
    {
    if (m_writer)
      m_writer->printf("Expected file size is %d\n",expected);
    else
      ESP_LOGD(TAG, "Expected file size is %d",expected);
    }

  if (m_writer)
    m_writer->puts("Preparing flash partition...");
  else
    ESP_LOGD(TAG, "Preparing flash partition...");

  esp_err_t err = esp_ota_begin(m_target, expected, &m_otah);
  if (err != ESP_OK)
    {
    if (m_writer)
      m_writer->printf("Error: ESP32 error #%d when starting OTA operation\n",err);
    else
      ESP_LOGE(TAG,"Error: ESP32 error #%d when starting OTA operation",err);
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_error = std::string("ESP32 error when starting OTA operation");
    if (m_waitcompletion != NULL) xSemaphoreGive(m_waitcompletion);
    return;
    }
  }

void OvmsOTAWriter::ConnectionBodyData(struct mg_connection *nc, uint8_t* data, size_t len)
  {
  m_filesize += len;
  m_sofar += len;
  if (m_sofar > 100000)
    {
    if (m_writer)
      m_writer->printf("Downloading... (%d bytes so far)\n",m_filesize);
    else
      ESP_LOGD(TAG, "Downloading... (%d bytes so far)",m_filesize);
    m_sofar = 0;
    }

  if (m_filesize > m_target->size)
    {
    if (m_writer)
      m_writer->printf("Error: Download firmware is bigger than available partition space - state is inconsistent\n");
    else
      ESP_LOGE(TAG, "Download firmware is bigger than available partition space - state is inconsistent");
    esp_ota_end(m_otah);
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_error = std::string("Download firmware is bigger than available partition space - state is inconsistent");
    if (m_waitcompletion != NULL) xSemaphoreGive(m_waitcompletion);
    return;
    }

  esp_err_t err = esp_ota_write(m_otah, data, len);
  if (err != ESP_OK)
    {
    if (m_writer)
      m_writer->printf("Error: ESP32 error #%d when writing to flash - state is inconsistent\n",err);
    else
      ESP_LOGE(TAG, "ESP32 error #%d when writing to flash - state is inconsistent",err);
    esp_ota_end(m_otah);
    nc->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_error = std::string("ESP32 error #%d when writing to flash - state is inconsistent");
    if (m_waitcompletion != NULL) xSemaphoreGive(m_waitcompletion);
    return;
    }
  }

void OvmsOTAWriter::ConnectionBodyFinish(struct mg_connection *nc)
  {
  if (m_writer)
    m_writer->printf("Download complete (at %d bytes)\n",m_filesize);
  else
    ESP_LOGD(TAG, "Download complete (at %d bytes)",m_filesize);

  if (m_filesize != m_bodysize)
    {
    if (m_writer)
      m_writer->printf("Error: Download file size (%d) does not match expected (%d)\n",m_filesize,m_bodysize);
    else
      ESP_LOGE(TAG, "Download file size (%d) does not match expected (%d)",m_filesize,m_bodysize);
    m_error = std::string("Download file size does not match expected");
    esp_ota_end(m_otah);
    return;
    }

  esp_err_t err = esp_ota_end(m_otah);
  if (err != ESP_OK)
    {
    if (m_writer)
      m_writer->printf("Error: ESP32 error #%d finalising OTA operation - state is inconsistent\n",err);
    else
      ESP_LOGE(TAG, "ESP32 error #%d finalising OTA operation - state is inconsistent",err);
    m_error = std::string("ESP32 error finalising OTA operation - state is inconsistent");
    return;
    }
  }

////////////////////////////////////////////////////////////////////////////////
// Utility functions

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

  if (info.version_firmware != "")
    len += writer->printf("Firmware:          %s\n", info.version_firmware.c_str());
  if (info.partition_running != "")
    len += writer->printf("Running partition: %s\n", info.partition_running.c_str());
  if (info.partition_boot != "")
    len += writer->printf("Boot partition:    %s\n", info.partition_boot.c_str());
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
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    writer->puts("Error: Flash operation already in progress - cannot flash again");
    return;
    }

  if (running==NULL)
    {
    writer->puts("Error: Current running image cannot be determined - aborting");
    return;
    }
  writer->printf("Current running partition is: %s\n",running->label);

  if (target==NULL)
    {
    writer->puts("Error: Target partition cannot be determined - aborting");
    return;
    }
  writer->printf("Target partition is: %s\n",target->label);

  if (running == target)
    {
    writer->puts("Error: Cannot flash to running image partition");
    return;
    }

  if (MyConfig.ProtectedPath(argv[0]))
    {
    writer->puts("Error: protected path");
    return;
    }

  struct stat ds;
  if (stat(argv[0], &ds) != 0)
    {
    writer->printf("Error: Cannot find file %s\n",argv[0]);
    return;
    }
  writer->printf("Source image is %d bytes in size\n",ds.st_size);

  FILE* f = fopen(argv[0], "r");
  if (f == NULL)
    {
    writer->printf("Error: Cannot open %s\n",argv[0]);
    return;
    }

  writer->puts("Preparing flash partition...");
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
  if (err != ESP_OK)
    {
    writer->printf("Error: ESP32 error #%d when starting OTA operation\n",err);
    fclose(f);
    return;
    }

  writer->puts("Flashing image partition...");
  char buf[512];
  while(size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    err = esp_ota_write(otah, buf, n);
    if (err != ESP_OK)
      {
      writer->printf("Error: ESP32 error #%d when writing to flash - state is inconsistent\n",err);
      esp_ota_end(otah);
      fclose(f);
      return;
      }
    }
  fclose(f);

  err = esp_ota_end(otah);
  if (err != ESP_OK)
    {
    writer->printf("Error: ESP32 error #%d finalising OTA operation - state is inconsistent\n",err);
    return;
    }

  fclose(f);

  writer->puts("Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    writer->printf("Error: ESP32 error #%d setting boot partition - check before rebooting\n",err);
    return;
    }

  writer->printf("OTA flash was successful\n  Flashed %d bytes from %s\n  Next boot will be from '%s'\n",
                 ds.st_size,argv[0],target->label);
  MyConfig.SetParamValue("ota", "vfs.mru", argv[0]);
  }

void ota_flash_http(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string url;
  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    writer->puts("Error: Flash operation already in progress - cannot flash again");
    return;
    }

  if (running==NULL)
    {
    writer->puts("Error: Current running image cannot be determined - aborting");
    return;
    }
  writer->printf("Current running partition is: %s\n",running->label);

  if (target==NULL)
    {
    writer->puts("Error: Target partition cannot be determined - aborting");
    return;
    }
  writer->printf("Target partition is: %s\n",target->label);

  if (running == target)
    {
    writer->puts("Error: Cannot flash to running image partition");
    return;
    }

  // URL
  if (argc == 0)
    {
    // Automatically build the URL based on firmware
    std::string tag = MyConfig.GetParamValue("ota","tag");
    url = MyConfig.GetParamValue("ota","server");
    if (url.empty())
      url = "api.openvehicles.com/firmware/ota";
#ifdef CONFIG_OVMS_HW_BASE_3_0
    url.append("/v3.0/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_0
#ifdef CONFIG_OVMS_HW_BASE_3_1
    url.append("/v3.1/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_1
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
  writer->printf("Download firmware from %s to %s\n",url.c_str(),target->label);

  // Download and flash...
  OvmsOTAWriter http(target, writer);
  if (!http.Request(url))
    {
    writer->printf("Error: Request failed: %s\n",http.GetError().c_str());
    }

  if (http.HasError())
    {
    return;
    }

  // All done
  writer->puts("Setting boot partition...");
  esp_err_t err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    writer->printf("Error: ESP32 error #%d setting boot partition - check before rebooting\n",err);
    return;
    }

  writer->printf("OTA flash was successful\n  Flashed %d bytes from %s\n  Next boot will be from '%s'\n",
                 http.GetBodySize(),url.c_str(),target->label);
  MyConfig.SetParamValue("ota", "http.mru", url);
  }

void ota_flash_auto(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  bool force = (strcmp(cmd->GetName(), "force")==0);

  writer->puts("Triggering automatic firmware update...");
  MyOTA.LaunchAutoFlash(force);
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
        writer->printf("Boot from %s at 0x%08x (size 0x%08x)\n",p->label,p->address,p->size);
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

////////////////////////////////////////////////////////////////////////////////
// OvmsOTA
//
// The main OTA functionality

#ifdef CONFIG_OVMS_COMP_SDCARD
void OvmsOTA::AutoFlashSD(std::string event, void* data)
  {
  FILE* f = fopen("/sd/ovms3.bin", "r");
  if (f == NULL) return;

  const esp_partition_t *running = esp_ota_get_running_partition();
  const esp_partition_t *target = esp_ota_get_next_update_partition(running);

  OvmsMutexLock m_lock(&MyOTA.m_flashing,0);
  if (!m_lock.IsLocked())
    {
    ESP_LOGW(TAG, "AutoFlashSD: Flash operation already in progress - cannot auto flash");
    fclose(f);
    return;
    }

  if (running==NULL)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Current running image cannot be determined - aborting");
    fclose(f);
    return;
    }
  ESP_LOGW(TAG, "AutoFlashSD Current running partition is: %s",running->label);

  if (target==NULL)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Target partition cannot be determined - aborting");
    fclose(f);
    return;
    }
  ESP_LOGW(TAG, "AutoFlashSD Target partition is: %s",target->label);

  if (running == target)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Cannot flash to running image partition");
    fclose(f);
    return;
    }

  struct stat ds;
  if (stat("/sd/ovms3.bin", &ds) != 0)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: Cannot stat file");
    fclose(f);
    return;
    }
  ESP_LOGW(TAG, "AutoFlashSD Source image is %d bytes in size",(int)ds.st_size);

  ESP_LOGW(TAG, "AutoFlashSD Preparing flash partition...");
  esp_ota_handle_t otah;
  esp_err_t err = esp_ota_begin(target, ds.st_size, &otah);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d when starting OTA operation",err);
    fclose(f);
    return;
    }

  ESP_LOGW(TAG, "AutoFlashSD Flashing image partition...");
  char buf[512];
  while(size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    err = esp_ota_write(otah, buf, n);
    if (err != ESP_OK)
      {
      ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d when writing to flash - state is inconsistent",err);
      esp_ota_end(otah);
      fclose(f);
      return;
      }
    }

  fclose(f);

  err = esp_ota_end(otah);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d finalising OTA operation - state is inconsistent",err);
    return;
    }

  ESP_LOGW(TAG, "AutoFlashSD Setting boot partition...");
  err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ESP32 error #%d setting boot partition - check before rebooting",err);
    return;
    }

  remove("/sd/ovms3.done"); // Ensure the target is removed first
  if (rename("/sd/ovms3.bin","/sd/ovms3.done") != 0)
    {
    ESP_LOGE(TAG, "AutoFlashSD Error: ovms3.bin could not be renamed to ovms3.done - check before rebooting");
    return;
    }

  ESP_LOGW(TAG, "AutoFlashSD OTA flash successful: Flashed %d bytes, and booting from '%s'",
                 (int)ds.st_size,target->label);

  vTaskDelay(2000 / portTICK_PERIOD_MS); // Delay for log display and settle
  ESP_LOGW(TAG, "AutoFlashSD restarting...");
  MyBoot.Restart();
  MyBoot.SetFirmwareUpdate();
  }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

OvmsOTA::OvmsOTA()
  {
  ESP_LOGI(TAG, "Initialising OTA (4400)");

  m_autotask = NULL;
  m_lastcheckday = -1;

  MyConfig.RegisterParam("ota", "OTA setup and status", true, true);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"ticker.600", std::bind(&OvmsOTA::Ticker600, this, _1, _2));

#ifdef CONFIG_OVMS_COMP_SDCARD
  MyEvents.RegisterEvent(TAG,"sd.mounted", std::bind(&OvmsOTA::AutoFlashSD, this, _1, _2));
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  OvmsCommand* cmd_ota = MyCommandApp.RegisterCommand("ota","OTA framework", ota_status, "", 0, 0, false);

  OvmsCommand* cmd_otastatus = cmd_ota->RegisterCommand("status","Show OTA status",ota_status,"[nocheck]",0,1);
  cmd_otastatus->RegisterCommand("nocheck","…skip check for available update",ota_status);

  OvmsCommand* cmd_otaflash = cmd_ota->RegisterCommand("flash","OTA flash");
  cmd_otaflash->RegisterCommand("vfs","OTA flash vfs",ota_flash_vfs,"<file>",1,1);
  cmd_otaflash->RegisterCommand("http","OTA flash http",ota_flash_http,"[<url>]",0,1);
  OvmsCommand* cmd_otaflash_auto = cmd_otaflash->RegisterCommand("auto","Automatic regular OTA flash (over web)",ota_flash_auto,"[force]",0,1);
  cmd_otaflash_auto->RegisterCommand("force","…force update (even if server version older)",ota_flash_auto);

  OvmsCommand* cmd_otaboot = cmd_ota->RegisterCommand("boot","OTA boot");
  cmd_otaboot->RegisterCommand("factory","Boot from factory image",ota_boot);
  cmd_otaboot->RegisterCommand("ota_0","Boot from ota_0 image",ota_boot);
  cmd_otaboot->RegisterCommand("ota_1","Boot from ota_1 image",ota_boot);
  }

OvmsOTA::~OvmsOTA()
  {
  }

void OvmsOTA::GetStatus(ota_info& info, bool check_update /*=true*/)
  {
  info.version_firmware = "";
  info.version_server = "";
  info.update_available = false;
  info.partition_running = "";
  info.partition_boot = "";
  info.changelog_server = "";

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
#ifdef CONFIG_OVMS_HW_BASE_3_0
      url.append("/v3.0/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_0
#ifdef CONFIG_OVMS_HW_BASE_3_1
      url.append("/v3.1/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_1
      if (tag.empty())
        url.append(CONFIG_OVMS_VERSION_TAG);
      else
        url.append(tag);
      url.append("/ovms3.ver");

      OvmsSyncHttpClient http;
      if (!http.Request(url))
        {
        ESP_LOGE(TAG, "HTTP request for ota status failed: %s", http.GetError().c_str());
        return;
        }
      OvmsBuffer *result = http.GetBodyAsBuffer();
      if (result->HasLine())
        {
        info.version_server = result->ReadLine();
        uint8_t rbuf[512];
        while (size_t k = result->Pop(512,rbuf))
          {
          info.changelog_server.append((char*)rbuf,k);
          }
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
  struct tm* tmu = localtime(&rawtime);

  if ((tmu->tm_hour == MyConfig.GetParamValueInt("ota","auto.hour",2)) &&
      (tmu->tm_mday != m_lastcheckday))
    {
    m_lastcheckday = tmu->tm_mday;  // So we only try once a day (unless cleared due to a temporary fault)
    LaunchAutoFlash();
    }
  }

static void OTAFlashTask(void *pvParameters)
  {
  bool force = (bool)pvParameters;

  ESP_LOGD(TAG, "AutoFlash: Tasks is running%s",(force)?" forced":"");

  bool result = MyOTA.AutoFlash(force);

  if (result)
    {
    // Flash has completed. We now need to reboot
    vTaskDelay(pdMS_TO_TICKS(5000));

    // All done. Let's restart...
    ESP_LOGI(TAG, "AutoFlash: Complete. Requesting restart...");
    MyBoot.Restart();
    MyBoot.SetFirmwareUpdate();
    }

  MyOTA.m_autotask = NULL;
  vTaskDelete(NULL);
  }

void OvmsOTA::LaunchAutoFlash(bool force)
  {
  if (m_autotask != NULL)
    {
    ESP_LOGW(TAG, "AutoFlash: Task already running (cannot launch new)");
    return;
    }

  xTaskCreatePinnedToCore(OTAFlashTask, "OVMS AutoFlash",
    6144, (void*)force, 5, &m_autotask, CORE(1));
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
#ifdef CONFIG_OVMS_HW_BASE_3_0
  url.append("/v3.0/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_0
#ifdef CONFIG_OVMS_HW_BASE_3_1
  url.append("/v3.1/");
#endif //#ifdef CONFIG_OVMS_HW_BASE_3_1
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

  // Download and flash...
  OvmsOTAWriter http(target, NULL);
  if (!http.Request(url))
    {
    ESP_LOGE(TAG, "AutoFlash: HTTP Request failed: %s", http.GetError().c_str());
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  if (http.HasError())
    {
    m_lastcheckday = -1; // Allow to try again within the same day
    return false;
    }

  // All done
  ESP_LOGI(TAG, "AutoFlash: Setting boot partition...");
  esp_err_t err = esp_ota_set_boot_partition(target);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "AutoFlash: ESP32 error #%d setting boot partition - check before rebooting", err);
    return false;
    }

  ESP_LOGI(TAG, "AutoFlash: Success flash of %d bytes from %s", http.GetBodySize(), url.c_str());
  MyNotify.NotifyStringf("info", "ota.update", "OTA firmware %s has been updated (OVMS will restart)", info.version_server.c_str());
  MyConfig.SetParamValue("ota", "http.mru", url);

  return true;
  }
