/*
;    Project:       BAP protocol library
;    Subproject:    e-Golf BatteryControl (LSG 0x25) semantics (implementation)
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

#include "egolf/battery_control.h"

namespace bap {
namespace egolf {

namespace {
// One full profile record read straight off a body cursor: fixed fields in
// order, then the length-prefixed name. All bounds live in the Reader.
bool readProfile(bap::Reader& r, Profile& out) {
  out.operation = r.u8();
  out.operation2 = r.u8();
  out.maxCurrent = r.u8();
  out.minChargeLevel = r.u8();
  out.minRange = r.u16le();
  out.targetChargeLevel = r.u8();
  out.targetChargeDuration = r.u8();
  out.targetChargeRange = r.u16le();
  out.unitRange = r.u8();
  out.rangeCalculationSetup = r.u8();
  out.temperatureRaw = r.u8();
  out.temperatureUnit = r.u8();
  out.leadTime = r.u8();
  out.holdingTimePlug = r.u8();
  out.holdingTimeBattery = r.u8();
  out.providerDataId = r.u16le();
  bool truncated = false;
  out.nameLen = r.strLp(out.name, sizeof(out.name), &truncated);
  out.nameTruncated = truncated;
  return r.ok();
}
}  // namespace

size_t encodeProfile(uint8_t* out, size_t cap, const Profile& p) {
  bap::Writer w(out, cap);
  w.u8(p.operation);
  w.u8(p.operation2);
  w.u8(p.maxCurrent);
  w.u8(p.minChargeLevel);
  w.u16le(p.minRange);
  w.u8(p.targetChargeLevel);
  w.u8(p.targetChargeDuration);
  w.u16le(p.targetChargeRange);
  w.u8(p.unitRange);
  w.u8(p.rangeCalculationSetup);
  w.u8(p.temperatureRaw);
  w.u8(p.temperatureUnit);
  w.u8(p.leadTime);
  w.u8(p.holdingTimePlug);
  w.u8(p.holdingTimeBattery);
  w.u16le(p.providerDataId);
  uint8_t nameLen = (p.nameLen > kProfileMaxName) ? kProfileMaxName : p.nameLen;
  w.strLp(p.name, nameLen);
  return w.ok() ? w.size() : 0;
}

bool decodeProfile(const uint8_t* rec, uint16_t len, Profile& out) {
  bap::Reader r(rec, len);
  out = Profile();
  return readProfile(r, out);
}

size_t decodeProfileArray(const uint8_t* body, uint16_t bodyLen,
                          Profile* out, size_t maxOut, ArrayResult* result) {
  ArrayResult res;
  bap::Reader r(body, bodyLen);
  r.u8();                          // arrayId (0x29 full list / 0x2A changed)
  r.u8();                          // totalElementsInList
  const uint8_t flags = r.u8();
  const bool largeIdx = (flags & 0x80) != 0;
  const bool posTransmit = (flags & 0x40) != 0;
  const uint8_t recordAddr = flags & 0x0F;
  const uint16_t startIndex   = largeIdx ? r.u16le() : r.u8();
  const uint16_t elementCount = largeIdx ? r.u16le() : r.u8();
  res.declared = elementCount;
  if (!r.ok()) { res.malformed = true; if (result) *result = res; return 0; }

  size_t count = 0;
  uint16_t i = 0;
  for (; i < elementCount && r.ok(); i++) {
    if (count >= maxOut) { res.truncated = true; break; }
    uint16_t position = (uint16_t)(startIndex + i);
    if (posTransmit) position = largeIdx ? r.u16le() : r.u8();

    if (recordAddr == 6) {         // compact 4-byte record: not modeled -- skip
      r.skip(4);                   // counted toward `declared`, not written to out[]
      continue;
    }

    Profile& p = out[count];
    p = Profile();
    if (!readProfile(r, p)) { res.malformed = true; break; }
    p.position = position;
    count++;
  }
  res.count = (uint16_t)count;
  if (res.truncated || res.malformed) {
    res.complete = false;
  } else if (i >= elementCount) {
    res.complete = true;                 // consumed every declared record
  } else {
    res.malformed = true;                // reader ran dry mid-array
    res.complete = false;
  }
  if (result) *result = res;
  return count;
}

}  // namespace egolf
}  // namespace bap
