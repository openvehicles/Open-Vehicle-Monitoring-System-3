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
#include "esp_vfs_fat.h"
#include "wear_levelling.h"
#include <string.h>
#include "ovms_partitions.h"
#include "ovms_malloc.h"
#include "crypt_md5.h"
#include "ovms_peripherals.h"
#include <list>
#include <dirent.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <limits.h>

static const char *TAG = "ovms-partitions";

size_t partition_table_address = 0;
ovms_flashpartition_t partition_table_type = OVMS_FlashPartition_Unknown;

esp_vfs_fat_mount_config_t ovms_store2_fat;
wl_handle_t ovms_store2_wlh;
bool ovms_store2_mounted = false;

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
  bool has_minstore = false;
  bool has_maxstore = false;

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
      else if (p->data.entry.type == ESP_PARTITION_TYPE_DATA && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_DATA_FAT && p->data.entry.address == 0xc10000 && p->data.entry.size == 0x100000)
        {
        has_minstore = true;
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_DATA && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_DATA_FAT && p->data.entry.address == 0xe10000 && p->data.entry.size == 0x1f0000)
        {
        has_maxstore = true;
        }
      }
    }
  ovms_partition_table_free(table);

  if (has_factory && has_ota1 && has_ota2 && !has_maxstore)
    partition_table_type = OVMS_FlashPartition_30;
  else if (has_factory && has_ota1 && has_ota2 && has_minstore && has_maxstore)
    partition_table_type = OVMS_FlashPartition_34;
  else if (has_ota1 && has_ota2 && !has_factory && has_maxstore)
    partition_table_type = OVMS_FlashPartition_35;
  else
    partition_table_type = OVMS_FlashPartition_Unknown;

  return partition_table_type;
  }

std::string ovms_partition_table_get_type_string(ovms_flashpartition_t type)
  {
  switch (type)
    {
    case OVMS_FlashPartition_Unknown: return "unknown";
    case OVMS_FlashPartition_30:      return "v3-30 (factory, ota1, ota2, 1MB store)";
    case OVMS_FlashPartition_34:      return "v3-34 (factory, ota1, ota2, dual store)";
    case OVMS_FlashPartition_35:      return "v3-35 (ota1, ota2, no factory, maximized store)";
    default:                          return "unknown";
    }
  }
  
bool ovms_partition_table_has_factory(void)
  {
  ovms_flashpartition_t type = ovms_partition_table_get_type();
  if ((type == OVMS_FlashPartition_30) || (type == OVMS_FlashPartition_34))
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

bool ovms_partition_table_upgrade_store(OvmsWriter* writer)
  {
  if (ovms_partition_table_get_type() != OVMS_FlashPartition_30)
    {
    writer->puts("Error: Cannot upgrade store - partition table is not in v3-30 format");
    return false;
    }

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

  ovms_esp_partition_t *p = table;
  OVMS_MD5_CTX md5_ctx;
  OVMS_MD5_Init(&md5_ctx);
  for (; p->magic_id == PARTITION_ENTRY_MAGIC; p++)
    {
    // Just update the MD5 checksum
    OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
    writer->printf("0x%08" PRIx32 " Skipping over %s/%s partition\n",
      p->data.entry.address,
      ovms_partition_type_name((esp_partition_type_t)p->data.entry.type),
      ovms_partition_subtype_name((esp_partition_type_t)p->data.entry.type, (esp_partition_subtype_t)p->data.entry.subtype));
    }      

  if (p->magic_id == PARTITION_MD5_MAGIC)
    {
    // We need to add our new store partition to the end of the partition table
    p->magic_id = PARTITION_ENTRY_MAGIC;
    p->data.entry.type = ESP_PARTITION_TYPE_DATA;
    p->data.entry.subtype = ESP_PARTITION_SUBTYPE_DATA_FAT;
    p->data.entry.address = 0xe10000;
    p->data.entry.size = 0x1f0000;
    p->data.entry.flags = 0;
    memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
    strncpy(p->data.entry.label, "store2", sizeof(p->data.entry.label));
    OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
    writer->puts("Added new store2 partition to partition table");

    // Then we need to add the MD5 checksum record
    p++;
    ovms_partition_table_void_entry(p);
    p->magic_id = PARTITION_MD5_MAGIC;
    OVMS_MD5_Final(p->data.md5.digest, &md5_ctx);
    writer->puts("Recalculated MD5 checksum");

    // Now re-write the partition table to flash
    if (ovms_partition_table_rewrite(table, writer))
      {
      writer->puts("Partition table upgraded successfully - reboot required");
      partition_table_type = OVMS_FlashPartition_34;
      }
    ovms_partition_table_free(table);

    // All done
    return true;
    }
  else
    {
    ovms_partition_table_free(table);
    writer->puts("Error: Failed to upgrade store (could not find MD5 checksum record)");
    return false;
    }
  }

bool ovms_partition_table_downgrade_store(OvmsWriter* writer)
  {
  if (ovms_partition_table_get_type() != OVMS_FlashPartition_34)
    {
    writer->puts("Error: Cannot downgrade store - partition table is not in v3-34 format");
    return false;
    }

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

  ovms_esp_partition_t *p = table;
  OVMS_MD5_CTX md5_ctx;
  OVMS_MD5_Init(&md5_ctx);
  for (; p->magic_id == PARTITION_ENTRY_MAGIC; p++)
    {
    if (p->magic_id == PARTITION_ENTRY_MAGIC &&
        p->data.entry.type == ESP_PARTITION_TYPE_DATA && 
        p->data.entry.subtype == ESP_PARTITION_SUBTYPE_DATA_FAT &&
        p->data.entry.address == 0xe10000 &&
        p->data.entry.size == 0x1f0000)
      {
      // We have found the store2 partition
      writer->puts("Removing store2 partition from partition table");
      memset((void*)p, 0, sizeof(ovms_esp_partition_t));
      p->magic_id = PARTITION_MD5_MAGIC;
      OVMS_MD5_Final(p->data.md5.digest, &md5_ctx);
      p++; ovms_partition_table_void_entry(p); // Clear the following redundant MD5 checksum record
      if (ovms_partition_table_rewrite(table, writer))  
        {
        writer->puts("Store partition downgraded successfully - reboot required");
        partition_table_type = OVMS_FlashPartition_30;
        return true;
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

  writer->puts("Error: Failed to downgrade store (could not find store2 partition)");
  ovms_partition_table_free(table);
  return false;
  }

bool ovms_partition_table_mount_store2(OvmsWriter* writer)
  {
  if (ovms_partition_table_get_type() != OVMS_FlashPartition_34)
    {
    writer->puts("Error: Cannot mount store - partition table is not in v3-34 format");
    return false;
    }

  if (ovms_store2_mounted)
    {
    writer->puts("Error: /store2 is already mounted");
    return false;
    }
  #ifdef CONFIG_OVMS_COMP_SDCARD
  if ((MyPeripherals->m_sdcard != NULL) && (MyPeripherals->m_sdcard->ismounted()))
    {
    writer->puts("Error: Please unmount SD CARD before migrating store");
    return false;
    }
  #endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  memset(&ovms_store2_fat,0,sizeof(ovms_store2_fat));
  ovms_store2_fat.format_if_mount_failed = true;
  ovms_store2_fat.max_files = 5;

  #if ESP_IDF_VERSION_MAJOR >= 5
  ovms_store2_mounted = (esp_vfs_fat_spiflash_mount_rw_wl("/store2", "store2", &ovms_store2_fat, &ovms_store2_wlh) == ESP_OK);
  #else
  ovms_store2_mounted = (esp_vfs_fat_spiflash_mount("/store2", "store2", &ovms_store2_fat, &ovms_store2_wlh) == ESP_OK);
  #endif

  if (ovms_store2_mounted)
    {
    writer->puts("Mounted /store2");
    return true;
    }
  else
    {
    writer->puts("Error: Failed to mount /store2");
    return false;
    }
  }

bool ovms_partition_table_unmount_store2(OvmsWriter* writer)
  {
  if (!ovms_store2_mounted)
    {
    writer->puts("Error: /store2 is not mounted");
    return false;
    }

  #if ESP_IDF_VERSION_MAJOR >= 5
  ovms_store2_mounted = (esp_vfs_fat_spiflash_unmount_rw_wl("/store2", ovms_store2_wlh) != ESP_OK);
  #else
  ovms_store2_mounted = (esp_vfs_fat_spiflash_unmount("/store2", ovms_store2_wlh) != ESP_OK);
  #endif

  if (!ovms_store2_mounted)
    {
    writer->puts("Unmounted /store2");
    return true;
    }
  else
    {
    writer->puts("Error: Failed to unmount /store2");
    return false;
    }
  }

// The source and destination paths for the store copy operation
static const char kCopyStoreFrom[] = "/store";
static const char kCopyStoreTo[]   = "/store2";

// Map a source path to the corresponding destination path
static bool copy_store_map_dst(const std::string& src, std::string& dst_out, OvmsWriter* writer)
  {
  const size_t from_len = sizeof(kCopyStoreFrom) - 1;
  if (src.size() < from_len || src.compare(0, from_len, kCopyStoreFrom) != 0)
    {
    writer->printf("Error: path not under %s: %s\n", kCopyStoreFrom, src.c_str());
    return false;
    }
  if (src.size() == from_len)
    {
    dst_out = kCopyStoreTo;
    return true;
    }
  if (src[from_len] != '/')
    {
    writer->printf("Error: invalid source path: %s\n", src.c_str());
    return false;
    }

  dst_out = std::string(kCopyStoreTo) + src.substr(from_len);
  return true;
  }

// Ensure a directory exists at the destination path
static bool copy_store_ensure_dir(const std::string& dst, OvmsWriter* writer, unsigned& errors)
  {
  struct stat st;
  if (stat(dst.c_str(), &st) == 0)
    {
    if (S_ISDIR(st.st_mode)) return true;
    writer->printf("Error: exists but is not a directory: %s\n", dst.c_str());
    errors++;
    return false;
    }
  if (errno != ENOENT)
    {
    writer->printf("Error: stat %s: %s\n", dst.c_str(), strerror(errno));
    errors++;
    return false;
    }
  if (mkdir(dst.c_str(), 0755) != 0)
    {
    if (errno == EEXIST)
      {
      if (stat(dst.c_str(), &st) == 0 && S_ISDIR(st.st_mode)) return true;
      }
    writer->printf("Error: mkdir %s: %s\n", dst.c_str(), strerror(errno));
    errors++;
    return false;
    }

  writer->printf("Create directory: %s\n", dst.c_str());
  return true;
  }

// Copy a single file from the source to the destination
static void copy_store_one_file(OvmsWriter* writer, const std::string& src, const std::string& dst,
  uint8_t* buf, size_t buf_size, unsigned& files_copied, unsigned& errors)
  {
  struct stat st_src;
  if (stat(src.c_str(), &st_src) != 0)
    {
    writer->printf("Error: stat %s: %s\n", src.c_str(), strerror(errno));
    errors++;
    return;
    }
  if (!S_ISREG(st_src.st_mode))
    {
    writer->printf("Error: not a regular file: %s\n", src.c_str());
    errors++;
    return;
    }

  struct stat st_dst;
  if (stat(dst.c_str(), &st_dst) == 0)
    {
    if (S_ISREG(st_dst.st_mode)) return;
    writer->printf("Error: destination exists but is not a file: %s\n", dst.c_str());
    errors++;
    return;
    }
  if (errno != ENOENT)
    {
    writer->printf("Error: stat %s: %s\n", dst.c_str(), strerror(errno));
    errors++;
    return;
    }

  int fd_in = open(src.c_str(), O_RDONLY);
  if (fd_in < 0)
    {
    writer->printf("Error: open %s for read: %s\n", src.c_str(), strerror(errno));
    errors++;
    return;
    }

  int fd_out = open(dst.c_str(), O_WRONLY | O_CREAT | O_TRUNC, 0644);
  if (fd_out < 0)
    {
    writer->printf("Error: open %s for write: %s\n", dst.c_str(), strerror(errno));
    errors++;
    close(fd_in);
    return;
    }

  writer->printf("Copy file: %s -> %s\n", src.c_str(), dst.c_str());

  bool io_ok = true;
  for (;;)
    {
    ssize_t nr = read(fd_in, buf, buf_size);
    if (nr < 0)
      {
      writer->printf("Error: read %s: %s\n", src.c_str(), strerror(errno));
      io_ok = false;
      break;
      }
    if (nr == 0) break;
    size_t off = 0;
    while (off < (size_t)nr)
      {
      ssize_t nw = write(fd_out, buf + off, (size_t)nr - off);
      if (nw < 0)
        {
        writer->printf("Error: write %s: %s\n", dst.c_str(), strerror(errno));
        io_ok = false;
        break;
        }
      if (nw == 0)
        {
        writer->printf("Error: write %s returned 0\n", dst.c_str());
        io_ok = false;
        break;
        }
      off += (size_t)nw;
      }
    if (!io_ok) break;
    }

  if (close(fd_in) != 0)
    {
    writer->printf("Error: close (read) %s: %s\n", src.c_str(), strerror(errno));
    io_ok = false;
    }

  if (io_ok && fsync(fd_out) != 0)
    {
    writer->printf("Error: fsync %s: %s\n", dst.c_str(), strerror(errno));
    io_ok = false;
    }

  if (close(fd_out) != 0)
    {
    writer->printf("Error: close (write) %s: %s\n", dst.c_str(), strerror(errno));
    io_ok = false;
    }

  if (!io_ok)
    {
    if (unlink(dst.c_str()) != 0 && errno != ENOENT)
      writer->printf("Error: unlink partial %s: %s\n", dst.c_str(), strerror(errno));
    errors++;
    return;
    }

  files_copied++;
  }

// Copy the contents of the store to the store2 partition
bool ovms_partition_table_copy_store(OvmsWriter* writer)
  {
  unsigned errors = 0;
  unsigned files_copied = 0;

  if (writer == NULL) return false;

  if (!ovms_store2_mounted)
    {
    writer->puts("Error: /store2 is not mounted - mount /store2 before copying");
    return false;
    }

  struct stat st;
  if (stat(kCopyStoreFrom, &st) != 0)
    {
    writer->printf("Error: cannot stat %s: %s\n", kCopyStoreFrom, strerror(errno));
    return false;
    }
  if (!S_ISDIR(st.st_mode))
    {
    writer->printf("Error: %s is not a directory\n", kCopyStoreFrom);
    return false;
    }
  if (stat(kCopyStoreTo, &st) != 0)
    {
    writer->printf("Error: cannot stat %s: %s\n", kCopyStoreTo, strerror(errno));
    return false;
    }
  if (!S_ISDIR(st.st_mode))
    {
    writer->printf("Error: %s is not a directory\n", kCopyStoreTo);
    return false;
    }

  const size_t kBufSize = 4096;
  uint8_t* const buf = (uint8_t*)ExternalRamMalloc(kBufSize);
  if (buf == NULL)
    {
    writer->puts("Error: out of memory allocating copy buffer");
    return false;
    }

  writer->puts("Copy /store -> /store2 ...");

  std::list<std::string> dir_todo;
  dir_todo.push_back(kCopyStoreFrom);

  while (!dir_todo.empty())
    {
    const std::string dir_src = dir_todo.front();
    dir_todo.pop_front();

    std::string dir_dst;
    if (!copy_store_map_dst(dir_src, dir_dst, writer))
      {
      errors++;
      continue;
      }

    if (stat(dir_src.c_str(), &st) != 0)
      {
      writer->printf("Error: stat source directory %s: %s\n", dir_src.c_str(), strerror(errno));
      errors++;
      continue;
      }
    if (!S_ISDIR(st.st_mode))
      {
      writer->printf("Error: source path is not a directory: %s\n", dir_src.c_str());
      errors++;
      continue;
      }

    if (dir_src != kCopyStoreFrom)
      {
      if (!copy_store_ensure_dir(dir_dst, writer, errors))
        continue;
      }

    writer->printf("Scanning directory: %s\n", dir_src.c_str());

    DIR* const d = opendir(dir_src.c_str());
    if (d == NULL)
      {
      writer->printf("Error: opendir %s: %s\n", dir_src.c_str(), strerror(errno));
      errors++;
      continue;
      }

    struct dirent* de;
    while ((de = readdir(d)) != NULL)
      {
      if (de->d_name[0] == '.' && (de->d_name[1] == '\0' || (de->d_name[1] == '.' && de->d_name[2] == '\0')))
        continue;

      const std::string child_src = dir_src + "/" + de->d_name;
      if (child_src.size() >= PATH_MAX)
        {
        writer->puts("Error: path too long; aborting copy");
        errors++;
        closedir(d);
        free(buf);
        writer->printf("Copy aborted: %u files copied, %u errors\n", files_copied, errors);
        return false;
        }

      struct stat st_ent;
      if (stat(child_src.c_str(), &st_ent) != 0)
        {
        writer->printf("Error: stat %s: %s\n", child_src.c_str(), strerror(errno));
        errors++;
        continue;
        }

      std::string child_dst;
      if (!copy_store_map_dst(child_src, child_dst, writer))
        {
        errors++;
        continue;
        }

      if (S_ISDIR(st_ent.st_mode))
        {
        if (!copy_store_ensure_dir(child_dst, writer, errors)) continue;
        dir_todo.push_back(child_src);
        writer->printf("Queued directory: %s\n", child_src.c_str());
        }
      else if (S_ISREG(st_ent.st_mode))
        {
        copy_store_one_file(writer, child_src, child_dst, buf, kBufSize, files_copied, errors);
        }
      else
        {
        writer->printf("Error: unsupported inode type (mode 0%o): %s\n", (unsigned)(st_ent.st_mode & S_IFMT), child_src.c_str());
        errors++;
        }
      }

    if (closedir(d) != 0)
      {
      writer->printf("Error: closedir %s: %s\n", dir_src.c_str(), strerror(errno));
      errors++;
      }
    }

  free(buf);
  writer->printf("Copy finished: %u files copied, %u errors\n", files_copied, errors);
  return (errors == 0);
  }

bool ovms_partition_table_migrate_store(OvmsWriter* writer)
  {
  if (ovms_partition_table_get_type() != OVMS_FlashPartition_34)
    {
    writer->puts("Error: Cannot migrate store - partition table is not in v3-34 format");
    return false;
    }

  // Try to mount the store2 partition if it is not already mounted
  if (!ovms_store2_mounted)
    {
    if (! ovms_partition_table_mount_store2(writer))
      {
      writer->puts("Error: Migration failed - could not mount /store2");
      return false;
      }
    }

  // Now recursively copy the contents of /store to /store2
  if (!ovms_partition_table_copy_store(writer))
    {
    writer->puts("Error: Migration failed - could not copy /store to /store2");
    return false;
    }

  // Now unmount the store2 partition
  if (!ovms_partition_table_unmount_store2(writer))
    {
    writer->puts("Error: Migration failed - could not unmount /store2");
    return false;
    }

  writer->puts("Store migrated successfully - please check, and then proceed");
  return true;
  }

bool ovms_partition_table_upgrade_factory(OvmsWriter* writer)
  {
  if (ovms_partition_table_get_type() != OVMS_FlashPartition_34)
    {
    writer->puts("Error: Cannot upgrade factory - partition table is not in v3-34 format");
    return false;
    }

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
        p->data.entry.size = 0x700000;
        memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
        strncpy(p->data.entry.label, "ota_0", sizeof(p->data.entry.label));
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Converted factory partition to 6MB OTA 0\n", factory_offset);
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_0)
        {
        // We have found the OTA 0 partition, so we need to change it to OTA 1
        ota1_offset = factory_offset + 0x700000;
        p->data.entry.subtype = ESP_PARTITION_SUBTYPE_APP_OTA_1;
        p->data.entry.address = ota1_offset;
        p->data.entry.size = 0x700000;
        memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
        strncpy(p->data.entry.label, "ota_1", sizeof(p->data.entry.label));
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Converted OTA 0 partition to 6MB OTA 1\n", ota1_offset);
        }
      else if (p->data.entry.type == ESP_PARTITION_TYPE_APP && p->data.entry.subtype == ESP_PARTITION_SUBTYPE_APP_OTA_1)
        {
        // We have found the old OTA 1 partition, so now have
        // p[0] -> OTA_1
        // p[1] -> old store
        // p[2] -> new store
        // p[3] -> MD5 checksum
        // We need to set this as the new store, the one after as the checksum, and then clear the two following that
        memcpy((void*)p, (void*)&p[2], sizeof(ovms_esp_partition_t));
        memset((void*)p->data.entry.label, 0, sizeof(p->data.entry.label));
        strncpy(p->data.entry.label, "store", sizeof(p->data.entry.label));    
        OVMS_MD5_Update(&md5_ctx, (const uint8_t*)p, sizeof(ovms_esp_partition_t));
        writer->printf("0x%08" PRIx32 " Moved %s/%s partition up two positions\n",
          p->data.entry.address,
          ovms_partition_type_name((esp_partition_type_t)p->data.entry.type),
          ovms_partition_subtype_name((esp_partition_type_t)p->data.entry.type, (esp_partition_subtype_t)p->data.entry.subtype));
        p++;
        ovms_partition_table_void_entry(p);
        p->magic_id = PARTITION_MD5_MAGIC;
        OVMS_MD5_Final(p->data.md5.digest, &md5_ctx);
        writer->puts("           Recalculated MD5 checksum");
        ovms_partition_table_void_entry(&p[1]);
        ovms_partition_table_void_entry(&p[2]);
        if (ovms_partition_table_rewrite(table, writer))
          {
          writer->puts("Partition table upgraded successfully - reboot required");
          partition_table_type = OVMS_FlashPartition_35;
          ovms_partition_table_free(table);
          return true;
          }
        else
          {
          ovms_partition_table_free(table);
          writer->puts("Error: Failed to upgrade partition table");
          return false;
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

  ovms_partition_table_free(table);
  writer->puts("Error: Failed to upgrade partition table");
  return false;
  }

bool ovms_partition_table_rewrite(ovms_esp_partition_t* table, OvmsWriter* writer)
  {
  size_t offset = ovms_partition_table_find();
  if (offset == 0)
    {
    if (writer) writer->puts("Error: Failed to find partition table");
    return false;
    }

  if (writer) writer->printf("Erasing old partition table (%d bytes at 0x%08zx)...\n", (int)PARTITION_TABLE_BLOCK_SIZE, offset);
  esp_err_t err = spi_flash_erase_range(offset, PARTITION_TABLE_BLOCK_SIZE);
  if (err != ESP_OK)
    {
    if (writer)writer->printf("Error: Failed to erase old partition table: %s\n", esp_err_to_name(err));
    return false;
    }

  if (writer) writer->printf("Writing new partition table (%d bytes at 0x%08zx)...\n", (int)PARTITION_TABLE_BLOCK_SIZE, offset);
  err = spi_flash_write(offset, table, PARTITION_TABLE_BLOCK_SIZE);
  if (err != ESP_OK)
    {
    if (writer) writer->printf("Error: Failed to write new partition table: %s\n", esp_err_to_name(err));
    return false;
    }

  return true;
  }

void ovms_partition_table_void_entry(ovms_esp_partition_t* entry)
  {
  memset((void*)entry, 0xff, sizeof(ovms_esp_partition_t));
  }