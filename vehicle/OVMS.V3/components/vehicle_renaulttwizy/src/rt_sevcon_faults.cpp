/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy SEVCON Gen4 access
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
#include <string>
#include <iomanip>

#include "ovms_notify.h"
#include "ovms_utils.h"

#include "vehicle_renaulttwizy.h"
#include "rt_sevcon.h"


typedef enum {
  FCT_Warning = 0,
  FCT_DriveInhibit,
  FCT_Severe,
  FCT_VerySevere,
  FCT_ReturnToBase
} SC_FaultType_t;

const char* const SC_FaultTypeName[] = {
  "Warning",
  "Drive Inhibit",
  "Severe",
  "Very Severe",
  "Return to Base"
};

typedef struct {
  uint16_t              code;
  SC_FaultType_t        type;
  const char* const     message;
  const char* const     description;
  const char* const     action;
} SC_FaultCode_t;

static const SC_FaultCode_t SC_FaultCode[] = {
  { 0x4481, FCT_Warning, "Handbrake Fault", "Handbrake is active when direction selected.", "Release handbrake" },
  { 0x4541, FCT_Warning, "Fan Fault", "No speed feedback from external heatsink fans", "Check operation of heatsink fans" },
  { 0x4545, FCT_Warning, "Isolation Fault", "Isolation fault detected between logic and power frame", "Check isolation between low and high voltage circuits" },
  { 0x4546, FCT_Warning, "No Motor Speed Signal", "No speed feedback from motor", "Check encoder wiring and speed measurement signal" },
  { 0x4581, FCT_Warning, "Throttle Fault (Warn)", "Warning level throttle fault. Used for Renault Twizy", "Check throttle wiring and installation." },
  { 0x4582, FCT_Warning, "Safety Case 1", "Throttle appears to be stuck. This fault will clear if throttle starts to work again.", "Check throttle wiring and installation." },
  { 0x4583, FCT_Warning, "Safety Case 2", "Throttle appears to be stuck. This fault will latch and can only be cleared by repairing the throttle and recycling power.", "Check throttle wiring and installation." },
  { 0x4584, FCT_Warning, "Analogue Output Over Current (warn)", "Contactor driver over current", "Ensure contactor doesn't exceed maximum current and check contactor wiring" },
  { 0x4585, FCT_Warning, "Analogue Output Off with Failsafe (warn)", "Contactor driver not working", "Internal hardware fault" },
  { 0x4586, FCT_Warning, "Analogue Output Over Temperature (warn)", "Contactor driver over temperature", "Ensure contactor doesn't exceed maximum current and check contactor wiring" },
  { 0x4587, FCT_Warning, "Analogue Output Under Current (warn)", "Contactor driver unable to achieve current target in current mode", "Ensure contactor driver current target is within range" },
  { 0x4588, FCT_Warning, "Analogue Output Short Circuit (warn)", "Contactor driver MOSFET short circuit detected", "Internal hardware fault" },
  { 0x45C1, FCT_Warning, "BDI Warning", "BDI remaining charge (0x2790,1) is less than BDI Warning level (0x2C30,5)", "Charge battery" },
  { 0x45C2, FCT_Warning, "BDI Cutout", "BDI remaining charge (0x2790,1) is less than BDI Cutout level (0x2C30,4)", "Charge battery" },
  { 0x45C3, FCT_Warning, "Low Battery Cut", "Battery voltage (0x5100,1) is less than Under Voltage limit (0x2C02,2) for longer than the protection delay (0x2C03,0)", "Charge battery" },
  { 0x45C4, FCT_Warning, "High Battery Cut", "Battery voltage (0x5100,1) is greater than Over Voltage limit (0x2C01,2) for longer than the protection delay (0x2C03,0)", "Charge battery" },
  { 0x45C5, FCT_Warning, "High Capacitor Cut", "Capacitor voltage (0x5100,3) is greater than Over Voltage limit (0x2C01,2) for longer than the protection delay (0x2C03,0)", "Charge battery" },
  { 0x45C6, FCT_Warning, "Vbat below rated min", "Battery voltage (0x5100,1) is less than rated minimum voltage for controller for longer than 1s. NOTE: This fault is sometimes seen at power down.", "Charge battery" },
  { 0x45C7, FCT_Warning, "Vbat above rated max", "Battery voltage (0x5100,1) is greater than rated maximum voltage for controller for longer than 1s.", "Charge battery" },
  { 0x45C8, FCT_Warning, "Vcap above rated max", "Capacitor voltage (0x5100,3) is greater than rated maximum voltage for controller for longer than 1s.", "Charge battery" },
  { 0x45C9, FCT_Warning, "Motor in low voltage cutback", "Motor control has entered low voltage cutback region.", "Charge battery" },
  { 0x45CA, FCT_Warning, "Motor in high voltage cutback", "Motor control has entered high voltage cutback region.", "Charge battery" },
  { 0x4601, FCT_Warning, "Device too cold", "Low heatsink temperature (0x5100,4) has reduced power to motor", "Allow controller to warm up to normal operating temperature." },
  { 0x4602, FCT_Warning, "Device too hot", "High heatsink temperature (0x5100,4) has reduced power to motor", "Allow controller to cool down to normal operating temperature." },
  { 0x4603, FCT_Warning, "Motor in thermal cutback", "High measured (0x4600,3) or estimated (0x4602,8) motor temperature has reduced power to motor", "Allow motor to cool down to normal operating temperature." },
  { 0x4604, FCT_Warning, "Motor too cold", "Low Measured temperature has reached -30deg", "Check motor thermistor connection or allow motor to warm up." },
  { 0x4681, FCT_Warning, "Unit in preoperational", "Controller is in pre-operational state", "If configured and ready for use, change state to operational." },
  { 0x4682, FCT_Warning, "IO can't init", "Controller has not received all configured RPDOs at power up", "Check PDOs on all CANbus nodes are configured correctly and match up." },
  { 0x4683, FCT_Warning, "RPDO Timeout (warning)", "One or more configured RPDOs not received with 3s at start up or 500ms during normal operation.", "Check status of all nodes on CANbus. Check PDOs on all CANbus nodes are configured correctly and match up." },
  { 0x46C1, FCT_Warning, "Encoder Alignment Warning", "Encoder is not aligned properly.", "Ensure encoder offset is correctly set or re-align encoder" },
  { 0x4701, FCT_Warning, "CAN warning", "Vehicle is operating in reduced power mode as some CAN messages are not being received (Renault only)", "Check status of nodes on CANbus expected to be transmitting data" },
  { 0x4781, FCT_Warning, "CANopen anon EMCY level 1", "EMCY message received from non-Sevcon node and anonymous EMCY level (0x2830,0) is set to 1.", "Check status of non-Sevcon nodes on CANbus" },
  { 0x47C1, FCT_Warning, "Vehicle Service Required", "Vehicle service next due time (0x2850,5) has expired. If supported Service driveability profile (0x2925) will activate.", "Service vehicle and reset service hours counter" },
  { 0x4881, FCT_DriveInhibit, "Seat Fault", "Valid direction selected with operator not seated or operator is not seated for a user configurable time in drive.", "Must be seated with switches inactive" },
  { 0x4882, FCT_DriveInhibit, "Two Direction Fault", "Both the forward and reverse switches have been active simultaneously for greater than 200 ms.", "Check vehicle wiring and reset switches" },
  { 0x4883, FCT_DriveInhibit, "SRO Fault", "FS1 active for user configurable delay (0x2914,2) without a direction selected.", "Deselect FS1" },
  { 0x4884, FCT_DriveInhibit, "Sequence Fault", "Any drive switch active at power up.", "Deselect all drive switches" },
  { 0x4885, FCT_DriveInhibit, "FS1 Recycle Fault", "FS1 active after a direction change and FS1 recycle function enabled (0x2914,1 bit 1)", "Deselect FS1" },
  { 0x4886, FCT_DriveInhibit, "Inch Fault", "Inch switch active along with any drive switch active (excluding inch switches), seat switch indicating operator present or handbrake switch active.", "" },
  { 0x4887, FCT_DriveInhibit, "Overload Fault", "Vehicle overloaded", "Remove overload condition" },
  { 0x4888, FCT_DriveInhibit, "Raised and Tilted Fault", "Scissor lift platform raised and tilted", "Lower platform" },
  { 0x4889, FCT_DriveInhibit, "Pothole Fault", "Scissor lift pothole protection active", "Move vehicle out of pot hole." },
  { 0x488A, FCT_DriveInhibit, "Traction Inhibit Fault", "Traction function inhibited using traction inhibit switch (0x2137)", "Deselect traction inhibit." },
  { 0x488B, FCT_DriveInhibit, "Illegal Mode Change Fault", "Vehicle changed from traction mode to pump mode (or vice versa) when direction selected", "Deselect all drive switches" },
  { 0x488C, FCT_DriveInhibit, "Tilt Sensor Fault", "Aichi error code (0x3802,0) set to 0x02", "Check tilt sensor" },
  { 0x488D, FCT_DriveInhibit, "Belly fault", "Belly function has activated.", "Deselect belly switch" },
  { 0x488E, FCT_DriveInhibit, "Mom dir fault", "Fault with momentary direction selection switch", "Release momentary direction switch" },
  { 0x4941, FCT_DriveInhibit, "Low Oil", "Not used", "" },
  { 0x4942, FCT_DriveInhibit, "PST Fault", "An issue has occurred with the PST unit", "Check PST unit" },
  { 0x4981, FCT_DriveInhibit, "Throttle Fault", "Throttle value (0x2620,0) is greater than 20% at power up.", "Release throttle" },
  { 0x49C1, FCT_DriveInhibit, "Slope Current Cutback Fault", "Motor model current limit has cutback back below level allowed by cutback table (0x3805) on slope", "Check for temperature or voltage cutback condition and take appropriate action" },
  { 0x49C2, FCT_DriveInhibit, "Entering Cutback", "Controller has entered thermal or voltage cutback region", "Check for temperature or voltage cutback condition and take appropriate action" },
  { 0x4A01, FCT_DriveInhibit, "Cutback", "Thermal or voltage cutback factors have reduced belowed user defined levels.", "Check for temperature or voltage cutback condition and take appropriate action" },
  { 0x4A81, FCT_DriveInhibit, "RPDO Timeout (drive inhibit)", "One or more configured RPDOs not received with 3s at start up or 500ms during normal operation.", "Check status of all nodes on CANbus. Check PDOs on all CANbus nodes are configured correctly and match up." },
  { 0x4B01, FCT_DriveInhibit, "CAN off bus (drive inhibit)", "CANbus off fault condition detected on multinode system. NOTE: This fault was added for Aichi, to replace Very Severe CAN off fault", "Check CANbus wiring" },
  { 0x4B02, FCT_DriveInhibit, "Ren Data", "Data missing from CAN (Renault only)", "Check connection to CANbus, ensure all devices on bus are communicating." },
  { 0x4B81, FCT_DriveInhibit, "CANopen anon EMCY level 2", "EMCY message received from non-Sevcon node and anonymous EMCY level (0x2830,0) is set to 2.", "Check status of non-Sevcon nodes on CANbus" },
  { 0x4C41, FCT_Severe, "Too many slaves", "Number of slaves (0x2810,0) set higher than maximum allowed number of slaves", "Check 0x2810,0 setting" },
  { 0x4D41, FCT_Severe, "Motor Isolation Fault", "Motor isolation contactor is open circuit", "Check isolation contactor and wiring" },
  { 0x4D42, FCT_Severe, "Motor Open Circuit Fault", "Motor terminal is open circuit or disconnected from controller", "Check motor wiring. Check controller condition" },
  { 0x4DC3, FCT_Severe, "Power Supply Critical", "Battery voltage has dropped below critical level", "Check controller voltage supply" },
  { 0x4E81, FCT_Severe, "RPDO Timeout (severe)", "One or more configured RPDOs not received with 3s at start up or 500ms during normal operation.", "Check status of all nodes on CANbus. Check PDOs on all CANbus nodes are configured correctly and match up." },
  { 0x4F01, FCT_Severe, "Unexpected slave state", "CANopen slave has changed to unexpected state", "Check status of all nodes on CANbus. " },
  { 0x4F02, FCT_Severe, "EMCY send failed", "Unable to transmit EMCY message", "Internal software fault" },
  { 0x4F41, FCT_Severe, "Internal Fault", "Internal software fault", "Internal software fault" },
  { 0x4F42, FCT_Severe, "Out of memory", "Out of memory", "Internal software fault" },
  { 0x4F43, FCT_Severe, "General DSP error", "Unknown error raised by motor model code", "Internal software fault" },
  { 0x4F44, FCT_Severe, "Timer Failed", "Unable to allocate timer", "Internal software fault" },
  { 0x4F45, FCT_Severe, "Queue Error", "Unable to post message to queue", "Internal software fault" },
  { 0x4F46, FCT_Severe, "Scheduler Error", "Unable to create task in scheduler", "Internal software fault" },
  { 0x4F47, FCT_Severe, "DSP Heartbeat Error", "Communication lost between host and DSP processors", "Internal hardware fault" },
  { 0x4F48, FCT_Severe, "I/O SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F49, FCT_Severe, "GIO SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4A, FCT_Severe, "LCM SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4B, FCT_Severe, "LCP SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4C, FCT_Severe, "OBD SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4D, FCT_Severe, "VA SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4E, FCT_Severe, "DMC SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F4F, FCT_Severe, "TracApp SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F50, FCT_Severe, "New Powerframe Detected", "New power frame detected.", "Recycle keyswitch" },
  { 0x4F51, FCT_Severe, "DSP Not Detected", "Communication lost between host and DSP processors", "Internal hardware fault" },
  { 0x4F52, FCT_Severe, "DSP Comms Error", "Communication lost between host and DSP processors", "Internal hardware fault" },
  { 0x4F53, FCT_Severe, "App Manager SS Error", "Internal software fault", "Internal software fault" },
  { 0x4F54, FCT_Severe, "Autozero range error", "Current sensor auto-zero current out of range", "Internal hardware fault" },
  { 0x4F55, FCT_Severe, "DSP parameter error", "Communication error between host and DSP processors", "Internal software fault" },
  { 0x4F56, FCT_Severe, "Motor in wrong direction", "Motor rotation detected as wrong direction. No longer supported", "Check motor wiring. " },
  { 0x4F57, FCT_Severe, "Motor stalled", "Motor rotation stalled. No longer supported", "Check motor wiring. " },
  { 0x4F81, FCT_Severe, "CANopen anon EMCY level 3", "EMCY message received from non-Sevcon node and anonymous EMCY level (0x2830,0) is set to 3.", "Check status of non-Sevcon nodes on CANbus" },
  { 0x5041, FCT_VerySevere, "Bad NVM Data", "EEPROM or flash configuration data corrupted and data can not be recovered.", "" },
  { 0x5042, FCT_VerySevere, "VPDO Out of Range", "VPDO mapped to non-existent or invalid object", "Check all VPDO mappings (0x3000 to 0x3400)" },
  { 0x5043, FCT_VerySevere, "Static Range Error", "At least one configuration object is out of range", "Set configuration object to valid value. Our of range object can be identified using 0x5621 or Engineering DVT CLI window." },
  { 0x5044, FCT_VerySevere, "Dynamic Range Error", "At least one configuration object is out of dynamic range. This is where one objects range depends on another object.", "Check all dynamic range objects. Engineering DVT CLI window indicates type of object which is out of range." },
  { 0x5045, FCT_VerySevere, "Auto-configuration Fault", "Unable to automatically configure I/O and vehicle setup.", "Check auto configuration objects (0x5810 and 0x5811)" },
  { 0x5081, FCT_VerySevere, "Invalid Steer Switches", "Steering switches are in an invalid state", "Check steering switches and wiring" },
  { 0x5101, FCT_VerySevere, "Line Contactor o/c", "Line contactor did not close when coil is energized.", "Check line contactor and wiring" },
  { 0x5102, FCT_VerySevere, "Line Contactor welded", "Line contactor closed when coil is denergized.", "Check line contactor and wiring" },
  { 0x5141, FCT_VerySevere, "Beltloader Fault", "Unable to change between traction and pump motors on beltloader.", "Check change over contactors and motor wiring. " },
  { 0x5142, FCT_VerySevere, "Ren Signal", "Fault signalled by Renault vehicle network", "Check peripheral Renault devices" },
  { 0x5143, FCT_VerySevere, "VERLOG", "VERLOG signal failure", "Check peripheral Renault devices" },
  { 0x5181, FCT_VerySevere, "Digital Input Wire Off", "Digital input wire-off", "Check wiring" },
  { 0x5182, FCT_VerySevere, "Analogue Input Wire Off", "Analogue input outside of allowed range (0x46cX)", "Check wiring" },
  { 0x5183, FCT_VerySevere, "Analogue Output Over Current", "Contactor driver over current", "Ensure contactor doesn't exceed maximum current and check contactor wiring" },
  { 0x5184, FCT_VerySevere, "Analogue Output On with No Failsafe", "Internal hardware failsafe circuitry not working", "Internal hardware fault" },
  { 0x5185, FCT_VerySevere, "Analogue Output Off with Failsafe", "Contactor driver not working", "Internal hardware fault" },
  { 0x5186, FCT_VerySevere, "Analogue Output Over Temperature", "Contactor driver over temperature", "Ensure contactor doesn't exceed maximum current and check contactor wiring" },
  { 0x5187, FCT_VerySevere, "Analogue Output Under Current", "Contactor driver unable to achieve current target in current mode", "Ensure contactor driver current target is within range" },
  { 0x5188, FCT_VerySevere, "Analogue Output Short Circuit", "Contactor driver MOSFET short circuit detected", "Internal hardware fault" },
  { 0x51C1, FCT_VerySevere, "Power Supply Interrupt", "Not used", "" },
  { 0x51C2, FCT_VerySevere, "Capacitor Precharge Failure", "Capacitor voltage (0x5100,3) did not rise above 5V at power up", "Check power wiring" },
  { 0x5201, FCT_VerySevere, "Heatsink overtemp", "Controller heat sink has reached critical high temperature, and has shut down.", "Allow controller to cool down to normal operating temperature." },
  { 0x52C1, FCT_VerySevere, "DSP Encoder Fault", "Encoder input wire-off is detected.", "Check encoder wiring" },
  { 0x52C2, FCT_VerySevere, "DSP Overcurrent Fault", "Motor current exceeded controller rated maximum", "Check motor configuration and wiring" },
  { 0x52C3, FCT_VerySevere, "DSP Control Fault", "Motor controller unable to maintain control of motor", "Check motor configuration. Ensure motor speed is not too high." },
  { 0x52C4, FCT_VerySevere, "Motor Overspeed Fault", "Motor control tripped due to motor overspeed", "Check motor configuration. Ensure motor speed is not too high." },
  { 0x52C5, FCT_VerySevere, "Encoder Alignment Severe", "Encoder is not aligned properly.", "Ensure encoder offset is correctly set or re-align encoder" },
  { 0x5301, FCT_VerySevere, "CANBUS Fault", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5302, FCT_VerySevere, "Bootup not received", "CANopen slave has not transmitted boot up message at power up", "Check status of all nodes on CANbus. " },
  { 0x5303, FCT_VerySevere, "LPRX queue overrun", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5304, FCT_VerySevere, "LPTX queue overrun", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5305, FCT_VerySevere, "HPRX queue overrun", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5306, FCT_VerySevere, "HPTX queue overrun", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5307, FCT_VerySevere, "CAN overrun", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5308, FCT_VerySevere, "CAN off bus", "CANbus fault condition detected on multinode system. ", "Check CANbus wiring" },
  { 0x5309, FCT_VerySevere, "Nodeguarding Failed", "Not used", "" },
  { 0x530A, FCT_VerySevere, "Short PDO received", "Received RPDO doesn't contains enough bytes", "Check PDOs on all CANbus nodes are configured correctly and match up." },
  { 0x530B, FCT_VerySevere, "CANopen Heartbeat Failed", "Heartbeat not received within configured time out (0x1016)", "Check status of all nodes on CANbus. " },
  { 0x530C, FCT_VerySevere, "CANopen slave in wrong state", "CANopen slave has changed to unexpected state", "Check status of all nodes on CANbus. " },
  { 0x530D, FCT_VerySevere, "CAN ESTAT set", "Internal CANbus fault ", "Internal software fault" },
  { 0x530E, FCT_VerySevere, "SDO HDL Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x530F, FCT_VerySevere, "SDO Timeout Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5310, FCT_VerySevere, "SDO Abort Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5311, FCT_VerySevere, "SDO State Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5312, FCT_VerySevere, "SDO Toggle Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5313, FCT_VerySevere, "SDO Rec Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5314, FCT_VerySevere, "SDO Len Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5315, FCT_VerySevere, "SDO Send Error", "Internal CANbus fault ", "Internal software fault" },
  { 0x5316, FCT_VerySevere, "SDO unknown event", "Internal CANbus fault ", "Internal software fault" },
  { 0x5317, FCT_VerySevere, "SDO Bad SRC", "Internal CANbus fault ", "Internal software fault" },
  { 0x5318, FCT_VerySevere, "SDO bad error number", "Internal CANbus fault ", "Internal software fault" },
  { 0x5319, FCT_VerySevere, "Motor slave in wrong state", "Motor slave in wrong state", "Check status of all nodes on CANbus controlling motor slaves. Check local motor slaves on master. Ensure configuration is correct." },
  { 0x531A, FCT_VerySevere, "Ren Protocol", "CAN device on Renault Twizy not responding", "Check connection to CANbus, ensure all devices on bus are communicating." },
  { 0x5341, FCT_VerySevere, "Invalid DSP Protocol", "DSP reports invalid protocol version on dual processor platform", "Internal software fault" },
  { 0x5342, FCT_VerySevere, "OSC Watchdog Fault", "Internal hardware fault", "Internal hardware fault" },
  { 0x5343, FCT_VerySevere, "Fault List Overflow", "Attempting to set too many faults.", "Internal software fault" },
  { 0x5344, FCT_VerySevere, "DSP SPI Comms Fault", "Communication error between host and DSP processors", "Internal hardware fault" },
  { 0x5381, FCT_VerySevere, "CANopen anon EMCY level 4", "EMCY message received from non-Sevcon node and anonymous EMCY level (0x2830,0) is set to 4.", "Check status of non-Sevcon nodes on CANbus" },
  { 0x5441, FCT_ReturnToBase, "Incompatible hardware version", "Detected controller hardware version incompatible with software", "Check correct software is programmed into controller. Reprogram if necessary" },
  { 0x5442, FCT_ReturnToBase, "Calibration Fault", "Calibration settings in controller are out of range", "Controller requires recalibration in production" },
  { 0x54C1, FCT_ReturnToBase, "DP Overvoltage", "Voltage on B+ terminal exceeds rated maximum for controller", "Check battery condition and wiring" },
  { 0x54C2, FCT_ReturnToBase, "DSP Powerframe Fault", "Motor current exceeded controller rated maximum", "Check motor configuration and wiring" },
  { 0x54C3, FCT_ReturnToBase, "MOSFET s/c M1>B+", "MOSFET s/c detection on M1 top devices", "Check motor wiring. Check controller condition" },
  { 0x54C4, FCT_ReturnToBase, "MOSFET s/c M1>B-", "MOSFET s/c detection on M1 bottom devices", "Check motor wiring. Check controller condition" },
  { 0x54C5, FCT_ReturnToBase, "MOSFET s/c M2>B+", "MOSFET s/c detection on M2 top devices", "Check motor wiring. Check controller condition" },
  { 0x54C6, FCT_ReturnToBase, "MOSFET s/c M2>B-", "MOSFET s/c detection on M2 bottom devices", "Check motor wiring. Check controller condition" },
  { 0x54C7, FCT_ReturnToBase, "MOSFET s/c M3>B+", "MOSFET s/c detection on M3 top devices", "Check motor wiring. Check controller condition" },
  { 0x54C8, FCT_ReturnToBase, "MOSFET s/c M3>B-", "MOSFET s/c detection on M3 bottom devices", "Check motor wiring. Check controller condition" },
  { 0x54C9, FCT_ReturnToBase, "MOSFET s/c checks incomplete", "Unable to complete MOSFET s/c tests at power up", "Internal software fault" },
  { 0x54CA, FCT_ReturnToBase, "Pump Mosfet S/C", "MOSFET s/c detection Pump Mosfet Devices", "Check motor wiring. Check controller condition" },
  { 0x5741, FCT_ReturnToBase, "Invalid Powerframe Rating", "Unable to identify hardware", "Internal hardware fault" },
  { 0x5781, FCT_ReturnToBase, "CANopen anon EMCY level 5", "EMCY message received from non-Sevcon node and anonymous EMCY level (0x2830,0) is set to 5.", "Check status of non-Sevcon nodes on CANbus" },
  { 0, FCT_ReturnToBase, NULL, NULL, NULL }
};


#define FC_MomDir         0x488e      // button D/R pushed before "GO"
#define FC_PreOp          0x4681      // controller in pre-operational state
#define FC_SlaveState     0x4f01      // unexpected slave state: no error/suppress in cfgmode

void SevconClient::EmcyListener(string event, void* data)
{
  CANopenEMCYEvent_t& emcy = *((CANopenEMCYEvent_t*)data);

  if (emcy.origin != m_twizy->m_can1 || emcy.nodeid != 1 || emcy.code == 0)
    return;

  uint32_t fault = emcy.data[1] << 8 | emcy.data[0];
  ESP_LOGW(TAG, "Sevcon: received fault code 0x%04x", fault);
  MyEvents.SignalEvent("vehicle.fault.code", (void*)fault);

  // button push?
  if (fault == FC_MomDir) {
    m_buttoncnt += 2;
    return;
  }

  // preop mode not requested by this session?
  if (fault == FC_PreOp && m_cfgmode_request == false) {
    ESP_LOGD(TAG, "Sevcon: detected preop state not requested by us; resolving");
    SendRequestState(CONC_Start);
    return;
  }
  // unexpected slave state requested by us?
  else if (fault == FC_SlaveState && m_cfgmode_request == true) {
    ESP_LOGD(TAG, "Sevcon: ignoring unexpected slave state requested by us");
    return;
  }

  // forward new faults to user:
  if (fault != m_lastfault) {
    if (xQueueSend(m_faultqueue, &fault, 0) == pdTRUE)
      m_lastfault = fault;
  }
}


void SevconClient::SendFaultAlert(uint16_t faultcode)
{
  // check for known fault code:
  int i;
  for (i = 0; SC_FaultCode[i].code; i++) {
    if (SC_FaultCode[i].code == faultcode) {
      // found:
      ESP_LOGW(TAG, "SEVCON Fault 0x%04x [%s]: %s\nInfo: %s\nAction: %s\n",
        faultcode, SC_FaultTypeName[SC_FaultCode[i].type],
        SC_FaultCode[i].message, SC_FaultCode[i].description, SC_FaultCode[i].action);
      MyNotify.NotifyStringf("alert", "xrt.sevcon.fault", "SEVCON Fault 0x%04x [%s]: %s\nInfo: %s\nAction: %s\n",
        faultcode, SC_FaultTypeName[SC_FaultCode[i].type],
        SC_FaultCode[i].message, SC_FaultCode[i].description, SC_FaultCode[i].action);
      return;
    }
  }

  // not found: fetch description from controller:
  CANopenJob job;
  uint8_t buf[200] = { '?', 0 };
  if (Write(job, 0x5610, 0x01, faultcode) == COR_OK) {
    if (Read(job, 0x5610, 0x02, buf, sizeof(buf)-1) == COR_OK) {
      buf[job.sdo.xfersize] = 0;
    }
  }
  ESP_LOGW(TAG, "SEVCON Fault 0x%04x: %s\n", faultcode, buf);
  MyNotify.NotifyStringf("alert", "xrt.sevcon.fault", "SEVCON Fault 0x%04x: %s\n", faultcode, buf);
}


const std::string SevconClient::GetDeviceErrorName(uint32_t device_error)
{
  std::string name;
  if (device_error < 0xde000000)
    name = CANopen::GetAbortCodeName(device_error);
  else switch (device_error & 0x00ffffff)
  {
    case  0:    name="No error"; break;
    case  1:    name="General error"; break;
    case  2:    name="Nothing to transmit"; break;
    case  3:    name="Invalid service"; break;
    case  4:    name="Not in pre-operational"; break;
    case  5:    name="Not in operational"; break;
    case  6:    name="Can't go to pre-operational"; break;
    case  7:    name="Can't go to operational"; break;
    case  8:    name="Access level too low"; break;
    case  9:    name="Login failed"; break;
    case 10:    name="Range underflow"; break;
    case 11:    name="Range overflow"; break;
    case 12:    name="Invalid value"; break;
    case 13:    name="EEPROM write failed"; break;
    case 14:    name="Unable to reset service time"; break;
    case 15:    name="Cannot reset log"; break;
    case 16:    name="Cannot read log"; break;
    case 17:    name="Invalid store command"; break;
    case 18:    name="Bootloader failure"; break;
    case 19:    name="DSP update failed"; break;
    case 20:    name="GIO module error failed"; break;
    case 21:    name="Backdoor write failed"; break;
    case 23:    name="Cannot write to DSP"; break;
    case 24:    name="Cannot read from DSP"; break;
    case 25:    name="Peek time out"; break;
    case 32:    name="Checksum calculation failed"; break;
    case 33:    name="PDO not copied"; break;
    default:
      char val[12];
      sprintf(val, "%d", (int) device_error & 0x00ffffff);
      name = val;
  }
  return name;
}

const std::string SevconClient::GetResultString(CANopenResult_t result, uint32_t detail /*=0*/)
{
  std::string name = CANopen::GetResultString(result);
  if (detail)
    {
    name.append("; ");
    name.append(GetDeviceErrorName(detail));
    }
  return name;
}

const std::string SevconClient::GetResultString(CANopenJob& job)
{
  if (job.type == COJT_ReadSDO || job.type == COJT_WriteSDO)
    return GetResultString(job.result, job.sdo.error);
  else
    return GetResultString(job.result, 0);
}


void SevconClient::AddFaultInfo(ostringstream& buf, uint16_t faultcode)
{
  char xbuf[200];

  // add fault code:
  sprintf(xbuf, "%04x", faultcode);
  buf << xbuf << ",";

  if (faultcode == 0) {
    buf << "<No fault>";
    return;
  }

  // check for known fault code:
  int i;
  for (i = 0; SC_FaultCode[i].code; i++) {
    if (SC_FaultCode[i].code == faultcode) {
      // found, add description from table:
      buf
        << "|" << SC_FaultTypeName[SC_FaultCode[i].type]
        << "|" << mp_encode(std::string(SC_FaultCode[i].message))
        << "|" << mp_encode(std::string(SC_FaultCode[i].description))
        << "|" << mp_encode(std::string(SC_FaultCode[i].action));
      return;
    }
  }

  // not found, fetch description from controller:
  CANopenJob job;
  strcpy(xbuf, "?");
  if (Write(job, 0x5610, 0x01, faultcode) == COR_OK) {
    if (Read(job, 0x5610, 0x02, (uint8_t*)xbuf, sizeof(xbuf)-1) == COR_OK) {
      xbuf[job.sdo.xfersize] = 0;
    }
  }
  buf << mp_encode(std::string(xbuf));
}


/**
 * QueryLogs:
 *  which: log category: 1=Alerts, 2=Faults FIFO, 3=System FIFO, 4=Event counter, 5=Min/max monitor
 *  start: first entry to fetch, default=0 (returns max 10 entries per call)
 *  *retcnt: number of total entries in the log category
 */
CANopenResult_t SevconClient::QueryLogs(int verbosity, OvmsWriter* writer, int which, int start, int* totalcnt, int* sendcnt)
{
  CANopenResult_t err = COR_OK;
  SevconJob sc(this);
  uint32_t sdoval=0;
  #define _readsdo(idx, sub)          ((err = sc.Read(idx, sub, sdoval)) == COR_OK)
  #define _writesdo(idx, sub, val)    ((err = sc.Write(idx, sub, val)) == COR_OK)
  #define _output(buf) \
    if (writer) { \
      if (verbosity > buf.tellp()) \
        verbosity -= writer->puts(buf.str().c_str()); \
    } else { \
      MyNotify.NotifyString("data", "xrt.sevcon.log", buf.str().c_str()); \
    }

  int n, cnt=0, outcnt=0;
  ostringstream buf;


  /* Key time:
      RT-ENG-LogKeyTime
              ,0,86400
              ,<KeyHour>,<KeyMinSec>
   */
  if (!_readsdo(0x5200, 0x01))
    return err;
  buf << "RT-ENG-LogKeyTime,0,86400," << sdoval;
  if (!_readsdo(0x5200, 0x02))
    return err;
  buf << "," << sdoval;
  _output(buf);


  /* Alerts (active faults):
      RT-ENG-LogAlerts
              ,<n>,86400
              ,<Code>,<Description>
   */
  if (which == 1) {
    cnt = _readsdo(0x5300, 0x01) ? sdoval : 0;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (!_writesdo(0x5300, 0x02, n)) break;
      if (!_readsdo(0x5300, 0x03)) break;
      buf.str(""); buf.clear();
      buf << "RT-ENG-LogAlerts," << n << ",86400,";
      AddFaultInfo(buf, sdoval);
      _output(buf);
      outcnt++;
    }
  }


  /* Faults FIFO:
      RT-ENG-LogFaults
              ,<n>,86400
              ,<Code>,<Description>
              ,<TimeHour>,<TimeMinSec>
              ,<Data1>,<Data2>,<Data3>
   */
  else if (which == 2) {
    cnt = _readsdo(0x4110, 0x02) ? sdoval : 0;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (!_writesdo(0x4111, 0x00, n)) break;
      if (!_readsdo(0x4112, 0x01)) break;
      buf.str(""); buf.clear();
      buf << "RT-ENG-LogFaults," << n << ",86400,";
      AddFaultInfo(buf, sdoval);
      _readsdo(0x4112, 0x02); buf << "," << sdoval;
      _readsdo(0x4112, 0x03); buf << "," << sdoval;
      _readsdo(0x4112, 0x04); buf << "," << sdoval;
      _readsdo(0x4112, 0x05); buf << "," << sdoval;
      _readsdo(0x4112, 0x06); buf << "," << sdoval;
      _output(buf);
      outcnt++;
    }
  }


  /* System FIFO:
      RT-ENG-LogSystem
              ,<n>,86400
              ,<Code>,<Description>
              ,<TimeHour>,<TimeMinSec>
              ,<Data1>,<Data2>,<Data3>
   */
  else if (which == 3) {
    cnt = _readsdo(0x4100, 0x02) ? sdoval : 0;
    for (n=start; n<cnt && n<(start+10); n++) {
      if (!_writesdo(0x4101, 0x00, n)) break;
      if (!_readsdo(0x4102, 0x01)) break;
      buf.str(""); buf.clear();
      buf << "RT-ENG-LogSystem," << n << ",86400,";
      AddFaultInfo(buf, sdoval);
      _readsdo(0x4102, 0x02); buf << "," << sdoval;
      _readsdo(0x4102, 0x03); buf << "," << sdoval;
      _readsdo(0x4102, 0x04); buf << "," << sdoval;
      _readsdo(0x4102, 0x05); buf << "," << sdoval;
      _readsdo(0x4102, 0x06); buf << "," << sdoval;
      _output(buf);
      outcnt++;
    }
  }


  /* Event counter:
      RT-ENG-LogCounts
              ,<n>,86400
              ,<Code>,<Description>
              ,<LastTimeHour>,<LastTimeMinSec>
              ,<FirstTimeHour>,<FirstTimeMinSec>
              ,<Count>
   */
  else if (which == 4) {
    if (start > 10)
      start = 10;
    cnt = 10;
    for (n=start; n<10; n++) {
      if (!_readsdo(0x4201+n, 0x01)) break;
      buf.str(""); buf.clear();
      buf << "RT-ENG-LogCounts," << n << ",86400,";
      AddFaultInfo(buf, sdoval);
      _readsdo(0x4201+n, 0x04); buf << "," << sdoval;
      _readsdo(0x4201+n, 0x05); buf << "," << sdoval;
      _readsdo(0x4201+n, 0x02); buf << "," << sdoval;
      _readsdo(0x4201+n, 0x03); buf << "," << sdoval;
      _readsdo(0x4201+n, 0x06); buf << "," << sdoval;
      _output(buf);
      outcnt++;
    }
  }


  /* Min / max monitor:
      MP-0 HRT-ENG-LogMinMax
	,<n>,86400
	,<BatteryVoltageMin>,<BatteryVoltageMax>
	,<CapacitorVoltageMin>,<CapacitorVoltageMax>
	,<MotorCurrentMin>,<MotorCurrentMax>
	,<MotorSpeedMin>,<MotorSpeedMax>
	,<DeviceTempMin>,<DeviceTempMax>
   */
  else if (which == 5) {
    if (start > 2)
      start = 2;
    cnt = 2;
    for (n=start; n<2; n++) {
      if (!_readsdo(0x4300+n, 0x02)) break;
      buf.str(""); buf.clear();
      buf << "RT-ENG-LogMinMax," << n << ",86400," << sdoval;
      _readsdo(0x4300+n, 0x03); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x04); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x05); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x06); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x07); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x0a); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x0b); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x0c); buf << "," << (int16_t) sdoval;
      _readsdo(0x4300+n, 0x0d); buf << "," << (int16_t) sdoval;
      _output(buf);
      outcnt++;
    }
  }


  if (totalcnt)
    *totalcnt = cnt;
  if (sendcnt)
    *sendcnt = outcnt;

  return err;

  #undef _readsdo
  #undef _writesdo
  #undef _output
}


/**
 * ResetLogs:
 *  which: 99=ALL, 2=Faults FIFO, 3=System FIFO, 4=Event counter, 5=Min/max monitor
 *  *retcnt: number of entries cleared
 *
 * Note: Alerts can only be reset by power cycle
 */
CANopenResult_t SevconClient::ResetLogs(int which, int* retcnt)
{
  CANopenResult_t err = COR_OK;
  uint32_t cnt=0, total=0;
  SevconJob sc(this);

  // Clear faults FIFO:
  if (which == 2 || which == 99) {
    cnt = 0;
    if (err == COR_OK)
      err = sc.Read(0x4110, 0x02, cnt);
    if (err == COR_OK)
      err = sc.Write(0x4110, 0x01, 1);
    if (err == COR_OK)
      total += cnt;
  }

  // Clear system FIFO:
  if (which == 3 || which == 99) {
    cnt = 0;
    if (err == COR_OK)
      err = sc.Read(0x4100, 0x02, cnt);
    if (err == COR_OK)
      err = sc.Write(0x4100, 0x01, 1);
    if (err == COR_OK)
      total += cnt;
  }

  // Clear event counter:
  if (which == 4 || which == 99) {
    if (err == COR_OK)
      err = sc.Write(0x4200, 0x01, 1);
    if (err == COR_OK)
      total += 10;
  }

  // Clear min/max monitor:
  if (which == 5 || which == 99) {
    if (err == COR_OK)
      err = sc.Write(0x4300, 0x01, 1);
    if (err == COR_OK)
      total += 2;
  }

  if (retcnt)
    *retcnt = total;
  return err;
}
