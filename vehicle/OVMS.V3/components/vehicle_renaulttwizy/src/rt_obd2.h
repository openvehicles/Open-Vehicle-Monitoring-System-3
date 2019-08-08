/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
 * 
 * (c) 2019  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __rt_obd2_h__
#define __rt_obd2_h__

#include <endian.h>
#include "rt_types.h"

using namespace std;


struct __attribute__ ((__packed__)) cluster_dtc {

  // Bytes 1…2:
  uint8_t   EcuName;
  uint8_t   NumDTC;

#if BYTE_ORDER == BIG_ENDIAN
  // big-endian: normal bit filling order MSB→LSB per byte (packed)

  // Byte 3:
  uint8_t   Revision:2;       // normally 0
  uint8_t   Memorize:1;       // 1 = Memorize this failure
  uint8_t   ServKey:1;        // 1 = Service Key telltale
  uint8_t   Customer:1;       // 0(?) = Failure visible by customer
  uint8_t   FailPresent:1;    // 1 = Failure currently present
  uint8_t   G1:1;             // 1 = First gravity telltale
  uint8_t   G2:1;             // 1 = Second gravity telltale

  // Byte 4:
  uint8_t   Bt:1;             // ?
  uint8_t   Ef:1;             // ?
  uint8_t   Dc:1;             // ?
  uint8_t   DNS:1;            // ?
  uint8_t   BV:4;             // +3 = 12V battery voltage [V]

  // Bytes 5…7:
  uint8_t   :6;
  uint8_t   Odometer2:2;      // Odometer [km] (18 bit, big-endian)
  uint8_t   Odometer1:8;      // Odometer [km] (18 bit, big-endian)
  uint8_t   Odometer0:8;      // Odometer [km] (18 bit, big-endian)

#else
  // little-endian (ESP32): reverse bit filling order LSB→MSB per byte (packed)

  // Byte 3:
  uint8_t   G2:1;             // 1 = Second gravity telltale
  uint8_t   G1:1;             // 1 = First gravity telltale
  uint8_t   FailPresent:1;    // 1 = Failure currently present
  uint8_t   Customer:1;       // 0(?) = Failure visible by customer
  uint8_t   ServKey:1;        // 1 = Service Key telltale
  uint8_t   Memorize:1;       // 1 = Memorize this failure
  uint8_t   Revision:2;       // normally 0

  // Byte 4:
  uint8_t   BV:4;             // +3 = 12V battery voltage [V]
  uint8_t   DNS:1;            // ?
  uint8_t   Dc:1;             // ?
  uint8_t   Ef:1;             // ?
  uint8_t   Bt:1;             // ?

  // Bytes 5…7:
  uint8_t   Odometer2:2;      // Odometer [km] (18 bit, big-endian)
  uint8_t   :6;
  uint8_t   Odometer1:8;      // Odometer [km] (18 bit, big-endian)
  uint8_t   Odometer0:8;      // Odometer [km] (18 bit, big-endian)

#endif

  // Bytes 8…11
  uint8_t   IgnitionCycle;    // ? 0xFF = off
  uint8_t   TimeCounter;      // ? reoccurrence count? normally 0
  uint8_t   BL;               // Battery SOC level [%]
  int8_t    Speed;            // [kph] / 0xFF


  // Methods:
  uint8_t getBV() { return BV+3; }
  void setBV(uint8_t val) { BV = val-3; }
  uint32_t getOdometer() {
    return (Odometer2 << 16) | (Odometer1 << 8) | Odometer0;
  }
  void setOdometer(uint32_t val) {
    Odometer2 = (val & 0x030000) >> 16;
    Odometer1 = (val & 0x00ff00) >> 8;
    Odometer0 = (val & 0x0000ff);
  }

  // Operators:
  bool operator ==(cluster_dtc& o) {
    // DTC considered unchanged if…
    return (EcuName       == o.EcuName &&
            NumDTC        == o.NumDTC &&
            Revision      == o.Revision &&
            getOdometer() == o.getOdometer() &&
            TimeCounter   == o.TimeCounter);
  }
  bool operator !=(cluster_dtc& o) {
    return !(*this == o);
  }
};

#define DTC_COUNT 10
struct __attribute__ ((__packed__)) cluster_dtc_store {
  cluster_dtc entry[DTC_COUNT];
};


#endif // __rt_obd2_h__
