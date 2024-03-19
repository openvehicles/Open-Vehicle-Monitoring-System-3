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


#endif
