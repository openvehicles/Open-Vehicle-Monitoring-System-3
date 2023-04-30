/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th March 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Shane Hunns
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

#ifndef ME56_PIDS_H_
#define ME56_PIDS_H_

// Module IDs
constexpr uint32_t broadcastId = 0x7dfu;
constexpr uint32_t bmstx = 0x748u;
constexpr uint32_t bmsrx = 0x7c8u;
constexpr uint32_t vcutx = 0x7e3u;
constexpr uint32_t vcurx = 0x7ebu;
constexpr uint32_t gwmtx = 0x710u;
constexpr uint32_t gwmrx = 0x790u;
constexpr uint32_t evcctx = 0x726u;
constexpr uint32_t evccrx = 0x7a6u;


// BMS PIDs
constexpr uint16_t bmssoc = 0xe014u;
constexpr uint16_t bmsStatusPid = 0xe037u;
 constexpr uint16_t bmssoh = 0xe053u;
constexpr uint16_t bmsccsamps = 0xe088u;
constexpr uint16_t bmsacamps = 0xe089u;
//constexpr uint16_t bmsccschargeon = 0xe090u;
//constexpr uint16_t bmsacchargeon = 0xe091u;
 constexpr uint16_t bmssocraw = 0xe096u;
constexpr uint16_t cellvolts = 0xe113u;
constexpr uint16_t celltemps = 0xe114u;
// constexpr uint16_t packamps = 0xe131u;

// VCU PIDs
constexpr uint16_t vcuspeed = 0xe000u; 
constexpr uint16_t vcutemp1 = 0xe005u;
constexpr uint16_t vcutemp2 = 0xe006u;
constexpr uint16_t vcupackvolts = 0xe019u;
constexpr uint16_t vcuvin = 0xf190u;
constexpr uint16_t vcu12vamps = 0xe022u;
constexpr uint16_t vcuchargervolts = 0xe020u;
//constexpr uint16_t vcuchargeramps = 0xe039u;

// EVCC PIDs
constexpr uint16_t evccacamps = 0xe005u;

#endif  // ME56_PIDS_H_
