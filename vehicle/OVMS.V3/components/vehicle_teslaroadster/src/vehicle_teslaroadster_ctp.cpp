//  Created by Tom Saxton on 2/22/13.
//  Updated for OVMS v3 on 5/14/2018
//
//  Copyright (c) 2013-2018 Tom Saxton. License granted for inclusion on OVMS.

#include <stdio.h>
#include <math.h>

#include "ovms_log.h"
static const char *TAG = "v-teslaroadster";

int vehicle_teslaroadster_minutestocharge(
    unsigned char chgmod,    // charge mode: 0 (Standard), 3 (Range) or 4 (Performance)
    int wAvailWall,          // watts available from the wall
    int imStart,             // ideal mi at start of charge
    int imTarget,            // ideal mi desired at end of charge (use <=0 for full charge)
    int pctTarget,           // if imTarget == -1, specified desired percent SOC
                             // use 100 for full charge
                             // pctTarget is ignored if imEnd != -1
    float cac,               // the battery pack CAC*100 (pass 0 if unknown)
    signed char degAmbient,  // ambient temperature in degrees C
    int *pimTarget           // if not null, will be filled in with expected ideal
                             // miles at target charge level
    )
{
float kwAvail;
float bIntercept;
float mSlope;
float whPerIM;
float secPerIMSteady;
float seconds;
int imCapacityNominal;
int imCapacity;
int imTaperBase;
float numTaper;
float denBaseTaper;

// convert watts to kilowatts
kwAvail = wAvailWall * 0.001;

if (cac == 0) cac=160.0; // Default to a nominal new battery pack
if (pctTarget <= 0) pctTarget=100;   // Default to a full charge

switch (chgmod)
  {
  case 0: // Standard
  // standard mode IM capacity is about 1.1891 * CAC + 0.8 (10/10/2013 survey data)
    imCapacity = (int)(1.1891 * cac + 0.8 + 0.5);

     // algorithm parameters, determined from OVMS Log Data, 2418 data points
    imCapacityNominal = 191;
    imTaperBase = 169;
    numTaper = 1975.78;
    denBaseTaper = 197.77;
    break;

  case 3: // Range
    // range mode IM capacity is about 1.5463 * CAC - 3.4 (10/10/2013 survey data)
    imCapacity = (int)(1.5463 * cac - 3.4 + 0.5);

    // algorithm parameters, determined from OVMS Log Data, 234 data points
    imCapacityNominal = 244;
    imTaperBase = 215;
    numTaper = 2121.56;
    denBaseTaper = 246.47;
    break;

  case 4: // Performance
    // interpolate standard and range to get performance mode
    // IM = 1.3677 * CAC - 1.3
    imCapacity = (int)(1.3677 * cac - 1.3 + 0.5);

    // algorithm parameters, determined from OVMS Log Data, 13 data points
    imCapacityNominal = 218;
    imTaperBase = 167;
    numTaper = 4292.04;
    denBaseTaper = 231.22;
    break;

  default: // invalid charge mode passed in (Storage mode doesn't make sense)
    return -(int)(100+chgmod);
  }

// if needed, calculate charge target from specified percent
if (imTarget <= 0)
  imTarget = (imCapacity * pctTarget + 50)/100;

// tell the caller the expected ideal miles at the requested charge level
if (pimTarget != NULL)
  *pimTarget = imTarget;

// check for silly cases
if (kwAvail <= 0.0)
  return -2;
if (imTarget <= imStart)
  return -3;

// I don't believe air temperatures above 60 C, and this avoids overflow issues
if (degAmbient > 60)
  degAmbient = 60;

// normalize the IM values to look like a nominal new pack
imStart  += imCapacityNominal - imCapacity;
imTarget += imCapacityNominal - imCapacity;
if (imTarget > imCapacityNominal)
  imTarget = imCapacityNominal;

// calculate temperature to charge rate equation using a linear function
// that converts ambient temperature to watthours per ideal miles
// b2=IF(B20>2.3,287.67,745.69-199.08*B20)
bIntercept = (kwAvail > 2.3) ? 287.67 : 745.69-199.08*kwAvail;
// m2=B20*0.0019-0.0297
mSlope = 3.5882-0.2498*kwAvail;

// the data says that 70A gets slightly faster in high heat,
// but I think that's an anomoly in the small data set,
// so take that out of the model
if (mSlope < 0)
  mSlope = 0;

// calculate seconds per ideal mile
whPerIM = bIntercept + mSlope * degAmbient;
secPerIMSteady = 3.600 * whPerIM / kwAvail;

// detect implausible low power values that can lead to overflowing the number of minutes
if ((0x7FFFL*60+30)/secPerIMSteady < 244)
  return -4;

// ready to calculate the charge duration
seconds = 0;

// in the taper region, the model predicts that the time to add another
// IM of range is:
//
// secPerIMTaper = numTaper/(denBaseTaper - im)
//
// run into the taper phase until the taper rate is lower than the steady rate
int imTaperStart = imTaperBase;
{
  int imTaperT = denBaseTaper - numTaper/secPerIMSteady;
  if (imTaperT > imTaperStart)
    imTaperStart = imTaperT;
}

// calculate time spent in the steady charge region
if (imStart < imTaperStart)
  {
  int imEndSteady = imTarget < imTaperStart ? imTarget : imTaperStart;
  seconds += secPerIMSteady * (imEndSteady - imStart);
  }

// figure out time spent in the tapered charge region
if (imTarget > imTaperStart)
  {
  // we may have started the charge inside the taper region
  int imCur = imStart > imTaperStart ? imStart : imTaperStart;

  // We put a limit on how low the charge rate will go,
  // ie, a maximum value on the number of seconds needed to add
  // another IM.
  float secPerIMMost = 1117.1 / (kwAvail > 2.0 ? 2.0 : kwAvail);
  int imTaperEnd = denBaseTaper - numTaper/secPerIMMost;
  if (imTarget > imTaperEnd)
  {
    // case where we started the charge after then end of tapering (top off, perhaps)
  if (imCur > imTaperEnd)
    imTaperEnd = imCur;
  seconds += (imTarget - imTaperEnd) * secPerIMMost;
    imTarget = imTaperEnd;
}

  // since the time function is strictly increasing,
  // shift down by half an IM so the integration
  // is closer summing the left edge rectangles
  if (imCur < imTaperEnd)
  seconds += numTaper * log((denBaseTaper - (imCur - 0.5))/(denBaseTaper - (imTarget - 0.5)));
  }

ESP_LOGD(TAG,"CTP(%d,%d,%d,%d,%d,%0.2f,%d)=%0.1f minutes",
  chgmod, wAvailWall, imStart, imTarget, pctTarget, cac, degAmbient,
  (seconds + 30) / 60);

return (int)((seconds + 30) / 60);
}
