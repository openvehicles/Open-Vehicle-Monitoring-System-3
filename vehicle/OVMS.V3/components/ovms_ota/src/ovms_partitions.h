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

#ifndef __OVMS_PARTITIONS_H__
#define __OVMS_PARTITIONS_H__

#include <string>
#include <esp_partition.h>
#include "ovms_command.h"

#define PARTITION_TABLE_SIZE 0x0C00   // Maximum size of the partition table in flash (32 bytes per partition record)
#define PARTITION_TABLE_BLOCK_SIZE  0x1000   // Size allocated for the partition table in flash
#define PARTITION_TABLE_RECORD_SIZE 32       // Size of the partition table record (32 bytes)
#define PARTITION_ENTRY_MAGIC 0x50aa
#define PARTITION_MD5_MAGIC 0xebeb

typedef struct __attribute__((packed))   // Partition table normal record (30 bytes)
  {
  uint8_t type;                       // Partition type (1 byte in flash)
  uint8_t subtype;                    // Partition subtype (1 byte in flash)
  uint32_t address;                   // Starting address of the partition in flash
  uint32_t size;                      // Size of the partition in flash (specified in bytes)
  char     label[16];                 // Partition label (should be zero termninated, but not if 16 characters in size)
  uint32_t flags;                     // Flags
  } ovms_esp_partition_entry_t;

typedef struct __attribute__((packed))   // Partition table MD5 checksum record (30 bytes)
  {
  uint8_t allffs[14];                 // 14 bytes of 0xff padding
  uint8_t digest[16];                 // MD5 checksum of the partition table
  } ovms_esp_partition_md5_t;

typedef struct __attribute__((packed))   // Partition table record (32 bytes)
  {
  uint16_t magic_id;                  // Magic ID (0x50aa for partition entry, 0xebeb for MD5 checksum terminator)
  union
    {
    ovms_esp_partition_entry_t entry;
    ovms_esp_partition_md5_t md5;
    } data;
  } ovms_esp_partition_t;

// Flash partition version
typedef enum
  {
  OVMS_FlashPartition_Unknown,    // Unknown partition format (or not yet discovered)
  OVMS_FlashPartition_f12,        // Original v3 partition format (factory, ota1, ota2)
  OVMS_FlashPartition_12,         // New partition format (ota1, ota2, no factory)
  } ovms_flashpartition_t;

extern const char* ovms_partition_type_name(esp_partition_type_t type);
extern const char* ovms_partition_subtype_name(esp_partition_type_t type, esp_partition_subtype_t subtype);

extern size_t ovms_partition_table_find(void);
extern ovms_esp_partition_t* ovms_partition_table_read(size_t offset, bool useinternalram = false);
extern void ovms_partition_table_free(ovms_esp_partition_t* table);

extern ovms_flashpartition_t ovms_partition_table_get_type(void);
extern std::string ovms_partition_table_get_type_string(ovms_flashpartition_t type);
extern bool ovms_partition_table_has_factory(void);

extern bool ovms_partition_table_list(OvmsWriter* writer);
extern bool ovms_partition_table_upgrade(OvmsWriter* writer);

#endif //#ifndef __OVMS_PARTITIONS_H__
