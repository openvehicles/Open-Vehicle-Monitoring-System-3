/*
;    Project:       Open Vehicle Monitor System
;    Date:          11th Sep 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2018       Marcos Mezo
;    (C) 2019       Thomas Heuer @Dimitrie78
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

#include "ovms_log.h"
static const char *TAG = "v-zoe";

#include <stdio.h>
#include <string>
#include <iomanip>
#include "pcp.h"
#include "ovms_metrics.h"
#include "ovms_events.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "ovms_notify.h"
#include "ovms_peripherals.h"
#include "ovms_netmanager.h"

#include "vehicle_renaultzoe.h"

// Pollstate 0 - POLLSTATE_OFF      - car is off
// Pollstate 1 - POLLSTATE_ON       - car is on
// Pollstate 2 - POLLSTATE_RUNNING  - car is driving
// Pollstate 3 - POLLSTATE_CHARGING - car is charging
static const OvmsVehicle::poll_pid_t vehicle_renaultzoe_polls[] = {
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2002, { 0, 10, 10, 10 } },  // SOC
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2006, { 0, 10, 10, 10 } },  // Odometer
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3203, { 0, 10, 10, 10 } },  // Battery Voltage
  { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3204, { 0, 30, 1, 2 }, 0 },  // Battery Current
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3028, { 0, 10, 10, 10 } },  // 12Battery Current
  // 7ec,24,39,.005,0,0,kwh,22320C,62320C,ff,Available discharge Energy
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x320C, { 0, 10, 10, 10 } },  // Available discharge Energy
  // 7ec,30,31,1,0,0,,223332,623332,ff,Electrical Machine functionning Authorization,0:Not used;1:Inverter Off;2:Inverter On\n" //
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3332, { 0, 10, 10, 10 } },  //  Inverter Off: 1; Inverter On: 2
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2005, { 0, 10, 10, 10 } },  // 12Battery Voltage
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3206, { 0, 10, 10, 10 } },  // Battery SOH
  //{ 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x33dc, { 0, 10, 10, 10 } },  // Consumed Domnestic Engergy
  //{ 0x75a, 0x77e, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x3018, { 0, 10, 10, 10 } },  // DCDC Temp
  //{ 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x504A, { 0, 10, 10, 10 } },  // Mains active Power consumed
  //{ 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5063, { 0, 10, 10, 10 } },  // Charging State
  // { 0x792, 0x793, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x5062, { 0, 10, 10, 10 } },  // Ground Resistance
  //{ 0x79b, 0x7bb, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, { 0, 10, 10, 10 } },  // Temp Bat Module 1
  //+"7bc,28,39,1,4094,0,N·m,224B7C,624B7C,ff,Electric brake wheels torque request\n" //
      //+ "Uncoupled Braking Pedal,2197,V,7bc,79c,UBP,-,5902ff\n"
  //{ 0x79c, 0x7bc, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, { 0, 120, 1, 120 } }, // Braking Pedal
  //{ 0x742, 0x762, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x012D, { 0, 0, 10, 10 } }, // Motor temperature (nope)
  // -END-
  { 0, 0, 0x00, 0x00, { 0, 0, 0, 0 }, 0 }
};

OvmsVehicleRenaultZoe* OvmsVehicleRenaultZoe::GetInstance(OvmsWriter* writer /*=NULL*/)
{
  OvmsVehicleRenaultZoe* zoe = (OvmsVehicleRenaultZoe*) MyVehicleFactory.ActiveVehicle();
  string type = StdMetrics.ms_v_type->AsString();
  if (!zoe || type != "RZ") {
    if (writer)
      writer->puts("Error: Renault Zoe vehicle module not selected");
    return NULL;
  }
  return zoe;
}

OvmsVehicleRenaultZoe::OvmsVehicleRenaultZoe() {
  ESP_LOGI(TAG, "Start Renault Zoe vehicle module");

  StandardMetrics.ms_v_type->SetValue("RZ");
  StandardMetrics.ms_v_charge_inprogress->SetValue(false);

	// Zoe CAN bus runs at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

	// Poll Zoe Specific PIDs
  POLLSTATE_OFF;
  PollSetPidList(m_can1,vehicle_renaultzoe_polls);
  
  MyConfig.RegisterParam("xrz", "Renault Zoe", true, true);
  ConfigChanged(NULL);
  
  // init metrics:
  mt_pos_odometer_start   = MyMetrics.InitFloat("xrz.v.pos.odometer.start", SM_STALE_MID, 0, Kilometers);
  mt_bus_awake            = MyMetrics.InitBool("xrz.v.bus.awake", SM_STALE_MIN, false);
  mt_available_energy     = MyMetrics.InitFloat("xrz.v.avail.energy", SM_STALE_MID, 0, kWh);
	
	// init commands:
  cmd_zoe = MyCommandApp.RegisterCommand("zoe", "Renault Zoe");
	cmd_zoe->RegisterCommand("trip", "Show vehicle trip", zoe_trip);
	//MyCommandApp.RegisterCommand("trip", "Show vehicle trip", zoe_trip);
  
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif
}

OvmsVehicleRenaultZoe::~OvmsVehicleRenaultZoe() {
  ESP_LOGI(TAG, "Stop Renault Zoe vehicle module");
  
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
}

/**
 * Handles incoming CAN-frames on bus 1
 */
void OvmsVehicleRenaultZoe::IncomingFrameCan1(CAN_frame_t* p_frame) {
	uint8_t *data = p_frame->data.u8;
	//ESP_LOGI(TAG, "PID:%x DATA: %02x %02x %02x %02x %02x %02x %02x %02x", p_frame->MsgID, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  
  if (m_candata_poll != 1 && m_ready) {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    mt_bus_awake->SetValue(true);
    //StandardMetrics.ms_v_env_awake->SetValue(true);
    m_candata_poll = 1;
    if (m_enable_write) POLLSTATE_ON;
  }
  
  m_candata_timer = RZ_CANDATA_TIMEOUT;
  
  static bool isCharging = false;
  static bool lastCharging = false;
  
  // # SID (defaults to ID.startBit.[responseId]), ID (hex), startBit, endBit, resolution, offset (aplied BEFORE resolution multiplication)
  // , decimals, unit, requestID (hex string), responseID (hex string), options (hex, see MainActivity for definitions), optional name, optional list
  switch (p_frame->MsgID) {
    case 0x023:
      // 023,0,15,1,0,0,,,,ff,AIRBAGCrash
      break;
    case 0x0c6:
      // 0c6,0,15,1,32768,1,°,,,ff,Steering Position
      // 0c6,16,31,1,32768,1,°/s,,,ff,Steering Acceleration
      // 0c6,32,47,1,32768,1,°,,,ff,SteeringWheelAngle_Offset
      // 0c6,48,50,1,0,0,,,,ff,SwaSensorInternalStatus
      // 0c6,51,54,1,0,0,,,,ff,SwaClock
      // 0c6,56,63,1,0,0,,,,ff,SwaChecksum
      break;
    case 0x12e:
      // 12e,0,7,1,198,0,,,,ff,LongitudinalAccelerationProc
      // 12e,8,23,1,32768,0,,,,ff,TransversalAcceleration
      // 12e,24,35,0.1,2047,1,deg/s,,,ff,Yaw rate
      break;
    case 0x130:
      // 130,8,10,1,0,0,,,,ff,UBP_Clock
      // 130,11,12,1,0,0,,,,ff,HBB_Malfunction
      // 130,16,17,1,0,0,,,,ff,EB_Malfunction
      // 130,18,19,1,0,0,,,,ff,EB_inProgress
      // 130,20,31,1,4094,0,Nm,,,ff,ElecBrakeWheelsTorqueRequest
      // 130,32,38,1,0,0,%,,,ff,BrakePedalDriverWill
      // 130,40,41,1,0,0,,,,ff,HBA_ActivationRequest
      // 130,42,43,1,0,0,,,,ff,PressureBuildUp
      // 130,44,55,-3,4094,0,Nm,,,ff,DriverBrakeWheelTq_Req 
      // 130,56,63,1,0,0,,,,ff,CheckSum_UBP
      break;
    case 0x17a:
      // 17a,24,27,1,0,0,,,,ff,Transmission Range
      // 17a,48,63,0.5,12800,1,Nm,,,ff,Estimated Wheel Torque
      break;
    case 0x17e:
      // 17e,40,41,1,0,0,,,,ff,CrankingAuthorisation_AT
      // 17e,48,51,1,0,0,,,,ff,GearLeverPosition
      break;
    case 0x186:
      // 186,0,15,0.125,0,2,rpm,,,ff,Engine RPM
      // 186,16,27,0.5,800,1,Nm,,,ff,MeanEffectiveTorque
      // 186,28,39,0.5,800,0,Nm,,,ff,RequestedTorqueAfterProc
      // 186,40,49,0.125,0,1,%,,,ff,Throttle
      // 186,50,50,1,0,0,,,,ff,ASR_MSRAcknowledgement
      // 186,51,52,1,0,0,,,,ff,ECM_TorqueRequestStatus
      break;
    case 0x18a:
      // 18a,16,25,0.125,0,2,%,,,ff,Throttle
      // 18a,27,38,0.5,800,1,Nm,,,ff,Coasting Torque
      break;
    case 0x1f6:
      // 1f6,0,1,1,0,0,,,,ff,Engine Fan Speed
      // 1f6,3,7,100,0,0,W,,,ff,Max Electrical Power Allowed
      // 1f6,8,9,1,0,0,,,,ff,ElectricalPowerCutFreeze
      // 1f6,10,11,1,0,0,,,,ff,EngineStatus_R
      // 1f6,12,15,1,0,0,,,,ff,EngineStopRequestOrigine
      // 1f6,16,17,1,0,0,,,,ff,CrankingAuthorization_ECM
      // 1f6,19,20,1,0,1,,,,ff,Brake Pedal
      // 1f6,23,31,0.1,0,1,bar,,,ff,AC High Pressure Sensor
      break;
    case 0x1f8:
      // 1f8,0,7,1,0,0,,,,ff,Checksum EVC
      // 1f8,12,13,1,0,0,,,,ff,EVCReadyAsActuator
      // 1f8,16,27,1,4096,0,Nm,,,ff,TotalPotentialResistiveWheelsTorque
      // 1f8,28,39,-1,4096,0,Nm,,,ff,ElecBrakeWheelsTorqueApplied
      // 1f8,40,50,10,0,0,Rpm,,,ff,ElecEngineRPM
      // 1f8,52,54,1,0,0,,,,ff,EVC_Clock
      // 1f8,56,58,1,0,0,,,,ff,GearRangeEngagedCurrent
      // 1f8,62,63,1,0,0,,,,ff,DeclutchInProgress
      break;
    case 0x1fd:
      // 1fd,0,7,0.390625,0,1,%,,,ff,14V Battery Current?
      StandardMetrics.ms_v_bat_12v_current->SetValue((float) (CAN_BYTE(0) != 0xff) ? CAN_BYTE(0) * 0.390625 : 0);
      // 1fd,8,9,1,0,0,,,,ff,SCH Refuse to Sleep
      // 1fd,17,18,1,0,0,,,,ff,Stop Preheating Counter
      // 1fd,19,20,1,0,0,,,,ff,Start Preheating Counter
      // 1fd,21,31,1,0,0,min,,,ff,Time left before vehicle wakeup
      // 1fd,32,32,1,0,0,,,,ff,Pre heating activation
      // 1fd,33,39,1,0,0,min,,,ff,LeftTimeToScheduledTime
      // 1fd,40,47,25,0,0,W,,,ff,Climate Available Power
      // 1fd,48,55,1,80,0,kW,,,ff,Consumption
      break;
    case 0x212:
      // 212,8,9,1,0,0,,,,ff,Starter Status
      // 212,10,11,1,0,0,,,,ff,Rear Gear Engaged
      break;
    case 0x242:
      // 242,0,0,1,0,0,,,,ff,ABS in Regulation
      // 242,1,1,1,0,0,,,,ff,ABS Malfunction
      // 242,2,2,1,0,0,,,,ff,ASR in Regulation
      // 242,3,3,1,0,0,,,,ff,ASR Malfunction
      // 242,5,5,1,0,0,,,,ff,AYC in Regulation
      // 242,6,6,1,0,0,,,,ff,AYC Malfunction
      // 242,7,7,1,0,0,,,,ff,MSR in Regulation
      // 242,8,8,1,0,0,,,,ff,MSR Malfunction
      // 242,9,12,1,0,0,,,,ff,ESP Clock
      // 242,13,15,1,0,0,,,,ff,ESP Torque Control Type
      // 242,16,27,0.5,800,1,Nm,,,ff,ASR Dynamic Torque Request
      // 242,28,39,0.5,800,1,Nm,,,ff,ASR Static Torque Request
      // 242,40,51,0.5,800,1,Nm,,,ff,MSR Torque Request
      break;
    case 0x29a:
      // 29a,0,15,0.0416666666666667,0,2,rpm,,,ff,Rpm Front Right
      // 29a,16,31,0.0416666666666667,0,2,rpm,,,ff,Rpm Front Left
      // 29a,32,47,0.01,0,2,km/h,,,ff,Vehicle Speed
      // 29a,52,55,1,0,0,,,,ff,Vehicle Speed Clock
      // 29a,56,63,1,0,0,,,,ff,Vehicle Speed Checksum
      break;
    case 0x29c:
      // 29c,0,15,0.0416666666666667,0,2,rpm,,,ff,Rpm Rear Right
      // 29c,16,31,0.0416666666666667,0,2,rpm,,,ff,Rpm Rear Left
      // 29c,48,63,0.01,0,2,km/h,,,ff,Vehicle Speed
      break;
    case 0x2b7:
      // 2b7,32,33,1,0,0,,,,ff,EBD Active
      // 2b7,34,35,1,0,0,,,,ff,HBA Active
      // 2b7,36,37,1,0,0,,,,ff,ESC HBB Malfunction
      break;
    case 0x352:
      // 352,0,1,1,0,0,,,,ff,ABS Warning Request
      // 352,2,3,1,0,0,,,,ff,ESP Stop Lamp Request
      // 352,24,31,1,0,0,,,,ff,Brake pressure
      break;
    case 0x35c:
      // 35c,0,1,1,0,0,,,,ff,BCM Wake Up Sleep Command
      // 35c,4,4,1,0,0,,,,ff,Wake Up Type
      // 35c,5,7,1,0,0,,,,ff,Vehicle State
      // 35c,8,8,1,0,0,,,,ff,Diag Mux On BCM
      // 35c,9,10,1,0,0,,,,ff,Starting Mode BCM R
      // 35c,11,11,1,0,0,,,,ff,Engine Stop Driver Requested
      // 35c,12,12,1,0,0,,,,ff,Switch Off SES Disturbers
      // 35c,15,15,1,0,0,,,,ff,Delivery Mode Information
      // 35c,16,39,1,0,0,min,,,ff,Absolute Time Since 1rst Ignition
      // 35c,40,42,1,0,0,,,,ff,Brake Info Status
      // 35c,47,47,1,0,0,,,,ff,ProbableCustomer Feed Back Need
      // 35c,48,51,1,0,0,,,,ff,Emergency Engine Stop
      // 35c,52,52,1,0,0,,,,ff,Welcome Phase State
      // 35c,53,54,1,0,0,,,,ff,Supposed Customer Departure
      // 35c,55,55,1,0,0,,,,ff,VehicleOutside Locked State
      // 35c,58,59,1,0,0,,,,ff,Generic Applicative Diag Enable
      // 35c,60,61,1,0,0,,,,ff,Parking Brake Status
      break;
    case 0x391:
      // 391,15,15,1,0,0,,,,e2,Climate Cooling Select
      // 391,16,16,1,0,0,,,,e2,Blower State
      // 391,22,22,1,0,0,,,,e2,ACVbat Tempo Maintain
      // 391,28,28,1,0,0,,,,e2,Rear Defrost Request
      // 391,32,33,1,0,0,,,,e2,Clim Customer Action
      // 391,36,39,1,0,0,,,,e2,PTC Number Thermal Request
      break;
    case 0x3b7:
      // 3b7,17,25,1,0,0,day,,,ff,Time Before Draining
      // 3b7,32,38,1,0,0,,,,ff,Global Eco Score
      break;
    case 0x3f7:
      // 3f7,0,4,1,0,0,,,,e2,Range
      // 3f7,10,10,1,0,0,,,,e2,AT Open Door Warning
      // 3f7,11,11,1,0,0,,,,e2,AT Press Brake Pedal Request
      // 3f7,12,12,1,0,0,,,,e2,AT Gear Shift Refused
      break;
    case 0x427:
      // 427,0,1,1,0,0,,,,ff,HV Connection Status 0x07!
      // 427,2,3,1,0,0,,,,ff,Charging Alert
      // 427,4,5,1,0,0,,,,ff,HV Battery Locked
      // 427,26,28,1,0,0,,,,ff,Pre Heating Progress
      // 427,40,47,0.3,0,0,kW,,,e2,Available Charging Power
      // 427,49,57,0.1,0,1,kWh,,,e2,Available Energy
      mt_available_energy->SetValue((float) (((CAN_UINT(6))>>6) & 0x3ff) * 0.1);
      // 427,58,58,1,0,0,,,,e2,Charge Available
      break;
    case 0x42a:
      // 42a,0,0,1,0,0,,,,e2,PreHeating Request
      // 42a,6,15,0.1,40,1,°C,,,e2,Evaporator Temp Set Point
      // 42a,24,29,1,0,0,%,,,e2,Clim Air Flow
      // 42a,30,39,0.1,40,1,°C,,,ff,Evaporator Temp Measure
      // 42a,48,50,1,0,0,,,,e2,Clim Loop Mode
      // 42a,51,52,1,0,0,,,,e2,PTC Activation Request
      // 42a,56,60,5,0,0,%,,,e2,Engine Fan Speed Request PWM
      break;
    case 0x42e:
      // 42e,0,12,0.02,0,2,%,,,e3,State of Charge
      // 42e,18,19,1,0,0,,,,ff,HV Bat Level2 Failure
      // 42e,20,24,5,0,0,%,,,e2,Engine Fan Speed
      // 42e,25,34,0.5,0,0,V,,,ff,HV Network Voltage
      StandardMetrics.ms_v_bat_voltage->SetValue((float) (((CAN_UINT(3))>>5) & 0x3ff) * 0.5); // HV Voltage
      // 42e,38,43,1,0,1,A,,,e3,Charging Pilot Current
      StandardMetrics.ms_v_charge_climit->SetValue((((CAN_UINT(4))>>4) & 0x3Fu));
      StandardMetrics.ms_v_charge_current->SetValue((((CAN_UINT(4))>>4) & 0x3Fu)); // Todo change to charger current
      // 42e,44,50,1,40,0,°C,,,e3,HV Battery Temp
      StandardMetrics.ms_v_bat_temp->SetValue((((CAN_UINT(5))>>5) & 0x7fu) - 40);
      // 42e,56,63,0.3,0,1,kW,,,ff,Charging Power
      break;
    case 0x430:
      // 430,0,9,10,0,0,rpm,,,e2,Clim Compressor Speed RPM Request 
      // 430,16,22,1,0,0,%,,,e2,High Voltage PTC Request PWM
      // 430,24,33,0.5,30,1,°C,,,e2,Comp Temperature Discharge
      // 430,34,35,1,0,0,,,,e2,DeIcing Request
      // 430,36,37,1,0,0,,,,e2,Clim Panel PC Activation Request
      // 430,38,39,1,0,0,,,,e2,HV Battery Cooling State
      // 430,40,49,0.1,40,1,°C,,,e2,HV Battery Evaporator Temp
      // 430,50,59,0.1,40,1,°C,,,e2,HV Battery Evaporator Setpoint
      break;
    case 0x432:
      // 432,0,1,1,0,0,,,,e2,Bat VE Shut Down Alert
      // 432,2,3,1,0,0,,,,e2,Immediate Preheating Authorization Status
      // 432,4,5,1,0,0,,,,e2,HV Battery Level Alert
      // 432,6,9,1,0,0,,,,e2,HVBatCondPriorityLevel
      // 432,10,19,10,0,0,rpm,,,e2,Clim Comp RPM Status
      // 432,20,22,1,0,0,,,,e2,Clim Comp Default Status
      // 432,26,28,1,0,0,,,,e2,PTC Default Status
      // 432,29,35,1,40,0,°C,,,e2,HV Batt Cond Temp Average
      // 432,36,37,1,0,0,,,,e2,HV Bat Conditionning Mode
      // 432,40,41,1,0,0,,,,e2,Eco Mode Request
      // 432,42,48,100,0,0,Wh,,,e2,Climate Available Energy
      // 432,56,57,1,0,0,,,,e2,Low Voltage Unballast Request
      // 432,59,60,1,0,0,,,,e2,DeIcing Authorisation
      break;
    case 0x433:
      // 433,0,2,1,0,0,,,,e2,AQM Frag Select Request
      // 433,7,8,1,0,0,,,,e2,AQM Ioniser Mode Selection Req
      // 433,9,12,1,0,0,,,,e2,AQM Frag Intensity Request
      // 433,15,16,1,0,0,,,,e2,Clim AQS Activation Request
      // 433,28,29,1,0,0,,,,e2,Ioniser Auto Launch Request
      break;
    case 0x4f8:
      // 4f8,0,1,-1,-2,0,,,,ff,Start
      // 4f8,4,5,-1,-2,0,,,,ff,Parking Brake
      // 4f8,8,9,1,0,0,,,,ff,AIRBAG Malfunction Lamp State
      // 4f8,12,12,1,0,0,,,,ff,Cluster Driven Lamps Auto Check
      // 4f8,13,13,1,0,0,,,,ff,Displayed Speed Unit
      // 4f8,24,39,0.01,0,2,,,,ff,Speed on Display
      break;
    case 0x534:
      // 534,32,40,1,40,0,°C,,,5,Temp out
      //StandardMetrics.ms_v_env_temp->SetValue((float) CAN_BYTE(4)-40);
      break;
    case 0x5d7:
      // 5d7,0,15,0.01,0,2,km/h,,,ff,Speed
      StandardMetrics.ms_v_pos_speed->SetValue((float) CAN_UINT(0) / 100);
      // 5d7,16,43,0.01,0,2,km,,,ff,Odometer
      StandardMetrics.ms_v_pos_odometer->SetValue((float) (CAN_UINT32(2)>>4) / 100);
      // 5d7,50,54,0.04,0,2,cm,,,ff,Fine distance
      break;
    case 0x5da:
      // 5da,0,7,1,40,0,ºC,,,5,Water temperature
      break;
    case 0x5de:
      // 5de,1,1,1,0,0,,,,ff,Right Indicator
      // 5de,2,2,1,0,0,,,,ff,Left Indicator
      // 5de,3,3,1,0,0,,,,ff,Rear Fog Light
      // 5de,5,5,1,0,0,,,,ff,Park Light
      // 5de,6,6,1,0,0,,,,ff,Head Light
      // 5de,7,7,1,0,0,,,,ff,Beam Light
      // 5de,8,9,1,0,0,,,,ff,Position Lights Omission Warning
      // 5de,10,10,1,0,0,,,,ff,ALS malfunction
      // 5de,11,12,1,0,0,,,,ff,Door Front Left
      // 5de,13,14,1,0,0,,,,ff,Door Front Right
      // 5de,16,17,1,0,0,,,,ff,Door Rear Left
      // 5de,18,19,1,0,0,,,,ff,Door Rear Right
      // 5de,21,22,1,0,0,,,,ff,Steering Lock Failure
      // 5de,23,23,1,0,0,,,,ff,Unlocking Steering Column Warning
      // 5de,24,24,1,0,0,,,,ff,Automatic Lock Up Activation State
      // 5de,25,25,1,0,0,,,,ff,Badge Battery Low
      // 5de,28,29,1,0,0,,,,ff,Trip Display Scrolling Request
      // 5de,32,35,1,0,0,,,,ff,Smart Keyless Information Display
      // 5de,36,36,1,0,0,,,,ff,Keyless Info Reemission Request
      // 5de,37,37,1,0,0,,,,ff,Keyless Card Reader Failure Display
      // 5de,47,47,1,0,0,,,,ff,Brake Switch Fault Display
      // 5de,49,49,1,0,0,,,,ff,Stop Lamp Failure Display
      // 5de,56,57,1,0,0,,,,ff,Rear Wiper Status
      // 5de,58,59,1,0,0,,,,ff,Boot Open Warning
      StandardMetrics.ms_v_env_headlights->SetValue((CAN_BYTE(0) & 0x04) > 0);
      StandardMetrics.ms_v_door_fl->SetValue((CAN_BYTE(1) & 0x08) > 0);
      StandardMetrics.ms_v_door_fr->SetValue((CAN_BYTE(1) & 0x02) > 0);
      StandardMetrics.ms_v_door_rl->SetValue((CAN_BYTE(2) & 0x40) > 0);
      StandardMetrics.ms_v_door_rr->SetValue((CAN_BYTE(2) & 0x10) > 0);
      StandardMetrics.ms_v_door_trunk->SetValue((CAN_BYTE(7) & 0x10) > 0);
      
      break;
    case 0x5e9:
      // 5e9,0,0,1,0,0,,,,ff,UPAFailureDisplayRequest
      // 5e9,9,11,1,0,0,,,,ff,FrontParkAssistVolState
      // 5e9,12,12,1,0,0,,,,ff,RearParkAssistState
      // 5e9,28,31,1,0,0,,,,ff,RearLeftObstacleZone
      // 5e9,32,35,1,0,0,,,,ff,RearCenterObstacleZone
      // 5e9,36,39,1,0,0,,,,ff,RearRightObstacleZone
      // 5e9,40,42,1,0,0,,,,ff,UPAMode
      // 5e9,48,48,1,0,0,,,,ff,UPA_SoundRecurrenceType
      // 5e9,49,55,10,0,1,Hz,,,ff,UPA_SoundRecurrencePeriod
      // 5e9,56,58,1,0,0,,,,ff,UPA_SoundObstacleZone
      // 5e9,59,59,1,0,0,,,,ff,UPA_SoundActivationBeep
      // 5e9,60,60,1,0,0,,,,ff,UPA_SoundErrorBeep
      // 5e9,61,62,1,0,0,,,,ff,UPA_SoundUseContext
      break;
    case 0x5ee:
      // 5ee,0,0,1,0,0,,,,ff,Parking Light
      // 5ee,1,1,1,0,0,,,,ff,Head Light
      // 5ee,2,2,1,0,0,,,,ff,Beam Light
      // 5ee,8,10,1,0,0,,,,ff,Front Wiping Request
      // 5ee,11,15,100,0,0,W,,,ff,Electrical Power Drived
      // 5ee,16,16,1,0,0,,,,ff,Climate Cooling Request
      // 5ee,19,19,1,0,0,,,,ff,PTC Thermal Regulator Freeze
      // 5ee,21,23,1,0,0,,,,ff,User Identification
      // 5ee,24,24,1,0,0,,,,ff,Day Night Status For Backlights
      // 5ee,27,27,1,0,0,,,,ff,Driver Door State
      // 5ee,28,28,1,0,0,,,,ff,Passenger Door State
      // 5ee,29,31,1,0,0,,,,ff,Start Button Pushed
      // 5ee,32,39,0.4,0,0,%,,,ff,Night Rheostated Light Max Percent
      // 5ee,40,40,1,0,0,,,,ff,Light Sensor Status
      // 5ee,43,47,50,0,0,W,,,ff,Right Solar Level Info
      // 5ee,48,52,50,0,0,W,,,ff,Left Solar Level Info
      // 5ee,59,59,1,0,0,,,,ff,Day Running Light Request
      // 5ee,60,63,50,0,0,W/m2,,,ff,Visible Solar Level Info
      break;
    case 0x62c:
      // 62c,0,1,1,0,0,,,,ff,EPS Warning
      break;
    case 0x62d:
      // 62d,0,9,0.1,0,0,kWh/100km,,,ff,Worst Average Consumption
      // 62d,10,19,0.1,0,0,kWh/100km,,,ff,Best Average Consumption
      // 62d,20,28,100,0,0,W,,,ff,BCB Power Mains
      break;
    case 0x634:
      // 634,0,1,1,0,0,,,,ff,TCU Refuse to Sleep
      // 634,2,3,1,0,0,,,,ff,Ecall Function Failure Display
      // 634,4,7,1,0,0,,,,ff,ECALL State Display
      // 634,8,14,15,0,0,min,,,ff,Charging Timer Value Status
      // 634,15,15,1,0,0,,,,ff,Remote Pre AC Activation
      // 634,16,17,1,0,0,,,,ff,Charging Timer Status
      // 634,18,19,1,0,0,,,,ff,Charge Prohibited
      // 634,20,21,1,0,0,,,,ff,Charge Authorization
      // 634,22,23,1,0,0,,,,ff,External Charging Manager
      break;
    case 0x637:
      // 637,0,9,10,0,0,kWh,,,ff,Consumption Since Mission Start
      // 637,10,19,10,0,0,kWh,,,ff,Recovery Since Mission Start
      // 637,20,29,10,0,0,kWh,,,ff,Aux Consumption Since Mission Start
      // 637,32,38,1,0,0,%,,,ff,Speed Score Indicator Display
      // 637,40,51,1,0,0,kWh,,,ff,Total Recovery
      // 637,52,52,1,0,0,,,,ff,Open Charge Flap Warning Display
      break;
    case 0x638:
      // 638,0,7,1,80,0,kW,,,ff,Traction Instant Consumption
      // 638,8,17,1,0,0,km,,,ff,Vehicle Autonomy Min
      // 638,18,27,1,0,0,km,,,ff,Vehicle Autonomy Max
      // 638,32,36,1,0,0,kW,,,ff,AuxInstant Consumption
      // 638,37,39,1,0,0,,,,ff,Battery 14v To Be Changed Display
      break;
    case 0x646:
      // 646,1,3,1,0,0,,,,ff,Trip Unit Consumption
      // 646,4,5,1,0,0,,,,ff,Trip Unit Distance
      // 646,6,15,0.1,0,1,kWh/100km,,,ff,Average trip B consumpion
      // 646,16,32,0.1,0,1,km,,,ff,Trip B distance
      // 646,33,47,0.1,0,1,kWh,,,ff,trip B consumption
      // 646,48,59,0.1,0,1,km/h,,,ff,Average trip B speed
      break;
    case 0x650:
      // 650,1,2,1,0,0,,,,ff,Energy Flow For Energy Recovering Display
      // 650,6,7,1,0,0,,,,ff,Energy Flow For Traction Display
      // 650,8,9,1,0,0,,,,ff,Short Range Display
      // 650,10,11,1,0,0,,,,ff,Quick Drop Iteration Exceeded Display
      // 650,12,13,1,0,0,,,,ff,Quick Drop Lock Failure Display
      // 650,14,15,1,0,0,,,,ff,Quick Drop Unlocked Display
      // 650,16,22,1,0,0,%,,,ff,Advisor Econometer
      // 650,30,31,1,0,0,,,,ff,Cranking Plugged Display
      // 650,40,41,1,0,0,,,,ff,Clim Programmed PC Display
      // 650,42,44,1,0,0,,,,ff,Pre Heating State Display
      break;
    case 0x653:
      // 653,0,0,1,0,0,,,,ff,Crash Detected
      // 653,1,1,1,0,0,,,,ff,Crash DetectionOutOfOrder
      // 653,8,9,1,0,0,,,,ff,Driver Safety Belt Reminder
      // 653,10,11,1,0,0,,,,ff,Front Passenger Safety Belt Reminder
      // 653,12,12,1,0,0,,,,ff,Passenger AIRBAG Inhibition
      // 653,13,13,1,0,0,,,,ff,AIRBAG Malfunction
      // 653,14,15,1,0,0,,,,ff,Second Row Center Safety Belt State
      // 653,16,17,1,0,0,,,,ff,Second Row Left Safety Belt State
      // 653,18,19,1,0,0,,,,ff,Second Row Right Safety Belt State
      // 653,20,20,1,0,0,,,,ff,Valid AIRBAG Information
      break;
    case 0x654: {
      // 654,2,2,1,0,0,,,,ff,Charging Plug Connected
      StandardMetrics.ms_v_door_chargeport->SetValue((CAN_BYTE(0) & 0x20));
      // 654,3,3,1,0,0,,,,ff,Driver Walk Away Engine ON
      // 654,4,4,1,0,0,,,,ff,HVBatteryUnballastAlert
      // 654,25,31,1,0,0,,,,ff,State of Charge
      StandardMetrics.ms_v_bat_soc->SetValue(CAN_BYTE(3));
      // 654,32,41,1,0,0,min,,,ff,Time to Full
      //StandardMetrics.ms_v_charge_duration_full->SetValue((UINT(d[4] << 2) | d[5] >> 6) & 1023);
      StandardMetrics.ms_v_charge_duration_full->SetValue((((CAN_UINT(4) >> 6) & 0x3ffu) < 0x3ff) ? (CAN_UINT(4) >> 6) & 0x3ffu : 0);
      // 654,42,51,1,0,0,km,,,ff,Available Distance
      //StandardMetrics.ms_v_bat_range_est->SetValue((UINT(d[5] << 2) | d[6] >> 4) & 1023);
      float zoe_range_est = (((CAN_UINT(5) >> 4) & 0x3ffu) < 0x3ff) ? (CAN_UINT(5) >> 4) & 0x3ffu : 0;
      StandardMetrics.ms_v_bat_range_est->SetValue(zoe_range_est, Kilometers);
      // 654,52,61,0.1,0,1,kWh/100km,,,ff,Average Consumption
      // 654,62,62,1,0,0,,,,ff,HV Battery Low
      float soc = StandardMetrics.ms_v_bat_soc->AsFloat();
      if(soc > 0) {
        float zoe_range_ideal = (m_range_ideal * soc) / 100.0;
        StandardMetrics.ms_v_bat_range_ideal->SetValue(zoe_range_ideal, Kilometers); // ToDo
        StandardMetrics.ms_v_bat_range_full->SetValue((zoe_range_est / soc) * 100.0, Kilometers); // ToDo
      }
      break;
    }
    case 0x656:
      // 656,3,3,1,0,0,,,,ff,Trip Data Reset
      // 656,21,31,1,0,0,min,,,ff,Cluster Scheduled Time
      // 656,32,42,1,0,0,min,,,ff,Cluster Scheduled Time 2
      // 656,48,55,1,40,0,°C,,,e2,External Temp
      StandardMetrics.ms_v_env_temp->SetValue((float) CAN_BYTE(6)-40);
      // 656,56,57,1,0,0,,,,e2,Clim PC Customer Activation
      break;
    case 0x657:
      // 657,0,1,1,0,0,,,,ff,PreHeatingActivationRequest
      // 657,8,9,1,0,0,,,,ff,PreHeatingActivationRequestedByKey
      // 657,10,11,1,0,0,,,,ff,TechnicalWakeUpType
      // 657,12,13,1,0,0,,,,ff,UnlockChargingPlugRequestedByKey
      break;
    case 0x658:
      // 658,0,31,1,0,0,,,,ff,Battery Serial N°
      // 658,33,39,1,0,0,%,,,ff,Battery Health
      StandardMetrics.ms_v_bat_soh->SetValue(CAN_BYTE(4) & 0x7Fu);
      // 658,42,42,1,0,0,,,,ff,Charging
      isCharging = (CAN_BYTE(5) & 0x20); // ChargeInProgress
      
      if (isCharging != lastCharging) { // EVENT charge state changed
        if (isCharging) { // EVENT started charging
          POLLSTATE_CHARGING;
          // Handle 12Vcharging
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          // Reset charge kWh
          StandardMetrics.ms_v_charge_kwh->SetValue(0);
          // Reset trip values
          if (m_reset_trip) {
            StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
            StandardMetrics.ms_v_bat_energy_used->SetValue(0);
            mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
            StandardMetrics.ms_v_pos_trip->SetValue(0);
          }
          // Start charging
          StandardMetrics.ms_v_charge_pilot->SetValue(true);
          StandardMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_type->SetValue("type2");
          StandardMetrics.ms_v_charge_state->SetValue("charging");
          StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        } else { // EVENT stopped charging
          POLLSTATE_OFF;
          // Handle 12Vcharging
          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          StandardMetrics.ms_v_charge_pilot->SetValue(false);
          StandardMetrics.ms_v_charge_inprogress->SetValue(isCharging);
          StandardMetrics.ms_v_charge_mode->SetValue("standard");
          StandardMetrics.ms_v_charge_type->SetValue("type2");
          if (StandardMetrics.ms_v_bat_soc->AsInt() < 95) {
            // Assume the charge was interrupted
            ESP_LOGI(TAG,"Car charge session was interrupted");
            StandardMetrics.ms_v_charge_state->SetValue("stopped");
            StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
          } else {
            // Assume the charge completed normally
            ESP_LOGI(TAG,"Car charge session completed");
            StandardMetrics.ms_v_charge_state->SetValue("done");
            StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
          }
        }
      }
      lastCharging = isCharging;
      break;
    case 0x65b:
      // 65b,0,10,1,0,0,min,,,ff,Schedule timer 1 min
      // 65b,12,22,1,0,0,min,,,ff,Schedule timer 2 min
      // 65b,24,30,1,0,0,%,,,ff,Fluent driver
      // 65b,33,34,1,0,0,,,,ff,Economy Mode displayed
      // 65b,39,40,1,0,0,,,,ff,Consider eco mode
      // 65b,41,43,1,0,0,,,,ff,Charging Status Display
      // 65b,44,45,1,0,0,,,,ff,Set park for charging
      break;
    case 0x665:
      // 665,0,1,1,0,0,,,,ff,Auto Lock Up Activation Request
      // 665,13,15,1,0,0,,,,ff,Front Park Assist Volume Req
      // 665,16,17,1,0,0,,,,ff,Rear Park Assist Activation Req
      // 665,28,29,1,0,0,,,,ff,Auto Rear Wiper Activation Request
      // 665,40,41,1,0,0,,,,ff,Charging Timer Request
      // 665,42,48,15,0,0,min,,,ff,Charging Timer Value Request
      break;
    case 0x666:
      // 666,0,0,1,0,0,,,,ff,ESP In Regulation Display Request
      // 666,1,1,1,0,0,,,,ff,ESP In Default Display Request
      // 666,3,3,1,0,0,,,,ff,ASR Activation State For Display
      // 666,4,4,1,0,0,,,,ff,ABS In Default Display Request
      // 666,6,6,1,0,0,,,,ff,EBV In Default Display Request
      // 666,7,7,1,0,0,,,,ff,Emergency Braking Failure
      // 666,8,8,1,0,0,,,,ff,ABS-ESP Lamps Auto Check
      // 666,14,14,1,0,0,,,,ff,ABS or ESP In Calibrating Diag
      // 666,15,15,1,0,0,,,,ff,ABS or ESP To Be Calibrated
      // 666,23,23,1,0,0,,,,ff,HSA Failure Display Request
      // 668,0,1,1,0,0,,,,ff,Clim AQS Activation State
      // 668,4,7,1,0,0,,,,ff,AQM Frag Intensity State
      // 668,8,10,1,0,0,,,,ff,AQM Frag Select State
      // 668,11,13,1,0,0,,,,ff,AQM Ioniser Mode State
      // 668,14,15,1,0,0,,,,ff,Ioniser Auto Launch State
      // 66a,5,7,1,0,0,,,,ff,Cruise Control Mode
      // 66a,8,15,1,0,0,km/h,,,ff,Cruise Control Speed
      // 66a,16,16,1,0,0,,,,ff,Cruise Control OverSpeed
      // 66d,0,1,1,0,0,,,,ff,Braking System Defective Display
      // 66d,2,3,1,0,0,,,,ff,Braking System To Be Checked Display
      // 66d,4,5,1,0,0,,,,ff,UBP To Be Calibrated
      // 66d,6,7,1,0,0,,,,ff,UBP In Calibrating Diag
      // 66d,8,9,1,0,0,,,,ff,UBP Lamp Auto Check
      break;
    case 0x673:
      // 673,0,0,1,0,0,,,,ff,Speed pressure misadaptation
      // 673,2,4,1,0,0,,,,ff,Rear right wheel state
      // 673,5,7,1,0,0,,,,ff,Rear left wheel state
      // 673,8,10,1,0,0,,,,ff,Front right wheel state
      // 673,11,13,1,0,0,,,,ff,Front left wheel state
      // 673,16,23,13.725,0,0,mbar,,,ff,Rear right wheel pressure
      if (CAN_BYTE(2) != 0xff)
        StandardMetrics.ms_v_tpms_rr_p->SetValue((float) (CAN_BYTE(2)*13.725)/10);
      // 673,24,31,13.725,0,0,mbar,,,ff,Rear left wheel pressure
      if (CAN_BYTE(3) != 0xff)
        StandardMetrics.ms_v_tpms_rl_p->SetValue((float) (CAN_BYTE(3)*13.725)/10);
      // 673,32,39,13.725,0,0,mbar,,,ff,Front right wheel pressure
      if (CAN_BYTE(4) != 0xff)
        StandardMetrics.ms_v_tpms_fr_p->SetValue((float) (CAN_BYTE(4)*13.725)/10);
      // 673,40,47,13.725,0,0,mbar,,,ff,Front left wheel pressure
      if (CAN_BYTE(5) != 0xff)
        StandardMetrics.ms_v_tpms_fl_p->SetValue((float) (CAN_BYTE(5)*13.725)/10);
      break;
    case 0x68b:
      // 68b,0,3,1,0,0,,,,ff,MM action counter
      break;
    case 0x68c:
      // 68c,21,31,1,0,0,min,,,ff,Local Time
      break;
    case 0x699:
      // 699,0,1,1,0,0,,,,e2,Clima off Request display
      // 699,2,3,1,0,0,,,,e2,Clima read defrost Request display
      // 699,4,6,1,1,0,,,,e2,Cima mode
      // 699,8,15,0.5,0,0,°C,,,e2,Temperature
      // 699,16,19,1,0,0,,,,e2,Clima Flow
      // 699,20,21,1,0,0,,,,e2,Forced recycling
      // 699,22,23,1,0,0,,,,e2,
      // 699,24,27,1,0,0,,,,e2,
      // 699,28,31,1,0,0,,,,e2,Clim Last Func Modified By Customer
      // 699,32,33,1,0,0,,,,e2,Clim MMI Activation Request
      // 699,34,39,2,0,0,%,,,e2,Clim AQS Indicator
      // 699,40,45,1,0,0,min,,,e2,Clim AQM Ioniser Max Timer Display
      // 699,46,51,1,0,0,min,,,e2,Clim AQM Ioniser Timer Display
      // 699,52,53,1,0,0,,,,e2,Clim Auto Display
      // 699,54,55,1,0,0,,,,e2,Clim AC Off Display
      // 699,56,57,1,0,0,,,,e2,Clim Clearness Display
      // 699,58,59,1,0,0,,,,e2,Clim Display Menu PC
      // 699,60,61,1,0,0,,,,e2,Energy Flow For Thermal Comfort Display
      // 699,63,63,1,0,0,,,,e2,Clim Eco Low Soc Display
      break;
    case 0x69f:
      // 69f,0,31,1,0,0,,,,ff,Car Serial N°
      if (!zoe_vin[0]) // we only need to process this once
      {
        zoe_vin[0] = '0' + CAN_NIB(6);
        zoe_vin[1] = '0' + CAN_NIB(5);
        zoe_vin[2] = '0' + CAN_NIB(4);
        zoe_vin[3] = '0' + CAN_NIB(3);
        zoe_vin[4] = '0' + CAN_NIB(2);
        zoe_vin[5] = '0' + CAN_NIB(1);
        zoe_vin[6] = '0' + CAN_NIB(0);
        zoe_vin[7] = 0;
        StandardMetrics.ms_v_vin->SetValue((string) zoe_vin);
      }
      break;
    case 0x6f8:
      // 6f8,0,1,1,0,0,,,,ff,USM Refuse to Sleep
      // 6f8,4,4,1,0,0,,,,ff,Ignition Supply Confirmation
      // 6f8,5,5,1,0,0,,,,ff,Front Wiper Stop Position
      // 6f8,6,7,1,0,0,,,,ff,Front Wiper Status
      // 6f8,11,11,1,0,0,,,,ff,Ignition Control State
      car_on((CAN_BYTE(1) & 0x10) > 0);
      // 6f8,16,23,0.0625,0,2,V,,,ff,14V Battery Voltage
      StandardMetrics.ms_v_bat_12v_voltage->SetValue((float) CAN_BYTE(2) * 0.0625);
      break;
    case 0x6fb:
      // 6fb,8,9,1,0,0,,,,ff,Global Vehicle Warning State
      // 6fb,32,39,250,0,0,km,,,ff,Fixed Maintenance Range
      break;
    
    case 0x701: // for testing....
      
      StandardMetrics.ms_v_bat_current->SetValue( (float(CAN_UINT(0))-32768) * 0.25 );
      break;
  }
}

/**
 * Handles incoming poll results
 */
void OvmsVehicleRenaultZoe::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	ESP_LOGV(TAG, "%03x TYPE:%x PID:%02x Lenght:%x Data:%02x %02x %02x %02x %02x %02x %02x %02x", m_poll_moduleid_low, type, pid, length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
	switch (m_poll_moduleid_low) {
		// ****** EPS *****
		case 0x762:
			IncomingEPS(bus, type, pid, data, length, remain);
			break;
    // ****** EVC *****
		case 0x7ec:
			IncomingEVC(bus, type, pid, data, length, remain);
			break;
    // ****** BCB *****
    case 0x793:
      IncomingBCB(bus, type, pid, data, length, remain);
      break;
    // ****** LBC *****
    case 0x7bb:
      IncomingLBC(bus, type, pid, data, length, remain);
      break;
    // ****** UBP *****
    case 0x7bc:
      IncomingUBP(bus, type, pid, data, length, remain);
      break;
    // ****** PEB *****
    case 0x77e:
      IncomingPEB(bus, type, pid, data, length, remain);
      break;
    
	}
}

/**
 * Handle incoming polls from the EPS Computer
 */
void OvmsVehicleRenaultZoe::IncomingEPS(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
  switch (pid) {
    case 0x012D: {			// Motor temperature
      //762,24,31,2,0,0,°C,22012D,62012D,ff,DID - Motor temperature
      StandardMetrics.ms_v_mot_temp->SetValue(CAN_BYTE(0), Celcius);
      break;
    }
  }
}
/**
 * Handle incoming polls from the EVC Computer
 */
void OvmsVehicleRenaultZoe::IncomingEVC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	switch (pid) {
    case 0x2001: {			// Bat. rack Temperature
      //StandardMetrics.ms_v_bat_temp->SetValue((float) CAN_BYTE(0)-40, Celcius);
      break;
    }
    case 0x2002: {			// SoC
      //StandardMetrics.ms_v_bat_soc->SetValue((float) CAN_UINT(0)*2/100, Percentage);
      break;
    }
    case 0x2006: {			// Odometer (Total Vehicle Distance)
      //StandardMetrics.ms_v_pos_odometer->SetValue((float) CAN_UINT24(0), Kilometers);
      break;
    }
    case 0x200e: {			// Key On/Off (0 off / 1 on)
      if (CAN_BYTE(0) && 0x01 == 0x01) {
        car_on(true);
      } else {
        car_on(false);
      }
      break;
    }
    case 0x3203: {
      // 7ec,24,39,0.5,0,2,V,223203,623203,ff\n" // HV Battery voltage
      //rz_bat_voltage = float(CAN_UINT(0)) * 50/100;
      break;
    }
    case 0x3204: {
      // 7ec,24,39,0.25,32768,2,A,223204,623204,ff\n" // HV Battery current
      //rz_bat_current = (float(CAN_UINT(0))-32768) * 25/100;
      StandardMetrics.ms_v_bat_current->SetValue((float(CAN_UINT(0))-32768) * 0.25);
      break;
    }
    case 0x320C: {
      // 7ec,24,39,.005,0,0,kwh,22320C,62320C,ff,Available discharge Energy
      //m_b_bat_avail_energy->SetValue(float(CAN_UINT(0))*0.005);
      break;
    }
    case 0x33F6: {
      // ,7ec,24,31,1,40,0,°C,2233F6,6233F6,ff,Temperature of the inverter given by PEB (CAN ETS)
      ESP_LOGI(TAG, "7ec inv temp: %d", CAN_BYTE(0) - 40);
      break;
    }
    case 0x33dc: {
      // 7ec,24,47,0.001,1,0,kWh,2233dc,6233dc,ff\n" // Consumed domestic energy
      //StdMetrics.ms_v_charge_kwh->SetValue(CAN_UINT24(0)*0.001);
      break;
    }
	}
}

/**
 * Handle incoming polls from the BCB Computer
 */
void OvmsVehicleRenaultZoe::IncomingBCB(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	switch (pid) {
    case 0x504A: {
      // 793,24,39,1,20000,0,W,22504A,62504A,ff\n" // Mains active power consumed
      //m_mains_power->SetValue((float(CAN_UINT(0)-20000)/1000),kW);
      break;
    }        
    case 0x5063: {
      // 793,24,31,1,0,0,,225063,625063,ff\n"
      // Supervisor state,0:Init;1:Wait;2:ClosingS2;3:InitType;4:InitLkg;5:InitChg;6:Charge;7:ZeroAmpMode;8:EndOfChg;9:OpeningS2;10:ReadyToSleep;11:EmergencyStop;12:InitChargeDF;13:OCPStop;14:WaitS2
      /* rz_charge_state_local=CAN_BYTE(0);
      m_b_temp1->SetValue((INT)rz_charge_state_local);
      if (rz_charge_state_local==0) {          // Init,Wait,ClosingS2,InitType,InitLkg,InitChg
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==1) {  // Charge
          SET_CHARGE_STATE("stopped");
      } else if (rz_charge_state_local==2) {  // Charge
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==3) {  // Charge
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==4) {  // Charge
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==5) {  // Charge
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==6) {  // Charge
          SET_CHARGE_STATE("charging");
      } else if (rz_charge_state_local==7) {  // ZeroAmpMode
          SET_CHARGE_STATE("topoff");
      } else if (rz_charge_state_local==8) {  // EndOfChg
          SET_CHARGE_STATE("done");
      } else if (rz_charge_state_local==9) {  // OpeningS2
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==10) { // ReadyToSleep
          SET_CHARGE_STATE("stopped");
      } else if (rz_charge_state_local==11) { //EmergencyStopp
          SET_CHARGE_STATE("stopped");
      } else if (rz_charge_state_local==12) { //InitChargeDF
          SET_CHARGE_STATE("prepare");
      } else if (rz_charge_state_local==13) { //OCPStop
          SET_CHARGE_STATE("stopped");
      } else if (rz_charge_state_local==14) { //WaitS2
          SET_CHARGE_STATE("prepare");
      } */
      break;
    }
	}
}

/**
 * Handle incoming polls from the LBC Computer
 */
void OvmsVehicleRenaultZoe::IncomingLBC(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	switch (pid) {
    case 0x01: {
      // Todo
      // 7bb,336,351,0.01,0,2,kW,2101,6101,e2\n" // Maximum battery input power
      break;
    }
    case 0x03: {
      // Todo
      // 7bb,56,71,10,0,0,°C,2103,6103,5\n" // Mean battery compartment temp
      // 7bb,96,111,.01,0,0,V,2103,6103,ff,21_03_#13_Maximum_Cell_voltage\n" //
      // 7bb,112,127,.01,0,0,V,2103,6103,ff,21_03_#15_Minimum_Cell_voltage\n" //
      // 7bb,192,207,0.01,0,2,%,2103,6103,e2\n" // Real State of Charge
      // if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
      // {
      // if (m_poll_ml_frame == 1)
      // {
      // m_b_cell_volt_max->SetValue(float( CAN_BYTE(6)*0.01 ),Volts);
      // }
      // else if (m_poll_ml_frame == 2)
      // {
      // m_b_cell_volt_min->SetValue(float( CAN_BYTE(1)*0.01 ),Volts);
      // }
      // }
      break;
    }
    case 0x04: {
      // Todo
      // 7bb,32,39,1,40,0,°C,2104,6104,e2\n" // Cell 1 Temperature
      // 7bb,56,63,1,40,0,°C,2104,6104,e2\n" // Cell 2 Temperature
      // 7bb,80,87,1,40,0,°C,2104,6104,e2\n" // Cell 3 Temperature
      // 7bb,104,111,1,40,0,°C,2104,6104,e2\n" // Cell 4 Temperature
      // 7bb,128,135,1,40,0,°C,2104,6104,e2\n" // Cell 5 Temperature
      // 7bb,152,159,1,40,0,°C,2104,6104,e2\n" // Cell 6 Temperature
      // 7bb,176,183,1,40,0,°C,2104,6104,e2\n" // Cell 7 Temperature
      // 7bb,200,207,1,40,0,°C,2104,6104,e2\n" // Cell 8 Temperature
      // 7bb,224,231,1,40,0,°C,2104,6104,e2\n" // Cell 9 Temperature
      // 7bb,248,255,1,40,0,°C,2104,6104,e2\n" // Cell 10 Temperature
      // 7bb,272,279,1,40,0,°C,2104,6104,e2\n" // Cell 11 Temperature
      // 7bb,296,303,1,40,0,°C,2104,6104,e2\n" // Cell 12 Temperature
      /* if (type == VEHICLE_POLL_TYPE_OBDIIGROUP)
      {
          if (m_poll_ml_frame == 0)
          {
              rz_battery_module_temp[0]=( (INT)CAN_BYTE(1)-40 );
          }
          else if (m_poll_ml_frame == 1)
          {
              rz_battery_module_temp[1]=( (INT)CAN_BYTE(1)-40 );
              rz_battery_module_temp[2]=( (INT)CAN_BYTE(4)-40 );
          }
          else if (m_poll_ml_frame == 2)
          {
              rz_battery_module_temp[3]=( (INT)CAN_BYTE(0)-40 );
              rz_battery_module_temp[4]=( (INT)CAN_BYTE(3)-40 );
              rz_battery_module_temp[5]=( (INT)CAN_BYTE(6)-40 );
          }
          else if (m_poll_ml_frame == 3)
          {
              rz_battery_module_temp[6]=( (INT)CAN_BYTE(2)-40 );
              rz_battery_module_temp[7]=( (INT)CAN_BYTE(5)-40 );
          }
          else if (m_poll_ml_frame == 4)
          {
              rz_battery_module_temp[8]=( (INT)CAN_BYTE(1)-40 );
              rz_battery_module_temp[9]=( (INT)CAN_BYTE(4)-40 );
          }
          else if (m_poll_ml_frame == 5)
          {
              rz_battery_module_temp[10]=( (INT)CAN_BYTE(0)-40 );
              rz_battery_module_temp[11]=( (INT)CAN_BYTE(3)-40 );
          }
          int sum=0;
          for (int i=0; i<12; i++) {
              sum+=rz_battery_module_temp[i];
          }
          StdMetrics.ms_v_bat_temp->SetValue(float(sum)/12); // Calculate Mean Value
          
      }*/
      break;
    }
	}
}

/**
 * Handle incoming polls from the UBP Computer
 */
void OvmsVehicleRenaultZoe::IncomingUBP(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	switch (pid) {
    case 0x4B7C: {
      // Todo
      // //+"7bc,28,39,1,4094,0,N·m,224B7C,624B7C,ff,Electric brake wheels torque request\n"  // Brake Torque
      break;
    }
    case 0x4B7D: {
      //"7bc,28,39,1,4094,0,N·m,224B7D,624B7D,ff,Total Hydraulic brake wheels torque request\n"
      //m_v_hydraulic_brake_power->SetValue(float(CAN_12NIBL(28) -4094)*StdMetrics.ms_v_pos_speed->AsFloat()/3.6/1.28*2*3.141 );
      break;
    }
	}
}

/**
 * Handle incoming polls from the PEB Computer
 */
void OvmsVehicleRenaultZoe::IncomingPEB(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain) {
	switch (pid) {
    case 0x3018: {
      // 77e,24,39,0.015625,0,2,ºC,223018,623018,ff\n" // DCDC converter temperature
      // zoe_dcdc_temp=CAN_UINT(4);
      break;
    }
    case 0x302b: {
      // 77e,24,31,0.015625,0,2,°C,22302b,62302b,ff\n" // inverter temperature
      // 77e,24,39,1,0,0,°C,22302B,62302B,ff,InverterTempOrder
      ESP_LOGI(TAG, "77e inv temp: %f", CAN_BYTE(0) * 0.015625);
      ESP_LOGI(TAG, "77e inv temp: %d", CAN_UINT(0));
      break;
    }
	}
}


/**
 * Takes care of setting all the state appropriate when the car is on
 * or off. Centralized so we can more easily make on and off mirror
 * images.
 */
void OvmsVehicleRenaultZoe::car_on(bool isOn) {
  if (isOn && !StandardMetrics.ms_v_env_on->AsBool()) {
		// Car is beeing turned ON
    ESP_LOGI(TAG,"CAR IS ON");
		StandardMetrics.ms_v_env_on->SetValue(isOn);
		StandardMetrics.ms_v_env_awake->SetValue(isOn);
    // Handle 12Vcharging
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    if (m_enable_write) POLLSTATE_RUNNING;
    // Reset trip values
    if (!m_reset_trip) {
      StandardMetrics.ms_v_bat_energy_recd->SetValue(0);
      StandardMetrics.ms_v_bat_energy_used->SetValue(0);
      mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
      StandardMetrics.ms_v_pos_trip->SetValue(0);
    }
  }
  else if(!isOn && StandardMetrics.ms_v_env_on->AsBool()) {
    // Car is being turned OFF
    ESP_LOGI(TAG,"CAR IS OFF");
    if (!StandardMetrics.ms_v_charge_inprogress->AsBool()) {
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
      if (m_enable_write) POLLSTATE_ON;
    } else {
      if (m_enable_write) POLLSTATE_CHARGING;
    }
		StandardMetrics.ms_v_env_on->SetValue( isOn );
		StandardMetrics.ms_v_env_awake->SetValue( isOn );
		StandardMetrics.ms_v_pos_speed->SetValue( 0 );
		if (StandardMetrics.ms_v_pos_trip->AsFloat(0) > 0.1)
			NotifyTrip();
  }
}

void OvmsVehicleRenaultZoe::Ticker1(uint32_t ticker) {
  if (m_candata_timer > 0) {
    if (--m_candata_timer == 0) {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      mt_bus_awake->SetValue(false);
      //StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_bat_12v_current->SetValue(0);
      m_candata_poll = 0;
      POLLSTATE_OFF;
    }
  }
  
  HandleEnergy();
  
  // Handle Tripcounter
  if (mt_pos_odometer_start->AsFloat(0) == 0 && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0) {
    mt_pos_odometer_start->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat());
  }
  if (StandardMetrics.ms_v_env_on->AsBool() && StandardMetrics.ms_v_pos_odometer->AsFloat(0) > 0.0 && mt_pos_odometer_start->AsFloat(0) > 0.0) {
    StandardMetrics.ms_v_pos_trip->SetValue(StandardMetrics.ms_v_pos_odometer->AsFloat(0) - mt_pos_odometer_start->AsFloat(0));
  }
  
  // Handle v2Server connection
  if (StandardMetrics.ms_s_v2_connected->AsBool()) {
    m_reboot_ticker = 5 * 60; // set reboot ticker
  }
  else if (m_reboot_ticker > 0 && --m_reboot_ticker == 0) {
    MyNetManager.RestartNetwork();
    m_reboot_ticker = 5 * 60;
    //MyBoot.Restart(); // restart Module
  }
}

void OvmsVehicleRenaultZoe::Ticker10(uint32_t ticker) {
  HandleCharging();
}

/**
 * Update derived energy metrics while driving
 * Called once per second
 */
void OvmsVehicleRenaultZoe::HandleEnergy() {
  float voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  float current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);

  // Power (in kw) resulting from voltage and current
  float power = voltage * current / 1000.0;

  // Are we driving?
  if (power != 0.0 && StandardMetrics.ms_v_env_on->AsBool()) {
    // Update energy used and recovered
    float energy = power / 3600.0;    // 1 second worth of energy in kwh's
    if (energy < 0.0f)
      StandardMetrics.ms_v_bat_energy_used->SetValue( StandardMetrics.ms_v_bat_energy_used->AsFloat() - energy);
    else // (energy > 0.0f)
      StandardMetrics.ms_v_bat_energy_recd->SetValue( StandardMetrics.ms_v_bat_energy_recd->AsFloat() + energy);
  }
}

/**
 * Update derived metrics when charging
 * Called once per 10 seconds from Ticker10
 */
void OvmsVehicleRenaultZoe::HandleCharging() {
  float limit_soc       = StandardMetrics.ms_v_charge_limit_soc->AsFloat(0);
  float limit_range     = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
  float max_range       = StandardMetrics.ms_v_bat_range_full->AsFloat(0, Kilometers);
  float charge_current  = StandardMetrics.ms_v_bat_current->AsFloat(0, Amps);
  float charge_voltage  = StandardMetrics.ms_v_bat_voltage->AsFloat(0, Volts);
  
  // Are we charging?
  if (!StandardMetrics.ms_v_charge_pilot->AsBool()      ||
      !StandardMetrics.ms_v_charge_inprogress->AsBool() ||
      (charge_current <= 0.0) ) {
    return;
  }
  
  // Check if we have what is needed to calculate energy and remaining minutes
  if (charge_voltage > 0 && charge_current > 0) {
    // Update energy taken
    // Value is reset to 0 when a new charging session starts...
    float power  = charge_voltage * charge_current / 1000.0;     // power in kw
    float energy = power / 3600.0 * 10.0;                        // 10 second worth of energy in kwh's
    StandardMetrics.ms_v_charge_kwh->SetValue( StandardMetrics.ms_v_charge_kwh->AsFloat() + energy);

    if (limit_soc > 0) {
      // if limit_soc is set, then calculate remaining time to limit_soc
      int minsremaining_soc = calcMinutesRemaining(limit_soc, charge_voltage, charge_current);

      StandardMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
    }
    if (limit_range > 0 && max_range > 0.0) {
      // if range limit is set, then compute required soc and then calculate remaining time to that soc
      float range_soc           = limit_range / max_range * 100.0;
      int   minsremaining_range = calcMinutesRemaining(range_soc, charge_voltage, charge_current);

      StandardMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
    }
  }
}

/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
int OvmsVehicleRenaultZoe::calcMinutesRemaining(float target_soc, float charge_voltage, float charge_current) {
  float bat_soc = StandardMetrics.ms_v_bat_soc->AsFloat(100);
  if (bat_soc > target_soc) {
    return 0;   // Done!
  }
  float remaining_wh    = m_battery_capacity * (target_soc - bat_soc) / 100.0;
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0;

  return MIN( 1440, (int)remaining_mins );
}

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleRenaultZoe::ConfigChanged(OvmsConfigParam* param) {
  if (param && param->GetName() != "xrz")
    return;

  ESP_LOGI(TAG, "Renault Zoe reload configuration");
  
  m_enable_write      = MyConfig.GetParamValueBool("xrz", "canwrite", false);
  m_range_ideal       = MyConfig.GetParamValueInt("xrz", "rangeideal", 160);
  m_battery_capacity  = MyConfig.GetParamValueInt("xrz", "battcapacity", 27000);
  m_enable_egpio      = MyConfig.GetParamValueBool("xrz", "enable_egpio", false);
  
  m_reset_trip        = MyConfig.GetParamValueBool("xrz", "reset.trip.charge", false);
  
  StandardMetrics.ms_v_charge_limit_soc->SetValue((float) MyConfig.GetParamValueInt("xrz", "suffsoc", 0), Percentage );
  StandardMetrics.ms_v_charge_limit_range->SetValue((float) MyConfig.GetParamValueInt("xrz", "suffrange", 0), Kilometers );
}

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleRenaultZoe::SetFeature(int key, const char *value) {
  switch (key) {
    case 10:
      MyConfig.SetParamValue("xrz", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xrz", "suffrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xrz", "canwrite",  (bits& 1)!=0);
      return true;
    }
    default:
      return OvmsVehicle::SetFeature(key, value);
  }
}

/**
 * GetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
const std::string OvmsVehicleRenaultZoe::GetFeature(int key) {
  switch (key) {
    case 10:
      return MyConfig.GetParamValue("xrz", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xrz", "suffrange", STR(0));
    case 15:
    {
      int bits = ( MyConfig.GetParamValueBool("xrz", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
  }
}


//-----------------------------------------------------------------------------
//
// RenaultZoeInit
//

class OvmsVehicleRenaultZoeInit {
  public: OvmsVehicleRenaultZoeInit();
} MyOvmsVehicleRenaultZoeInit  __attribute__ ((init_priority (9000)));

OvmsVehicleRenaultZoeInit::OvmsVehicleRenaultZoeInit() {
  ESP_LOGI(TAG, "Registering Vehicle: Renault Zoe (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleRenaultZoe>("RZ","Renault Zoe");
}
