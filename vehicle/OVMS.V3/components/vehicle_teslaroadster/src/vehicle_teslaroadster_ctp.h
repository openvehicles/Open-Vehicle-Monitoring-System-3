//  Created by Tom Saxton on 2/22/13.
//  Updated for OVMS v3 on 5/14/2018
//
//  Copyright (c) 2013-2018 Tom Saxton. License granted for inclusion on OVMS.

#ifndef __VEHICLE_TESLAROADSTER_CTP_H__
#define __VEHICLE_TESLAROADSTER_CTP_H__

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
    );

#endif //#ifndef __VEHICLE_TESLAROADSTER_CTP_H__
