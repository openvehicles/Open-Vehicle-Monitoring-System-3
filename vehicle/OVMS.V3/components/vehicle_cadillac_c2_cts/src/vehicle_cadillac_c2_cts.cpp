/*
;    Project:       Open Vehicle Monitor System
;    Date:          9th May 2020
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019 Michael Stegen / Stegen Electronics
;    (C) 2019 Mark Webb-Johnson
;    (C) 2020 Craig Leres
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
static const char *TAG = "v-cadillacc2cts";

#define F2C(f) (((f) - 32) / 1.8)

#include <stdio.h>
#include "vehicle_cadillac_c2_cts.h"

OvmsVehicleCadillaccC2CTS* MyCadillaccC2CTS = NULL;

static const OvmsVehicle::poll_pid_t obdii_polls[] =
  {
    // Engine coolant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x05, {  0, 30, 30 } },
    // Engine RPM
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0c, { 10, 10, 10 } },
    // Speed
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0d, {  0, 10, 10 } },
    // Engine air intake temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x0f, {  0, 30, 30 } },
    // Fuel level
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x2f, {  0, 30, 30 } },
    // Ambiant temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x46, {  0, 30, 30 } },
    // Engine oil temp
    { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIICURRENT, 0x5c, {  0, 30, 30 } },
#ifdef notdef
    // VIN
    // { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE, 0x02, {999,999,999 } },
#endif
    { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  };

static volatile bool processing;

OvmsVehicleCadillaccC2CTS::OvmsVehicleCadillaccC2CTS()
  {
  ESP_LOGI(TAG, "Cadillac 2nd gen CTS vehicle module");
  memset(m_vin, 0, sizeof(m_vin));
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  RegisterCanBus(2, CAN_MODE_ACTIVE, CAN_SPEED_33KBPS);
  PollSetPidList(m_can1, obdii_polls);
  PollSetState(0);
  MyCadillaccC2CTS = this;
  }

#ifdef notdef
      StandardMetrics.ms_v_env_headlights->SetValue(?);
#endif

OvmsVehicleCadillaccC2CTS::~OvmsVehicleCadillaccC2CTS()
  {
  ESP_LOGI(TAG, "Shutdown Cadillac 2nd gen CTS vehicle module");
  MyCadillaccC2CTS = NULL;
  }

void OvmsVehicleCadillaccC2CTS::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  int i, len;
  uint8_t *d;

  processing = 1;
  d = p_frame->data.u8;
  len = p_frame->FIR.B.DLC;
  if (len > sizeof(p_frame->data))
	  len = sizeof(p_frame->data);

  switch (p_frame->MsgID)
    {
#ifdef notdef
    case 0x1004C040:
      /* Maybe soc (fuel level)? */
      // 2R29 1004C040 00 68 05 7b 00 bb 00 00
      //                0  1  2  3  4  5  6  7
      break;
#endif

#ifdef notdef
    case 0x1004C040:
      /* Engine RPM and run state */
      bool isRunning = (dp5] != 0);
      if (StandardMetrics.ms_v_env_on->AsBool() != isRunning) {
        StdMetrics.ms_v_env_on->SetValue(isRunning);

ESP_LOGI(TAG, "running: \"%s\"", isRunning ? "yes" : "no");
      }
      StandardMetrics.ms_v_mot_rpm->SetValue((d[2] << 8) + d[3]);
      break;
#endif

#ifdef notdef
    case 0x12A:
      /* XXX 0F9 might be a better "awake" pid */
      /* ??? is d[5] pressure? */

      /* XXX we can probably find a better pid for this */
      if (!StandardMetrics.ms_v_env_awake->AsBool() ) {
        StandardMetrics.ms_v_env_awake->SetValue(true);

ESP_LOGI(TAG, "now awake");
      }
      break;
#endif

    case 0x10024040:
      /* First 8 of vin */
      if (m_vin[0] == '\0') {
	i = len;
	if (i > 8)
	  i = 8;
	/* The World Manufacturer number: United States */
        m_vin[0] = '1';
        memcpy(m_vin + 1, d, i);
        if (m_vin[1 + 8] != '\0')
          StandardMetrics.ms_v_vin->SetValue(m_vin);
      }
      break;

    case 0x10026040:
      /* Last 8 of vin */
      if (m_vin[1 + 8] == '\0') {
	i = len;
	if (i > 8)
	  i = 8;
        memcpy(m_vin + 1 + 8, d, i);
        if (m_vin[0] != '\0')
          StandardMetrics.ms_v_vin->SetValue(m_vin);
      }
      break;

#ifdef notdef
    case 0x0C9:
      /* Throttle percentage */
      StandardMetrics.ms_v_env_throttle->SetValue(d[1]);
      break;
#endif

#ifdef notdef
    case 0x0f1:
      /* ??? d[1]  brake 0-100% (0-254) */
      StandardMetrics.ms_v_env_footbrake->SetValue(d[2]);
#ifdef notdef
      StandardMetrics.ms_v_env_handbrake->SetValue(?);
#endif
      break;
#endif

#ifdef notdef
    case 0x120:
      /* ??? Might be locks (vehicle_kiasoulev) */
      break;
#endif

#ifdef notdef
    case 0x1f5:
      // ??? d[3] is transmission gear: ?? might only know R, N and other
      // 1=park, 2=reverse, F=neutral, ?=drive, ?=low ??
      // 0x20 is probably reverse
      // Gear/direction; negative=reverse, 0=neutral [1]
      StandardMetrics.ms_v_env_gear->SetValue(?);
      break;
#endif

#ifdef notdef
    case 0x771:
      /* ??? Might be open/lock doors (vehicle_kiasoulev) */
      StandardMetrics.ms_v_door_fl->SetValue(?);
      StandardMetrics.ms_v_door_fr->SetValue(?);
      StandardMetrics.ms_v_door_rl->SetValue(?);
      StandardMetrics.ms_v_door_rr->SetValue(?);

      StandardMetrics.ms_v_door_hood->SetValue(?);
      StandardMetrics.ms_v_door_trunk->SetValue(?);
      break;
#endif

    case 0x4c1:
      /* ??? Ambient temperature (probably wrong) */
      /* ??? probably pressure (psi) of some kind */
#ifdef notdef
      StandardMetrics.ms_v_env_temp->SetValue(F2C(d[4] - 0x28));
      // Cabin temperature
      StandardMetrics.ms_v_env_cabintemp->SetValue(F2C(?));
#endif
      break;

#ifdef notdef
    case 0x???:
      /* ??? Speed */
      StandardMetrics.ms_v_pos_speed->SetValue(?);
#endif

#ifdef notdef
    case 0x???:
      /* ??? Odometer */
      StandardMetrics.ms_v_pos_odometer->SetValue(?);
#endif

#ifdef notdef
    case 0x???:
      /* ??? TMPS */
      StandardMetrics.ms_v_tpms_fl_t->SetValue(?);
      StandardMetrics.ms_v_tpms_fr_t->SetValue(?);
      StandardMetrics.ms_v_tpms_rr_t->SetValue(?);
      StandardMetrics.ms_v_tpms_rl_t->SetValue(?);

      StandardMetrics.ms_v_tpms_fl_p->SetValue(?);
      StandardMetrics.ms_v_tpms_fr_p->SetValue(?);
      StandardMetrics.ms_v_tpms_rr_p->SetValue(?);
      StandardMetrics.ms_v_tpms_rl_p->SetValue(?);
      break;
#endif

    default:
      break;
    }
  processing = 0;
  }

void
OvmsVehicleCadillaccC2CTS::IncomingPollReply(canbus* bus, uint16_t type,
  uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  int value1 = (int)data[0];
  int value2 = ((int)data[0] << 8) + (int)data[1];

  switch (pid)
    {
#ifdef notdef
    case 0x02:  // VIN (multi-line response)
      strncat(m_vin,(char*)data,length);
      if (mlremain==0)
        {
        StandardMetrics.ms_v_vin->SetValue(m_vin);
        m_vin[0] = 0;
        }
      break;
#endif

    case 0x05:  // Engine coolant temperature
      StandardMetrics.ms_v_bat_temp->SetValue(value1 - 0x28);
      break;

    case 0x0f:  // Engine intake air temperature
      StandardMetrics.ms_v_inv_temp->SetValue(value1 - 0x28);
      break;

    case 0x5c:  // Engine oil temperature
      StandardMetrics.ms_v_mot_temp->SetValue(value1 - 0x28);
      break;

    case 0x46:  // Ambient temperature
      StandardMetrics.ms_v_env_temp->SetValue(value1 - 0x28);
      break;

    case 0x0d:  // Speed
      StandardMetrics.ms_v_pos_speed->SetValue(value1);
      break;

    case 0x2f:  // Fuel Level
      StandardMetrics.ms_v_bat_soc->SetValue((value1 * 100) >> 8);
      break;

    case 0x0c:  // Engine RPM
      if (value2 == 0)
        { // Car engine is OFF
        PollSetState(0);
        // StandardMetrics.ms_v_env_handbrake->SetValue(true);
        // StandardMetrics.ms_v_env_on->SetValue(false);
        // StandardMetrics.ms_v_pos_speed->SetValue(0);
        StandardMetrics.ms_v_env_charging12v->SetValue(false);
        }
      else
        { // Car engine is ON
        PollSetState(1);
        // StandardMetrics.ms_v_env_handbrake->SetValue(false);
        // StandardMetrics.ms_v_env_on->SetValue(true);
        StandardMetrics.ms_v_env_charging12v->SetValue(true);
        }
      break;
    }
  }

class OvmsVehicleCadillaccC2CTSInit
  {
  public: OvmsVehicleCadillaccC2CTSInit();
} MyOvmsVehicleCadillaccC2CTSInit  __attribute__ ((init_priority (9000)));

OvmsVehicleCadillaccC2CTSInit::OvmsVehicleCadillaccC2CTSInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Cadillac 2nd gen CTS (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleCadillaccC2CTS>("C2CTS",
    "Cadillac 2nd gen CTS");
  }

// XXX from vehicle_nissanleaf.cpp
/**
 * Ticker10: Called every ten seconds
 */
void OvmsVehicleCadillaccC2CTS::Ticker10(uint32_t ticker)
  {
  // FIXME
  // detecting that on is stale and therefor should turn off probably shouldn't
  // be done like this
  // perhaps there should be a car on-off state tracker and event generator in
  // the core framework?
  // perhaps interested code should be able to subscribe to "onChange" and
  // "onStale" events for each metric?

  /* Skip if we're processing a can frame */
  if (!processing && StandardMetrics.ms_v_env_awake->AsBool() &&
      StandardMetrics.ms_v_env_awake->IsStale())
    {
      StandardMetrics.ms_v_env_awake->SetValue(false);
ESP_LOGI(TAG, "now asleep");
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleCadillaccC2CTS::CommandWakeup()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));
  
  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0a;
  m_can2->Write(&frame);

#ifdef notdef
// XXX roadster version
  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x06;
  frame.data.u8[1] = 0x2c;
  frame.data.u8[2] = 0x01;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0x00;
  frame.data.u8[5] = 0x09;
  frame.data.u8[6] = 0x10;
  frame.data.u8[7] = 0x00;
  m_can2->Write(&frame);
#endif

  return Success;
  }

OvmsVehicle::vehicle_command_t
OvmsVehicleCadillaccC2CTS::CommandLock(const char* pin)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t
OvmsVehicleCadillaccC2CTS::CommandUnlock(const char* pin)
  {
  return NotImplemented;
  }
