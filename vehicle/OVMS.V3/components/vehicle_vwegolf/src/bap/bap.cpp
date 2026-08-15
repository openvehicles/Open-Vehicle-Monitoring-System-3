/*
;    Project:       BAP protocol library
;    Subproject:    Generic BAP transport + framing (implementation)
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

#include "bap/bap.h"

namespace bap {

void encodeHeader(uint8_t out[2], uint8_t opcode, uint8_t lsg, uint8_t func) {
  uint16_t func12 = (uint16_t)(((lsg & 0x3F) << 6) | (func & 0x3F));
  out[0] = (uint8_t)(((opcode & 0x0F) << 4) | ((func12 >> 8) & 0x0F));
  out[1] = (uint8_t)(func12 & 0xFF);
}

void decodeHeader(const uint8_t in[2], uint8_t& opcode, uint8_t& lsg, uint8_t& func) {
  opcode = in[0] >> 4;
  uint16_t func12 = (uint16_t)(((in[0] & 0x0F) << 8) | in[1]);
  lsg = (uint8_t)(func12 >> 6);
  func = (uint8_t)(func12 & 0x3F);
}

bool decodeElement(const uint8_t* elem, uint16_t len, Element& out) {
  if (len < 2) return false;
  decodeHeader(elem, out.opcode, out.lsg, out.func);
  out.body = (len > 2) ? (elem + 2) : nullptr;
  out.bodyLen = (uint16_t)(len - 2);
  return true;
}

SendResult sendElementFn(FrameSinkFn sink, void* ctx, uint8_t opcode, uint8_t lsg,
                         uint8_t func, const uint8_t* body, uint16_t bodyLen,
                         uint8_t group) {
  // A body with no bytes is fine (bodyLen==0); a length with no buffer is not.
  if (!body && bodyLen) return {SendResult::Refused, 0};

  uint8_t hdr[2];
  encodeHeader(hdr, opcode, lsg, func);

  // Short: element header + up to 6 body bytes in one frame. Sent at exact
  // length (not zero-padded to 8) so the frame is self-describing on receive:
  // a short frame carries no body-length field, so padding would be
  // indistinguishable from real body. Observed unpadded frames confirm the ECUs
  // use variable DLC.
  if (bodyLen <= 6) {
    uint8_t f[8] = {0};
    f[0] = hdr[0];
    f[1] = hdr[1];
    if (bodyLen) memcpy(f + 2, body, bodyLen);
    if (!sink(ctx, f, (uint8_t)(2 + bodyLen))) return {SendResult::SinkFailedFirst, 0};
    return {SendResult::Ok, 1};
  }

  // The start frame's length field is one byte, so a body over 255 cannot be
  // framed -- refuse rather than silently truncate the length.
  if (bodyLen > 255) return {SendResult::Refused, 0};

  // Long: start frame carries the header + first 4 body bytes.
  uint16_t frames = 0;
  uint8_t f[8] = {0};
  f[0] = 0x80 | ((group & 0x3) << 4);
  f[1] = (uint8_t)bodyLen;  // body length only
  f[2] = hdr[0];
  f[3] = hdr[1];
  memcpy(f + 4, body, 4);
  if (!sink(ctx, f, 8)) return {SendResult::SinkFailedFirst, 0};
  frames++;

  // Continuations carry up to 7 body bytes each; last frame is not padded.
  uint16_t off = 4;
  uint8_t index = 0;
  while (off < bodyLen) {
    uint8_t chunk = (uint8_t)((bodyLen - off > 7) ? 7 : (bodyLen - off));
    uint8_t c[8] = {0};
    c[0] = 0xC0 | ((group & 0x3) << 4) | (index & 0xF);
    memcpy(c + 1, body + off, chunk);
    // A failure here has already put a start (+ maybe continuations) on the bus:
    // a truncated telegram is live -- report it distinctly.
    if (!sink(ctx, c, (uint8_t)(1 + chunk))) return {SendResult::SinkFailedPartial, frames};
    frames++;
    off += chunk;
    index = (index + 1) & 0xF;
  }
  return {SendResult::Ok, frames};
}

}  // namespace bap
