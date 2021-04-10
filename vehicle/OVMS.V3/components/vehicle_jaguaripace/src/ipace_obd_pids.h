/*
;    Project:       Open Vehicle Monitor System
;    Date:          1st April 2021
;
;    Changes:
;    0.0.1  Initial release
;
;    (C) 2021       Didier Ernotte
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

#ifndef IPACE_OBD_PIDS_H_
#define IPACE_OBD_PIDS_H_

// Module IDs
static const uint16_t broadcastId = 0x7dfu;
static const uint16_t ipmaId  = 0x706u;     // Image Processing Module
static const uint16_t chcmId  = 0x710u;     // Chassis Control Module
static const uint16_t sdlcId  = 0x716u;     // ???
static const uint16_t ipcId   = 0x720u;     // Instrument Panel Cluster
static const uint16_t bcmId   = 0x726u;     // Body Control Module/Gateway Module
static const uint16_t pscmId  = 0x730u;     // Power Steering Control Module
static const uint16_t rfaId   = 0x731u;     // Remote function Actuator
static const uint16_t gsmId   = 0x732u;     // Gear Shift Control Module
static const uint16_t hvacId  = 0x733u;     // HVAC Control Module
static const uint16_t hcmId   = 0x734u;     // Headlamp Control Module
static const uint16_t pamId   = 0x736u;     // Parking Assist Control Module
static const uint16_t rcmId   = 0x737u;     // Restraints Control Module
static const uint16_t ddmId   = 0x740u;     // Driver Door Module
static const uint16_t pdmId   = 0x741u;     // Passenger Door Module
static const uint16_t drdmId  = 0x742u;     // Driver Rear Door Module
static const uint16_t prdmId  = 0x743u;     // Passenger Rear Door Module
static const uint16_t dsmId   = 0x744u;     // Driver Seat Module
static const uint16_t epiccId = 0x746u;     // Rear Electric Power Inverter Converter
static const uint16_t epicbId = 0x747u;     // Front Electric Power Inverter Converter
static const uint16_t tpmsId  = 0x751u;     // Tire pressure Monitoring System
static const uint16_t ommId   = 0x752u;     // Occupant Monitoring Model
static const uint16_t dcdcId  = 0x753u;     // Direct Current to Direct Current Converter
static const uint16_t tcuId   = 0x754u;     // Telematics Control Module
static const uint16_t ascmId  = 0x764u;     // Adaptative Speed Control Module
static const uint16_t rgtmId  = 0x775u;     // Rear Gate/Trunk Module
static const uint16_t rhvacId = 0x785u;     // Rear HVAC
static const uint16_t atcmId  = 0x792u;     // Jaguar Drive Switchpack
static const uint16_t sasmId  = 0x797u;     // Steering Angle Sensing Module
static const uint16_t idmaId  = 0x7a2u;     // Interactive Display Module
static const uint16_t psmId   = 0x7A3u;     // Passenger Seat Module
static const uint16_t aamId   = 0x7a4u;     // Audio Amplifier Module
static const uint16_t cmrId   = 0x7b1u;     // Camera Module Rear
static const uint16_t hudId   = 0x7b2u;     // Head-up Display
static const uint16_t imcId   = 0x7b3u;     // Infotainment Master Control
static const uint16_t hcmbId  = 0x7c3u;     // Headlight Control Module B
static const uint16_t sodlId  = 0x7c4u;     // Side Object Detection Control Module - Left
static const uint16_t sodrId  = 0x7c6u;     // Side Object Detection Control Module - Right
static const uint16_t pcmId   = 0x7e0u;     // Powertrain Control Module
static const uint16_t bbmId   = 0x7e2u;     // Break Booster Module
static const uint16_t becmId  = 0x7e4u;     // Battery Energy Control Module
static const uint16_t bccmId  = 0x7e5u;     // Battery Charger Control Module
static const uint16_t absId   = 0x7e6u;     // Anti-lock Blocking System

static const uint16_t rxFlag = 0x8u;


// BECM PIDs
static const uint16_t batterySoCPid = 0x4910u;
static const uint16_t batterySoCMinPid = 0x4911u;
static const uint16_t batterySoCMaxPid = 0x4914u;
static const uint16_t batterySoHCapacityPid = 0x4918u;
static const uint16_t batterySoHCapacityMinPid = 0x4919u;
static const uint16_t batterySoHCapacityMaxPid = 0x491au;
static const uint16_t batterySoHPowerPid = 0x4915u;
static const uint16_t batterySoHPowerMinPid = 0x4916u;
static const uint16_t batterySoHPowerMaxPid = 0x4917u;
static const uint16_t batteryCellMinVoltPid = 0x4904u;
static const uint16_t batteryCellMaxVoltPid = 0x4903u;
static const uint16_t batteryVoltPid = 0x490Fu;
static const uint16_t batteryCurrentPid = 0x490Cu;
static const uint16_t batteryTempMinPid = 0x4906u;
static const uint16_t batteryTempMaxPid = 0x4905u;
static const uint16_t batteryTempAvgPid = 0x4907u;
static const uint16_t batteryMaxRegenPid = 0x4913u;

static const uint16_t odometerPid = 0xdd01u;
static const uint16_t cabinTempPid = 0xdd04u;

static const uint16_t vinPid = 0xf190u;


// HVAC PIDs
static const uint16_t ambientTempPid = 0x9924u;

// BCM/GWM PIDs
static const uint16_t vehicleSpeedPid = 0xdd09u;

// TPMS PIDs
static const uint16_t frontLeftTirePressurePid = 0x2076u;
static const uint16_t frontRightTirePressurePid = 0x2077u;
static const uint16_t rearLeftTirePressurePid = 0x2078u;
static const uint16_t rearRightTirePressurePid = 0x2079u;

static const uint16_t frontLeftTireTempPid = 0x2a0au;
static const uint16_t frontRightTireTempPid = 0x2a0bu;
static const uint16_t rearLeftTireTempPid = 0x2a0cu;
static const uint16_t rearRightTireTempPid = 0x2a0du;

//TCU PIDs
static const uint16_t localizationPid = 0xa0a6u;


#endif  // IPACE_OBD_PIDS_H_






