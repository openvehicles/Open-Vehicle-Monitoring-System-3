/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th September 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2017       Tom Parker
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
static const char *TAG = "v-nissanleaf";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_nissanleaf.h"
#include "ovms_events.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

OvmsVehicleNissanLeaf::OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Nissan Leaf v3.0 vehicle module");

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsVehicleNissanLeaf::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "ticker.60", std::bind(&OvmsVehicleNissanLeaf::Ticker60, this, _1, _2));
  }

OvmsVehicleNissanLeaf::~OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Shutdown Nissan Leaf vehicle module");
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_car_on()
// Takes care of setting all the state appropriate when the car is on
// or off. Centralized so we can more easily make on and off mirror
// images.
//

void vehicle_nissanleaf_car_on(bool isOn)
  {
  StandardMetrics.ms_v_env_on->SetValue(isOn);
  StandardMetrics.ms_v_env_awake->SetValue(isOn);
  // We don't know if handbrake is on or off, but it's usually off when the car is on.
  StandardMetrics.ms_v_env_handbrake->SetValue(!isOn);
// TODO this is supposed to be a one-shot but car_parktime doesn't work in v3 yet
// further it's not clear if we even need to do this
//  if (isOn && car_parktime != 0)
//    {
//    StandardMetrics.ms_v_door_chargeport->SetValue(false);
//    StandardMetrics.ms_v_charge_pilot->SetValue(false);
//    StandardMetrics.ms_v_charge_inprogress->SetValue(false);
//    StandardMetrics.ms_v_charge_state->SetValue("stopped");
//    StandardMetrics.ms_v_charge_substate->SetValue("stopped");
//    }
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_charger_status()
// Takes care of setting all the charger state bit when the charger
// switches on or off. Separate from vehicle_nissanleaf_poll1() to make
// it clearer what is going on.
//

void vehicle_nissanleaf_charger_status(ChargerStatus status)
  {
  switch (status)
    {
    case CHARGER_STATUS_IDLE:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      break;
    case CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT:
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("stopped");
      StandardMetrics.ms_v_charge_substate->SetValue("stopped");
      break;
    case CHARGER_STATUS_QUICK_CHARGING:
    case CHARGER_STATUS_CHARGING:
      if (!StandardMetrics.ms_v_charge_inprogress->AsBool())
        {
        StandardMetrics.ms_v_charge_kwh->SetValue(0); // Reset charge kWh
        }
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");

      // TODO only use battery current for Quick Charging, for regular charging
      // we should return AC line current and voltage, not battery
      // TODO does the leaf know the AC line current and voltage?
      // TODO v3 supports negative values here, what happens if we send a negative charge current to a v2 client?
      if (StandardMetrics.ms_v_bat_current->AsFloat() < 0)
        {
        // battery current can go negative when climate control draws more than
        // is available from the charger. We're abusing the line current which
        // is unsigned, so don't underflow it
        //
        // TODO quick charging can draw current from the vehicle
        StandardMetrics.ms_v_charge_current->SetValue(0);
        }
      else
        {
        StandardMetrics.ms_v_charge_current->SetValue(StandardMetrics.ms_v_bat_current->AsFloat());
        }
      StandardMetrics.ms_v_charge_voltage->SetValue(StandardMetrics.ms_v_bat_voltage->AsFloat());
      break;
    case CHARGER_STATUS_FINISHED:
      // Charging finished
      StandardMetrics.ms_v_charge_current->SetValue(0);
      // TODO set this in ovms v2
      // TODO the charger probably knows the line voltage, when we find where it's
      // coded, don't zero it out when we're plugged in but not charging
      StandardMetrics.ms_v_charge_voltage->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_charge_state->SetValue("done");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      break;
    }
  if (status != CHARGER_STATUS_CHARGING && status != CHARGER_STATUS_QUICK_CHARGING)
    {
    StandardMetrics.ms_v_charge_current->SetValue(0);
    // TODO the charger probably knows the line voltage, when we find where it's
    // coded, don't zero it out when we're plugged in but not charging
    StandardMetrics.ms_v_charge_voltage->SetValue(0);
    }
  }

////////////////////////////////////////////////////////////////////////
// PollStart()
// Send the initial message to poll for data. Further data is requested
// in vehicle_nissanleaf_poll_continue() after the recept of each page.
//

void OvmsVehicleNissanLeaf::PollStart(void)
  {
  // Request Group 1
  uint8_t data[] = {0x02, 0x21, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00};
  nl_poll_state = ZERO;
  m_can1->WriteStandard(0x79b, 8, data);
  }

////////////////////////////////////////////////////////////////////////
// vehicle_nissanleaf_poll_continue()
// Process a 0x7bb polling response and request the next page
//

void OvmsVehicleNissanLeaf::PollContinue(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  uint16_t hx;
  uint32_t ah;
  if (nl_poll_state == IDLE)
    {
    // we're not expecting anything, maybe something else is polling?
    return;
    }
  if ((d[0] & 0x0f) != nl_poll_state)
    {
    // not the page we were expecting, abort
    nl_poll_state = IDLE;
    return;
    }

  switch (nl_poll_state)
    {
    case IDLE:
      // this isn't possible due to the idle check above
      abort();
    case ZERO:
    case ONE:
    case TWO:
    case THREE:
      // TODO this might not be idomatic C++ but I want to keep the delta to
      // the v2 code small until the porting is finished
      nl_poll_state = static_cast<PollState>(static_cast<int>(nl_poll_state) + 1);
      break;
    case FOUR:
      hx = d[2];
      hx = hx << 8;
      hx = hx | d[3];
      // LeafSpy calculates SOH by dividing Ah by the nominal capacity.
      // Since SOH is derived from Ah, we don't bother storing it separately.
      // Instead we store Ah in CAC (below) and store Hx in SOH.
      StandardMetrics.ms_v_bat_soh->SetValue(hx / 100.0);
      nl_poll_state = static_cast<PollState>(static_cast<int>(nl_poll_state) + 1);
      break;
    case FIVE:
      ah = d[2];
      ah = ah << 8;
      ah = ah | d[3];
      ah = ah << 8;
      ah = ah | d[4];
      StandardMetrics.ms_v_bat_cac->SetValue(ah / 10000.0);
      nl_poll_state = IDLE;
      break;
    }
  if (nl_poll_state != IDLE)
    {
    // request the next page of data
    uint8_t next[] = {0x30, 0x01, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00};
    this->m_can1->WriteStandard(0x79b, 8, next);
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  uint16_t nl_gids;
  uint16_t nl_max_gids = GEN_1_NEW_CAR_GIDS;

  switch (p_frame->MsgID)
    {
    case 0x1db:
    {
      // sent by the LBC, measured inside the battery box
      // current is 11 bit twos complement big endian starting at bit 0
      int16_t nl_battery_current = ((int16_t) d[0] << 3) | (d[1] & 0xe0) >> 5;
      if (nl_battery_current & 0x0400)
        {
        // negative so extend the sign bit
        nl_battery_current |= 0xf800;
        }
      StandardMetrics.ms_v_bat_current->SetValue(nl_battery_current / 2.0f);

      // voltage is 10 bits unsigned big endian starting at bit 16
      int16_t nl_battery_voltage = ((uint16_t) d[2] << 2) | (d[3] & 0xc0) >> 6;
      StandardMetrics.ms_v_bat_voltage->SetValue(nl_battery_voltage / 2.0f);
    }
      break;
    case 0x284:
    {
      vehicle_nissanleaf_car_on(true);

      uint16_t car_speed16 = d[4];
      car_speed16 = car_speed16 << 8;
      car_speed16 = car_speed16 | d[5];
      // this ratio determined by comparing with the dashboard speedometer
      // it is approximately correct and converts to km/h on my car with km/h speedo
      StandardMetrics.ms_v_pos_speed->SetValue(car_speed16 / 92);
    }
      break;
    case 0x390:
      // Gen 2 Charger
      //
      // When the data is valid, can_databuffer[6] is the J1772 maximum
      // available current if we're plugged in, and 0 when we're not.
      // can_databuffer[3] seems to govern when it's valid, and is probably a
      // bit field, but I don't have a full decoding
      //
      // specifically the last few frames before shutdown to wait for the charge
      // timer contain zero in can_databuffer[6] so we ignore them and use the
      // valid data in the earlier frames
      //
      // During plug in with charge timer activated, byte 3 & 6:
      //
      // 0x390 messages start
      // 0x00 0x21
      // 0x60 0x21
      // 0x60 0x00
      // 0xa0 0x00
      // 0xb0 0x00
      // 0xb2 0x15 -- byte 6 now contains valid j1772 pilot amps
      // 0xb3 0x15
      // 0xb1 0x00
      // 0x71 0x00
      // 0x69 0x00
      // 0x61 0x00
      // 0x390 messages stop
      //
      // byte 3 is 0xb3 during charge, and byte 6 contains the valid pilot amps
      //
      // so far, except briefly during startup, when byte 3 is 0x00 or 0x03,
      // byte 6 is 0x00, correctly indicating we're unplugged, so we use that
      // for unplugged detection.
      //
      if (d[3] == 0xb3 ||
        d[3] == 0x00 ||
        d[3] == 0x03)
        {
        // can_databuffer[6] is the J1772 pilot current, 0.5A per bit
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("type1");
        uint8_t current_limit = (d[6] + 1) / 2;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        StandardMetrics.ms_v_charge_pilot->SetValue(current_limit != 0);
        StandardMetrics.ms_v_door_chargeport->SetValue(current_limit != 0);
        }
      switch (d[5])
        {
        case 0x80:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x83:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x88:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x98:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        }
      break;
    case 0x54b:
    {
      bool hvac_candidate;
      // this might be a bit field? So far these 6 values indicate HVAC on
      hvac_candidate = 
        d[1] == 0x0a || // Gen 1 Remote
        d[1] == 0x48 || // Manual Heating or Fan Only
        d[1] == 0x4b || // Gen 2 Remote Heating
        d[1] == 0x71 || // Gen 2 Remote Cooling
        d[1] == 0x76 || // Auto
        d[1] == 0x78;   // Manual A/C on
      StandardMetrics.ms_v_env_hvac->SetValue(hvac_candidate);
    }
      break;
    case 0x54c:
    {
      if (d[6] == 0xff)
        {
        break;
        }
      // TODO this temperature isn't quite right
      int8_t ambient_temp = d[6] - 56; // Fahrenheit
      ambient_temp = (ambient_temp - 32) / 1.8f; // Celsius
      StandardMetrics.ms_v_env_temp->SetValue(ambient_temp);
      break;
    }
    case 0x5bc:
    {
      uint16_t nl_gids_candidate = ((uint16_t) d[0] << 2) | ((d[1] & 0xc0) >> 6);
      if (nl_gids_candidate == 1023)
        {
        // ignore invalid data seen during startup
        break;
        }
      nl_gids = nl_gids_candidate;
      StandardMetrics.ms_v_bat_soc->SetValue((nl_gids * 100 + (nl_max_gids / 2)) / nl_max_gids);
      StandardMetrics.ms_v_bat_range_ideal->SetValue((nl_gids * GEN_1_NEW_CAR_RANGE_MILES + (GEN_1_NEW_CAR_GIDS / 2)) / GEN_1_NEW_CAR_GIDS);
    }
      break;
    case 0x5bf:
      if (d[4] == 0xb0)
        {
        // Quick Charging
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("chademo");
        StandardMetrics.ms_v_charge_climit->SetValue(120);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        }
      else
        {
        // Maybe J1772 is connected
        // can_databuffer[2] is the J1772 maximum available current, 0 if we're not plugged in
        // TODO enum?
        StandardMetrics.ms_v_charge_type->SetValue("type1");
        uint8_t current_limit = d[2] / 5;
        StandardMetrics.ms_v_charge_climit->SetValue(current_limit);
        StandardMetrics.ms_v_charge_pilot->SetValue(current_limit != 0);
        StandardMetrics.ms_v_door_chargeport->SetValue(current_limit != 0);
        }

      switch (d[4])
        {
        case 0x28:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_IDLE);
          break;
        case 0x30:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT);
          break;
        case 0xb0: // Quick Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_QUICK_CHARGING);
          break;
        case 0x60: // L2 Charging
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_CHARGING);
          break;
        case 0x40:
          vehicle_nissanleaf_charger_status(CHARGER_STATUS_FINISHED);
          break;
        }
      break;
    case 0x5c0:
      if (d[0] == 0x40)
        {
        StandardMetrics.ms_v_bat_temp->SetValue(d[2] / 2 - 40);
        }
      break;
    case 0x7bb:
      PollContinue(p_frame);
      break;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  }

void OvmsVehicleNissanLeaf::Ticker1(std::string event, void* data)
  {
  // FIXME
  // detecting that on is stale and therefor should turn off probably shouldn't
  // be done like this
  // perhaps there should be a car on-off state tracker and event generator in
  // the core framework?
  // perhaps interested code should be able to subscribe to "onChange" and
  // "onStale" events for each metric?
  if (StandardMetrics.ms_v_env_on->IsStale())
    {
    vehicle_nissanleaf_car_on(false);
    }
  }

void OvmsVehicleNissanLeaf::Ticker60(std::string event, void* data)
  {
  if (StandardMetrics.ms_v_env_on->AsBool())
    {
    // we only poll while the car is on -- polling at other times causes a
    // relay to click
    PollStart();
    }
  }

class OvmsVehicleNissanLeafInit
  {
  public: OvmsVehicleNissanLeafInit();
  } MyOvmsVehicleNissanLeafInit  __attribute__ ((init_priority (9000)));

OvmsVehicleNissanLeafInit::OvmsVehicleNissanLeafInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Nissan Leaf (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleNissanLeaf>("NL","Nissan Leaf");
  }
