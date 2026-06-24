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
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x07, { 0,298,33,33 }, 0, ISOTP_STD },   // Battery State
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, { 0,294,294,94 }, 0, ISOTP_STD },  // HV Contactor Cycles
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x08, { 0,0,298,298 }, 0, ISOTP_STD },   // SOC
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x25, { 0,0,0,98 }, 0, ISOTP_STD },      // SOC Recalibration
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x61, { 0,0,6665,0 }, 0, ISOTP_STD },    // Battery Health (SOH)
};

//   -> HandleOBDpolling() will add the following PIDs
static const OvmsPoller::poll_pid_t obdii_79b_cell_vrt_polls[] = 
{
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x41, { 0,0,302,102 }, 0, ISOTP_STD },   // Battery Voltages Part 1 (Cells 1-48) 
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x42, { 0,0,303,102 }, 0, ISOTP_STD },   // Battery Voltages Part 2 (Cells 49-96)  
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x10, { 0,0,3601,601 }, 0, ISOTP_STD },  // Cell Resistance P1      (Cells 1-48)   
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x11, { 0,0,3603,603 }, 0, ISOTP_STD },  // Cell Resistance P2      (Cells 49-96)
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, { 0,304,304,104 }, 0, ISOTP_STD }, // Battery Temperatures (27 sensors)    
};

static const OvmsPoller::poll_pid_t obdii_745_polls[] =
{
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x25, { 0,8,8,8 }, 0, ISOTP_STD },         // Doorlock EEPROM
};

static const OvmsPoller::poll_pid_t obdii_745_tpms_polls[] =
{
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x74, { 0,0,152,0 }, 0, ISOTP_STD },        // TPMS input capture
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x79, { 0,0,154,0 }, 0, ISOTP_STD },        // TPMS counters/status (missing transmitters)
};

static const OvmsPoller::poll_pid_t obdii_7e4_polls[] =
{
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x339D, { 0,9,0,9 }, 0, ISOTP_STD },       // Charging plug detected (B_PlugConnected_bcb_status_S)  
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x320C, { 0,598,119,59 }, 0, ISOTP_STD },  // rqHV_Energy HV bat capacity
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x34CB, { 0,122,122,0 }, 0, ISOTP_STD },   // Cabin blower command
};

//   -> HandleOBDpolling() will add the following PIDs
static const OvmsPoller::poll_pid_t obdii_7e4_dcdc_polls[] =
{
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3495, { 0,294,59,59 }, 0, ISOTP_STD }, // rqDCDC_Load
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3022, { 0,12,12,12 }, 0, ISOTP_STD },  // DCDC activation request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3023, { 0,294,37,37 }, 0, ISOTP_STD }, // 14V DCDC voltage request
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3024, { 0,291,31,31 }, 0, ISOTP_STD }, // 14V DCDC voltage measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3025, { 0,291,31,31 }, 0, ISOTP_STD }, // 14V DCDC current measure
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3494, { 0,294,31,31 }, 0, ISOTP_STD }, // rqDCDC_Power
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3301, { 0,58,17,17 }, 0, ISOTP_STD },  // USM 14V voltage (CAN) - needed for ADC calculation
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, { 0,291,29,29 }, 0, ISOTP_STD }, // Battery voltage 14V
};

static const OvmsPoller::poll_pid_t obdii_743_polls[] =
{
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x200C, { 0,906,906,906 }, 0, ISOTP_STD }, // extern temp byte 2+3
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A0, { 0,306,306,0 }, 0, ISOTP_STD },   // OBD start Trip Distance km 
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2104, { 0,306,306,0 }, 0, ISOTP_STD },   // OBD Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x01A2, { 0,306,306,0 }, 0, ISOTP_STD },   // OBD start Trip time s
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0204, { 0,0,6633,0 }, 0, ISOTP_STD },    // maintenance data days
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0203, { 0,0,6633,0 }, 0, ISOTP_STD },    // maintenance data usual km
  { 0x743, 0x763, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x0188, { 0,0,6633,0 }, 0, ISOTP_STD },    // maintenance level
};

//   -> HandleOBDpolling() will add the slow_charger_polls/fast_charger_polls PIDs when m_poll_on_charge is true, and remove them when m_poll_on_charge is false
static const OvmsPoller::poll_pid_t slow_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7303, { 0,0,0,10 }, 0, ISOTP_STD },  // rqChargerAC
};

static const OvmsPoller::poll_pid_t fast_charger_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503F, { 0,0,0,9 }, 0, ISOTP_STD }, // rqJB2AC_Ph12_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5041, { 0,0,0,9 }, 0, ISOTP_STD }, // rqJB2AC_Ph23_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5042, { 0,0,0,9 }, 0, ISOTP_STD }, // rqJB2AC_Ph31_RMS_V
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2001, { 0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph1_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503A, { 0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph2_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x503B, { 0,0,0,10 }, 0, ISOTP_STD }, // rqJB2AC_Ph3_RMS_A
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, { 0,0,0,11 }, 0, ISOTP_STD },  // rqJB2AC Mains active power consumed
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5049, { 0,0,0,303 }, 0, ISOTP_STD }, // rqJB2AC_Mains phase frequency
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5070, { 0,0,0,72 }, 0, ISOTP_STD },  // rqJB2AC_Max Current limitation
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5062, { 0,0,0,304 }, 0, ISOTP_STD }, // rqJB2AC_Ground Resistance
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5064, { 0,0,0,305 }, 0, ISOTP_STD }, // rqJB2AC_Leakage Diag
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5065, { 0,0,0,306 }, 0, ISOTP_STD }, // rqJB2AC_DC Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5066, { 0,0,0,307 }, 0, ISOTP_STD }, // rqJB2AC_HF10kHz Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5067, { 0,0,0,308 }, 0, ISOTP_STD }, // rqJB2AC_HF Current
  { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5068, { 0,0,0,309 }, 0, ISOTP_STD }, // rqJB2AC_LF Current
};
//   -> HandleOBDpolling() will add the following PIDs once m_static_ids_read is false and IsOnEQ() is true
static const OvmsPoller::poll_pid_t obdii_onetime_polls[] =
{
  // { tx, rx, type, pid, {OFF,AWAKE,ON,CHARGING}, bus, protocol }
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x80, { 0,6666,6666,0 }, 0, ISOTP_STD },    // DataRead.Identification.RenaultR2
  { 0x79B, 0x7BB, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x90, { 0,6667,6667,0 }, 0, ISOTP_STD },    // BMS Production Number Supplier Read
  { 0x7E4, 0x7EC, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x84, { 0,6668,6668,0 }, 0, ISOTP_STD },    // Frame Traceability Information
  { 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x81, { 0,6669,6669,0 }, 0, ISOTP_STD },    // req.VIN
};
