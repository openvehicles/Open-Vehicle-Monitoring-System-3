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
#include "ovms_webserver.h"

#define MAX_POLL_DATA_LEN 196

static const OvmsVehicle::poll_pid_t obdii_polls[] =
  {
    { 0x797, 0x79a, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x81, {  0,999,999 } }, // VIN [19]
    { 0x797, 0x79a, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1203, {  0,999,999 } }, // QC [4]
    { 0x797, 0x79a, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1205, {  0,999,999 } }, // L0/L1/L2 [4]
    { 0x79b, 0x7bb, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x01, {  0, 61, 61 } }, // bat [39]
    { 0x79b, 0x7bb, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x02, {  0, 67, 67 } }, // battery voltages [196]
    { 0x79b, 0x7bb, VEHICLE_POLL_TYPE_OBDIIGROUP, 0x04, {  0,307,307 } }, // battery temperatures [14]
    { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  };

typedef enum
  {
  CHARGER_STATUS_IDLE,
  CHARGER_STATUS_PLUGGED_IN_TIMER_WAIT,
  CHARGER_STATUS_CHARGING,
  CHARGER_STATUS_QUICK_CHARGING,
  CHARGER_STATUS_FINISHED
  } ChargerStatus;

void remoteCommandTimer(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->RemoteCommandTimer();
  }

void ccDisableTimer(TimerHandle_t timer)
  {
  OvmsVehicleNissanLeaf* nl = (OvmsVehicleNissanLeaf*) pvTimerGetTimerID(timer);
  nl->CcDisableTimer();
  }

OvmsVehicleNissanLeaf::OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Nissan Leaf v3.0 vehicle module");

  m_gids = MyMetrics.InitInt("xnl.v.b.gids", SM_STALE_HIGH, 0);
  m_hx = MyMetrics.InitFloat("xnl.v.b.hx", SM_STALE_HIGH, 0);
  m_soc_new_car = MyMetrics.InitFloat("xnl.v.b.soc.newcar", SM_STALE_HIGH, 0, Percentage);
  m_soc_instrument = MyMetrics.InitFloat("xnl.v.b.soc.instrument", SM_STALE_HIGH, 0, Percentage);
  m_bms_thermistor = new OvmsMetricVector<int>("xnl.bms.thermistor", SM_STALE_MIN, Native);
  m_bms_temp_int = new OvmsMetricVector<int>("xnl.bms.temp_int", SM_STALE_MIN, Celcius);
  BmsSetCellArrangementVoltage(96, 32);
  BmsSetCellArrangementTemperature(3, 1);

  m_soh_new_car = MyMetrics.InitFloat("xnl.v.b.soh.newcar", SM_STALE_HIGH, 0, Percentage);
  m_soh_instrument = MyMetrics.InitInt("xnl.v.b.soh.instrument", SM_STALE_HIGH, 0, Percentage);
  m_battery_energy_capacity = MyMetrics.InitFloat("xnl.v.b.e.capacity", SM_STALE_HIGH, 0, kWh);
  m_battery_energy_available = MyMetrics.InitFloat("xnl.v.b.e.available", SM_STALE_HIGH, 0, kWh);
  m_charge_duration_full_l2 = MyMetrics.InitFloat("xnl.v.c.duration.full.l2", SM_STALE_HIGH, 0, Minutes);
  m_charge_duration_full_l1 = MyMetrics.InitFloat("xnl.v.c.duration.full.l1", SM_STALE_HIGH, 0, Minutes);
  m_charge_duration_full_l0 = MyMetrics.InitFloat("xnl.v.c.duration.full.l0", SM_STALE_HIGH, 0, Minutes);
  m_charge_duration_range_l2 = MyMetrics.InitFloat("xnl.v.c.duration.range.l2", SM_STALE_HIGH, 0, Minutes);
  m_charge_duration_range_l1 = MyMetrics.InitFloat("xnl.v.c.duration.range.l1", SM_STALE_HIGH, 0, Minutes);
  m_charge_duration_range_l0 = MyMetrics.InitFloat("xnl.v.c.duration.range.l0", SM_STALE_HIGH, 0, Minutes);
  m_charge_count_qc     = MyMetrics.InitInt("xnl.v.c.count.qc",     SM_STALE_MIN, 0);
  m_charge_count_l0l1l2 = MyMetrics.InitInt("xnl.v.c.count.l0l1l2", SM_STALE_MIN, 0); 

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  PollSetPidList(m_can2,obdii_polls);
  PollSetState(0);

  MyConfig.RegisterParam("xnl", "Nissan Leaf", true, true);
  ConfigChanged(NULL);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif

  m_remoteCommandTimer = xTimerCreate("Nissan Leaf Remote Command", 100 / portTICK_PERIOD_MS, pdTRUE, this, remoteCommandTimer);
  m_ccDisableTimer = xTimerCreate("Nissan Leaf CC Disable", 1000 / portTICK_PERIOD_MS, pdFALSE, this, ccDisableTimer);

  using std::placeholders::_1;
  using std::placeholders::_2;
  }

OvmsVehicleNissanLeaf::~OvmsVehicleNissanLeaf()
  {
  ESP_LOGI(TAG, "Shutdown Nissan Leaf vehicle module");

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebDeInit();
#endif
  }

void OvmsVehicleNissanLeaf::ConfigChanged(OvmsConfigParam* param)
{
  ESP_LOGD(TAG, "Nissan Leaf reload configuration");

  // Note that we do not store a configurable maxRange like Kia Soal and Renault Twizy.
  // Instead, we derive maxRange from maxGids

  // Instances:
  // xnl
  //  suffsoc          	Sufficient SOC [%] (Default: 0=disabled)
  //  suffrange        	Sufficient range [km] (Default: 0=disabled)
  
  StandardMetrics.ms_v_charge_limit_soc->SetValue(   (float) MyConfig.GetParamValueInt("xnl", "suffsoc"),   Percentage );
  StandardMetrics.ms_v_charge_limit_range->SetValue( (float) MyConfig.GetParamValueInt("xnl", "suffrange"), Kilometers );

  //TODO nl_enable_write = MyConfig.GetParamValueBool("xnl", "canwrite", false);
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

void OvmsVehicleNissanLeaf::PollReply_Battery(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 39 &&    // 24 KWh Leafs
      reply_len != 41)      // 30 KWh Leafs with Nissan BMS fix
    {
    ESP_LOGI(TAG, "PollReply_Battery: len=%d != 39 && != 41", reply_len);
    return;
    }

  //  > 0x79b 21 01
  //  < 0x7bb 61 01
  // [ 0.. 5] 0000020d 0287
  // [ 6..15] 00000363 ffffffff001c
  // [16..23] 2af89a89 2e4b 03a4
  // [24..31] 005c 269a 000ecf81
  // [32..38] 0009d7b0 800001
  //
  // or
  // > 0x79b 21 01
  // < 0x7bb 61 01
  // [ 0,, 5] fffffc4e 028a
  // [ 6..15] ffffff46 ffffffff 1290
  // [16..23] 2af88fcd 32de 038b
  // [24..31] 005c 1f3d 000896c6
  // [32..38] 000b3290 800001
  // [39..40] 0000

  uint16_t hx = (reply_data[26] << 8)
              |  reply_data[27];
  m_hx->SetValue(hx / 100.0);

  uint32_t ah10000 = (reply_data[33] << 16)
                   | (reply_data[34] << 8)
                   |  reply_data[35];
  float ah = ah10000 / 10000.0;
  StandardMetrics.ms_v_bat_cac->SetValue(ah);

  // there may be a way to get the SoH directly from the BMS, but for now
  // divide by a configurable battery size
  // - For 24 KWh : xnl.newCarAh = 66 (default)
  // - For 30 KWh : xnl.newCarAh = 80 (i.e. shell command "config set xnl newCarAh 80")
  float newCarAh = MyConfig.GetParamValueFloat("xnl", "newCarAh", GEN_1_NEW_CAR_AH);
  float soh = ah / newCarAh * 100;
  m_soh_new_car->SetValue(soh);
  if (MyConfig.GetParamValueBool("xnl", "soh.newcar", false))
    {
    StandardMetrics.ms_v_bat_soh->SetValue(soh);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_BMS_Volt(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 196)
    {
    ESP_LOGI(TAG, "PollReply_BMS_Volt: len=%d != 196", reply_len);
    return;
    }
  //  > 0x79b 21 02
  //  < 0x7bb 61 02
  // [ 0..191]: Contains all 96 of the cell voltages, in volts/1000 (2 bytes per cell)
  // [192,193]: Pack voltage, in volts/100
  // [194,195]: Bus voltage, in volts/100

  int i;
  for(i=0; i<96; i++)
    {
    int millivolt = reply_data[i*2] << 8 | reply_data[i*2+1];
    BmsSetCellVoltage(i, millivolt / 1000.0);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_BMS_Temp(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 14)
    {
    ESP_LOGI(TAG, "PollReply_BMS_Temp: len=%d != 14", reply_len);
    return;
    }
  //  > 0x79b 21 04
  //  < 0x7bb 61 04
  // [ 0, 1] pack 1 thermistor
  // [    2] pack 1 temp in degC
  // [ 3, 4] pack 2 thermistor
  // [    5] pack 2 temp in degC
  // [ 6, 7] pack 3 thermistor (unused)
  // [    8] pack 3 temp in degC (unused)
  // [ 9,10] pack 4 thermistor
  // [   11] pack 4 temp in degC
  // [   12] pack 5 temp in degC
  // [   13] unknown; varies in interesting ways
  // 14 bytes:
  //      0  1  2   3  4  5   6  7  8   9 10 11  12 13
  // 14 [02 51 0c  02 4d 0d  ff ff ff  02 4d 0d  0c 00 ]
  // 14 [02 52 0c  02 4e 0c  ff ff ff  02 4e 0c  0c 00 ]
  // 14 [02 57 0c  02 57 0c  ff ff ff  02 58 0b  0b 00 ]
  // 14 [02 5a 0b  02 59 0b  ff ff ff  02 5a 0b  0b 00 ]
  //

  int thermistor[4];
  int temp_int[6];
  int i;
  int out = 0;
  for(i=0; i<4; i++)
    {
    thermistor[i] = reply_data[i*3] << 8 | reply_data[i*3+1];
    temp_int[i] = reply_data[i*3+2];
    if(thermistor[i] != 0xffff)
      {
      BmsSetCellTemperature(out++, -0.102 * (thermistor[i] - 710));
      }
    }
  temp_int[i++] = reply_data[12];
  temp_int[i++] = reply_data[13];
  m_bms_thermistor->SetElemValues(0, 4, thermistor);
  m_bms_temp_int->SetElemValues(0, 6, temp_int);
  }

void OvmsVehicleNissanLeaf::PollReply_QC(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 4)
    {
    ESP_LOGI(TAG, "PollReply_QC: len=%d != 4", reply_len);
    return;
    }
  //  > 0x797 22 12 03
  //  < 0x79a 62 12 03
  // [ 0..3] 00 0C 00 00

  uint16_t count = reply_data[0] * 0x100 + reply_data[1];
  if (count != 0xFFFF)
    {
    m_charge_count_qc->SetValue(count);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_L0L1L2(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 4)
    {
    ESP_LOGI(TAG, "PollReply_L0L1L2: len=%d != 4", reply_len);
    return;
    }
  //  > 0x797 22 12 05
  //  < 0x79a 62 12 05
  // [ 0..3] 00 5D 00 00

  uint16_t count = reply_data[0] * 0x100 + reply_data[1];
  if (count != 0xFFFF)
    {
    m_charge_count_l0l1l2->SetValue(count);
    }
  }

void OvmsVehicleNissanLeaf::PollReply_VIN(uint8_t reply_data[], uint16_t reply_len)
  {
  if (reply_len != 19)
    {
    ESP_LOGI(TAG, "PollReply_VIN: len=%d != 19", reply_len);
    return;
    }
  //  > 0x797 21 81
  //  < 0x79a 61 81
  // [ 0..16] 53 4a 4e 46 41 41 5a 45 30 55 3X 3X 3X 3X 3X 3X 3X
  // [17..18] 00 00

  StandardMetrics.ms_v_vin->SetValue((char*)reply_data);
  }

// Reassemble all pieces of a multi-frame reply.
void OvmsVehicleNissanLeaf::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t remain)
  {
  static int last_pid = -1;
  static int last_remain = -1;
  static uint8_t buf[MAX_POLL_DATA_LEN];
  static int bufpos = 0;

  int i;
  if ( pid != last_pid || remain >= last_remain )
    {
    // must be a new reply, so reset to the beginning
    last_pid=pid;
    last_remain=remain;
    bufpos=0;
    }
  for (i=0; i<length; i++)
    {
    if ( bufpos < sizeof(buf) ) buf[bufpos++] = data[i];
    }
  if (remain==0)
    {
    uint32_t id_pid = m_poll_moduleid_low<<16 | pid;
    switch (id_pid)
      {
      case 0x7bb0001: // battery
        PollReply_Battery(buf, bufpos);
        break;
      case 0x7bb0002:
        PollReply_BMS_Volt(buf, bufpos);
        break;
      case 0x7bb0004:
        PollReply_BMS_Temp(buf, bufpos);
        break;
      case 0x79a1203: // QC
        PollReply_QC(buf, bufpos);
        break;
      case 0x79a1205: // L0/L1/L2
        PollReply_L0L1L2(buf, bufpos);
        break;
      case 0x79a0081: // VIN
        PollReply_VIN(buf, bufpos);
        break;
      default:
        ESP_LOGI(TAG, "IncomingPollReply: unknown reply module|pid=%#x len=%d", id_pid, bufpos);
        break;
      }
    last_pid=-1;
    last_remain=-1;
    bufpos=0;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

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

      // soc displayed on the instrument cluster
      uint8_t soc = d[4] & 0x7f;
      if (soc != 0x7f)
        {
        m_soc_instrument->SetValue(soc);
        // we use this unless the user has opted otherwise
        if (!MyConfig.GetParamValueBool("xnl", "soc.newcar", false))
          {
          StandardMetrics.ms_v_bat_soc->SetValue(soc);
          }
        }
    }
      break;
    case 0x284:
    {
      // certain CAN bus activity counts as state "awake"
      StandardMetrics.ms_v_env_awake->SetValue(true);

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
      /* These may only reflect the centre console LEDs, which
       * means they don't change when remote CC is operating.
       * They should be replaced when we find better signals that
       * indicate when heating or cooling is actually occuring.
       */
      StandardMetrics.ms_v_env_cooling->SetValue(d[1] & 0x10);
      StandardMetrics.ms_v_env_heating->SetValue(d[1] & 0x02);
    }
      break;
    case 0x54c:
      /* Ambient temperature.  This one has half-degree C resolution,
       * and seems to stay within a degree or two of the "eyebrow" temp display.
       * App label: AMBIENT
       */
      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_temp->SetValue(d[6] / 2.0 - 40);
        }
      break;
    case 0x54f:
      /* Climate control's measurement of temperature inside the car.
       * Subtracting 14 is a bit of a guess worked out by observing how
       * auto climate control reacts when this reaches the target setting.
       */
      if (d[0] != 20)
        {
        StandardMetrics.ms_v_env_cabintemp->SetValue(d[0] / 2.0 - 14);
        }
      break;
    case 0x59e:
      {
      uint16_t cap_gid = d[2] << 4 | d[3] >> 4;
      m_battery_energy_capacity->SetValue(cap_gid * GEN_1_WH_PER_GID, WattHours);
      }
      break;
    case 0x5bc:
      {
      uint16_t nl_gids = ((uint16_t) d[0] << 2) | ((d[1] & 0xc0) >> 6);
      // gids is invalid during startup
      if (nl_gids != 1023)
        {
        m_gids->SetValue(nl_gids);
        m_battery_energy_available->SetValue(nl_gids * GEN_1_WH_PER_GID, WattHours);

        // new car soc -- 100% when the battery is new, less when it's degraded
        uint16_t max_gids = MyConfig.GetParamValueInt("xnl", "maxGids", GEN_1_NEW_CAR_GIDS);
        float soc_new_car = (nl_gids * 100.0) / max_gids;
        m_soc_new_car->SetValue(soc_new_car);

        // we use the instrument cluster soc from 0x1db unless the user has opted otherwise
        if (MyConfig.GetParamValueBool("xnl", "soc.newcar", false))
          {
          StandardMetrics.ms_v_bat_soc->SetValue(soc_new_car);
          }

        /* range calculation now done in HandleRange(), called from Ticker10
        float km_per_kwh = MyConfig.GetParamValueFloat("xnl", "kmPerKWh", GEN_1_KM_PER_KWH);
        float wh_per_gid = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);
        StandardMetrics.ms_v_bat_range_ideal->SetValue((nl_gids * wh_per_gid * km_per_kwh) / 1000);
        */
        }
      /* Estimated charge time is a set of multiplexed values:
       *   d[5]     d[6]     d[7]
       * 76543210 76543210 76543210
       * ......mm mmmvvvvv vvvvvvv.
       */
      uint16_t mx  = ( d[5] << 3 | d[6] >> 5 ) & 0x1f;
      uint16_t val = ( d[6] << 7 | d[7] >> 1 ) & 0xfff;
      switch (mx)
        {
        case 0x05: //  5 = 0+5
          m_charge_duration_full_l2->SetValue(val);
          break;
        case 0x08: //  8 = 3+5
          m_charge_duration_full_l1->SetValue(val);
          break;
        case 0x0b: // 11 = 6+5
          m_charge_duration_full_l0->SetValue(val);
          break;
        case 0x12: // 18 = 0+18
          m_charge_duration_range_l2->SetValue(val);
          break;
        case 0x15: // 21 = 3+18
          m_charge_duration_range_l1->SetValue(val);
          break;
        case 0x18: // 24 = 6+18
          m_charge_duration_range_l0->SetValue(val);
          break;
        }
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
      /* Battery Temperature as reported by the LBC.
       * Effectively has only 7-bit precision, as the bottom bit is always 0.
       */
      if ( (d[0]>>6) == 1 )
        {
        StandardMetrics.ms_v_bat_temp->SetValue(d[2] / 2 - 40);
        }
      break;
    }
  }

void OvmsVehicleNissanLeaf::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x180:
      if (d[5] != 0xff)
        {
        StandardMetrics.ms_v_env_throttle->SetValue(d[5] / 2.00);
        }
      break;
    case 0x292:
      if (d[6] != 0xff)
        {
        StandardMetrics.ms_v_env_footbrake->SetValue(d[6] / 1.39);
        }
      break;
   case 0x355:
     // The odometer value on the bus is always in units appropriate
     // for the car's intended market and is unaffected by the dash
     // display preference (in d[4] bit 5).
     if ((d[6]>>5) & 1)
       {
         m_odometer_units = Miles;
       }
     else
       {
         m_odometer_units = Kilometers;
       }
     break;
    case 0x385:
      // not sure if this order is correct
      if (d[2]) StandardMetrics.ms_v_tpms_fl_p->SetValue(d[2] / 4.0, PSI);
      if (d[3]) StandardMetrics.ms_v_tpms_fr_p->SetValue(d[3] / 4.0, PSI);
      if (d[4]) StandardMetrics.ms_v_tpms_rl_p->SetValue(d[4] / 4.0, PSI);
      if (d[5]) StandardMetrics.ms_v_tpms_rr_p->SetValue(d[5] / 4.0, PSI);
      break;
    case 0x421:
      switch ( (d[0] >> 3) & 7 )
        {
        case 0: // Parking
        case 1: // Park
        case 3: // Neutral
        case 5: // undefined
        case 6: // undefined
          StandardMetrics.ms_v_env_gear->SetValue(0);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          break;
        case 4: // Drive
        case 7: // Drive/B (ECO on some models)
          StandardMetrics.ms_v_env_gear->SetValue(1);
          break;
        }
      break;
    case 0x510:
      /* This seems to be outside temperature with half-degree C accuracy.
       * It reacts a bit more rapidly than what we get from the battery.
       * App label: PEM
       */
      if (d[7] != 0xff)
        {
        StandardMetrics.ms_v_inv_temp->SetValue(d[7] / 2.0 - 40);
        }
      break;
    case 0x5b3:
      {
      // soh as percentage
      uint8_t soh = d[1] >> 1;
      if (soh != 0)
        {
        m_soh_instrument->SetValue(soh);
        // we use this unless the user has opted otherwise
        if (!MyConfig.GetParamValueBool("xnl", "soh.newcar", false))
          {
          StandardMetrics.ms_v_bat_soh->SetValue(soh);
          }
        }
      break;
      }
    case 0x5c5:
      // This is the parking brake (which is foot-operated on some models).
      StandardMetrics.ms_v_env_handbrake->SetValue(d[0] & 4);
      if (m_odometer_units != Other)
        {
        StandardMetrics.ms_v_pos_odometer->SetValue(d[1] << 16 | d[2] << 8 | d[3], m_odometer_units);
        }
      break;
    case 0x60d:
      StandardMetrics.ms_v_door_trunk->SetValue(d[0] & 0x80);
      StandardMetrics.ms_v_door_rr->SetValue(d[0] & 0x40);
      StandardMetrics.ms_v_door_rl->SetValue(d[0] & 0x20);
      StandardMetrics.ms_v_door_fr->SetValue(d[0] & 0x10);
      StandardMetrics.ms_v_door_fl->SetValue(d[0] & 0x08);
      StandardMetrics.ms_v_env_headlights->SetValue((d[0] & 0x02) | // dip beam
                                                    (d[1] & 0x08)); // main beam
      /* d[1] bits 1 and 2 indicate Start button states:
       *   No brake:   off -> accessory -> on -> off
       *   With brake: [off, accessory, on] -> ready to drive -> off
       * Using "ready to drive" state to set ms_v_env_on seems sensible,
       * though maybe "on, not ready to drive" should also count.
       */
      switch ((d[1]>>1) & 3)
        {
        case 0: // off
        case 1: // accessory
        case 2: // on (not ready to drive)
          StandardMetrics.ms_v_env_on->SetValue(false);
          PollSetState(0);
          break;
        case 3: // ready to drive
          StandardMetrics.ms_v_env_on->SetValue(true);
          PollSetState(1);
          break;
        }
      // The two lock bits are 0x10 driver door and 0x08 other doors.
      // We should only say "locked" if both are locked.
      StandardMetrics.ms_v_env_locked->SetValue( (d[2] & 0x18) == 0x18);
      break;
    }
  }

////////////////////////////////////////////////////////////////////////
// Send a RemoteCommand on the CAN bus.
// Handles pre and post 2016 model year cars based on xnl.modelyear config
//
// Does nothing if @command is out of range

void OvmsVehicleNissanLeaf::SendCommand(RemoteCommand command)
  {
  unsigned char data[4];
  uint8_t length;
  canbus *tcuBus;

  if (MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR) >= 2016)
    {
    ESP_LOGI(TAG, "New TCU on CAR Bus");
    length = 4;
    tcuBus = m_can2;
    }
  else
    {
    ESP_LOGI(TAG, "OLD TCU on EV Bus");
    length = 1;
    tcuBus = m_can1;
    }

  switch (command)
    {
    case ENABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Enable Climate Control");
      data[0] = 0x4e;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    case DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Disable Climate Control");
      data[0] = 0x56;
      data[1] = 0x00;
      data[2] = 0x01;
      data[3] = 0x00;
      break;
    case START_CHARGING:
      ESP_LOGI(TAG, "Start Charging");
      data[0] = 0x66;
      data[1] = 0x08;
      data[2] = 0x12;
      data[3] = 0x00;
      break;
    case AUTO_DISABLE_CLIMATE_CONTROL:
      ESP_LOGI(TAG, "Auto Disable Climate Control");
      data[0] = 0x46;
      data[1] = 0x08;
      data[2] = 0x32;
      data[3] = 0x00;
      break;
    case UNLOCK_DOORS:
      ESP_LOGI(TAG, "Unlook Doors");
      data[0] = 0x11;
      data[1] = 0x00;
      data[2] = 0x00;
      data[3] = 0x00;
      break;
    case LOCK_DOORS:
      ESP_LOGI(TAG, "Look Doors");
      data[0] = 0x60;
      data[1] = 0x80;
      data[2] = 0x00;
      data[3] = 0x00;
      break;
    default:
      // shouldn't be possible, but lets not send random data on the bus
      return;
    }
    tcuBus->WriteStandard(0x56e, length, data);
  }

////////////////////////////////////////////////////////////////////////
// implements the repeated sending of remote commands and releases
// EV SYSTEM ACTIVATION REQUEST after an appropriate amount of time

void OvmsVehicleNissanLeaf::RemoteCommandTimer()
  {
  ESP_LOGI(TAG, "RemoteCommandTimer %d", nl_remote_command_ticker);
  if (nl_remote_command_ticker > 0)
    {
    nl_remote_command_ticker--;
    if (nl_remote_command != AUTO_DISABLE_CLIMATE_CONTROL)
      {
      SendCommand(nl_remote_command);
      }
    if (nl_remote_command_ticker == 1 && nl_remote_command == ENABLE_CLIMATE_CONTROL)
      {
      xTimerStart(m_ccDisableTimer, 0);
      }

    // nl_remote_command_ticker is set to REMOTE_COMMAND_REPEAT_COUNT in
    // RemoteCommandHandler() and we decrement it every 10th of a
    // second, hence the following if statement evaluates to true
    // ACTIVATION_REQUEST_TIME tenths after we start
    // TODO re-implement to support Gen 1 leaf
    //if (nl_remote_command_ticker == REMOTE_COMMAND_REPEAT_COUNT - ACTIVATION_REQUEST_TIME)
    //  {
    //  // release EV SYSTEM ACTIVATION REQUEST
    //  output_gpo3(FALSE);
    //  }
    }
    else
    {
      xTimerStop(m_remoteCommandTimer, 0);
    }
  }

void OvmsVehicleNissanLeaf::CcDisableTimer()
  {
  ESP_LOGI(TAG, "CcDisableTimer");
  SendCommand(AUTO_DISABLE_CLIMATE_CONTROL);
  }

/**
 * Ticker1: Called every second
 */
void OvmsVehicleNissanLeaf::Ticker1(uint32_t ticker)
  {
  // FIXME
  // detecting that on is stale and therefor should turn off probably shouldn't
  // be done like this
  // perhaps there should be a car on-off state tracker and event generator in
  // the core framework?
  // perhaps interested code should be able to subscribe to "onChange" and
  // "onStale" events for each metric?
  if (StandardMetrics.ms_v_env_awake->IsStale())
    {
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }
  }

/**
 * Ticker10: Called every 10 seconds
 */
void OvmsVehicleNissanLeaf::Ticker10(uint32_t ticker)
  {
    // Update any derived values
    // Range and Charging both mainly depend on SOC, which will change 1% in less than a minute when fast-charging.
    HandleRange();
    HandleCharging();
  }

/**
 * Update derived metrics when charging
 */
void OvmsVehicleNissanLeaf::HandleCharging()
  {
  float limit_soc       = StandardMetrics.ms_v_charge_limit_soc->AsFloat(0);
  float limit_range     = StandardMetrics.ms_v_charge_limit_range->AsFloat(0, Kilometers);
  float charge_current  = StandardMetrics.ms_v_charge_current->AsFloat(0, Amps);
  float charge_voltage  = StandardMetrics.ms_v_charge_voltage->AsFloat(0, Volts);

  // Are we charging?
  if (!StandardMetrics.ms_v_charge_pilot->AsBool()      ||
      !StandardMetrics.ms_v_charge_inprogress->AsBool() ||
      (charge_current <= 0.0) )
    {
    return;
    }

  // Check if we have what is needed to calculate remaining minutes
  if (charge_voltage > 0 && charge_current > 0)
    {
    // always calculate remaining charge time to full
    float full_soc           = 100.0;     // 100%
    int   minsremaining_full = calcMinutesRemaining(full_soc);

    StandardMetrics.ms_v_charge_duration_full->SetValue(minsremaining_full, Minutes);
    ESP_LOGV(TAG, "Time remaining: %d mins to full", minsremaining_full);

    if (limit_soc > 0) 
      {
      // if limit_soc is set, then calculate remaining time to limit_soc
      int minsremaining_soc = calcMinutesRemaining(limit_soc);

      StandardMetrics.ms_v_charge_duration_soc->SetValue(minsremaining_soc, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins to %0.0f%% soc", minsremaining_soc, limit_soc);
      }
    if (limit_range > 0)  
      {
      // if range limit is set, then compute required soc and then calculate remaining time to that soc
      float km_per_kwh    = MyConfig.GetParamValueFloat("xnl", "kmPerKWh", GEN_1_KM_PER_KWH);
      float wh_per_gid    = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);
      float max_gids      = MyConfig.GetParamValueFloat("xnl", "maxGids",  GEN_1_NEW_CAR_GIDS);
      float max_range     = max_gids * wh_per_gid / 1000.0 * km_per_kwh;

      float range_soc           = limit_range / max_range * 100.0;
      int   minsremaining_range = calcMinutesRemaining(range_soc);

      StandardMetrics.ms_v_charge_duration_range->SetValue(minsremaining_range, Minutes);
      ESP_LOGV(TAG, "Time remaining: %d mins for %0.0f km (%0.0f%% soc)", minsremaining_range, limit_range, range_soc);
      }
    }
  }


/**
 * Calculates minutes remaining before target is reached. Based on current charge speed.
 * TODO: Should be calculated based on actual charge curve. Maybe in a later version?
 */
int OvmsVehicleNissanLeaf::calcMinutesRemaining(float target_soc)
  {
  float bat_soc = m_soc_instrument->AsFloat(100);
  if (bat_soc > target_soc)
    {
    return 0;   // Done!
    }

  float charge_current  = StandardMetrics.ms_v_charge_current->AsFloat(0, Amps);
  float charge_voltage  = StandardMetrics.ms_v_charge_voltage->AsFloat(0, Volts);

  float max_gids        = MyConfig.GetParamValueFloat("xnl", "maxGids",  GEN_1_NEW_CAR_GIDS);
  float wh_per_gid      = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);

  float remaining_gids  = max_gids * (target_soc - bat_soc) / 100.0;
  float remaining_wh    = remaining_gids * wh_per_gid;
  float remaining_hours = remaining_wh / (charge_current * charge_voltage);
  float remaining_mins  = remaining_hours * 60.0;

  return MIN( 1440, (int)remaining_mins );
  }

/**
 * Update derived metrics related to range while charging or driving
 */
void OvmsVehicleNissanLeaf::HandleRange()
  {
  float bat_soc     = StandardMetrics.ms_v_bat_soc->AsFloat(0);
  float bat_soh     = StandardMetrics.ms_v_bat_soh->AsFloat(100);
  float bat_temp    = StandardMetrics.ms_v_bat_temp->AsFloat(20, Celcius);
  float amb_temp    = StandardMetrics.ms_v_env_temp->AsFloat(20, Celcius);

  float km_per_kwh  = MyConfig.GetParamValueFloat("xnl", "kmPerKWh", GEN_1_KM_PER_KWH);
  float wh_per_gid  = MyConfig.GetParamValueFloat("xnl", "whPerGid", GEN_1_WH_PER_GID);
  float max_gids    = MyConfig.GetParamValueFloat("xnl", "maxGids",  GEN_1_NEW_CAR_GIDS);

  // Temperature compensation:
  //   - Assumes standard max range specified at 20 degrees C
  //   - Range halved at -20C and at +60C.
  // See: https://www.fleetcarma.com/nissan-leaf-chevrolet-volt-cold-weather-range-loss-electric-vehicle/
  //
  float avg_temp    = (amb_temp + bat_temp*3) / 4.0;
  float factor_temp = (100.0 - ABS(avg_temp - 20.0) * 1.25) / 100.0;

  // Derive max and ideal range, taking into account SOC, but not SOH or temperature
  float range_max   = max_gids * wh_per_gid / 1000.0 * km_per_kwh;
  float range_ideal = range_max * bat_soc / 100.0;

  // Derive max range when full and estimated range, taking into account SOC, SOH and temperature
  float range_full = range_max   * bat_soh / 100.0 * factor_temp;
  float range_est  = range_ideal * bat_soh / 100.0 * factor_temp;

  // Store the metrics
  if (range_ideal > 0.0)
    {
    StandardMetrics.ms_v_bat_range_ideal->SetValue(range_ideal, Kilometers);
    }

  if (range_full > 0.0)
	{
	StandardMetrics.ms_v_bat_range_full->SetValue(range_full, Kilometers);
	}

  if (range_est > 0.0)
	{
    StandardMetrics.ms_v_bat_range_est->SetValue(range_est, Kilometers);
    }

  // Trace
  ESP_LOGV(TAG, "Range: ideal=%0.0f km, est=%0.0f km, full=%0.0f km", range_ideal, range_est, range_full);
  }


////////////////////////////////////////////////////////////////////////
// Wake up the car & send Climate Control or Remote Charge message to VCU,
// replaces Nissan's CARWINGS and TCU module, see
// http://www.mynissanleaf.com/viewtopic.php?f=44&t=4131&hilit=open+CAN+discussion&start=416
//
// On Generation 1 Cars, TCU pin 11's "EV system activation request signal" is
// driven to 12V to wake up the VCU. This function drives RC3 high to
// activate the "EV system activation request signal". Without a circuit
// connecting RC3 to the activation signal wire, remote climate control will
// only work during charging and for obvious reasons remote charging won't
// work at all.
//
// On Generation 2 Cars, a CAN bus message is sent to wake up the VCU. This
// function sends that message even to Generation 1 cars which doesn't seem to
// cause any problems.
//

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::RemoteCommandHandler(RemoteCommand command)
  {
  ESP_LOGI(TAG, "RemoteCommandHandler");

  // Use GPIO to wake up GEN 1 Leaf with EV SYSTEM ACTIVATION REQUEST
  // TODO re-implement to support Gen 1 leaf
  //output_gpo3(TRUE);

  // The Gen 2 Wakeup frame works on some Gen1, and doesn't cause a problem
  // so we send it on all models. We have to take care to send it on the
  // correct can bus in newer cars.
  ESP_LOGI(TAG, "Sending Gen 2 Wakeup Frame");
  int modelyear = MyConfig.GetParamValueInt("xnl", "modelyear", DEFAULT_MODEL_YEAR);
  canbus* tcuBus = modelyear >= 2016 ? m_can2 : m_can1;
  unsigned char data = 0;
  tcuBus->WriteStandard(0x68c, 1, &data);

  // The GEN 2 Nissan TCU module sends the command repeatedly, so we start
  // m_remoteCommandTimer (which calls RemoteCommandTimer()) to do this
  // EV SYSTEM ACTIVATION REQUEST is released in the timer too
  nl_remote_command = command;
  nl_remote_command_ticker = REMOTE_COMMAND_REPEAT_COUNT;
  xTimerStart(m_remoteCommandTimer, 0);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandHomelink(int button, int durationms)
  {
  ESP_LOGI(TAG, "CommandHomelink");
  if (button == 0)
    {
    return RemoteCommandHandler(ENABLE_CLIMATE_CONTROL);
    }
  if (button == 1)
    {
    return RemoteCommandHandler(DISABLE_CLIMATE_CONTROL);
    }
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandClimateControl(bool climatecontrolon)
  {
  ESP_LOGI(TAG, "CommandClimateControl");
  return RemoteCommandHandler(climatecontrolon ? ENABLE_CLIMATE_CONTROL : DISABLE_CLIMATE_CONTROL);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandLock(const char* pin)
  {
  return RemoteCommandHandler(LOCK_DOORS);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandUnlock(const char* pin)
  {
  return RemoteCommandHandler(UNLOCK_DOORS);
  }

OvmsVehicle::vehicle_command_t OvmsVehicleNissanLeaf::CommandStartCharge()
  {
  ESP_LOGI(TAG, "CommandStartCharge");
  return RemoteCommandHandler(START_CHARGING);
  }

/**
 * SetFeature: V2 compatibility config wrapper
 *  Note: V2 only supported integer values, V3 values may be text
 */
bool OvmsVehicleNissanLeaf::SetFeature(int key, const char *value)
{
  switch (key)
  {
    case 10:
      MyConfig.SetParamValue("xnl", "suffsoc", value);
      return true;
    case 11:
      MyConfig.SetParamValue("xnl", "suffrange", value);
      return true;
    case 15:
    {
      int bits = atoi(value);
      MyConfig.SetParamValueBool("xnl", "canwrite",  (bits& 1)!=0);
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
const std::string OvmsVehicleNissanLeaf::GetFeature(int key)
{
  switch (key)
  {
    case 0:
    case 10:
      return MyConfig.GetParamValue("xnl", "suffsoc", STR(0));
    case 11:
      return MyConfig.GetParamValue("xnl", "suffrange", STR(0));
    case 15:
    {
      int bits =
        ( MyConfig.GetParamValueBool("xnl", "canwrite",  false) ?  1 : 0);
      char buf[4];
      sprintf(buf, "%d", bits);
      return std::string(buf);
    }
    default:
      return OvmsVehicle::GetFeature(key);
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
