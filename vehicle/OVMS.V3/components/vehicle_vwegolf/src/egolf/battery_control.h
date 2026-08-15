/*
;    Project:       BAP protocol library
;    Subproject:    e-Golf BatteryControl (LSG 0x25) semantics
;
;    (C) 2026  Jona Wagner <jona@jonawagner.me>
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

// Vehicle-specific layer: the Battery Control Unit (BCU) logical device,
// LSG 0x25, on the MQB comfort bus. It manages both charging AND EV climate
// preconditioning through "Battery Control Profiles" (charge locations). This
// layer sits on top of the generic bap:: transport and adds only the meaning
// of LSG 0x25's functions and payloads.
//
// Provenance is tagged per item: [WIRE] = confirmed on a real capture; [RE] =
// from reverse-engineered firmware / a third-party OCU implementation, not all
// independently capture-confirmed.

#ifndef BAP_EGOLF_BATTERY_CONTROL_H_
#define BAP_EGOLF_BATTERY_CONTROL_H_

#include <cstddef>
#include <cstdint>

#include "bap/bap.h"

namespace bap {
namespace egolf {

// 29-bit CAN ids. The LSG is embedded in the id: base(16) | lsg(8) | role(8),
// role 0x01 = ASG (display/command side), 0x10 = FSG (function ECU broadcast).
constexpr uint8_t  kLsg              = 0x25;  // [WIRE] BatteryControl
constexpr uint8_t  kSubsystemCommand = 0x01;  // [WIRE] BCU command/ASG-side low byte

// Composed from the framework CAN-id scheme, not hardcoded: change kLsg and both
// ids follow. Yields 0x17332501 (command) and 0x17332510 (status).
constexpr uint32_t kCanIdCommand = bap::mqbCanId(kLsg, kSubsystemCommand);
constexpr uint32_t kCanIdStatus  = bap::mqbCanId(kLsg, bap::kSubsystemFsg);

// LSG 0x25 function ids (the 6-bit `func` in the element header).
enum Func : uint8_t {
  FUNC_BAP_GETALL      = 0x01,  // [WIRE] BAP channel-open "GetAll" (GET "19 41" -> status "49 41");
                                // its reply is the registration-complete ack the GET path waits for
  FUNC_BAP_CONFIG      = 0x02,  // [WIRE] BAP channel-open "BapConfig" (GET "19 42" -> status "49 42",
                                // also a ~2 s periodic heartbeat) — the fallback handshake ack
  FUNC_PLUG_STATE      = 0x10,  // [WIRE] plug/connection state (status "49 50")
  FUNC_CHARGE_STATE    = 0x11,  // [WIRE] charge status (status "49 51")
  FUNC_CLIMATE_STATE   = 0x12,  // [WIRE] climate status (status "49 52")
  FUNC_TIMER_STATE     = 0x13,  // [FW+WIRE] SetBatteryControlTimerState (enable bitmask)
  FUNC_TIMER_1         = 0x14,  // [FW+WIRE] SetBatteryControlTimer, departure-timer slot 1
  FUNC_TIMER_2         = 0x15,  // [FW+WIRE] slot 2   (element header "29 55")
  FUNC_TIMER_3         = 0x16,  // [FW+WIRE] slot 3   (element header "29 56")
  FUNC_TIMER_4         = 0x17,  // [FW+WIRE] slot 4   (element header "29 57")
  FUNC_OPERATION_MODE  = 0x18,  // [WIRE] execute/stop the immediate/global profile
  FUNC_PROFILES_ARRAY  = 0x19,  // [WIRE] battery-control profile array
  FUNC_POWER_PROVIDER  = 0x1A,  // [FW+WIRE] SetBatteryControlPowerProvider (night-rate tariff)
};
// Departure timers are a 4-SLOT family: slot 1..4 => func 0x14..0x17 (headers
// "29 54".."29 57"), proven by the MIB2 packer jump table + wire. Use
// funcForTimerSlot()/timerSlotForFunc() below. (An OCU reference mislabels
// 0x14/0x15 as START_STOP_CHARGE/CLIMATE; disregard -- charge and climate are
// executed via the OperationMode 0x18 trigger, and 0x14-0x17 are the timers.)

// ProfileOperation bitfield (profile record byte [0]). [RE]
enum ProfileOperation : uint8_t {
  PO_CHARGING      = 0x01,  // charge at this location
  PO_CLIMATE       = 0x02,  // climatise (preconditioning)
  PO_ALLOW_BATTERY = 0x04,  // climatise without external supply (use HV battery)
};

// ⚠⚠ HARD SAFETY LIMIT — the BCU must NEVER be written a maxCurrent above 0x20 (32 A). A higher
// value BREAKS the car's charging and requires a dealer/factory reset to recover (confirmed on-car).
// 0x20 is the top of the allowed set; EVERY profile-write path must clamp to this ceiling, and the
// value is capped again in the arm encoder (SendArm) so even a preserved/garbled read-back can't
// exceed it. Do not raise this constant.
constexpr uint8_t kMaxCurrentHardLimit = 0x20;  // 32 A — absolute ceiling, do not exceed, ever

// Allowed BCU charge-current settings, raw amps PER PHASE (profile record byte [2]). The car's UI
// only offers 5, 10, 13, and "Max" = 0x20 (32; the hardware itself caps around 16 A) — any other
// value is rejected/overturned by the car, so snap a requested amp value to the nearest allowed one
// before writing. [ON-CAR observed; note the firmware-RE CarE_TractionProfileListTransformer set
// also listed 16, but the car does NOT expose/accept it.]
constexpr uint8_t kMaxCurrentSteps[] = {5, 10, 13, 32};
inline uint8_t clampMaxCurrent(uint16_t amps) {
  if (amps > kMaxCurrentHardLimit) amps = kMaxCurrentHardLimit;  // never snap above the hard ceiling
  uint8_t best = kMaxCurrentSteps[0];
  uint16_t bestDist = 0xFFFF;
  for (uint8_t v : kMaxCurrentSteps) {
    uint16_t dist = amps > v ? (uint16_t)(amps - v) : (uint16_t)(v - amps);
    if (dist < bestDist) { bestDist = dist; best = v; }
  }
  return best;
}

// ProfileOperation2 bitfield (profile record byte [1]). [RE]
enum ProfileOperation2 : uint8_t {
  PO2_WINDOW_HEATER_FRONT = 0x01,
  PO2_WINDOW_HEATER_REAR  = 0x02,
  PO2_PARK_HEATER         = 0x04,
  PO2_PARK_HEATER_AUTO    = 0x08,
};

// ---------------------------------------------------------------------------
// Temperature encoding
// ---------------------------------------------------------------------------
// Profile target temperature is a single byte at RA0 record offset [12].
// [FW+WIRE] confirmed by a 10-point live temperature sweep:
// 0x37=15.5, 0x4b=17.5, 0x50=18.0, 0x5f=19.5, 0x78=22.0, 0x87=23.5, 0x91=24.5,
// 0xaf=27.5, 0xb9=28.5, 0xc8=30.0 degC (range 15.5-30.0, 0.5 deg steps).
//
// Integer API (preferred on the MCU -- no floating point): temperature in
// deci-degrees C (tenths), so 22.0 degC == 220.
//     raw = deciDegC - 100   (== degC*10 - 100)   inverse: deciDegC = raw + 100
inline uint8_t tempToRawDeci(int deciDegC) {
  int r = deciDegC - 100;
  if (r < 0) r = 0;
  if (r > 255) r = 255;
  return (uint8_t)r;
}
inline int rawToDeciDegC(uint8_t raw) { return (int)raw + 100; }

// Floating-point convenience -- HOST/UI ONLY. Avoid on the CAN task/ISR:
// touching float pins the Xtensa FPU context across FreeRTOS task switches, and
// float in an ISR is unsupported. The decode paths never call these; they keep
// the raw byte and callers convert with rawToDeciDegC() where FP is free.
inline uint8_t tempToRaw(float degC) {
  return tempToRawDeci((int)(degC * 10.0f + (degC < 0 ? -0.5f : 0.5f)));
}
inline float rawToTemp(uint8_t raw) { return rawToDeciDegC(raw) / 10.0f; }

// ---------------------------------------------------------------------------
// Departure timers (func 0x14..0x17 = slot 1..4), timer-state, power provider
// ---------------------------------------------------------------------------
// The SET timer record is a fixed 8-byte body after the "29 5{4..7}" header
// (all [FW+WIRE] from the MIB2 packer FUN_00460734 + real captures):
//   [0] year   T-2000 (0x1a=2026); [0:2] all 0xFF => no next-date (recurring).
//       One-shot timers carry the fire date here; enable/disable is func 0x13.
//   [1] month  (1-12)
//   [2] day    (1-31)
//   [3] hour
//   [4] minute
//   [5] weekdays bitmask (see Weekday; bit0 is the one-shot flag, NOT a day)
//   [6] refId = assigned profile posId (0 = global "Optionen", 1.. = charge
//       locations). The fired timer sources its target SoC / maxCurrent /
//       temperature from THIS profile -- it is NOT a charge/climate mode.
//   [7] 0x00 pad (chargeSchedule/climateSchedule are hard-zero on the e-Golf)
// Firmware: DSICarHybrid.setBatteryControlTimer(id,y,m,d,h,min,weekdays,refId);
// refId proven via BatterManagerMainController.assignProfileToTimer().

// Weekday bitmask (record byte [5]). NB bit0 = one-shot (NOT cyclic) flag, not a
// weekday; recurring == bit0 clear. bit1..bit7 = Mon..Sun.
// [FW; Mon/Tue/Sat/Sun + one-shot are WIRE-confirmed, Wed/Thu/Fri FW-only.]
enum Weekday : uint8_t {
  WD_ONESHOT = 0x01,  // bit0: 1 = one-shot (fire on the [0:2] date), 0 = recurring
  WD_MON     = 0x02,
  WD_TUE     = 0x04,
  WD_WED     = 0x08,
  WD_THU     = 0x10,
  WD_FRI     = 0x20,
  WD_SAT     = 0x40,
  WD_SUN     = 0x80,
};

// LSG 0x25 func for departure-timer slot 1..4 (0x14..0x17).
inline uint8_t funcForTimerSlot(uint8_t slot) { return (uint8_t)(FUNC_TIMER_1 + (slot - 1)); }
// Slot 1..4 for a timer func, or 0 if `func` is not a timer func.
inline uint8_t timerSlotForFunc(uint8_t func) {
  return (func >= FUNC_TIMER_1 && func <= FUNC_TIMER_4) ? (uint8_t)(func - FUNC_TIMER_1 + 1) : 0;
}

struct Timer {
  uint8_t year = 0xFF;   // T-2000; [0:2] all 0xFF = "no next-date"
  uint8_t month = 0xFF;
  uint8_t day = 0xFF;
  uint8_t hour = 0;
  uint8_t minute = 0;
  uint8_t weekdays = 0;  // Weekday bitmask
  uint8_t refId = 0;     // assigned profile posId (0 = global "Optionen")
  // [0:2] is a next-fire date. Observed: one-shot timers carry the date; recurring
  // timers carry 0xFF/0xFF/0xFF (they fire by weekday, no single date). NB the
  // timer's enable/disable is a SEPARATE message (func 0x13 TimerState), not this
  // field, so hasDate() is "has a concrete fire date", not "enabled".
  bool hasDate()   const { return !(year == 0xFF && month == 0xFF && day == 0xFF); }
  bool oneShot()   const { return (weekdays & WD_ONESHOT) != 0; }
  bool recurring() const { return !oneShot(); }
};

// Encode/decode the 8-byte timer SET record. encode returns 8 (0 if cap<8).
inline size_t encodeTimer(uint8_t* out, size_t cap, const Timer& t) {
  if (cap < 8) return 0;
  out[0] = t.year;  out[1] = t.month;   out[2] = t.day;
  out[3] = t.hour;  out[4] = t.minute;  out[5] = t.weekdays;
  out[6] = t.refId; out[7] = 0x00;
  return 8;
}
inline bool decodeTimer(const uint8_t* rec, uint16_t len, Timer& out) {
  if (len < 8) return false;
  out = Timer();
  out.year = rec[0]; out.month = rec[1]; out.day = rec[2];
  out.hour = rec[3]; out.minute = rec[4]; out.weekdays = rec[5]; out.refId = rec[6];
  return true;
}

// TimerState (func 0x13): 2-byte body {enabledMask, 0x00}; bit i => timer (i+1)
// programmed/enabled. [FW+WIRE]
inline size_t encodeTimerState(uint8_t* out, size_t cap, uint8_t enabledMask) {
  if (cap < 2) return 0;
  out[0] = (uint8_t)(enabledMask & 0x0F); out[1] = 0x00; return 2;
}
inline bool timerEnabled(uint8_t stateByte, uint8_t slot /*1..4*/) {
  return (slot >= 1 && slot <= 4) && (stateByte & (1u << (slot - 1))) != 0;
}

// PowerProvider night-rate window (func 0x1A, RecordAddr 2). The RA2 record body
// (after the array header, which carries `pos`) is 4 bytes: nr = "night rate".
// [FW+WIRE] wire "14 00 17 00" = 20:00-23:00.
struct PowerProviderWindow {
  uint8_t nrStartHour = 0, nrStartMinute = 0, nrEndHour = 0, nrEndMinute = 0;
};
inline size_t encodePowerProviderRA2(uint8_t* out, size_t cap, const PowerProviderWindow& w) {
  if (cap < 4) return 0;
  out[0] = w.nrStartHour; out[1] = w.nrStartMinute;
  out[2] = w.nrEndHour;   out[3] = w.nrEndMinute; return 4;
}
inline bool decodePowerProviderRA2(const uint8_t* rec, uint16_t len, PowerProviderWindow& out) {
  if (len < 4) return false;
  out = PowerProviderWindow();
  out.nrStartHour = rec[0]; out.nrStartMinute = rec[1];
  out.nrEndHour = rec[2];   out.nrEndMinute = rec[3]; return true;
}

// ---------------------------------------------------------------------------
// OperationMode (0x18) -- execute / stop
// ---------------------------------------------------------------------------
// Two-byte body. PROVEN on the wire (command id 0x17332501): {0x00,0x01} starts
// the immediate/global profile (profile 0), {0x00,0x00} stops it.
//
// The byte model for NON-global (timer) profiles is UNRESOLVED -- two RE sources
// disagree and no capture exercises it:
//   * OCU firmware:  {0x00, bitmap}    bit0=profile0, bits 1..3=timer 1..3
//   * MIB2 firmware: {profileId, ctl}  byte0 selects the profile, byte1 = 0/1
// Both collapse to {0x00,0x01}/{0x00,0x00} for the global case, which is all the
// captures show. Only the proven global start/stop is exposed, plus a raw builder;
// neither timer model is baked in.
inline void buildOperationModeRaw(uint8_t (&body)[2], uint8_t b0, uint8_t b1) {
  body[0] = b0;
  body[1] = b1;
}
inline void buildClimateStart(uint8_t (&body)[2]) { buildOperationModeRaw(body, 0x00, 0x01); }
inline void buildClimateStop(uint8_t (&body)[2])  { buildOperationModeRaw(body, 0x00, 0x00); }

// ---------------------------------------------------------------------------
// Battery Control Profile record (full, RecordAddr = 0)
// ---------------------------------------------------------------------------
// Byte layout of one profile record. The record LENGTH (20 fixed + name) and the
// [WIRE]-marked offsets are confirmed against real captured profile dumps; the
// [RE]-marked middle fields read as padding (0xFF/0x00) in every capture, so
// their positions fit but their semantics rest on the RE sources only.
//   [0]  operation (ProfileOperation bits)      [WIRE]
//   [1]  operation2 (ProfileOperation2 bits)    [WIRE]
//   [2]  maxCurrent (amps)                       [WIRE]
//   [3]  minChargeLevel (%)                       [WIRE]
//   [4-5] minRange (LE u16)                        [RE]
//   [6]  targetChargeLevel (%)                    [WIRE]
//   [7]  targetChargeDuration                       [RE]
//   [8-9] targetChargeRange (LE u16)                [RE]
//   [10] unitRange                                  [RE]
//   [11] rangeCalculationSetup                      [RE]
//   [12] temperature (raw; see tempToRaw)          [WIRE]
//   [13] temperatureUnit                            [RE]
//   [14] leadTime                                   [RE]
//   [15] holdingTimePlug                            [RE]
//   [16] holdingTimeBattery                         [RE]
//   [17-18] providerDataId (LE u16)                 [RE]
//   [19] nameLength                                [WIRE]
//   [20..] name (ASCII, nameLength bytes)          [WIRE]
constexpr uint8_t kProfileFixedLen = 20;   // fixed fields before the name
constexpr uint8_t kProfileMaxName  = 24;   // longest observed name (~17) + margin;
                                           // longer names decode truncated (nameTruncated)

struct Profile {
  uint8_t operation = 0;
  uint8_t operation2 = 0;
  uint8_t maxCurrent = 0;
  uint8_t minChargeLevel = 0;
  uint16_t minRange = 0;
  uint8_t targetChargeLevel = 0;
  uint8_t targetChargeDuration = 0;
  uint16_t targetChargeRange = 0;
  uint8_t unitRange = 0;
  uint8_t rangeCalculationSetup = 0;
  uint8_t temperatureRaw = 0;
  uint8_t temperatureUnit = 0;
  uint8_t leadTime = 0;
  uint8_t holdingTimePlug = 0;
  uint8_t holdingTimeBattery = 0;
  uint16_t providerDataId = 0;
  uint8_t nameLen = 0;
  char name[kProfileMaxName + 1] = {0};  // NUL-terminated for convenience
  bool nameTruncated = false;  // true if the on-wire name exceeded kProfileMaxName
  uint16_t position = 0;  // array position index (from a PosTransmit STATUS array)
};

// Encode a full profile record into `out` (capacity `cap`). Returns the number
// of bytes written (20 + nameLen), or 0 if it would not fit.
size_t encodeProfile(uint8_t* out, size_t cap, const Profile& p);

// Decode one full profile record (20 + nameLen bytes). `len` must cover the
// whole record. Returns false if the record is too short or its declared name
// runs past `len`; on false, `out` is zero-initialized and must not be used.
bool decodeProfile(const uint8_t* rec, uint16_t len, Profile& out);

// Outcome of decodeProfileArray, so a caller can tell a clean decode from a
// short one. `count` profiles were written; `declared` is the header's element
// count. Exactly one of the flags may be set when count < declared.
struct ArrayResult {
  uint16_t count = 0;        // profiles written to out[]
  uint16_t declared = 0;     // elementCount from the array header
  bool complete = false;     // decoded all `declared` records
  bool truncated = false;    // stopped because out[] (maxOut) filled first
  bool malformed = false;    // stopped on a bad record / header (or header too short)
};

// ---------------------------------------------------------------------------
// Profile array (0x19) STATUS decode
// ---------------------------------------------------------------------------
// A BCU status broadcast on FUNC_PROFILES_ARRAY (element header "49 59", from
// 0x17332510) carries a BAP array of profile records. The on-wire framing,
// confirmed against real dumps and the RE sources, is:
//
//   [0] arrayId (0x29 = full list / 0x2A = changed-profile notification)
//   [1] totalElementsInList
//   [2] flags: [LargeIdx:1][PosTransmit:1][Backward:1][Shift:1][RecordAddr:4]
//   [3] startIndex     (2 bytes, LE, if LargeIdx=1)
//   [4] elementCount   (2 bytes, LE, if LargeIdx=1)
//   then, per element: [position (1 or 2 bytes) if PosTransmit] + record
//
// This decoder handles RecordAddr=0 (the 20-byte full record). Only the full
// format has been observed; compact (RecordAddr=6, 4 bytes) elements are skipped.
// The global "Optionen"/immediate profile is element 0; it holds the climate
// target temperature and global charge limits.
//
// Fills up to `maxOut` Profile entries and returns the count. Pass `result` to
// learn whether the decode was complete, truncated by maxOut, or stopped on a
// malformed record (otherwise a short count is indistinguishable from success).
size_t decodeProfileArray(const uint8_t* body, uint16_t bodyLen,
                          Profile* out, size_t maxOut, ArrayResult* result = nullptr);

// ---------------------------------------------------------------------------
// Array WRITE header + transaction id (func 0x19 ProfilesArray / 0x1A PowerProvider)
// ---------------------------------------------------------------------------
// An ASG array-WRITE element body is:
//     [ASG-ID:4 | txn:4]  [RecordAddr]  [startIndex]  [count]   <count records>
// Byte[0] is a transaction/sequence byte: high nibble = client (ASG) id
// (1 = MIB display, 2 = OCU/remote), low nibble = a per-client counter the client
// bumps on each write. The BCU echoes it in its 49 59 / 49 5A status so a writer
// can match the reply. [FW BatteryControlProfilesAH header; WIRE rolling 0x11..0x1f observed
// on the bus.]  NB this is the COMMAND-side header; the FSG STATUS broadcast uses a different
// 5-byte header (see decodeProfileArray).
constexpr uint8_t kAsgIdMib  = 0x1;   // MIB2 display
constexpr uint8_t kAsgIdOcu  = 0x2;   // factory OCU / remote client
constexpr uint8_t kAsgIdOvms = 0x3;   // OVMS: a distinct own client id, separate from the MIB (1) and
                                      // OCU (2). The BCU answers a GET on this id directly once the
                                      // channel-open handshake has been acked, so the profile read does
                                      // not depend on any ambient MIB/OCU poll.

// Compose byte[0]: asgId in the high nibble, txn (1..0xF) in the low nibble.
inline uint8_t asgTxnByte(uint8_t asgId, uint8_t txn) {
  return (uint8_t)(((asgId & 0x0F) << 4) | (txn & 0x0F));
}

// Rolling per-client transaction counter. next() returns 1..0xF and wraps
// 0xF -> 1 (never 0), so consecutive writes always differ.
struct TxnCounter {
  uint8_t last = 0;
  uint8_t next() { last = (last >= 0x0F) ? 0x01 : (uint8_t)(last + 1); return last; }
};

// Parsed COMMAND array-write header + a view of the trailing record block.
struct ArrayWriteHeader {
  uint8_t asgId = 0, txn = 0, recordAddr = 0, startIndex = 0, count = 0;
  const uint8_t* records = nullptr;
  uint16_t recordsLen = 0;
};
inline bool parseArrayWriteHeader(const uint8_t* body, uint16_t len, ArrayWriteHeader& out) {
  if (len < 4) return false;
  out = ArrayWriteHeader();
  out.asgId = (uint8_t)(body[0] >> 4);
  out.txn = (uint8_t)(body[0] & 0x0F);
  out.recordAddr = body[1];
  out.startIndex = body[2];
  out.count = body[3];
  out.records = body + 4;
  out.recordsLen = (uint16_t)(len - 4);
  return true;
}

// Build a COMMAND array-write body (pass to sendElement with FUNC_PROFILES_ARRAY
// or FUNC_POWER_PROVIDER). `records` = the pre-encoded record block (e.g. from
// encodeProfile). Returns total body length, or 0 if it won't fit in `cap`.
inline size_t buildArrayWriteBody(uint8_t* out, size_t cap, uint8_t asgTxn,
                                  uint8_t recordAddr, uint8_t startIndex, uint8_t count,
                                  const uint8_t* records, size_t recordsLen) {
  if (cap < (size_t)4 + recordsLen) return 0;
  out[0] = asgTxn; out[1] = recordAddr; out[2] = startIndex; out[3] = count;
  for (size_t i = 0; i < recordsLen; i++) out[4 + i] = records[i];
  return 4 + recordsLen;
}

// Write ONE profile via a func-0x19 array write: encode `p` at `recordAddr`, wrap
// it with a fresh transaction id from `tc`, and emit. `startIndex` = the target
// profile position (0 = global "Optionen"). Returns Refused if it doesn't fit.
template <typename Sink>
bap::SendResult sendProfileWrite(Sink&& sink, TxnCounter& tc, uint8_t asgId,
                                 uint8_t recordAddr, uint8_t startIndex, const Profile& p) {
  uint8_t rec[kProfileFixedLen + kProfileMaxName + 1];
  size_t rn = encodeProfile(rec, sizeof(rec), p);
  uint8_t body[4 + sizeof(rec)];
  size_t bn = rn ? buildArrayWriteBody(body, sizeof(body), asgTxnByte(asgId, tc.next()),
                                       recordAddr, startIndex, 1, rec, rn)
                 : 0;
  if (bn == 0) return bap::SendResult{bap::SendResult::Refused, 0};
  return bap::sendElement(sink, OP_SET_GET, kLsg, FUNC_PROFILES_ARRAY, body, (uint16_t)bn);
}

// ---------------------------------------------------------------------------
// Climate commands
// ---------------------------------------------------------------------------
// Selector byte of the ProfilesArray (0x19) climate telegram. NB this "selector"
// is really the array-write byte[0] = asgTxnByte(): the captured 0x22/0x23 are
// ASG2(OCU) with txn 2/3 -- see asgTxnByte() above. The trailing compact-record
// bytes are the observed payload with partly [RE] semantics.
constexpr uint8_t kClimateSelectStart = 0x22;
constexpr uint8_t kClimateSelectStop  = 0x23;

// Fill the 8-byte ProfilesArray (0x19) climate-select body exactly as captured:
// selector + a compact profile-0 record (maxCurrent 0x20).
inline void buildClimateProfileSelect(uint8_t (&body)[8], bool start) {
  const uint8_t tail[7] = {0x06, 0x00, 0x01, 0x06, 0x00, 0x20, 0x00};
  body[0] = start ? kClimateSelectStart : kClimateSelectStop;
  for (uint8_t i = 0; i < 7; i++) body[1 + i] = tail[i];
}

// Start/stop the immediate/global profile via OperationMode (0x18) ALONE:
// `on` -> "29 58 00 01", off -> "29 58 00 00".
template <typename Sink>
bap::SendResult sendClimate(Sink&& sink, bool on) {
  uint8_t body[2];
  if (on) buildClimateStart(body); else buildClimateStop(body);
  return bap::sendElement(sink, OP_SET_GET, kLsg, FUNC_OPERATION_MODE, body, 2);
}

// Full climate command as seen on the wire: the ProfilesArray (0x19) select
// telegram THEN the OperationMode (0x18). Sends the array telegram first and
// proceeds only if it left the sink cleanly; returns the OperationMode result,
// or the array telegram's result if that one failed.
template <typename Sink>
bap::SendResult sendClimateSequence(Sink&& sink, bool on) {
  uint8_t sel[8];
  buildClimateProfileSelect(sel, on);
  bap::SendResult r = bap::sendElement(sink, OP_SET_GET, kLsg, FUNC_PROFILES_ARRAY, sel, 8);
  if (!r.ok()) return r;
  return sendClimate(sink, on);
}

}  // namespace egolf
}  // namespace bap

#endif  // BAP_EGOLF_BATTERY_CONTROL_H_
