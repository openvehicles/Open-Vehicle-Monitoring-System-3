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
static const char *TAG = "version";

#include <esp_image_format.h>
#include <esp_ota_ops.h>
#include <esp_system.h>
#include "ovms.h"
#include "ovms_version.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_events.h"


std::string GetOVMSPartitionVersion(esp_partition_subtype_t t)
  {
  int i, len;
  size_t offset;
  size_t n, size;
  char *cp, *cp2, *buf2;
  std::string version;
  const esp_partition_t *p;
  esp_image_header_t *pfhdr;
  esp_image_segment_header_t *header;
  char buf[512 + 1];
  const char prefix[] = OVMS_VERSION_PREFIX;
  const char postfix[] = OVMS_VERSION_POSTFIX;

  /* Find the partition detail */
  p = esp_partition_find_first(ESP_PARTITION_TYPE_APP, t, NULL);
  if (p == NULL)
      return "";

  /* Read image header */
  pfhdr = (esp_image_header_t *)buf;
  offset = 0;
  if (esp_partition_read(p, offset, pfhdr, sizeof(*pfhdr)) != ESP_OK)
      return "";
  if (pfhdr->magic != ESP_IMAGE_HEADER_MAGIC)
      return "";
  if (pfhdr->segment_count > ESP_IMAGE_MAX_SEGMENTS)
      return "";

  /* Read the first segment header */
  header = (esp_image_segment_header_t *)buf;
  offset += sizeof(*pfhdr);
  if (esp_partition_read(p, offset, header, sizeof(*header)) != ESP_OK)
      return "";

  /* Search for the version string */
  offset += sizeof(*header);
  len = header->data_len;
  size = sizeof(buf) - 1;
  buf2 = buf;
  while (len > 0) {
    n = size;
    if (n > len)
      n = len;
    if (esp_partition_read(p, offset, buf2, n) != ESP_OK)
      return "";
    offset += n;

    /* Insure EOS */
    buf2[n] = '\0';
    len -= n;

    n = sizeof(buf) - (sizeof(prefix) + sizeof(postfix) - 2);
    for (i = 0, cp = buf; i < n; ++i, ++cp) {
      if (*cp != prefix[0])
        continue;
      if (strncmp(cp, prefix, sizeof(prefix) - 1) != 0)
	continue;
      cp2 = strstr(cp, postfix);
      if (cp2 == NULL)
	continue;
      *cp2 = '\0';
      cp += sizeof(prefix) - 1;
      *cp2 = '\0';
      version.assign(cp);
      return version;
    }

    /* Shift the buffer left to avoid splitting the string */
    n = (sizeof(buf) - 1) / 2;
    buf2 = buf + n;
    memcpy(buf, buf2, n);
    size = n;
  }
  return "";
  }

std::string GetOVMSVersion()
  {
  std::string searchversion(OVMS_VERSION_PREFIX OVMS_VERSION OVMS_VERSION_POSTFIX);
  std::string version(OVMS_VERSION);
  std::string tag = MyConfig.GetParamValue("ota","tag");

  const esp_partition_t *p = esp_ota_get_running_partition();
  if (p != NULL)
    {
    version.append("/");
    version.append(p->label);
    }

  version.append("/");
  if (tag.empty())
    version.append(CONFIG_OVMS_VERSION_TAG);
  else
    version.append(tag);

  return version;
  }

std::string GetOVMSBuild()
  {
  std::string build("idf ");
  build.append(esp_get_idf_version());
  build.append(" ");
  build.append(__DATE__);
  build.append(" ");
  build.append(__TIME__);

  return build;
  }

std::string GetOVMSHardware()
  {
  std::string hardware("OVMS ");
  esp_chip_info_t chip;
  esp_chip_info(&chip);
  if (chip.features & CHIP_FEATURE_EMB_FLASH) hardware.append("EMBFLASH ");
  if (chip.features & CHIP_FEATURE_WIFI_BGN) hardware.append("WIFI ");
  if (chip.features & CHIP_FEATURE_BLE) hardware.append("BLE ");
  if (chip.features & CHIP_FEATURE_BT) hardware.append("BT ");
  char buf[32]; sprintf(buf,"cores=%d ",chip.cores); hardware.append(buf);
  sprintf(buf,"rev=ESP32/%d",chip.revision); hardware.append(buf);

  return hardware;
  }

void Version(std::string event, void* data)
  {
//  char buf[20];
//  uint8_t mac[6];

  std::string metric = GetOVMSVersion();
  metric.append(" (build ");
  metric.append(GetOVMSBuild());
  metric.append(")");
  StandardMetrics.ms_m_version->SetValue(metric.c_str());

  metric = GetOVMSHardware();
  StandardMetrics.ms_m_hardware->SetValue(metric.c_str());

//  esp_efuse_mac_get_default(mac);
//  sprintf(buf,"%02x:%02x:%02x:%02x:%02x:%02x",
//          mac[0],mac[1],mac[2],mac[3],mac[4],mac[5]);
//  StandardMetrics.ms_m_serial->SetValue(buf);
  }

class VersionInit
  {
  public: VersionInit();
} MyVersionInit  __attribute__ ((init_priority (9900)));

VersionInit::VersionInit()
  {
  ESP_LOGI(TAG, "Initialising Versioning (9900)");

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.start", std::bind(&Version, _1, _2));
  }
