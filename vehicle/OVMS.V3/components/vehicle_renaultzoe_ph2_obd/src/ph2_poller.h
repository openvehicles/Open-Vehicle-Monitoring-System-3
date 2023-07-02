/*
;    Project:       Open Vehicle Monitor System
;    Date:          15th Apr 2022
;
;    (C) 2022       Carsten Schmiemann
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

#define SESSION_EXTDIAG 0x1003
#define SESSION_DEFAULT 0x1001
#define SESSION_AfterSales 0x10C0

// Pollstate 0 - POLLSTATE_OFF      - car is off
// Pollstate 1 - POLLSTATE_ON       - car is on
// Pollstate 2 - POLLSTATE_DRIVING  - car is driving
// Pollstate 3 - POLLSTATE_CHARGING - car is charging
static const OvmsVehicle::poll_pid_t renault_zoe_polls[] = {
//***TX-ID, ***RX-ID, ***SID, ***PID, { Polltime (seconds) for Pollstate 0, 1, 2, 3}, ***CAN BUS Interface, ***FRAMETYPE

//Motor Inverter
//{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_EXTDIAG,  { 0, 60, 60, 60 }, 0, ISOTP_EXTFRAME }, // OBD Extended Diagnostic Session
{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700C, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // Inverter temperature
{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x700F, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // Stator Temperature 1
{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7010, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // Stator Temperature 2
{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2004, { 0, 60, 3, 60 }, 0, ISOTP_EXTFRAME },  // Battery voltage sense
{ 0x18dadff1, 0x18daf1df, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x7049, { 0, 60, 3, 60 }, 0, ISOTP_EXTFRAME },  // Current voltage sense

//EVC-HCM-VCM
//{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_EXTDIAG,  { 0, 60, 60, 60 }, 0, ISOTP_EXTFRAME }, // OBD Extended Diagnostic Session
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, { 0, 10, 10, 300 }, 0, ISOTP_EXTFRAME },  // Odometer
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2003, { 0, 60, 2, 300 }, 0, ISOTP_EXTFRAME },  // Vehicle Speed
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // 12Battery Voltage
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x21CF, { 0, 2, 10, 300 }, 0, ISOTP_EXTFRAME },  // Inverter Status
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2218, { 0, 20, 20, 20 }, 0, ISOTP_EXTFRAME },  // Ambient air temperature
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2A09, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // Power usage by consumer
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2191, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // Power usage by ptc
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B85, { 0, 2, 300, 10 }, 0, ISOTP_EXTFRAME },  // Charge plug present
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B6D, { 0, 2, 300, 10 }, 0, ISOTP_EXTFRAME },  // Charge MMI states
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B7A, { 0, 2, 300, 10 }, 0, ISOTP_EXTFRAME },  // Charge type
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3064, { 0, 10, 3, 10 }, 0, ISOTP_EXTFRAME },  // Motor rpm
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300F, { 0, 2, 300, 3 }, 0, ISOTP_EXTFRAME },  // AC charging power available
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300D, { 0, 10, 300, 3 }, 0, ISOTP_EXTFRAME },  // AC mains current
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x300B, { 0, 2, 300, 10 }, 0, ISOTP_EXTFRAME },  // AC phases
{ 0x18dadaf1, 0x18daf1da, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2B8A, { 0, 2, 300, 10 }, 0, ISOTP_EXTFRAME },  // AC mains voltage

//BCM
//{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_DEFAULT,  { 0, 60, 60, 60 }, 0, ISOTP_STD }, // OBD Extended Diagnostic Session
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6300, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS pressure - front left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6301, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS pressure - front right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6302, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS pressure - rear left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6303, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS pressure - rear right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6310, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS temp - front left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6311, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS temp - front right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6312, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS temp - rear left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6313, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS temp - rear right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4109, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS alert - front left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410A, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS alert - front right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410B, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS alert - rear left
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x410C, { 0, 300, 300, 300 }, 0, ISOTP_STD },  // TPMS alert - rear right
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8004, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Car secure aka vehicle locked
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6026, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Front left door
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6027, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Front right door
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x61B2, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Rear left door
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x61B3, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Rear right door
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x609B, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Tailgate
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4186, { 0, 3, 3, 3 }, 0, ISOTP_STD },  // Low beam lights
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x60C6, { 0, 3, 6, 60 }, 0, ISOTP_STD },  // Ignition relay (switch), working but behavior on CHARING testing needed
{ 0x745, 0x765, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4060, { 0, 600, 0, 0 }, 0, ISOTP_STD },  // Vehicle identificaftion number

//LBC
//{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_DEFAULT,  { 0, 60, 60, 60 }, 0, ISOTP_EXTFRAME }, // OBD Extended Diagnostic Session
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9002, { 0, 10, 10, 10 }, 0, ISOTP_EXTFRAME },  // SOC
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9003, { 0, 60, 60, 10 }, 0, ISOTP_EXTFRAME },  // SOH
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9005, { 0, 10, 3, 5 }, 0, ISOTP_EXTFRAME },  // Battery Voltage
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x925D, { 0, 10, 3, 5 }, 0, ISOTP_EXTFRAME },  // Battery Current
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9012, { 0, 10, 60, 5 }, 0, ISOTP_EXTFRAME },  // Battery Average Temperature
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x91C8, { 0, 60, 60, 5 }, 0, ISOTP_EXTFRAME },  // Battery Available Energy kWh
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9243, { 0, 60, 60, 10 }, 0, ISOTP_EXTFRAME },  // Energy charged kWh
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9245, { 0, 60, 10, 60 }, 0, ISOTP_EXTFRAME },  // Energy discharged kWh
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9247, { 0, 60, 10, 60 }, 0, ISOTP_EXTFRAME },  // Energy regenerated kWh
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9210, { 0, 60, 60, 60 }, 0, ISOTP_EXTFRAME },  // Number of complete cycles
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9018, { 0, 10, 60, 10 }, 0, ISOTP_EXTFRAME },  // Max Charge Power
//LBC Cell voltages and temperatures, OBD Grouppoll not working
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9131, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 1
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9132, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 2
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9133, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 3
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9134, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 4
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9135, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 5
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9136, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 6
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9137, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 7
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9138, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 8
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9139, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 9
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 10
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 11
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x913C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Pack temperature 12
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9021, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 1
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9022, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 2
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9023, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 3
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9024, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 4
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9025, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 5
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9026, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 6
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9027, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 7
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9028, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 8
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9029, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 9
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 10
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 11
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 12
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 13
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 14
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x902F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 15
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9030, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 16
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9031, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 17
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9032, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 18
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9033, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 19
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9034, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 20
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9035, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 21
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9036, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 22
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9037, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 23
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9038, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 24
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9039, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 25
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 26
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 27
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 28
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 29
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 30
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x903F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 31
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9041, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 32
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9042, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 33
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9043, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 34
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9044, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 35
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9045, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 36
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9046, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 37
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9047, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 38
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9048, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 39
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9049, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 40
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 41
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 42
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 43
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 44
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 45
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x904F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 46
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9050, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 47
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9051, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 48
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9052, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 49
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9053, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 50
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9054, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 51
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9055, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 52
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9056, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 53
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9057, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 54
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9058, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 55
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9059, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 56
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 57
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 58
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 59
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 60
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 61
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x905F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 62
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9061, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 63
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9062, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 64
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9063, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 65
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9064, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 66
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9065, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 67
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9066, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 68
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9067, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 69
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9068, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 70
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9069, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 71
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 72
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 73
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 74
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 75
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 76
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x906F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 77
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9070, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 78
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9071, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 79
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9072, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 80
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9073, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 81
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9074, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 82
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9075, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 83
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9076, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 84
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9077, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 85
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9078, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 86
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9079, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 87
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907A, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 88
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907B, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 89
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907C, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 90
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907D, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 91
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907E, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 92
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x907F, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 93
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9081, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 94
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9082, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 95
{ 0x18dadbf1, 0x18daf1db, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x9083, { 0, 60, 300, 60 }, 0, ISOTP_EXTFRAME },  // Cell voltage 96
 
//HVAC
//{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_EXTDIAG,  { 0, 60, 60, 60 }, 0, ISOTP_STD }, // OBD Extended Diagnostic Session
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4009,  { 0, 10, 60, 10 }, 0, ISOTP_STD }, // Cabin temp
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4360,  { 0, 60, 60, 60 }, 0, ISOTP_STD }, // Cabin setpoint
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x43D8,  { 0, 10, 10, 5 }, 0, ISOTP_STD },  // Compressor speed
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4402,  { 0, 10, 10, 10 }, 0, ISOTP_STD }, // Compressor state
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369,  { 0, 10, 10, 10 }, 0, ISOTP_STD }, // Compressor pressure in BAR
{ 0x744, 0x764, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4436,  { 0, 10, 10, 10 }, 0, ISOTP_STD }, // Compressor power

//UCM
//{ 0x74D, 0x76D, VEHICLE_POLL_TYPE_OBDIISESSION, SESSION_AfterSales,  { 0, 60, 60, 60 }, 0, ISOTP_STD }, // OBD Extended Diagnostic Session
{ 0x74D, 0x76D, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x6079, { 0, 10, 10, 10 }, 0, ISOTP_STD },  // 12V Battery current

POLL_LIST_END
};