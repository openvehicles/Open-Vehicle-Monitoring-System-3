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
// Pollstate 1 - POLLSTATE_AWAKE    - car is awake
// Pollstate 2 - POLLSTATE_ON       - car is on (driving)
// Pollstate 3 - POLLSTATE_CHARGING - car is charging

static const OvmsPoller::poll_pid_t obdii_79b_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }  <- old style, replaced by ts array below
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }  <- new style, allows different delay time before start polling for each pollstate
  // e.g. interval 60 with shift 10 is polled at n*60 + 10.
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x07, {.ts={ { 0,300,30,30 }, { 0,8,8,8 } }}, 0, ISOTP_STD },         // Battery State
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {.ts={ { 0,300,300,60 }, { 0,10,10,10 } }}, 0, ISOTP_STD },     // HV Contactor Cycles
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x08, {.ts={ { 0,300,300,300 }, { 0,27,27,27 } }}, 0, ISOTP_STD },    // SOC more detailed
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x25, {.ts={ { 0,0,0,100 }, { 0,0,0,155 } }}, 0, ISOTP_STD },         // SOC Recalibration
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x61, {.ts={ { 0,300,300,300 }, { 0,57,57,57 } }}, 0, ISOTP_STD },    // Battery Health SOH/CAP
};

//   -> HandleOBDpolling() will add the following PIDs
static const OvmsPoller::poll_pid_t obdii_79b_cell_vrt_polls[] = 
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, {.ts={ { 0,300,300,100 }, { 0,25,25,25 } }}, 0, ISOTP_STD },    // Battery Voltages Part 1 (Cells 1-48) 
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, {.ts={ { 0,300,300,100 }, { 0,25,25,25 } }}, 0, ISOTP_STD },    // Battery Voltages Part 2 (Cells 49-96)  
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x10, {.ts={ { 0,600,600,600 }, { 0,40,40,40 } }}, 0, ISOTP_STD },    // Cell Resistance P1      (Cells 1-48)   
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x11, {.ts={ { 0,600,600,600 }, { 0,40,40,40 } }}, 0, ISOTP_STD },    // Cell Resistance P2      (Cells 49-96)
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {.ts={ { 0,300,300,100 }, { 0,30,30,30 } }}, 0, ISOTP_STD },    // Battery Temperatures (27 sensors)    
};

static const OvmsPoller::poll_pid_t obdii_745_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x25, {.ts={ { 0,8,8,8 }, { 0,2,2,2 } }}, 0, ISOTP_STD },             // Doorlock EEPROM
};

static const OvmsPoller::poll_pid_t obdii_745_tpms_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x74, {.ts={ { 0,0,150,0 }, { 0,0,34,0 } }}, 0, ISOTP_STD },          // TPMS input capture
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x79, {.ts={ { 0,0,150,0 }, { 0,0,35,0 } }}, 0, ISOTP_STD },          // TPMS counters/status (missing transmitters)
};

static const OvmsPoller::poll_pid_t obdii_7e4_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x339D, {.ts={ { 0,9,9,9 }, { 0,2,2,2 } }}, 0, ISOTP_STD },          // Charging plug detected (B_PlugConnected_bcb_status_S)  
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x320C, {.ts={ { 0,600,120,60 }, { 0,45,45,45 } }}, 0, ISOTP_STD },  // rqHV_Energy HV bat capacity
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x34CB, {.ts={ { 0,120,120,120 }, { 0,15,15,25 } }}, 0, ISOTP_STD }, // Cabin blower command
};

//   -> HandleOBDpolling() will add the following PIDs
static const OvmsPoller::poll_pid_t obdii_7e4_dcdc_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  // AWAKE short interval needed for trickle charging (HVAC on)
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3022, {.ts={ { 0,10,10,10 }, { 0,5,5,9 } }}, 0, ISOTP_STD },    // DCDC activation request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3023, {.ts={ { 0,30,30,30 }, { 0,19,19,19 } }}, 0, ISOTP_STD }, // 14V DCDC voltage request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3024, {.ts={ { 0,20,20,20 }, { 0,8,8,14 } }}, 0, ISOTP_STD },   // 14V DCDC voltage measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3025, {.ts={ { 0,30,30,30 }, { 0,20,20,20 } }}, 0, ISOTP_STD }, // 14V DCDC current measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3494, {.ts={ { 0,30,30,30 }, { 0,21,21,21 } }}, 0, ISOTP_STD }, // rqDCDC_Power
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3495, {.ts={ { 0,30,30,30 }, { 0,22,22,22 } }}, 0, ISOTP_STD }, // rqDCDC_Load
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3301, {.ts={ { 0,20,20,20 }, { 0,6,6,11 } }}, 0, ISOTP_STD },   // USM 14V voltage (CAN) - needed for ADC calculation
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, {.ts={ { 0,20,20,20 }, { 0,7,7,13 } }}, 0, ISOTP_STD },   // Battery voltage 14V
};

static const OvmsPoller::poll_pid_t obdii_743_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200C, {.ts={ { 0,600,600,600 }, { 0,31,31,31 } }}, 0, ISOTP_STD }, // extern temp byte 2+3
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A0, {.ts={ { 0,600,300,0 }, { 0,32,32,0 } }}, 0, ISOTP_STD },    // OBD start Trip Distance km 
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2104, {.ts={ { 0,600,300,0 }, { 0,33,33,0 } }}, 0, ISOTP_STD },    // OBD Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A2, {.ts={ { 0,600,300,0 }, { 0,34,34,0 } }}, 0, ISOTP_STD },    // OBD start Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0204, {.ts={ { 0,6661,6661,0 }, { 0,63,63,0 } }}, 0, ISOTP_STD },  // maintenance data days
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0203, {.ts={ { 0,6661,6661,0 }, { 0,64,64,0 } }}, 0, ISOTP_STD },  // maintenance data usual km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0188, {.ts={ { 0,6661,6661,0 }, { 0,65,65,0 } }}, 0, ISOTP_STD },  // maintenance level
};

//   -> HandleOBDpolling() will add the slow_charger_polls/fast_charger_polls PIDs when m_poll_on_charge is true, and remove them when m_poll_on_charge is false
static const OvmsPoller::poll_pid_t slow_charger_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7303, {.ts={ { 0,0,0,5 }, { 0,0,0,6 } }}, 0, ISOTP_STD }, // rqChargerAC
};

static const OvmsPoller::poll_pid_t fast_charger_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503F, {.ts={ { 0,0,0,5 }, { 0,0,0,6 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph12_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5041, {.ts={ { 0,0,0,5 }, { 0,0,0,6 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph23_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5042, {.ts={ { 0,0,0,5 }, { 0,0,0,6 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph31_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2001, {.ts={ { 0,0,0,5 }, { 0,0,0,7 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph1_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503A, {.ts={ { 0,0,0,5 }, { 0,0,0,7 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph2_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503B, {.ts={ { 0,0,0,5 }, { 0,0,0,7 } }}, 0, ISOTP_STD },    // rqJB2AC_Ph3_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, {.ts={ { 0,0,0,5 }, { 0,0,0,8 } }}, 0, ISOTP_STD },    // rqJB2AC Mains active power consumed
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5049, {.ts={ { 0,0,0,601 }, { 0,0,0,58 } }}, 0, ISOTP_STD }, // rqJB2AC_Mains phase frequency
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5070, {.ts={ { 0,0,0,601 }, { 0,0,0,59 } }}, 0, ISOTP_STD }, // rqJB2AC_Max Current limitation
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5062, {.ts={ { 0,0,0,601 }, { 0,0,0,61 } }}, 0, ISOTP_STD }, // rqJB2AC_Ground Resistance
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5064, {.ts={ { 0,0,0,601 }, { 0,0,0,62 } }}, 0, ISOTP_STD }, // rqJB2AC_Leakage Diag
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5065, {.ts={ { 0,0,0,601 }, { 0,0,0,63 } }}, 0, ISOTP_STD }, // rqJB2AC_DC Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5066, {.ts={ { 0,0,0,601 }, { 0,0,0,64 } }}, 0, ISOTP_STD }, // rqJB2AC_HF10kHz Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5067, {.ts={ { 0,0,0,601 }, { 0,0,0,65 } }}, 0, ISOTP_STD }, // rqJB2AC_HF Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5068, {.ts={ { 0,0,0,601 }, { 0,0,0,66 } }}, 0, ISOTP_STD }, // rqJB2AC_LF Current
};
//   -> HandleOBDpolling() will add the following PIDs once m_static_ids_read is false and IsOnEQ() is true
static const OvmsPoller::poll_pid_t obdii_onetime_polls[] =
{
  // { tx, rx, type, pid, {.ts={ {OFF,AWAKE,ON,CHARGING}, {<delayed_start>OFF,AWAKE,ON,CHARGING} }}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x80, {.ts={ { 0,6662,6662,0 }, { 0,66,66,0 } }}, 0, ISOTP_STD },    // DataRead.Identification.RenaultR2
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x90, {.ts={ { 0,6662,6662,0 }, { 0,67,67,0 } }}, 0, ISOTP_STD },    // BMS Production Number Supplier Read
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x84, {.ts={ { 0,6662,6662,0 }, { 0,68,68,0 } }}, 0, ISOTP_STD },    // Frame Traceability Information
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x81, {.ts={ { 0,6662,6662,0 }, { 0,69,69,0 } }}, 0, ISOTP_STD },    // req.VIN
};
