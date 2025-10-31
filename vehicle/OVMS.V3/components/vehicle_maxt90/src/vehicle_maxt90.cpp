/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
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
static const char *TAG = "v-maxt90";

#include <stdio.h>
#include "vehicle_maxt90.h"
#include "vehicle_obdii.h"

//
// ────────────────────────────────────────────────────────────────
//   Class Definition
// ────────────────────────────────────────────────────────────────
//

OvmsVehicleMaxt90::OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Initialising Maxus T90 EV vehicle module (derived from OBDII)");

  // Register CAN1 bus at 500 kbps
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);

  //
  // Define our T90 poll list (VIN, SOC, SOH)
  //
  static const OvmsPoller::poll_pid_t maxt90_polls[] = {
    // VIN – Service 0x22, PID 0xF190
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xF190,
      { 0, 999, 999 }, 0, ISOTP_STD },

    // SOC – Service 0x22, PID 0xE002
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE002,
      { 10, 10, 10 }, 0, ISOTP_STD },

    // SOH – Service 0x22, PID 0xE003
    { 0x7e3, 0x7eb, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0xE003,
      { 30, 30, 30 }, 0, ISOTP_STD },

    POLL_LIST_END
  };

  // Attach the poll list to CAN1
  PollSetPidList(m_can1, maxt90_polls);
  PollSetState(0);

  ESP_LOGI(TAG, "Maxus T90 EV poller configured on CAN1 @ 500 kbps");
}

OvmsVehicleMaxt90::~OvmsVehicleMaxt90()
{
  ESP_LOGI(TAG, "Shutdown Maxus T90 EV vehicle module");
}

//
// ────────────────────────────────────────────────────────────────
//   Module Registration
// ────────────────────────────────────────────────────────────────
//

class OvmsVehicleMaxt90Init
{
public:
  OvmsVehicleMaxt90Init();
} MyOvmsVehicleMaxt90Init __attribute__((init_priority(9000)));

OvmsVehicleMaxt90Init::OvmsVehicleMaxt90Init()
{
  ESP_LOGI(TAG, "Registering Vehicle: Maxus T90 EV (9000)");
  MyVehicleFactory.RegisterVehicle<OvmsVehicleMaxt90>("MAXT90", "Maxus T90 EV");
}
