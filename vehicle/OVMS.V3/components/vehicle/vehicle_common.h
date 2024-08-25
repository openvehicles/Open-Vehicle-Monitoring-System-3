/*
;    Project:       Open Vehicle Monitor System
;    Date:          19th March 2024

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
#ifndef __VEHICLE_COMMON_H__
#define __VEHICLE_COMMON_H__

// ISOTP Defines needed by the Poller as well as the CanOpen code.
// Required if polling is not enabled in the configuration.

// ISO TP:
//  (see https://en.wikipedia.org/wiki/ISO_15765-2)

#define ISOTP_FT_SINGLE                 0
#define ISOTP_FT_FIRST                  1
#define ISOTP_FT_CONSECUTIVE            2
#define ISOTP_FT_FLOWCTRL               3

// Protocol variant:
#define ISOTP_STD                       0     // standard addressing (11 bit IDs)
#define ISOTP_EXTADR                    1     // extended addressing (19 bit IDs)
#define ISOTP_EXTFRAME                  2     // extended frame mode (29 bit IDs)
#define VWTP_16                         16    // VW/VAG Transport Protocol 1.6 (placeholder, unsupported)
#define VWTP_20                         20    // VW/VAG Transport Protocol 2.0

// Argument tag:
#define POLL_TXDATA                     0xff  // poll_pid_t using xargs for external payload up to 4095 bytes

// OBD (ISO 15031) service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDIICURRENT    0x01 // Mode 01 "current data" (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIFREEZE     0x02 // Mode 02 "freeze frame data" (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_ERDTC      0x03 // Mode 03 read emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_CLEAR_ERDTC     0x04 // Mode 04 clear/reset emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_READOXSTEST     0x05 // Mode 05 read oxygen sensor monitoring test results (16 bit PID)
#define VEHICLE_POLL_TYPE_READOBMTEST     0x06 // Mode 06 read on-board monitoring test results (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_DCERDTC    0x07 // Mode 07 read driving cycle emission-related DTC (no PID)
#define VEHICLE_POLL_TYPE_REQOBUCTRL      0x08 // Mode 08 request on-board unit control (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIVEHICLE    0x09 // Mode 09 "vehicle information" (8 bit PID)
#define VEHICLE_POLL_TYPE_READ_PERMDTC    0x0A // Mode 0A read permanent (cleared) DTC (no PID)

// UDS (ISO 14229) service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDIISESSION    0x10 // UDS: Diagnostic Session Control (8 bit PID)
#define VEHICLE_POLL_TYPE_TESTERPRESENT   0x3E // UDS: TesterPresent (8 bit PID)
#define VEHICLE_POLL_TYPE_SECACCESS       0x27 // UDS: SecurityAccess (8 bit PID)
#define VEHICLE_POLL_TYPE_COMCONTROL      0x28 // UDS: CommunicationControl (8 bit PID)
#define VEHICLE_POLL_TYPE_ECURESET        0x11 // UDS: ECUReset (8 bit PID)
#define VEHICLE_POLL_TYPE_CLEARDTC        0x14 // UDS: ClearDiagnosticInformation (no PID)
#define VEHICLE_POLL_TYPE_READDTC         0x19 // UDS: ReadDTCInformation (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIEXTENDED   0x22 // UDS: ReadDataByIdentifier (16 bit PID) (legacy alias for READDATA)
#define VEHICLE_POLL_TYPE_READDATA        0x22 // UDS: ReadDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_READMEMORY      0x23 // UDS: ReadMemoryByAddress (no PID)
#define VEHICLE_POLL_TYPE_READSCALING     0x24 // UDS: ReadScalingDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_WRITEDATA       0x2E // UDS: WriteDataByIdentifier (16 bit PID)
#define VEHICLE_POLL_TYPE_ROUTINECONTROL  0x31 // UDS: Routine Control (8 bit PID)
#define VEHICLE_POLL_TYPE_WRITEMEMORY     0x3D // UDS: WriteMemoryByAddress (8 bit PID)
#define VEHICLE_POLL_TYPE_IOCONTROL       0x2F // UDS: InputOutputControlByIdentifier (16 bit PID)

// Other service identifiers supported:
#define VEHICLE_POLL_TYPE_OBDII_18        0x18 // Custom: VW request type 18 (no PID)
#define VEHICLE_POLL_TYPE_OBDII_1A        0x1A // Custom: Mode 1A (8 bit PID)
#define VEHICLE_POLL_TYPE_OBDIIGROUP      0x21 // Custom: Read data by 8 bit PID
#define VEHICLE_POLL_TYPE_OBDII_32        0x32 // Custom: VW routine control extension (8 bit PID)
#define VEHICLE_POLL_TYPE_ZOMBIE_VCU_16   0x2A // Custom: ZombieVerterVCU (16 bit PID)

#define VEHICLE_OBD_BROADCAST_MODULE_TX   0x7df
#define VEHICLE_OBD_BROADCAST_MODULE_RX   0x0

#endif
