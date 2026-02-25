/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd February 2026
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
#include <cinttypes>
#include <esp_system.h>
#include <esp_ota_ops.h>
#include <esp_spi_flash.h>
#include <string.h>
#include "ovms_partitions.h"
#include "ovms_malloc.h"
#include "crypt_md5.h"

static const char *TAG = "ovms-partitions";

size_t partition_table_address = 0;
ovms_flashpartition_t partition_table_type = OVMS_FlashPartition_Unknown;

const char* ovms_partition_type_name(esp_partition_type_t type)
  {
  switch (type)
    {
    case ESP_PARTITION_TYPE_APP:   return "app";
    case ESP_PARTITION_TYPE_DATA:  return "data";
    }

  static char unknown_type[12];
  snprintf(unknown_type, sizeof(unknown_type), "0x%02x", type);
  return unknown_type;
  }

const char* ovms_partition_subtype_name(esp_partition_type_t type, esp_partition_subtype_t subtype)
  {
  if (type == ESP_PARTITION_TYPE_APP)
    {
    switch (subtype)
      {
      case ESP_PARTITION_SUBTYPE_APP_FACTORY:   return "factory";
      case ESP_PARTITION_SUBTYPE_APP_OTA_0:     return "ota_0";
      case ESP_PARTITION_SUBTYPE_APP_OTA_1:     return "ota_1";
      case ESP_PARTITION_SUBTYPE_APP_OTA_2:     return "ota_2";
      case ESP_PARTITION_SUBTYPE_APP_OTA_3:     return "ota_3";
      case ESP_PARTITION_SUBTYPE_APP_OTA_4:     return "ota_4";
      case ESP_PARTITION_SUBTYPE_APP_OTA_5:     return "ota_5";
      case ESP_PARTITION_SUBTYPE_APP_OTA_6:     return "ota_6";
      case ESP_PARTITION_SUBTYPE_APP_OTA_7:     return "ota_7";
      case ESP_PARTITION_SUBTYPE_APP_OTA_8:     return "ota_8";
      case ESP_PARTITION_SUBTYPE_APP_OTA_9:     return "ota_9";
      case ESP_PARTITION_SUBTYPE_APP_OTA_10:    return "ota_10";
      case ESP_PARTITION_SUBTYPE_APP_OTA_11:    return "ota_11";
      case ESP_PARTITION_SUBTYPE_APP_OTA_12:    return "ota_12";
      case ESP_PARTITION_SUBTYPE_APP_OTA_13:    return "ota_13";
      case ESP_PARTITION_SUBTYPE_APP_OTA_14:    return "ota_14";
      case ESP_PARTITION_SUBTYPE_APP_OTA_15:    return "ota_15";
      case ESP_PARTITION_SUBTYPE_APP_TEST:      return "test";
      default: break;
      }
    }
  else if (type == ESP_PARTITION_TYPE_DATA)
    {
    switch (subtype)
      {
      case ESP_PARTITION_SUBTYPE_DATA_OTA:      return "ota";
      case ESP_PARTITION_SUBTYPE_DATA_PHY:      return "phy";
      case ESP_PARTITION_SUBTYPE_DATA_NVS:      return "nvs";
      case ESP_PARTITION_SUBTYPE_DATA_COREDUMP: return "coredump";
      case ESP_PARTITION_SUBTYPE_DATA_NVS_KEYS: return "nvs_keys";
      case ESP_PARTITION_SUBTYPE_DATA_EFUSE_EM: return "efuse_em";
      case ESP_PARTITION_SUBTYPE_DATA_ESPHTTPD: return "esphttpd";
      case ESP_PARTITION_SUBTYPE_DATA_FAT:      return "fat";
      case ESP_PARTITION_SUBTYPE_DATA_SPIFFS:   return "spiffs";
      default: break;
      }
    }

  static char unknown_subtype[12];
  snprintf(unknown_subtype, sizeof(unknown_subtype), "0x%02x", subtype);
  return unknown_subtype;
  }

size_t ovms_partition_table_find(void)
  {
  if (partition_table_address != 0) return partition_table_address;

  // Some sanity checks...
  if (sizeof(ovms_esp_partition_t) != PARTITION_TABLE_RECORD_SIZE)
    {
    ESP_LOGE(TAG, "Invalid partition table record size (%d expecting %d)", sizeof(ovms_esp_partition_t), PARTITION_TABLE_RECORD_SIZE);
    return 0;
    }

  ovms_esp_partition_t buffer;
  for (size_t addr = 0x8000; addr < 0xA000; addr += 0x1000)
    {
    esp_err_t err = spi_flash_read(addr, (uint8_t*)&buffer, sizeof(buffer));
    ESP_LOGD(TAG, "Checking partition table at 0x%04x found magic 0x%04x", addr, (uint16_t)buffer.magic_id);
    if (err != ESP_OK) continue;
    if (buffer.magic_id == PARTITION_ENTRY_MAGIC)
      {
      partition_table_address = addr;
      return addr;
      }
    }
  return 0;
  }

ovms_esp_partition_t* ovms_partition_table_read(size_t offset, bool useinternalram)
  {
  if (offset == 0)
    {
    ESP_LOGE(TAG, "Invalid offset for partition table");
    return NULL;
    }

  char *buffer = useinternalram
    ? (char *)InternalRamMalloc(PARTITION_TABLE_BLOCK_SIZE)
    : (char *)ExternalRamMalloc(PARTITION_TABLE_BLOCK_SIZE);
  if (buffer == NULL)
    {
    ESP_LOGE(TAG, "Failed to allocate memory for partition table");
    return NULL;
     }
  esp_err_t err =
    spi_flash_read(offset, buffer, PARTITION_TABLE_BLOCK_SIZE);
  if (err != ESP_OK)
    {
    ESP_LOGE(TAG, "Failed to read partition table: 0x%x", err);
    free(buffer);
    return NULL;
    }

  return (ovms_esp_partition_t*)buffer;
  }

void ovms_partition_table_free(ovms_esp_partition_t* table)
  {
  free(table);
  }

ovms_flashpartition_t ovms_partition_table_get_type(void)
  {
  if (partition_table_type != OVMS_FlashPartition_Unknown)
    return partition_table_type;

  ovms_esp_partition_t *table = ovms_partition_table_read(ovms_partition_table_find());
  if (table == NULL)
    {
    ESP_LOGE(TAG, "Failed to read partition table");
    return OVMS_FlashPartition_Unknown;
    }

  bool has_factory = false;
  bool has_ota1 = false;
  bool has_ota2 = false;

  for (ovms_esp_partition_t *p = table; p->magic_id == PARTITION_ENTRY_MAGIC; p++)
    {
    ESP_LOGD(TAG, "Checking partition at %p magic 0x%04x", (void*)p, (uint16_t)p->magic_id);
    if (p->magic_id == PARTITION_ENTRY_MAGIC)
      {
      if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
        {
        has_factory = true;
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)
        {
        has_ota1 = true;
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1) 
        {
        has_ota2 = true;
        }
      }
    }
  ovms_partition_table_free(table);

  if (has_factory && has_ota1 && has_ota2)
    partition_table_type = OVMS_FlashPartition_f12;
  else if (has_ota1 && has_ota2 && !has_factory)
    partition_table_type = OVMS_FlashPartition_12;
  else
    partition_table_type = OVMS_FlashPartition_Unknown;

  return partition_table_type;
  }

std::string ovms_partition_table_get_type_string(ovms_flashpartition_t type)
  {
  switch (type)
    {
    case OVMS_FlashPartition_Unknown: return "unknown";
    case OVMS_FlashPartition_f12:     return "v3-f12 (factory, ota1, ota2)";
    case OVMS_FlashPartition_12:      return "v3-12 (ota1, ota2, no factory)";
    default:                          return "unknown";
    }
  }
  
bool ovms_partition_table_has_factory(void)
  {
  if (ovms_partition_table_get_type() == OVMS_FlashPartition_f12)
    return true;
  return false;
  }

const char* ovms_partition_size_str(uint32_t size)
  {
  static char buf[16];
  const uint32_t KB = 1024u;
  const uint32_t MB = 1024u * 1024u;
  if ((size % MB) == 0)
    {
    snprintf(buf, sizeof(buf), "%" PRIu32 " MB", size / MB);
    return buf;
    }
  if ((size % KB) == 0)
    {
    snprintf(buf, sizeof(buf), "%" PRIu32 " KB", size / KB);
    return buf;
    }
  snprintf(buf, sizeof(buf), "0x%08" PRIx32, size);
  return buf;
  }

bool ovms_partition_table_list(OvmsWriter* writer)
  {
  // Note that we are going to use our own functions for this, as the ESP-IDF v3.3
  // has only very rudimentary partition table support, and in particular no way of simply iterating
  // through the partitions in the partition table (no support for ESP_PARTITION_TYPE_ANY).

  ovms_esp_partition_t *table = ovms_partition_table_read(ovms_partition_table_find());
  if (table == NULL)
    {
    writer->puts("Error: Failed to read partition table");
    return false;
    }

  writer->puts("Partition table:");
  writer->printf("%-16s %4s %-12s %10s %-10.10s\n",
    "Label", "Type", "Subtype", "Address", "Size");

  OVMS_MD5_CTX md5_ctx;
  OVMS_MD5_Init(&md5_ctx);
  ovms_esp_partition_t *p = table;
  for (; p->magic_id == PARTITION_ENTRY_MAGIC; p++)
    {
    ESP_LOGD(TAG, "Checking partition at %p magic 0x%04x", (void*)p, (uint16_t)p->magic_id);
    if (p->magic_id == PARTITION_ENTRY_MAGIC)
      {
      OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
      ovms_esp_partition_entry_t *entry = &p->data.entry;

      writer->printf("%-16s %-4.4s %-12.12s 0x%08" PRIx32 " %s\n",
        entry->label,
        ovms_partition_type_name((esp_partition_type_t)entry->type),
        ovms_partition_subtype_name((esp_partition_type_t)entry->type, (esp_partition_subtype_t)entry->subtype),
        (uint32_t)entry->address,
        ovms_partition_size_str((uint32_t)entry->size));
      }
    }
  ovms_partition_table_free(table);
  if (p->magic_id == PARTITION_MD5_MAGIC)
    {
    uint8_t digest[OVMS_MD5_SIZE];
    OVMS_MD5_Final(digest, &md5_ctx);
    writer->printf("Digest:          %02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x%02x %s\n",
      p->data.md5.digest[0], p->data.md5.digest[1], p->data.md5.digest[2], p->data.md5.digest[3],
      p->data.md5.digest[4], p->data.md5.digest[5], p->data.md5.digest[6], p->data.md5.digest[7],
      p->data.md5.digest[8], p->data.md5.digest[9], p->data.md5.digest[10], p->data.md5.digest[11],
      p->data.md5.digest[12], p->data.md5.digest[13], p->data.md5.digest[14], p->data.md5.digest[15],
      (memcmp(&digest, &p->data.md5.digest, OVMS_MD5_SIZE) == 0) ? "pass" : "FAIL");
    }

  return true;
  }

bool ovms_partition_table_upgrade(OvmsWriter* writer)
  {
  size_t offset = ovms_partition_table_find();
  if (offset == 0)
    {
    writer->puts("Error: Failed to find partition table");
    return false;
    }

  ovms_esp_partition_t *table = ovms_partition_table_read(offset, true); // N.B. Use internal RAM for this operation
  if (table == NULL)
    {
    writer->puts("Error: Failed to read partition table");
    return false;
    }

  uint32_t factory_offset = 0;
  uint32_t ota1_offset = 0;
  ovms_esp_partition_t *p = table;
  OVMS_MD5_CTX md5_ctx;
  OVMS_MD5_Init(&md5_ctx);
  for (; p->magic_id == PARTITION_ENTRY_MAGIC; p++)
    {
    if (p->magic_id == PARTITION_ENTRY_MAGIC)
      {
      if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_FACTORY)
        {
        // We have found the factory partition, so we need to change it to OTA 0
        factory_offset = p->data.entry.address;
        p->data.entry.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_0;
        p->data.entry.size = 0x600000;
        memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
        strncpy(p->data.entry.label, "ota_0", sizeof(p->data.entry.label));
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Converted factory partition to 6MB OTA 0\n", factory_offset);
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)
        {
        // We have found the OTA 0 partition, so we need to change it to OTA 1
        ota1_offset = factory_offset + 0x600000;
        p->data.entry.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        p->data.entry.address = ota1_offset;
        p->data.entry.size = 0x600000;
        memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
        strncpy(p->data.entry.label, "ota_1", sizeof(p->data.entry.label));
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Converted OTA 0 partition to 6MB OTA 1\n", ota1_offset);
        }
      else if ((factory_offset != 0) && (ota1_offset != 0))
        {
        // We need to move the next partition to this one
        memcpy((void*)p, (void*)&p[1], sizeof(ovms_esp_partition_t));
        if (p->magic_id == PARTITION_MD5_MAGIC)
          {
          // That was a MD5, so recalculate it...`
          OVMS_MD5_Final(p->data.md5.digest, &md5_ctx);
          writer->puts("           Recalculated MD5 checksum");
          }
        else if (p->magic_id == PARTITION_ENTRY_MAGIC)
          {
          OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
          writer->printf("0x%08" PRIx32 " Moved %s/%s partition up one position\n",
            p->data.entry.address,
            ovms_partition_type_name((esp_partition_type_t)p->data.entry.type),
            ovms_partition_subtype_name((esp_partition_type_t)p->data.entry.type, (esp_partition_subtype_t)p->data.entry.subtype));
          }
        }
      else
        {
        // Just update the MD5 checksum
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Skipping over %s/%s partition\n",
          p->data.entry.address,
          ovms_partition_type_name((esp_partition_type_t)p->data.entry.type),
          ovms_partition_subtype_name((esp_partition_type_t)p->data.entry.type, (esp_partition_subtype_t)p->data.entry.subtype));
        }
      }
    }
  if ((p->magic_id == PARTITION_MD5_MAGIC) &&
      (factory_offset != 0) &&
      (ota1_offset != 0))
    {
    writer->puts("           Clearing trailing old MD5 checksum record");
    memset((void*)p, 0xff, sizeof(ovms_esp_partition_t));
    }

  if ((factory_offset != 0)&&(ota1_offset != 0))
    {
    // Now ok to re-write partition table to flash
    // Erase: address and size must be multiples of SPI_FLASH_SEC_SIZE (4KB)
    writer->printf("Erasing old partition table (%d bytes at 0x%08zx)...\n", (int)PARTITION_TABLE_BLOCK_SIZE, offset);
    esp_err_t err = spi_flash_erase_range(offset, PARTITION_TABLE_BLOCK_SIZE);
    if (err != ESP_OK)
      {
      writer->printf("Error: Failed to erase old partition table: %s\n", esp_err_to_name(err));
      ovms_partition_table_free(table);
      return false;
      }
    writer->printf("Writing new partition table (%d bytes at 0x%08zx)...\n", (int)PARTITION_TABLE_BLOCK_SIZE, offset);
    err = spi_flash_write(offset, table, PARTITION_TABLE_BLOCK_SIZE);
    if (err != ESP_OK)
      {
      writer->printf("Error: Failed to write new partition table: %s\n", esp_err_to_name(err));
      ovms_partition_table_free(table);
      return false;
      }
    ovms_partition_table_free(table);
    writer->puts("Partition table upgraded successfully - reboot required");
    partition_table_type = OVMS_FlashPartition_12;
    return true;
    }
  else
    {
    ovms_partition_table_free(table);
    writer->puts("Error: Failed to upgrade partition table");
    return false;
    }
  }
