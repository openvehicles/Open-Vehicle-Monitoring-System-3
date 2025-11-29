/*
 ;    Project:       Open Vehicle Monitor System
 ;    Date:          1th October 2018
 ;
 ;    Changes:
 ;    1.0  Initial release
 ;
 ;    (C) 2018       Martin Graml
 ;    (C) 2019       Thomas Heuer
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
 ;
 ; Most of the CAN Messages are based on https://github.com/MyLab-odyssey/ED_BMSdiag
 ; https://github.com/MyLab-odyssey/ED4scan
 */

// Pollstate 0 - POLLSTATE_OFF      - car is off
// Pollstate 1 - POLLSTATE_ON       - car is on
// Pollstate 2 - POLLSTATE_DRIVING  - car is driving
// Pollstate 3 - POLLSTATE_CHARGING - car is charging

static const OvmsPoller::poll_pid_t obdii_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x07, {  0,60,5,5 }, 0, ISOTP_STD },     // Battery State
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0,300,300,60 }, 0, ISOTP_STD }, // Battery Temperatures (27 sensors)
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, {  0,300,300,60 }, 0, ISOTP_STD }, // Battery Voltages Part 1 (Cells 1-48)
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, {  0,300,300,60 }, 0, ISOTP_STD }, // Battery Voltages Part 2 (Cells 49-96)

  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200c, {  0,300,300,300 }, 0, ISOTP_STD }, // extern temp byte 2+3
  //{ 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2101, {  0,300,60,0 }, 0, ISOTP_STD }, // OBD Trip Distance km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A0, {  0,300,60,0 }, 0, ISOTP_STD }, // OBD start Trip Distance km 
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2104, {  0,300,60,0 }, 0, ISOTP_STD }, // OBD Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A2, {  0,300,60,0 }, 0, ISOTP_STD }, // OBD start Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0204, {  0,3600,0,0 }, 0, ISOTP_STD }, // maintenance data days
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0203, {  0,3600,0,0 }, 0, ISOTP_STD }, // maintenance data usual km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0188, {  0,3600,0,0 }, 0, ISOTP_STD }, // maintenance level

  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x81, {  0,3600,0,0 }, 0, ISOTP_STD }, // req.VIN
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x74, {  0,0,60,0 }, 0, ISOTP_STD }, // TPMS input capture
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x79, {  0,0,60,0 }, 0, ISOTP_STD }, // TPMS counters/status (missing transmitters)
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8003, {  0,5,5,5 }, 0, ISOTP_STD }, // rq VehicleState
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x605e, {  0,5,5,5 }, 0, ISOTP_STD }, // rq UNDERHOOD_OPENED  
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8079, {  0,10,10,10 }, 0, ISOTP_STD }, // Generator mode

  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x320c, {  0,300,60,10 }, 0, ISOTP_STD }, // rqHV_Energy
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3495, {  0,10,10,10 }, 0, ISOTP_STD }, // rqDCDC_Load
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3022, {  0,10,10,10 }, 0, ISOTP_STD }, // DCDC activation request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3023, {  0,10,10,10 }, 0, ISOTP_STD }, // 14V DCDC voltage request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3024, {  0,10,10,10 }, 0, ISOTP_STD }, // 14V DCDC voltage measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3025, {  0,10,10,10 }, 0, ISOTP_STD }, // 14V DCDC current measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3494, {  0,10,10,10 }, 0, ISOTP_STD }, // rqDCDC_Power
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3301, {  0,10,10,10 }, 0, ISOTP_STD }, // USM 14V voltage (CAN)
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3433, {  0,60,60,10 }, 0, ISOTP_STD }, // Battery voltage request (internal)
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x34CB, {  0,30,30,30 }, 0, ISOTP_STD }, // Cabin blower command
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, {  0,10,10,10 }, 0, ISOTP_STD }, // Battery voltage 14V request
};

static const OvmsPoller::poll_pid_t slow_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7303, {  0,0,0,5 }, 0, ISOTP_STD }, // rqChargerAC
};

static const OvmsPoller::poll_pid_t fast_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503F, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph12_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5041, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph23_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5042, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph31_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2001, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph1_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503A, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph2_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503B, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC_Ph3_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, {  0,0,0,5 }, 0, ISOTP_STD }, // rqJB2AC Mains active power consumed
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5049, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_Mains phase frequency
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5070, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_Max Current limitation
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5062, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_Ground Resistance
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5064, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_Leakage Diag
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5065, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_DC Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5066, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_HF10kHz Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5067, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_HF Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5068, {  0,0,0,60 }, 0, ISOTP_STD }, // rqJB2AC_LF Current
};
