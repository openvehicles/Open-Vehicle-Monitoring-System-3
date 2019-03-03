/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON Gen4 access: tuning
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
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

#include "ovms_log.h"
static const char *TAG = "v-twizy";

#include <stdio.h>
#include <math.h>
#include <string>
#include <iomanip>

#include "crypt_base64.h"
#include "ovms_notify.h"

#include "vehicle_renaulttwizy.h"
#include "rt_sevcon.h"

#define AUTOPOWER_CHECKPOINT(pwr,ref,min) (((pwr)<<16)+((ref)<<8)+((min)))


/**
 * Default tuning configuration for Twizy80 [0] & Twizy45 [1]
 */

struct twizy_cfg_params {
  uint8_t     DefaultKphMax;
  uint16_t    DefaultRpmMax;
  uint16_t    DefaultRpmRev;
  uint16_t    DeltaBrkStart;
  uint16_t    DeltaBrkEnd;
  uint16_t    DeltaBrkDown;

  uint8_t     DefaultKphWarn;
  uint16_t    DefaultRpmWarn;
  uint16_t    DeltaWarnOff;

  uint16_t    DefaultTrq;
  uint16_t    DefaultTrqRated;
  uint32_t    DefaultTrqLim;
  uint16_t    DeltaMapTrq;
  
  uint32_t    DefaultCurrLim;
  uint16_t    DefaultCurrStatorMax;
  uint16_t    BoostCurr;
  uint16_t    DefaultFMAP[4];             // only upper 2 points needed
  uint16_t    ExtendedFMAP[4];            // only upper 2 points needed

  uint16_t    DefaultPwrLo;
  uint16_t    DefaultPwrLoLim;
  uint16_t    DefaultPwrHi;
  uint16_t    DefaultPwrHiLim;
  uint16_t    DefaultMaxMotorPwr;

  uint8_t     DefaultRecup;
  uint8_t     DefaultRecupPrc;

  uint16_t    DefaultRampStart;
  uint8_t     DefaultRampStartPrm;
  uint16_t    DefaultRampAccel;
  uint8_t     DefaultRampAccelPrc;

  uint16_t    DefaultPMAP[18];
  
  uint16_t    DefaultMapSpd[4];
};

const struct twizy_cfg_params twizy_cfg_params[] =
{
  {
    // CFG[0] = TWIZY80:
    //
    //    55 Nm (0x4611.0x01) = default max power map (PMAP) torque
    //    55 Nm (0x6076.0x00) = default peak torque
    //    57 Nm (0x2916.0x01) = rated torque (??? should be equal to 0x6076...)
    //    70.125 Nm (0x4610.0x11) = max motor torque according to flux map
    //
    //    7250 rpm (0x2920.0x05) = default max fwd speed = ~80 kph
    //    8050 rpm = default overspeed warning trigger (STOP lamp ON) = ~89 kph
    //    8500 rpm = default overspeed brakedown trigger = ~94 kph
    //    10000 rpm = max neutral speed (0x3813.2d) = ~110 kph
    //    11000 rpm = severe overspeed fault (0x4624.00) = ~121 kph

    80,     // DefaultKphMax
    7250,   // DefaultRpmMax
    900,    // DefaultRpmMaxRev
    400,    // DeltaBrkStart
    800,    // DeltaBrkEnd
    1250,   // DeltaBrkDown

    89,     // DefaultKphWarn
    8050,   // DefaultRpmWarn
    550,    // DeltaWarnOff

    55000,  // DefaultTrq
    57000,  // DefaultTrqRated
    70125,  // DefaultTrqLim
    0,      // DeltaMapTrq
    
    450000, // DefaultCurrLim
    450,    // DefaultCurrStatorMax
    540,    // BoostCurr
    { 964, 9728, 1122, 9984 }, // DefaultFMAP
    { 1122, 10089, 2240, 11901 }, // ExtendedFMAP

    12182,  // DefaultPwrLo
    17000,  // DefaultPwrLoLim
    13000,  // DefaultPwrHi
    17000,  // DefaultPwrHiLim
    4608,   // DefaultMaxMotorPwr [1/256 kW]

    182,    // DefaultRecup
    18,     // DefaultRecupPrc

    400,    // DefaultRampStart
    40,     // DefaultRampStartPrm
    2500,   // DefaultRampAccel
    25,     // DefaultRampAccelPrc

            // DefaultPMAP:
    { 880,0, 880,2115, 659,2700, 608,3000, 516,3500,
      421,4500, 360,5500, 307,6500, 273,7250 },

            // DefaultMapSpd:
    { 3000, 3500, 4500, 6000 }
  
  } // CFG[0] = TWIZY80

  ,{
    // CFG[1] = TWIZY45:
    //
    //    32.5 Nm (0x4611.0x01) = default max power map (PMAP) torque
    //    33 Nm (0x6076.0x00) = default peak torque
    //    33 Nm (0x2916.0x01) = rated torque (??? should be equal to 0x6076...)
    //    36 Nm (0x4610.0x11) = max motor torque according to flux map
    //
    //    5814 rpm (0x2920.0x05) = default max fwd speed = ~45 kph
    //    7200 rpm = default overspeed warning trigger (STOP lamp ON) = ~56 kph
    //    8500 rpm = default overspeed brakedown trigger = ~66 kph
    //    10000 rpm = max neutral speed (0x3813.2d) = ~77 kph
    //    11000 rpm = severe overspeed fault (0x4624.00) = ~85 kph

    45,     // DefaultKphMax
    5814,   // DefaultRpmMax
    1307,   // DefaultRpmMaxRev
    686,    // DeltaBrkStart
    1386,   // DeltaBrkEnd
    2686,   // DeltaBrkDown

    56,     // DefaultKphWarn
    7200,   // DefaultRpmWarn
    900,    // DeltaWarnOff

    32500,  // DefaultTrq
    33000,  // DefaultTrqRated
    36000,  // DefaultTrqLim
    500,    // DeltaMapTrq

    270000, // DefaultCurrLim
    290,    // DefaultCurrStatorMax
    330,    // BoostCurr
    { 480, 8192, 576, 8960 }, // DefaultFMAP
    { 656, 9600, 1328, 11901 }, // ExtendedFMAP
    
    7050,   // DefaultPwrLo
    10000,  // DefaultPwrLoLim
    7650,   // DefaultPwrHi
    10000,  // DefaultPwrHiLim
    2688,   // DefaultMaxMotorPwr [1/256 kW]

    209,    // DefaultRecup
    21,     // DefaultRecupPrc

    300,    // DefaultRampStart
    30,     // DefaultRampStartPrm
    2083,   // DefaultRampAccel
    21,     // DefaultRampAccelPrc

            // DefaultPMAP:
    { 520,0, 520,2050, 437,2500, 363,3000, 314,3500,
      279,4000, 247,4500, 226,5000, 195,6000 },

            // DefaultMapSpd:
    { 4357, 5083, 6535, 8714 }
  
  } // CFG[1] = TWIZY45

  ,{
    // CFG[2] = T80 SEVCON with T45 Gearbox:
    //    - use if mounting a T80 SEVCON G4845 instead of the G4827 in a T45
    //    - hybrid: power & torque values of T80, speed & rpm values of T45
    //
    //    55 Nm (0x4611.0x01) = default max power map (PMAP) torque
    //    55 Nm (0x6076.0x00) = default peak torque
    //    57 Nm (0x2916.0x01) = rated torque (??? should be equal to 0x6076...)
    //    70.125 Nm (0x4610.0x11) = max motor torque according to flux map
    //
    //    5814 rpm (0x2920.0x05) = default max fwd speed = ~45 kph
    //    7200 rpm = default overspeed warning trigger (STOP lamp ON) = ~56 kph
    //    8500 rpm = default overspeed brakedown trigger = ~66 kph
    //    10000 rpm = max neutral speed (0x3813.2d) = ~77 kph
    //    11000 rpm = severe overspeed fault (0x4624.00) = ~85 kph

    45,     // DefaultKphMax
    5814,   // DefaultRpmMax
    1307,   // DefaultRpmMaxRev
    686,    // DeltaBrkStart
    1386,   // DeltaBrkEnd
    2686,   // DeltaBrkDown

    56,     // DefaultKphWarn
    7200,   // DefaultRpmWarn
    900,    // DeltaWarnOff

    55000,  // DefaultTrq
    57000,  // DefaultTrqRated
    70125,  // DefaultTrqLim
    0,      // DeltaMapTrq
    
    450000, // DefaultCurrLim
    450,    // DefaultCurrStatorMax
    540,    // BoostCurr
    { 964, 9728, 1122, 9984 }, // DefaultFMAP
    { 1122, 10089, 2240, 11901 }, // ExtendedFMAP

    12182,  // DefaultPwrLo
    17000,  // DefaultPwrLoLim
    13000,  // DefaultPwrHi
    17000,  // DefaultPwrHiLim
    4608,   // DefaultMaxMotorPwr [1/256 kW]

    182,    // DefaultRecup
    18,     // DefaultRecupPrc

    400,    // DefaultRampStart
    40,     // DefaultRampStartPrm
    2500,   // DefaultRampAccel
    25,     // DefaultRampAccelPrc

            // DefaultPMAP:
    { 880,0, 880,2050, 659,2500, 608,3000, 516,3500,
      421,4000, 360,4500, 307,5000, 273,6000 },

            // DefaultMapSpd:
    { 4357, 5083, 6535, 8714 }
  
  } // CFG[2] = T80 SEVCON with T45 Gearbox

}; // const struct twizy_cfg_params twizy_cfg_params[]

// ROM parameter access macro:
#define CFG twizy_cfg_params[m_drivemode.type]


// scale utility:
//  returns <deflt> value scaled <from> <to> limited by <min> and <max>
uint32_t scale(uint32_t deflt, uint32_t from, uint32_t to, uint32_t min, uint32_t max)
{
  uint32_t val;

  if (to == from)
    return deflt;

  val = (deflt * to) / from;

  if (val < min)
    return min;
  else if (val > max)
    return max;
  else
    return val;
}


// CfgMakePowermap():
//  Internal utility for CfgSpeed() and CfgPower()
//  builds torque/speed curve (SDO 0x4611) for current max values...
//  - twizy_max_rpm
//  - twizy_max_trq
//  - twizy_max_pwr_lo
//  - twizy_max_pwr_hi
// See "Twizy Powermap Calculator" spreadsheet for details.

#if 1 // new version

//
// The Twizy motor needs to define all three operating regions of
// an AC induction motor with variable voltage & frequency control:
//  - Region 1: constant torque
//  - Region 2: constant power (we allow power to raise/decline linearly)
//  - Region 3: max slip speed (called "high speed region" in SEVCON manual)
// (see e.g. http://people.ucalgary.ca/~aknigh/vsd/ssim/sine/vvvf.html)
// 
// From monitoring register 0x4600 it seems the maximum motor voltage level the
// controller and battery can provide is 52.8 Vrms (may correspond with 0x4641.0c).
// On a 100 Nm configuration this level is reached at ~32 kph (with ~60 Nm).
// Maximum slip frequency seems to be 272.3 rad/s and is reached at ~61 kph.
// Raising current to 111% shifts this to ~64 kph. Assuming torque scaling
// to be sqrt(curr%) fits the data (but not the motor equations, possibly
// because current level change is not applied to the rated stator current).
// 
// So:
// - Start of region 2 is defined by (max_trq, max_pwr_lo):
//   rpm2 = max_pwr_lo * 9.549 / max_trq
// - Start of region 3 is defined by (max_pwr_hi, breakdown torque, current level):
//   rpm3 = trq_brk * rpm_rtd^2 / 9.549 / max_pwr_hi * sqrt(curr%)
//
// Note: region 3 may cut region 1 (= no region 2)
//    or cut region 2 above max_rpm (= no region 3).
//
// Power in region 2 raises/declines linearly from max_pwr_lo@rpm1 to max_pwr_hi@rpm2.
// Power in region 3 then declines inverse proportionally by rpm.
//
// See "Powermap Calculator" spreadsheet for (simplified) simulations.
//

#define DEFAULT_TRQ_BRK     0
#define DEFAULT_RPM_RTD     0
// #define DEFAULT_TRQ_BRK     (3 * 70.125)
// #define DEFAULT_RPM_RTD     (2100)
// …assumed based on breakdown torque measurements

#define CURRENT_SCALING     ((double)twizy_max_curr / 100.0f)
// #define CURRENT_SCALING     (sqrt((double)twizy_max_curr / 100.0f))

#define MAP_SEGMENT_CNT 7

CANopenResult_t SevconClient::CfgMakePowermap(void
  /* twizy_max_rpm, twizy_max_trq, twizy_max_pwr_lo, twizy_max_pwr_hi */)
{
  const uint8_t pmap_fib[8] = { 0, 1, 2, 3, 5, 8, 13, 21 };
  
  SevconJob sc(this);
  CANopenResult_t err;
  uint8_t i, pt;
  uint32_t rpm, rpm2, rpm3;
  uint32_t trq;
  uint32_t pwr;
  float rpm_d2, pwr_d2, rpm_d3;
  float trq_brk, rpm_rtd;

  if (twizy_max_rpm == CFG.DefaultRpmMax && twizy_max_trq == CFG.DefaultTrq
    && twizy_max_pwr_lo == CFG.DefaultPwrLo && twizy_max_pwr_hi == CFG.DefaultPwrHi) {

    // restore default torque map:
    for (i=0; i<18; i++) {
      if ((err = sc.Write(0x4611, 0x01+i, CFG.DefaultPMAP[i])) != COR_OK)
        return err;
    }
    
    // restore default flux map (only last 2 points):
    for (i=0; i<4; i++) {
      if ((err = sc.Write(0x4610, 0x0f+i, CFG.DefaultFMAP[i])) != COR_OK)
        return err;
    }
  }

  else {

    // get breakdown torque config:
    trq_brk = MyConfig.GetParamValueFloat("xrt", "motor_trq_breakdown", DEFAULT_TRQ_BRK);
    rpm_rtd = MyConfig.GetParamValueFloat("xrt", "motor_rpm_rated", DEFAULT_RPM_RTD);
    
    // disable breakdown calculation?
    if (trq_brk == 0 || rpm_rtd == 0) {
      trq_brk = 1000;
      rpm_rtd = 10000;
    }

    // Calculate region transition points:
    //
    // Start of region 2 is defined by (max_trq, max_pwr_lo):
    //   rpm2 = max_pwr_lo * 9.549 / max_trq
    
    rpm2 = (((uint32_t)twizy_max_pwr_lo * 9549) / twizy_max_trq);
    
    if (rpm2 > twizy_max_rpm)
        rpm2 = twizy_max_rpm;
    
    // Check if region 3 cuts region 1 (at max_trq):
    
    rpm3 = sqrt(trq_brk * 1000 / twizy_max_trq * CURRENT_SCALING) * rpm_rtd;
    
    if (rpm3 < rpm2) {
      
      // yes: this also means there is no region 2
      rpm2 = rpm3;
      pt = 0;
      
      // adjust twizy_max_pwr_*:
      twizy_max_pwr_lo = twizy_max_trq * rpm2 / 9549;
      twizy_max_pwr_hi = twizy_max_pwr_lo;
      
    }
    else {
      
      // Check if region 3 cuts region 2 (at pwr_hi):
      //
      // Start of region 3 is defined by (max_pwr_hi, breakdown torque, current level):
      //   rpm3 = trq_brk * rpm_rtd^2 / 9.549 / max_pwr_hi * sqrt(curr%)
      //
      // If rpm3 > max_rpm then no region 3 is needed.
      
      rpm3 = (trq_brk / 9.549f * rpm_rtd * rpm_rtd) * CURRENT_SCALING / twizy_max_pwr_hi;

      if (rpm3 >= twizy_max_rpm) {
        
        // no need for region 3:
        rpm3 = MAX(rpm2, twizy_max_rpm);
        pt = 8;
        
      }
      else {
        
        // in a config with very high pwr_hi, region 3 may calc as starting
        // even in region 1. To maximize torque we stick to region 1 as
        // calculated before, and skip region 2:
        if (rpm3 < rpm2) {
          rpm3 = rpm2;
          twizy_max_pwr_hi = twizy_max_pwr_lo;
        }
        
        // calculate map point index for start of region 3:
        rpm_d2 = (twizy_max_rpm - rpm2) / MAP_SEGMENT_CNT;
        pt = ((float) rpm3 - rpm2) / rpm_d2 + 0.6f;
        
        // Note: the +0.6 gives a tendency for 1 more point within
        //    region 2 => better curve approximation
    
        // if region 3 would now start on the last map point available, shift
        // down by one so we still can model the region 3 segment:
        if (pt == MAP_SEGMENT_CNT)
          pt = MAP_SEGMENT_CNT-1;
      }
      
    }
    
    // Now rpm2 & rpm3 mark the start of regions 2 & 3,
    // and pt is the map index for rpm3.
    
    ESP_LOGD(TAG, "CfgMakePowermap: pwr2=%d, rpm2=%d, pwr3=%d, rpm3=%d, pt=%d\n",
      twizy_max_pwr_lo, rpm2, twizy_max_pwr_hi, rpm3, pt);
    
    // write map start point:
    
    trq = (twizy_max_trq * 16 + 500) / 1000;
    
    if ((err = sc.Write(0x4611, 0x01, trq)) != COR_OK)
      return err;
    if ((err = sc.Write(0x4611, 0x02, 0)) != COR_OK)
      return err;

    // adjust flux map (only last 2 points):
    
    if (trq > CFG.DefaultFMAP[2]) {
      for (i=0; i<4; i++) {
        if ((err = sc.Write(0x4610, 0x0f+i, CFG.ExtendedFMAP[i])) != COR_OK)
          return err;
      }
    }
    else {
      for (i=0; i<4; i++) {
        if ((err = sc.Write(0x4610, 0x0f+i, CFG.DefaultFMAP[i])) != COR_OK)
          return err;
      }
    }
    
    // calculate rpm & power deltas:
    
    if (pt > 0) {
      rpm_d2 = (rpm3 - rpm2) / pmap_fib[MIN(pt, MAP_SEGMENT_CNT)];
      pwr_d2 = ((int)twizy_max_pwr_hi - (int)twizy_max_pwr_lo) / pmap_fib[MIN(pt, MAP_SEGMENT_CNT)];
    } else {
      // no region 2:
      rpm_d2 = 0;
      pwr_d2 = 0;
    }
    if (pt < MAP_SEGMENT_CNT) {
      rpm_d3 = (twizy_max_rpm - rpm3) / pmap_fib[MAX(MAP_SEGMENT_CNT-pt, 1)];
    } else {
      // no region 3:
      rpm_d3 = 0;
    }

    // write map points:
    
    for (i=0; i<=MAP_SEGMENT_CNT; i++) {

      // calculate rpm & power at point i:
      if (i < pt) {
        // region 2:
        rpm = rpm2 + (pmap_fib[i] * rpm_d2);
        pwr = twizy_max_pwr_lo + (pmap_fib[i] * pwr_d2);
      } else {
        // region 3:
        rpm = rpm3 + (pmap_fib[i-pt] * rpm_d3);
        pwr = ((UINT32)twizy_max_pwr_hi * rpm3) / rpm;
      }

      // calculate torque at point i:
      trq = ((((uint32_t)pwr * 9549 + (rpm>>1)) / rpm) * 16 + 500) / 1000;

      ESP_LOGD(TAG, "CfgMakePowermap: #%d rpm=%d trq=%.2f pwr=%d", i, rpm, trq/16.0, pwr);

      // write into map:
      if ((err = sc.Write(0x4611, 0x03+(i<<1), trq)) != COR_OK)
        return err;
      if ((err = sc.Write(0x4611, 0x04+(i<<1), rpm)) != COR_OK)
        return err;
    }
    
  }

  // commit map changes:
  if ((err = sc.Write(0x4641, 0x01, 1)) != COR_OK)
    return err;
  vTaskDelay(pdMS_TO_TICKS(50));

  return COR_OK;
}

#else // old version

CANopenResult_t SevconClient::CfgMakePowermap(void
  /* twizy_max_rpm, twizy_max_trq, twizy_max_pwr_lo, twizy_max_pwr_hi */)
{
  const uint8_t pmap_fib[7] = { 1, 2, 3, 5, 8, 13, 21 };
  
  SevconJob sc(this);
  CANopenResult_t err;
  uint8_t i;
  uint16_t rpm_0, rpm;
  uint16_t trq;
  uint16_t pwr;
  int rpm_d, pwr_d;

  if (twizy_max_rpm == CFG.DefaultRpmMax && twizy_max_trq == CFG.DefaultTrq
    && twizy_max_pwr_lo == CFG.DefaultPwrLo && twizy_max_pwr_hi == CFG.DefaultPwrHi) {

    // restore default torque map:
    for (i=0; i<18; i++) {
      if ((err = sc.Write(0x4611, 0x01+i, CFG.DefaultPMAP[i])) != COR_OK)
        return err;
    }
    
    // restore default flux map (only last 2 points):
    for (i=0; i<4; i++) {
      if ((err = sc.Write(0x4610, 0x0f+i, CFG.DefaultFMAP[i])) != COR_OK)
        return err;
    }
  }

  else {

    // calculate constant torque part:

    rpm_0 = (((uint32_t)twizy_max_pwr_lo * 9549 + (twizy_max_trq>>1)) / twizy_max_trq);
    trq = (twizy_max_trq * 16 + 500) / 1000;

    if ((err = sc.Write(0x4611, 0x01, trq)) != COR_OK)
      return err;
    if ((err = sc.Write(0x4611, 0x02, 0)) != COR_OK)
      return err;
    if ((err = sc.Write(0x4611, 0x03, trq)) != COR_OK)
      return err;
    if ((err = sc.Write(0x4611, 0x04, rpm_0)) != COR_OK)
      return err;

    // adjust flux map (only last 2 points):
    
    if (trq > CFG.DefaultFMAP[2]) {
      for (i=0; i<4; i++) {
        if ((err = sc.Write(0x4610, 0x0f+i, CFG.ExtendedFMAP[i])) != COR_OK)
          return err;
      }
    }
    else {
      for (i=0; i<4; i++) {
        if ((err = sc.Write(0x4610, 0x0f+i, CFG.DefaultFMAP[i])) != COR_OK)
          return err;
      }
    }
    
    // calculate constant power part:

    if (twizy_max_rpm > rpm_0)
      rpm_d = (twizy_max_rpm - rpm_0 + (pmap_fib[6]>>1)) / pmap_fib[6];
    else
      rpm_d = 0;

    pwr_d = ((int)twizy_max_pwr_hi - (int)twizy_max_pwr_lo + (pmap_fib[5]>>1)) / pmap_fib[5];

    for (i=0; i<7; i++) {

      if (i<6)
        rpm = rpm_0 + (pmap_fib[i] * rpm_d);
      else
        rpm = twizy_max_rpm;

      if (i<5)
        pwr = twizy_max_pwr_lo + (pmap_fib[i] * pwr_d);
      else
        pwr = twizy_max_pwr_hi;

      trq = ((((uint32_t)pwr * 9549 + (rpm>>1)) / rpm) * 16 + 500) / 1000;

      if ((err = sc.Write(0x4611, 0x05+(i<<1), trq)) != COR_OK)
        return err;
      if ((err = sc.Write(0x4611, 0x06+(i<<1), rpm)) != COR_OK)
        return err;

    }

  }

  // commit map changes:
  if ((err = sc.Write(0x4641, 0x01, 1)) != COR_OK)
    return err;
  vTaskDelay(pdMS_TO_TICKS(50));

  return COR_OK;
}

#endif


// CfgReadMaxPower:
// read twizy_max_pwr_lo & twizy_max_pwr_hi & twizy_max_curr
// from SEVCON registers (only if necessary / undefined)
CANopenResult_t SevconClient::CfgReadMaxPower(void)
{
  SevconJob sc(this);
  CANopenResult_t err;
  uint32_t rpm, trq, curr;

  // the controller does not store max power, derive it from rpm/trq:

  if (twizy_max_pwr_lo == 0) {
    if ((err = sc.Read(0x4611, 0x04, rpm)) != COR_OK)
      return err;
    if ((err = sc.Read(0x4611, 0x03, trq)) != COR_OK)
      return err;
    if (trq == CFG.DefaultPMAP[2] && rpm == CFG.DefaultPMAP[3])
      twizy_max_pwr_lo = CFG.DefaultPwrLo;
    else
      twizy_max_pwr_lo = (((trq*1000)>>4) * rpm + (9549>>1)) / 9549;
  }

  if (twizy_max_pwr_hi == 0) {
    if ((err = sc.Read(0x4611, 0x12, rpm)) != COR_OK)
      return err;
    if ((err = sc.Read(0x4611, 0x11, trq)) != COR_OK)
      return err;
    if (trq == CFG.DefaultPMAP[16] && rpm == CFG.DefaultPMAP[17])
      twizy_max_pwr_hi = CFG.DefaultPwrHi;
    else
      twizy_max_pwr_hi = (((trq*1000)>>4) * rpm + (9549>>1)) / 9549;
  }

  // read max current level:
  
  if (twizy_max_curr == 0) {
    if ((err = sc.Read(0x4641, 0x02, curr)) != COR_OK)
      return err;
    twizy_max_curr = curr / CFG.DefaultCurrStatorMax;
  }
  
  return COR_OK;
}


CANopenResult_t SevconClient::CfgSpeed(int max_kph, int warn_kph)
// max_kph: 6..?, -1=reset to default (80)
// warn_kph: 6..?, -1=reset to default (89)
{
  SevconJob sc(this);
  CANopenResult_t err;
  uint32_t rpm, trq;

  // parameter validation:

  if (max_kph == -1)
    max_kph = CFG.DefaultKphMax;
  else if (max_kph < 6)
    return COR_ERR_ParamRange;

  if (warn_kph == -1)
    warn_kph = CFG.DefaultKphWarn;
  else if (warn_kph < 6)
    return COR_ERR_ParamRange;


  // get max torque for map scaling:

  if (twizy_max_trq == 0) {
    if ((err = sc.Read(0x6076, 0x00, trq)) != COR_OK)
      return err;
    twizy_max_trq = trq - CFG.DeltaMapTrq;
  }

  // get max power for map scaling:

  if ((err = CfgReadMaxPower()) != COR_OK)
    return err;
  
  // set overspeed warning range (STOP lamp):
  rpm = scale(CFG.DefaultRpmWarn, CFG.DefaultKphWarn, warn_kph, 400, 65535);
  if ((err = sc.Write(0x3813, 0x34, rpm)) != COR_OK) // lamp ON
    return err;
  if ((err = sc.Write(0x3813, 0x3c, rpm-CFG.DeltaWarnOff)) != COR_OK) // lamp OFF
    return err;

  // calc fwd rpm:
  rpm = scale(CFG.DefaultRpmMax, CFG.DefaultKphMax, max_kph, 400, 65535);

  // set fwd rpm:
  if (CarLocked())
    err = sc.Write(0x2920, 0x05, scale(CFG.DefaultRpmMax, CFG.DefaultKphMax, twizy_lock_speed, 0, 65535));
  else
    err = sc.Write(0x2920, 0x05, rpm);
  if (err)
    return err;

  // set rev rpm:
  if (CarLocked())
    err = sc.Write(0x2920, 0x06, scale(CFG.DefaultRpmMax, CFG.DefaultKphMax, twizy_lock_speed, 0, CFG.DefaultRpmRev));
  else
    err = sc.Write(0x2920, 0x06, LIMIT_MAX(rpm, CFG.DefaultRpmRev));
  if (err)
    return err;

  // adjust overspeed braking points (using fixed offsets):
  if ((err = sc.Write(0x3813, 0x33, rpm+CFG.DeltaBrkStart)) != COR_OK) // neutral braking start
    return err;
  if ((err = sc.Write(0x3813, 0x35, rpm+CFG.DeltaBrkEnd)) != COR_OK) // neutral braking end
    return err;
  if ((err = sc.Write(0x3813, 0x3b, rpm+CFG.DeltaBrkDown)) != COR_OK) // drive brakedown trigger
    return err;

  // adjust overspeed limits:
  if ((err = sc.Write(0x3813, 0x2d, rpm+CFG.DeltaBrkDown+1500)) != COR_OK) // neutral max speed
    return err;
  if ((err = sc.Write(0x4624, 0x00, rpm+CFG.DeltaBrkDown+2500)) != COR_OK) // severe overspeed fault
    return err;
  
  twizy_max_rpm = rpm;

  return COR_OK;
}


CANopenResult_t SevconClient::CfgPower(int trq_prc, int pwr_lo_prc, int pwr_hi_prc, int curr_prc)
// See "Twizy Powermap Calculator" spreadsheet.
// trq_prc: 10..130, -1=reset to default (100%)
// i.e. TWIZY80:
//    100% = 55.000 Nm
//    128% = 70.125 Nm (130 allowed for easy handling)
// pwr_lo_prc: 10..139, -1=reset to default (100%)
//    100% = 12182 W (mechanical)
//    139% = 16933 W (mechanical)
// pwr_hi_prc: 10..130, -1=reset to default (100%)
//    100% = 13000 W (mechanical)
//    130% = 16900 W (mechanical)
// curr_prc: 10..123, -1=reset to default current limits
//    100% = 450 A (Twizy 45: 270 A)
//    120% = 540 A (Twizy 45: 324 A)
//    123% = 540 A (Twizy 45: 330 A)
//    setting this to any value disables high limits of trq & pwr
//    (=> flux map extended to enable higher torque)
//    safety max limits = SEVCON model specific boost current level
{
  SevconJob sc(this);
  CANopenResult_t err;
  bool limited = false;

  // parameter validation:

  if (curr_prc == -1) {
    curr_prc = 100;
    limited = true;
  }
  else if (curr_prc < 10 || curr_prc > 123)
    return COR_ERR_ParamRange;

  if (trq_prc == -1)
    trq_prc = 100;
  else if (trq_prc < 10 || (limited && trq_prc > 130))
    return COR_ERR_ParamRange;

  if (pwr_lo_prc == -1)
    pwr_lo_prc = 100;
  else if (pwr_lo_prc < 10 || (limited && pwr_lo_prc > 139))
    return COR_ERR_ParamRange;

  if (pwr_hi_prc == -1)
    pwr_hi_prc = 100;
  else if (pwr_hi_prc < 10 || (limited && pwr_hi_prc > 130))
    return COR_ERR_ParamRange;

  // get max fwd rpm for map scaling:
  if (twizy_max_rpm == 0) {
    if ((err = sc.Read(0x2920, 0x05, twizy_max_rpm)) != COR_OK)
      return err;
  }

  // set current limits:
  twizy_max_curr = curr_prc;
  if ((err = sc.Write(0x4641, 0x02, scale(
          CFG.DefaultCurrStatorMax, 100, curr_prc, 0, CFG.BoostCurr))) != COR_OK)
    return err;
  if ((err = sc.Write(0x6075, 0x00, scale(
          CFG.DefaultCurrLim, 100, curr_prc, 0, CFG.BoostCurr*1000L))) != COR_OK)
    return err;

  // calc peak use torque:
  twizy_max_trq = scale(CFG.DefaultTrq, 100, trq_prc, 10000,
    (limited) ? CFG.DefaultTrqLim : 200000);

  // set peak use torque:
  if ((err = sc.Write(0x6076, 0x00, twizy_max_trq + CFG.DeltaMapTrq)) != COR_OK)
    return err;

  // set rated torque:
  if ((err = sc.Write(0x2916, 0x01, (trq_prc==100)
          ? CFG.DefaultTrqRated
          : (twizy_max_trq + CFG.DeltaMapTrq))) != COR_OK)
    return err;

  // calc peak use power:
  twizy_max_pwr_lo = scale(CFG.DefaultPwrLo, 100, pwr_lo_prc, 500,
    (limited) ? CFG.DefaultPwrLoLim : 200000);
  twizy_max_pwr_hi = scale(CFG.DefaultPwrHi, 100, pwr_hi_prc, 500,
    (limited) ? CFG.DefaultPwrHiLim : 200000);
  
  // set motor max power:
  if ((err = sc.Write(0x3813, 0x23, (pwr_lo_prc==100 && pwr_hi_prc==100)
          ? CFG.DefaultMaxMotorPwr
          : MAX(twizy_max_pwr_lo, twizy_max_pwr_hi)*0.353)) != COR_OK)
    return err;

  return COR_OK;
}


CANopenResult_t SevconClient::CfgTSMap(
  char map,
  int8_t t1_prc, int8_t t2_prc, int8_t t3_prc, int8_t t4_prc,
  int t1_spd, int t2_spd, int t3_spd, int t4_spd)
// map: 'D'=Drive 'N'=Neutral 'B'=Footbrake
//
// t1_prc: 0..100, -1=reset to default (D=100, N/B=100)
// t2_prc: 0..100, -1=reset to default (D=100, N/B=80)
// t3_prc: 0..100, -1=reset to default (D=100, N/B=50)
// t4_prc: 0..100, -1=reset to default (D=100, N/B=20)
//
// t1_spd: 0..?, -1=reset to default (D/N/B=33)
// t2_spd: 0..?, -1=reset to default (D/N/B=39)
// t3_spd: 0..?, -1=reset to default (D/N/B=50)
// t4_spd: 0..?, -1=reset to default (D/N/B=66)
{
  SevconJob sc(this);
  CANopenResult_t err;
  uint8_t base;
  uint32_t val, bndlo, bndhi;
  uint8_t todo;

  // parameter validation:

  if (map != 'D' && map != 'N' && map != 'B')
    return COR_ERR_ParamRange;

  // torque points:

  if ((t1_prc != -1) && (t1_prc < 0 || t1_prc > 100))
    return COR_ERR_ParamRange;
  if ((t2_prc != -1) && (t2_prc < 0 || t2_prc > 100))
    return COR_ERR_ParamRange;
  if ((t3_prc != -1) && (t3_prc < 0 || t3_prc > 100))
    return COR_ERR_ParamRange;
  if ((t4_prc != -1) && (t4_prc < 0 || t4_prc > 100))
    return COR_ERR_ParamRange;

  // speed points:

  if ((t1_spd != -1) && (t1_spd < 0))
    return COR_ERR_ParamRange;
  if ((t2_spd != -1) && (t2_spd < 0))
    return COR_ERR_ParamRange;
  if ((t3_spd != -1) && (t3_spd < 0))
    return COR_ERR_ParamRange;
  if ((t4_spd != -1) && (t4_spd < 0))
    return COR_ERR_ParamRange;

  // get map base subindex in SDO 0x3813:

  if (map=='B')
    base = 0x07;
  else if (map=='N')
    base = 0x1b;
  else // 'D'
    base = 0x24;

  // set:
  // we need to adjust point by point to avoid the "Param dyn range" alert,
  // ensuring a new speed has no conflict with the previous surrounding points

  todo = 0x0f;

  while (todo) {

    // point 1:
    if (todo & 0x01) {

      // get speed boundaries:
      bndlo = 0;
      if ((err = sc.Read(0x3813, base+3, bndhi)) != COR_OK)
        return err;

      // calc new speed:
      if (t1_spd >= 0)
        val = scale(CFG.DefaultMapSpd[0], 33, t1_spd, 0, 65535);
      else
        val = (map=='D') ? 3000 : CFG.DefaultMapSpd[0];

      if (val >= bndlo && val <= bndhi) {
        // ok, change:
        if ((err = sc.Write(0x3813, base+1, val)) != COR_OK)
          return err;

        if (t1_prc >= 0)
          val = scale(32767, 100, t1_prc, 0, 32767);
        else
          val = 32767;
        if ((err = sc.Write(0x3813, base+0, val)) != COR_OK)
          return err;

        todo &= ~0x01;
      }
    }

    // point 2:
    if (todo & 0x02) {

      // get speed boundaries:
      if ((err = sc.Read(0x3813, base+1, bndlo)) != COR_OK)
        return err;
      if ((err = sc.Read(0x3813, base+5, bndhi)) != COR_OK)
        return err;

      // calc new speed:
      if (t2_spd >= 0)
        val = scale(CFG.DefaultMapSpd[1], 39, t2_spd, 0, 65535);
      else
        val = (map=='D') ? 3500 : CFG.DefaultMapSpd[1];

      if (val >= bndlo && val <= bndhi) {
        // ok, change:
        if ((err = sc.Write(0x3813, base+3, val)) != COR_OK)
          return err;

        if (t2_prc >= 0)
          val = scale(32767, 100, t2_prc, 0, 32767);
        else
          val = (map=='D') ? 32767 : 26214;
        if ((err = sc.Write(0x3813, base+2, val)) != COR_OK)
          return err;

        todo &= ~0x02;
      }
    }

    // point 3:
    if (todo & 0x04) {

      // get speed boundaries:
      if ((err = sc.Read(0x3813, base+3, bndlo)) != COR_OK)
        return err;
      if ((err = sc.Read(0x3813, base+7, bndhi)) != COR_OK)
        return err;

      // calc new speed:
      if (t3_spd >= 0)
        val = scale(CFG.DefaultMapSpd[2], 50, t3_spd, 0, 65535);
      else
        val = (map=='D') ? 4500 : CFG.DefaultMapSpd[2];

      if (val >= bndlo && val <= bndhi) {
        // ok, change:
        if ((err = sc.Write(0x3813, base+5, val)) != COR_OK)
          return err;

        if (t3_prc >= 0)
          val = scale(32767, 100, t3_prc, 0, 32767);
        else
          val = (map=='D') ? 32767 : 16383;
        if ((err = sc.Write(0x3813, base+4, val)) != COR_OK)
          return err;

        todo &= ~0x04;
      }
    }

    // point 4:
    if (todo & 0x08) {

      // get speed boundaries:
      if ((err = sc.Read(0x3813, base+5, bndlo)) != COR_OK)
        return err;
      bndhi = 65535;

      // calc new speed:
      if (t4_spd >= 0)
        val = scale(CFG.DefaultMapSpd[3], 66, t4_spd, 0, 65535);
      else
        val = (map=='D') ? 6000 : CFG.DefaultMapSpd[3];

      if (val >= bndlo && val <= bndhi) {
        // ok, change:
        if ((err = sc.Write(0x3813, base+7, val)) != COR_OK)
          return err;

        if (t4_prc >= 0)
          val = scale(32767, 100, t4_prc, 0, 32767);
        else
          val = (map=='D') ? 32767 : 6553;
        if ((err = sc.Write(0x3813, base+6, val)) != COR_OK)
          return err;

        todo &= ~0x08;
      }
    }

  } // while (todo)


  return COR_OK;
}


CANopenResult_t SevconClient::CfgDrive(int max_prc, int autodrive_ref, int autodrive_minprc, bool async /*=false*/)
// max_prc: 10..100, -1=reset to default (100)
// autodrive_ref: 0..250, -1=default (off) (not a direct SEVCON control)
//      sets power 100% ref point to <autodrive_ref> * 100 W
// autodrive_minprc: 0..100, -1=default (off) (not a direct SEVCON control)
//      sets lower limit on auto power adjustment
// async: true = do asynchronous SDO write, don't wait for ACK
{
  CANopenResult_t err;

  // parameter validation:

  if (max_prc == -1)
    max_prc = 100;
  else if (max_prc < 10 || max_prc > 100)
    return COR_ERR_ParamRange;

  if (autodrive_ref == -1)
    ;
  else if (autodrive_ref < 0 || autodrive_ref > 250)
    return COR_ERR_ParamRange;

  if (autodrive_minprc == -1)
    autodrive_minprc = 0;
  else if (autodrive_minprc < 0 || autodrive_minprc > 100)
    return COR_ERR_ParamRange;

  if ((autodrive_ref > 0) && (!m_twizy->twizy_flags.DisableAutoPower))
  {
    // calculate max drive level:
    twizy_autodrive_level = LIMIT_MAX(((long) m_twizy->twizy_batt[0].max_drive_pwr * 5L * 1000L)
            / autodrive_ref, 1000);
    twizy_autodrive_level = LIMIT_MIN(twizy_autodrive_level, autodrive_minprc * 10);

    // set autopower checkpoint:
    twizy_autodrive_checkpoint = AUTOPOWER_CHECKPOINT(m_twizy->twizy_batt[0].max_drive_pwr, autodrive_ref, autodrive_minprc);
  }
  else
  {
    twizy_autodrive_level = 1000;
  }

  // set:
  m_drive_level = LIMIT_MAX(max_prc*10, twizy_autodrive_level);
  if (async) {
    err = SendWrite(0x2920, 0x01, &m_drive_level);
    if (err == COR_WAIT)
      err = COR_OK;
  } else {
    SevconJob sc(this);
    err = sc.Write(0x2920, 0x01, m_drive_level);
  }

  return err;
}


CANopenResult_t SevconClient::CfgRecup(int neutral_prc, int brake_prc, int autorecup_ref, int autorecup_minprc)
// neutral_prc: 0..100, -1=reset to default (18)
// brake_prc: 0..100, -1=reset to default (18)
// autorecup_ref: 0..250, -1=default (off) (not a direct SEVCON control)
//      ATT: parameter function changed in V3.6.0!
//      now sets power 100% ref point to <autorecup_ref> * 100 W
// autorecup_minprc: 0..100, -1=default (off) (not a direct SEVCON control)
//      sets lower limit on auto power adjustment
{
  SevconJob sc(this);
  CANopenResult_t err;
  uint16_t level;

  // parameter validation:

  if (neutral_prc == -1)
    neutral_prc = CFG.DefaultRecupPrc;
  else if (neutral_prc < 0 || neutral_prc > 100)
    return COR_ERR_ParamRange;

  if (brake_prc == -1)
    brake_prc = CFG.DefaultRecupPrc;
  else if (brake_prc < 0 || brake_prc > 100)
    return COR_ERR_ParamRange;

  if (autorecup_ref == -1)
    ;
  else if (autorecup_ref < 0 || autorecup_ref > 250)
    return COR_ERR_ParamRange;

  if (autorecup_minprc == -1)
    autorecup_minprc = 0;
  else if (autorecup_minprc < 0 || autorecup_minprc > 100)
    return COR_ERR_ParamRange;

  if ((autorecup_ref > 0) && (!m_twizy->twizy_flags.DisableAutoPower))
  {
    // calculate max recuperation level:
    twizy_autorecup_level = LIMIT_MAX(((long) m_twizy->twizy_batt[0].max_recup_pwr * 5L * 1000L)
            / autorecup_ref, 1000);
    twizy_autorecup_level = LIMIT_MIN(twizy_autorecup_level, autorecup_minprc * 10);
    
    // set autopower checkpoint:
    twizy_autorecup_checkpoint = AUTOPOWER_CHECKPOINT(m_twizy->twizy_batt[0].max_recup_pwr, autorecup_ref, autorecup_minprc);
  }
  else
  {
    twizy_autorecup_level = 1000;
  }

  // set neutral recup level:
  level = scale(CFG.DefaultRecup, CFG.DefaultRecupPrc, neutral_prc, 0, 1000);
  if (twizy_autorecup_level != 1000)
      level = (((long) level) * twizy_autorecup_level) / 1000;
  if ((err = sc.Write(0x2920, 0x03, level)) != COR_OK)
    return err;
  
  // set brake recup level:
  level = scale(CFG.DefaultRecup, CFG.DefaultRecupPrc, brake_prc, 0, 1000);
  if (twizy_autorecup_level != 1000)
      level = (((long) level) * twizy_autorecup_level) / 1000;
  if ((err = sc.Write(0x2920, 0x04, level)) != COR_OK)
    return err;

  return COR_OK;
}


// Auto recup & drive power update function:
// check for BMS max pwr change, update SEVCON settings accordingly
// this is called by state_ticker1() = approx. once per second
void SevconClient::CfgAutoPower(void)
{
  int ref, minprc;
  CANopenResult_t err;

  // check for SEVCON write access:
  if ((!m_twizy->twizy_flags.EnableWrite) || (m_twizy->twizy_flags.DisableAutoPower)
          || ((m_twizy->twizy_status & CAN_STATUS_KEYON) == 0)
          || !CtrlLoggedIn() || CtrlCfgMode())
    return;

  // adjust recup levels?
  ref = cfgparam(autorecup_ref);
  minprc = cfgparam(autorecup_minprc);
  if (minprc == -1)
    minprc = 0;
  if ((ref > 0)
    && (twizy_autorecup_checkpoint != AUTOPOWER_CHECKPOINT(m_twizy->twizy_batt[0].max_recup_pwr, ref, minprc)))
  {
    err = CfgRecup(cfgparam(neutral), cfgparam(brake), ref, minprc);
    ESP_LOGD(TAG, "CfgAutoPower: recup level adjusted; max_recup_pwr=%d level=%.1f; result=%s",
      m_twizy->twizy_batt[0].max_recup_pwr*500, (float)twizy_autorecup_level/10, GetResultString(err).c_str());
  }
  
  // adjust drive level?
  ref = cfgparam(autodrive_ref);
  minprc = cfgparam(autodrive_minprc);
  if (minprc == -1)
    minprc = 0;
  if ((ref > 0)
    && (twizy_autodrive_checkpoint != AUTOPOWER_CHECKPOINT(m_twizy->twizy_batt[0].max_drive_pwr, ref, minprc))
    && (m_twizy->twizy_kickdown_hold == 0))
  {
    err = CfgDrive(cfgparam(drive), ref, minprc);
    ESP_LOGD(TAG, "CfgAutoPower: drive level adjusted; max_drive_pwr=%d level=%.1f; result=%s",
      m_twizy->twizy_batt[0].max_drive_pwr*500, (float)twizy_autodrive_level/10, GetResultString(err).c_str());
  }
}


CANopenResult_t SevconClient::CfgRamps(int start_prm, int accel_prc, int decel_prc, int neutral_prc, int brake_prc)
// start_prm: 1..250, -1=reset to default (40) (Att! changed in V3.4 from prc to prm!)
// accel_prc: 1..100, -1=reset to default (25)
// decel_prc: 0..100, -1=reset to default (20)
// neutral_prc: 0..100, -1=reset to default (40)
// brake_prc: 0..100, -1=reset to default (40)
{
  SevconJob sc(this);
  CANopenResult_t err;

  // parameter validation:

  if (start_prm == -1)
    start_prm = CFG.DefaultRampStartPrm;
  else if (start_prm < 1 || start_prm > 250)
    return COR_ERR_ParamRange;

  if (accel_prc == -1)
    accel_prc = CFG.DefaultRampAccelPrc;
  else if (accel_prc < 1 || accel_prc > 100)
    return COR_ERR_ParamRange;

  if (decel_prc == -1)
    decel_prc = 20;
  else if (decel_prc < 0 || decel_prc > 100)
    return COR_ERR_ParamRange;

  if (neutral_prc == -1)
    neutral_prc = 40;
  else if (neutral_prc < 0 || neutral_prc > 100)
    return COR_ERR_ParamRange;

  if (brake_prc == -1)
    brake_prc = 40;
  else if (brake_prc < 0 || brake_prc > 100)
    return COR_ERR_ParamRange;

  // set:

  if ((err = sc.Write(0x291c, 0x02, scale(CFG.DefaultRampStart, CFG.DefaultRampStartPrm, start_prm, 10, 10000))) != COR_OK)
    return err;
  if ((err = sc.Write(0x2920, 0x07, scale(CFG.DefaultRampAccel, CFG.DefaultRampAccelPrc, accel_prc, 10, 10000))) != COR_OK)
    return err;
  if ((err = sc.Write(0x2920, 0x0b, scale(2000, 20, decel_prc, 10, 10000))) != COR_OK)
    return err;
  if ((err = sc.Write(0x2920, 0x0d, scale(4000, 40, neutral_prc, 10, 10000))) != COR_OK)
    return err;
  if ((err = sc.Write(0x2920, 0x0e, scale(4000, 40, brake_prc, 10, 10000))) != COR_OK)
    return err;

  return COR_OK;
}


CANopenResult_t SevconClient::CfgRampLimits(int accel_prc, int decel_prc)
// accel_prc: 1..100, -1=reset to default (30)
// decel_prc: 0..100, -1=reset to default (30)
{
  SevconJob sc(this);
  CANopenResult_t err;

  // parameter validation:

  if (accel_prc == -1)
    accel_prc = 30;
  else if (accel_prc < 1 || accel_prc > 100)
    return COR_ERR_ParamRange;

  if (decel_prc == -1)
    decel_prc = 30;
  else if (decel_prc < 0 || decel_prc > 100)
    return COR_ERR_ParamRange;

  // set:

  if ((err = sc.Write(0x2920, 0x0f, scale(6000, 30, accel_prc, 0, 20000))) != COR_OK)
    return err;
  if ((err = sc.Write(0x2920, 0x10, scale(6000, 30, decel_prc, 0, 20000))) != COR_OK)
    return err;

  return COR_OK;
}


CANopenResult_t SevconClient::CfgSmoothing(int prc)
// prc: 0..100, -1=reset to default (70)
{
  SevconJob sc(this);
  CANopenResult_t err;

  // parameter validation:

  if (prc == -1)
    prc = 70;
  else if (prc < 0 || prc > 100)
    return COR_ERR_ParamRange;

  // set:
  if ((err = sc.Write(0x290a, 0x01, 1+(prc/10))) != COR_OK)
    return err;
  if ((err = sc.Write(0x290a, 0x03, scale(800, 70, prc, 0, 1000))) != COR_OK)
    return err;

  return COR_OK;
}


CANopenResult_t SevconClient::CfgBrakelight(int on_lev, int off_lev)
// *** NOT FUNCTIONAL WITHOUT HARDWARE MODIFICATION ***
// *** SEVCON cannot control Twizy brake lights ***
// on_lev: 0..100, -1=reset to default (100=off)
// off_lev: 0..100, -1=reset to default (100=off)
// on_lev must be >= off_lev
// ctrl bit in 0x2910.1 will be set/cleared accordingly
{
  SevconJob sc(this);
  CANopenResult_t err;
  uint32_t flags;

  // parameter validation:

  if (on_lev == -1)
    on_lev = 100;
  else if (on_lev < 0 || on_lev > 100)
    return COR_ERR_ParamRange;

  if (off_lev == -1)
    off_lev = 100;
  else if (off_lev < 0 || off_lev > 100)
    return COR_ERR_ParamRange;

  if (on_lev < off_lev)
    return COR_ERR_ParamRange;

  // set range:
  if ((err = sc.Write(0x3813, 0x05, scale(1024, 100, off_lev, 64, 1024))) != COR_OK)
    return err;
  if ((err = sc.Write(0x3813, 0x06, scale(1024, 100, on_lev, 64, 1024))) != COR_OK)
    return err;

  // set ctrl bit:
  if ((err = sc.Read(0x2910, 0x01, flags)) != COR_OK)
    return err;
  if (on_lev != 100 || off_lev != 100)
    flags |= 0x2000;
  else
    flags &= ~0x2000;
  if ((err = sc.Write(0x2910, 0x01, flags)) != COR_OK)
    return err;

  return COR_OK;
}


// CalcProfileChecksum: get checksum for m_profile
//
// Note: for extendability of struct m_profile, 0-Bytes will
//  not affect the checksum, so new fields can simply be added at the end
//  without losing version compatibility.
//  (0-bytes translate to value -1 = default)
uint8_t SevconClient::CalcProfileChecksum(cfg_profile& profile)
{
  uint16_t checksum;
  uint8_t i;

  checksum = 0x0101; // version tag

  for (i=1; i<sizeof(m_profile); i++)
    checksum += ((uint8_t*)&profile)[i];

  if ((checksum & 0x0ff) == 0)
    checksum >>= 8;

  return (checksum & 0x0ff);
}


// GetParamProfile: read profile from param
//    if checksum invalid initializes to default config & returns false
bool SevconClient::GetParamProfile(uint8_t key, cfg_profile& profile)
{
  if (key >= 1 && key <= 99) {
    // read custom profile from config:
    char name[12];
    sprintf(name, "profile%02d", key);
    string stored_profile = MyConfig.GetParamValueBinary("xrt", name, "", Encoding_BASE64);
    memset(&profile, 0, sizeof(profile));
    memcpy(&profile, stored_profile.data(), MIN(stored_profile.size(), sizeof(profile)));
    
    // check consistency:
    if (profile.checksum == CalcProfileChecksum(profile))
      return true;
    else {
      // init to defaults:
      memset(&profile, 0, sizeof(profile));
      return false;
    }
  }
  else {
    // no custom cfg: load defaults
    memset(&profile, 0, sizeof(profile));
    return true;
  }
}


// SetParamProfile: write profile to param
//    with checksum calculation
bool SevconClient::SetParamProfile(uint8_t key, cfg_profile& profile)
{
  if (key >= 1 && key <= 99) {
    // update profile checksum:
    profile.checksum = CalcProfileChecksum(profile);
    
    // write profile to config:
    char name[12];
    sprintf(name, "profile%02d", key);
    string stored_profile((char*) &profile, sizeof(profile));
    MyConfig.SetParamValueBinary("xrt", name, stored_profile, Encoding_BASE64);
    
    return true;
  }
  else {
    return false;
  }
}


// CfgApplyProfile: configure current profile
//    return value: 0 = no error, else error code
//    sets: m_drivemode.profile_cfgmode, m_drivemode.profile_user
CANopenResult_t SevconClient::CfgApplyProfile(uint8_t key)
{
  SevconJob sc(this);
  CANopenResult_t err;
  int pval;

  // clear success flag:
  m_drivemode.applied = 0;
  
  // login:
  
  if ((err = sc.Login(true)) != COR_OK)
    return err;
  
  // update op (user) mode params:

  if ((err = CfgDrive(cfgparam(drive), cfgparam(autodrive_ref), cfgparam(autodrive_minprc))) != COR_OK)
    return err;

  if ((err = CfgRecup(cfgparam(neutral), cfgparam(brake), cfgparam(autorecup_ref), cfgparam(autorecup_minprc))) != COR_OK)
    return err;

  if ((err = CfgRamps(cfgparam(ramp_start), cfgparam(ramp_accel), cfgparam(ramp_decel), cfgparam(ramp_neutral), cfgparam(ramp_brake))) != COR_OK)
    return err;

  if ((err = CfgRampLimits(cfgparam(ramplimit_accel), cfgparam(ramplimit_decel))) != COR_OK)
    return err;

  if ((err = CfgSmoothing(cfgparam(smooth))) != COR_OK)
    return err;

  // update user profile status:
  m_drivemode.profile_user = key;
  MyConfig.SetParamValueInt("xrt", "profile_user", m_drivemode.profile_user);

  
  // update pre-op (admin) mode params if configmode possible at the moment:

  err = sc.CfgMode(true);
  
  if (!err) {

    err = CfgSpeed(cfgparam(speed), cfgparam(warn));
    if (err == COR_OK)
      err = CfgPower(cfgparam(torque), cfgparam(power_low), cfgparam(power_high), cfgparam(current));
    if (err == COR_OK)
      err = CfgMakePowermap();

    if (err == COR_OK)
      err = CfgTSMap('D',
              cfgparam(tsmap[0].prc1), cfgparam(tsmap[0].prc2), cfgparam(tsmap[0].prc3), cfgparam(tsmap[0].prc4),
              cfgparam(tsmap[0].spd1), cfgparam(tsmap[0].spd2), cfgparam(tsmap[0].spd3), cfgparam(tsmap[0].spd4));
    if (err == COR_OK)
      err = CfgTSMap('N',
              cfgparam(tsmap[1].prc1), cfgparam(tsmap[1].prc2), cfgparam(tsmap[1].prc3), cfgparam(tsmap[1].prc4),
              cfgparam(tsmap[1].spd1), cfgparam(tsmap[1].spd2), cfgparam(tsmap[1].spd3), cfgparam(tsmap[1].spd4));
    if (err == COR_OK)
      err = CfgTSMap('B',
              cfgparam(tsmap[2].prc1), cfgparam(tsmap[2].prc2), cfgparam(tsmap[2].prc3), cfgparam(tsmap[2].prc4),
              cfgparam(tsmap[2].spd1), cfgparam(tsmap[2].spd2), cfgparam(tsmap[2].spd3), cfgparam(tsmap[2].spd4));

    if (err == COR_OK)
      err = CfgBrakelight(cfgparam(brakelight_on), cfgparam(brakelight_off));

    // update cfgmode profile status:
    if (err == COR_OK) {
      m_drivemode.profile_cfgmode = key;
      MyConfig.SetParamValueInt("xrt", "profile_cfgmode", m_drivemode.profile_cfgmode);
    }

    // switch back to op-mode (5 tries):
    key = 5;
    while (sc.CfgMode(false) != COR_OK) {
      if (--key == 0)
        break;
      vTaskDelay(pdMS_TO_TICKS(100));
    }

  } else {
    
    // pre-op mode currently not possible;
    // just set speed limit:
    if (!CarLocked()) {
      pval = cfgparam(speed);
      if (pval == -1)
        pval = CFG.DefaultKphMax;

      SevconJob sc(this);
      err = sc.Write(0x2920, 0x05, scale(CFG.DefaultRpmMax, CFG.DefaultKphMax, pval, 0, 65535));
      if (err == COR_OK)
        err = sc.Write(0x2920, 0x06, scale(CFG.DefaultRpmMax, CFG.DefaultKphMax, pval, 0, CFG.DefaultRpmRev));
    }
    
  }
  
  // success:
  m_drivemode.applied = 1;
  MyEvents.SignalEvent("vehicle.drivemode.changed", NULL);

  // reset error cnt:
  m_buttoncnt = 0;

  // reset kickdown detection:
  m_twizy->twizy_kickdown_hold = 0;
  m_twizy->twizy_kickdown_level = 0;

  return err;
}


// CfgSwitchProfile: load and configure a profile
//    return value: 0 = no error, else error code
//    sets: m_drivemode.profile_cfgmode, m_drivemode.profile_user
CANopenResult_t SevconClient::CfgSwitchProfile(uint8_t key)
{
  ESP_LOGI(TAG, "CfgSwitchProfile: key=%d", key);
  
  // check key:
  if (key > 99)
    return COR_ERR_ParamRange;

  // load profile:
  GetParamProfile(key, m_profile);
  m_drivemode.unsaved = 0;
  
  // try to apply profile:
  return CfgApplyProfile(key);
}


string SevconClient::FmtSwitchProfileResult(CANopenResult_t res)
{
  ostringstream buf;
  
  if (res == COR_OK)
    buf << "OK, tuning changed";
  else if (res == COR_ERR_StateChangeFailed)
    buf << "Live tuning changed, base tuning needs STOP mode";
  else
    buf << "Failed: " << GetResultString(res);
  
  if (res != COR_OK) {
    buf << "\nActive profile: ";
    if (m_drivemode.profile_user == m_drivemode.profile_cfgmode) {
      buf << "#" << m_drivemode.profile_user;
    }
    else {
      buf << "base #" << m_drivemode.profile_cfgmode;
      buf << ", live #" << m_drivemode.profile_user;
    }
  }
  
  return buf.str();
}


void SevconClient::Kickdown(bool engage)
{
  //int kickdown_led = 0;
  CANopenResult_t res;
  
  if (!CtrlLoggedIn() || CtrlCfgMode()) {
    ESP_LOGD(TAG, "Kickdown %d: ignored, not logged in or in cfg mode", engage);
    return;
  }
  
  if (engage) {
    // engage kickdown:
    if ((m_twizy->twizy_kickdown_hold == 0) && (m_twizy->twizy_kickdown_level > 0)
            && (m_twizy->twizy_flags.EnableWrite)
            && (!m_twizy->twizy_flags.DisableKickdown)
            && (m_twizy->cfg_kd_threshold > 0)
            && (((cfgparam(drive) != -1) && (cfgparam(drive) != 100))
              || (twizy_autodrive_level < 1000))
            ) {
      // set drive level to boost (100%, no autopower limit):
      res = CfgDrive(100, -1, -1, true);
      if (res == COR_OK) {
        m_twizy->twizy_kickdown_hold = TWIZY_KICKDOWN_HOLDTIME;
        xTimerStart(m_kickdown_timer, 0);
        MyEvents.SignalEvent("vehicle.kickdown.engaged", NULL);
        //kickdown_led = 1;
      } else {
        ESP_LOGD(TAG, "Kickdown engage failed: %s", GetResultString(res).c_str());
      }
    }
  }
  else {
    // release kickdown:
    if ((int)m_twizy->twizy_accel_pedal <= ((int)m_twizy->twizy_kickdown_level
            - (m_twizy->KickdownThreshold(m_twizy->twizy_kickdown_level) / 2))) {
      // releasing, count down:
      if (--m_twizy->twizy_kickdown_hold == 0) {
        // reset drive level to normal:
        res = CfgDrive(cfgparam(drive), cfgparam(autodrive_ref), cfgparam(autodrive_minprc), true);
        if (res != COR_OK) {
          m_twizy->twizy_kickdown_hold = 2; // error: retry
          ESP_LOGD(TAG, "Kickdown release failed: %s", GetResultString(res).c_str());
        }
        else {
          m_twizy->twizy_kickdown_level = 0; // ok, reset kickdown detection
          xTimerStop(m_kickdown_timer, 0);
          MyEvents.SignalEvent("vehicle.kickdown.released", NULL);
        }
      }
    }
    else {
      // not releasing, reset counter:
      m_twizy->twizy_kickdown_hold = TWIZY_KICKDOWN_HOLDTIME;
    }

    // show kickdown state on SimpleConsole, flash last 2 hold seconds:
    if (m_twizy->twizy_kickdown_hold > 20) {
      //kickdown_led = 1;
    }
    else if (m_twizy->twizy_kickdown_hold == 20) {
      //kickdown_led = 1;
      MyEvents.SignalEvent("vehicle.kickdown.releasing", NULL);
    }
    else {
      //kickdown_led = (m_twizy->twizy_kickdown_hold & 2) >> 1;
    }
  }
  
  // TODO: map kickdown_led to port, send CAN status frame
}

void SevconClient::KickdownTimer(TimerHandle_t timer)
{
  SevconClient* me = SevconClient::GetInstance();
  if (me)
    me->Kickdown(false);
  else
    xTimerStop(timer, 0);
}
