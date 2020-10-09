/*
;    Project:       Open Vehicle Monitor System
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris van der Meijden
;    (C) 2020       Soko
;    (C) 2020       sharkcow <sharkcow@gmx.de>
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

#ifndef __VEHICLE_EUP_OBD_H__
#define __VEHICLE_EUP_OBD_H__

#include "vehicle.h"
#include "ovms_webserver.h"
#include "poll_reply_helper.h"

using namespace std;

// ECUs TX/RX
#define VWUP_MOT_ELEC_TX 0x7E0  //ECU 01 motor elecronics
#define VWUP_MOT_ELEC_RX 0x7E8
#define VWUP_BAT_MGMT_TX 0x7E5  //ECU 8C hybrid battery management
#define VWUP_BAT_MGMT_RX 0x7ED
#define VWUP_ELD_TX 0x7E6  //ECU 51 electric drive
#define VWUP_ELD_RX 0x7EE
#define VWUP_CHG_TX 0x744 //ECU C6 high voltage charger
#define VWUP_CHG_RX 0x7AE
#define VWUP_MFD_TX 0x714 //ECU 17 multi-function display
#define VWUP_MFD_RX 0x77E
#define VWUP_BRK_TX 0x713 //ECU 03 brake electronics
#define VWUP_BRK_RX 0x77D


// PIDs of ECUs
#define VWUP_MOT_ELEC_SOC_NORM 0x1164
#define VWUP_BAT_MGMT_U 0x1E3B
#define VWUP_BAT_MGMT_I 0x1E3D
#define VWUP_BAT_MGMT_SOC_ABS 0x028C
#define VWUP_BAT_MGMT_ENERGY_COUNTERS 0x1E32
#define VWUP_BAT_MGMT_CELL_MAX 0x1E33
#define VWUP_BAT_MGMT_CELL_MIN 0x1E34
#define VWUP_BAT_MGMT_TEMP 0x2A0B
#define VWUP_CHG_EXTDIAG 0x03
#define VWUP_CHG_POWER_EFF 0x15D6
#define VWUP_CHG_POWER_LOSS 0x15E1
#define VWUP1_CHG_AC_U 0x1DA9
#define VWUP1_CHG_AC_I 0x1DA8
#define VWUP1_CHG_DC_U 0x1DA7
#define VWUP1_CHG_DC_I 0x4237
#define VWUP2_CHG_AC_U 0x41FC
#define VWUP2_CHG_AC_I 0x41FB
#define VWUP2_CHG_DC_U 0x41F8
#define VWUP2_CHG_DC_I 0x41F9
#define VWUP_MFD_ODOMETER 0x2203
#define VWUP_MFD_MAINT_DIST 0x2260
#define VWUP_MFD_MAINT_TIME 0x2261
#define VWUP_BRK_TPMS 0x1821
//#define VWUP_MOT_ELEC_TEMP_AMB 0xF446 //not working for some reason (value always 0)
#define VWUP_MOT_ELEC_TEMP_DCDC 0x116F
#define VWUP_MOT_ELEC_TEMP_PEM 0x1116
#define VWUP_ELD_TEMP_PEM 0x3EB5
#define VWUP_ELD_TEMP_MOT 0x3E94
//#define VWUP__TEMP_CABIN 0x


#endif //#ifndef __VEHICLE_EUP_OBD_H__
