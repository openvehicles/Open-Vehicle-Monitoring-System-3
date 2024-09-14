/*
;    Project:       Open Vehicle Monitor System
;    Date:          3rd September 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2018  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2020       Chris Staite
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

#ifndef MG_OBD_PIDS_H_
#define MG_OBD_PIDS_H_

// Module IDs
constexpr uint32_t broadcastId = 0x7dfu;
constexpr uint32_t bmsId = 0x781u;
constexpr uint32_t bmsMk2Id = 0x7e5u;
constexpr uint32_t dcdcId = 0x785u;
constexpr uint32_t vcuId = 0x7e3u;
constexpr uint32_t atcId = 0x750u;
constexpr uint32_t bcmId = 0x740u;
constexpr uint32_t gwmId = 0x710u;
constexpr uint32_t tpmsId = 0x724u;
constexpr uint32_t pepsId = 0x745u;
constexpr uint32_t ipkId = 0x760u;
constexpr uint32_t evccId = 0x784u;
constexpr uint32_t ccuId = 0x7e4u;
constexpr uint32_t rxFlag = 0x8u;

// Generic PIDs
constexpr uint16_t softwarePid = 0xf194u;

// BMS PIDs
constexpr uint16_t engineSpeedPid = 0xcu;
constexpr uint16_t vehicleSpeedPid = 0xdu;
constexpr uint16_t ambTempPid = 0x46u;
constexpr uint16_t socPid = 0x5bu;
constexpr uint16_t odometerPid = 0xa6u;
constexpr uint16_t cell1StatPid = 0xb001u;
constexpr uint16_t cell2StatPid = 0xb009u;
constexpr uint16_t cell3StatPid = 0xb011u;
constexpr uint16_t cell4StatPid = 0xb019u;
constexpr uint16_t cell5StatPid = 0xb021u;
constexpr uint16_t cell6StatPid = 0xb029u;
constexpr uint16_t cell7StatPid = 0xb0a3u;
constexpr uint16_t cell8StatPid = 0xb0abu;
constexpr uint16_t cell9StatPid = 0xb0b3u;
constexpr uint16_t cell10StatPid = 0xb0bbu; //new Mk2
constexpr uint16_t cell11StatPid = 0xb0c3u; //new Mk2
constexpr uint16_t cell12StatPid = 0xb0cbu; //new Mk2
constexpr uint16_t cell13StatPid = 0xb0dbu; //new Mk2
constexpr uint16_t cell14StatPid = 0xb0dcu; //new Mk2
constexpr uint16_t cell15StatPid = 0xb0ddu; //new Mk2
constexpr uint16_t cell16StatPid = 0xb0deu; //new Mk2
constexpr uint16_t cell17StatPid = 0xb10eu; //new Mk2
constexpr uint16_t cell18StatPid = 0xb10fu; //new Mk2
constexpr uint16_t cell19StatPid = 0xb110u; //new Mk2
constexpr uint16_t cell20StatPid = 0xb111u; //new Mk2
constexpr uint16_t cell21StatPid = 0xb112u; //new Mk2
constexpr uint16_t cell22StatPid = 0xb113u; //new Mk2
constexpr uint16_t cell23StatPid = 0xb114u; //new Mk2
constexpr uint16_t cell24StatPid = 0xb115u; //new Mk2
constexpr uint16_t batteryBusVoltagePid = 0xb041u;
constexpr uint16_t batteryVoltagePid = 0xb042u;
constexpr uint16_t batteryCurrentPid = 0xb043u;
constexpr uint16_t batteryResistancePid = 0xb045u;
constexpr uint16_t batterySoCPid = 0xb046u;
constexpr uint16_t batteryErrorPid = 0xb047u;
constexpr uint16_t bmsStatusPid = 0xb048u;
constexpr uint16_t batteryCoolantTempPid = 0xb05cu;
constexpr uint16_t batterySoHPid = 0xb061u;
constexpr uint16_t batteryTempPid = 0xb056u;
constexpr uint16_t bmsRangePid = 0xb0ceu;
constexpr uint16_t bmsMaxCellVoltagePid = 0xb058u;
constexpr uint16_t bmsMinCellVoltagePid = 0xb059u;
constexpr uint16_t bmsSystemMainRelayBPid = 0xb049u;
constexpr uint16_t bmsSystemMainRelayGPid = 0xb04au;
constexpr uint16_t bmsSystemMainRelayPPid = 0xb052u;
constexpr uint16_t bmsTimePid = 0xb06du;

// DCDC PIDs
constexpr uint16_t dcdcLvCurrentPid = 0xb022u;
constexpr uint16_t dcdcPowerLoadPid = 0xb025u;
constexpr uint16_t dcdcTemperaturePid = 0xb026u;

// VCU PIDs
constexpr uint16_t vcu12vSupplyPid = 0x0112u;
constexpr uint16_t vcuChargerConnectedPid = 0xb71bu;
constexpr uint16_t vcuHvContactorPid = 0xb703u;
constexpr uint16_t vcuIgnitionStatePid = 0xb18cu;
constexpr uint16_t vcuCoolantTempPid = 0xb309u;
constexpr uint16_t vcuMotorTempPid = 0xb405u;
constexpr uint16_t vcuVehicleSpeedPid = 0xba00u;
constexpr uint16_t vcuVinPid = 0xf190u;
constexpr uint16_t vcuMotorSpeedPid = 0xb402u;
constexpr uint16_t vcuMotorTorquePid = 0xb401u;
constexpr uint16_t vcuOdometerPid = 0xe101u;
constexpr uint16_t vcuHandbrakePid = 0xba40u;
constexpr uint16_t vcuGearPid = 0xb900u;
constexpr uint16_t vcuBrakePid = 0xb120u;
constexpr uint16_t vcuBonnetPid = 0xbb06u;
constexpr uint16_t chargeRatePid = 0xb712u;
constexpr uint16_t vcuAcPowerPid = 0xbb01u;
constexpr uint16_t vcuBatteryVoltagePid = 0xb705u;
constexpr uint16_t vcuRadiatorFanPid = 0xbb91u;
constexpr uint16_t vcuDcDcModePid = 0xb580u;
constexpr uint16_t vcuDcDcInputCurrentPid = 0xb581u;
constexpr uint16_t vcuDcDcInputVoltagePid = 0xb582u;
constexpr uint16_t vcuDcDcOutputCurrentPid = 0xb583u;
constexpr uint16_t vcuDcDcOutputVoltagetPid = 0xb584u;
constexpr uint16_t vcuDcDcTempPid = 0xb587u;

// ATC PIDs
constexpr uint16_t atcAmbientTempPid = 0xe01bu;
constexpr uint16_t atcSetTempPid = 0xb001u;
constexpr uint16_t atcFaceOutletTempPid = 0xe013u;
constexpr uint16_t atcBlowerSpeedPid = 0xe00fu;
constexpr uint16_t atcPtcTempPid = 0xe01cu;

// BCM PIDs
// Talking to the BCM while the alarm is set will set it off
constexpr uint16_t bcmDoorPid = 0xd112u;
constexpr uint16_t bcmLightPid = 0xb512u;
constexpr uint16_t bcmDrlPid = 0xd117u;

// TPMS PIDs
constexpr uint16_t tyrePressurePid = 0xb001u;
constexpr uint16_t tyreTemperaturePid = 0xb003u;

// PEPS PIDs
constexpr uint16_t pepsLockPid = 0xb013u;

// EVCC PIDs
constexpr uint16_t evccVoltagePid = 0xb010u;
constexpr uint16_t evccAmperagePid = 0xb001u;
constexpr uint16_t evccMaxAmperagePid = 0xb003u;
constexpr uint16_t evccChargingPid = 0xb00fu;
constexpr uint16_t evccStatusPid = 0xb013u;

// GWM PIDs
constexpr uint16_t vehTimePid = 0x010bu;
constexpr uint16_t odoPid = 0xb921u;
constexpr uint16_t vehStatusPid = 0xd001u;
constexpr uint16_t vinPid = 0xf190u;

#endif  // MG_OBD_PIDS_H_
