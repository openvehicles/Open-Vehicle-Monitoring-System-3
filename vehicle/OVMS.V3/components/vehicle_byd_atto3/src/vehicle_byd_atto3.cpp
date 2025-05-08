/*
;    Project:       Open Vehicle Monitor System
;    Date:          22nd July 2023
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2023       Alan Harper
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
static const char *TAG = "v-atto3";

#include <stdio.h>
#include "vehicle_byd_atto3.h"

#define POLL_FOR_BATTERY_SOC             0x0005
#define POLL_FOR_BATTERY_VOLTAGE         0x0008
#define POLL_FOR_BATTERY_CURRENT         0x0009
#define POLL_FOR_LOWEST_TEMP_CELL        0x002f
#define POLL_FOR_HIGHEST_TEMP_CELL       0x0031
#define POLL_FOR_BATTERY_PACK_AVG_TEMP   0x0032
#define POLL_FOR_BATTERY_CELL_MV_MAX     0x002D
#define POLL_FOR_BATTERY_CELL_MV_MIN     0x002B
#define POLL_FOR_MAX_CHARGE_POWER        0x000A
#define POLL_MODULE_1_LOWEST_MV_NUMBER   0x016C
#define POLL_MODULE_1_LOWEST_CELL_MV     0x016D
#define POLL_MODULE_1_HIGHEST_MV_NUMBER  0x016E
#define POLL_MODULE_1_HIGH_CELL_MV       0x016F
#define POLL_MODULE_1_HIGH_TEMP          0x0171
#define POLL_MODULE_1_LOW_TEMP           0x0173
#define POLL_MODULE_2_LOWEST_MV_NUMBER   0x0174
#define POLL_MODULE_2_LOWEST_CELL_MV     0x0175
#define POLL_MODULE_2_HIGHEST_MV_NUMBER  0x0176
#define POLL_MODULE_2_HIGH_CELL_MV       0x0177
#define POLL_MODULE_2_HIGH_TEMP          0x0179
#define POLL_MODULE_2_LOW_TEMP           0x017B
#define POLL_MODULE_3_LOWEST_MV_NUMBER   0x017C
#define POLL_MODULE_3_LOWEST_CELL_MV     0x017D
#define POLL_MODULE_3_HIGHEST_MV_NUMBER  0x017E
#define POLL_MODULE_3_HIGH_CELL_MV       0x017F
#define POLL_MODULE_3_HIGH_TEMP          0x0181
#define POLL_MODULE_3_LOW_TEMP           0x0183
#define POLL_MODULE_4_LOWEST_MV_NUMBER   0x0184
#define POLL_MODULE_4_LOWEST_CELL_MV     0x0185
#define POLL_MODULE_4_HIGHEST_MV_NUMBER  0x0186
#define POLL_MODULE_4_HIGH_CELL_MV       0x0187
#define POLL_MODULE_4_HIGH_TEMP          0x0189
#define POLL_MODULE_4_LOW_TEMP           0x018B
#define POLL_MODULE_5_LOWEST_MV_NUMBER   0x018C
#define POLL_MODULE_5_LOWEST_CELL_MV     0x018D
#define POLL_MODULE_5_HIGHEST_MV_NUMBER  0x018E
#define POLL_MODULE_5_HIGH_CELL_MV       0x018F
#define POLL_MODULE_5_HIGH_TEMP          0x0191
#define POLL_MODULE_5_LOW_TEMP           0x0193
#define POLL_MODULE_6_LOWEST_MV_NUMBER   0x0194
#define POLL_MODULE_6_LOWEST_CELL_MV     0x0195
#define POLL_MODULE_6_HIGHEST_MV_NUMBER  0x0196
#define POLL_MODULE_6_HIGH_CELL_MV       0x0197
#define POLL_MODULE_6_HIGH_TEMP          0x0199
#define POLL_MODULE_6_LOW_TEMP           0x019B
#define POLL_MODULE_7_LOWEST_MV_NUMBER   0x019C
#define POLL_MODULE_7_LOWEST_CELL_MV     0x019D
#define POLL_MODULE_7_HIGHEST_MV_NUMBER  0x019E
#define POLL_MODULE_7_HIGH_CELL_MV       0x019F
#define POLL_MODULE_7_HIGH_TEMP          0x01A1
#define POLL_MODULE_7_LOW_TEMP           0x01A3
#define POLL_MODULE_8_LOWEST_MV_NUMBER   0x01A4
#define POLL_MODULE_8_LOWEST_CELL_MV     0x01A5
#define POLL_MODULE_8_HIGHEST_MV_NUMBER  0x01A6
#define POLL_MODULE_8_HIGH_CELL_MV       0x01A7
#define POLL_MODULE_8_HIGH_TEMP          0x01A9
#define POLL_MODULE_8_LOW_TEMP           0x01AB
#define POLL_MODULE_9_LOWEST_MV_NUMBER   0x01AC
#define POLL_MODULE_9_LOWEST_CELL_MV     0x01AD
#define POLL_MODULE_9_HIGHEST_MV_NUMBER  0x01AE
#define POLL_MODULE_9_HIGH_CELL_MV       0x01AF
#define POLL_MODULE_9_HIGH_TEMP          0x01B1
#define POLL_MODULE_9_LOW_TEMP           0x01B3
#define POLL_MODULE_10_LOWEST_MV_NUMBER  0x01B4
#define POLL_MODULE_10_LOWEST_CELL_MV    0x01B5
#define POLL_MODULE_10_HIGHEST_MV_NUMBER 0x01B6
#define POLL_MODULE_10_HIGH_CELL_MV      0x01B7
#define POLL_MODULE_10_HIGH_TEMP         0x01B9
#define POLL_MODULE_10_LOW_TEMP          0x01BB

#define POLL_STATE_CAR_OFF       0
#define POLL_STATE_CAR_CHARGING  1
#define POLL_STATE_CAR_ON        2

static const OvmsPoller::poll_pid_t vehicle_atto3_polls[] = {
  //                                                                                    Off On  Chrg
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_BATTERY_VOLTAGE,               { 60, 1,  10}, 0, ISOTP_STD },
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_BATTERY_CURRENT,               { 60, 1,  10}, 0, ISOTP_STD },
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_BATTERY_SOC,                   { 60, 10, 10}, 0, ISOTP_STD },
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_LOWEST_TEMP_CELL,              { 60, 5,  10}, 0, ISOTP_STD },
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_HIGHEST_TEMP_CELL,             { 60, 5,  10}, 0, ISOTP_STD },
  { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_READDATA, POLL_FOR_BATTERY_PACK_AVG_TEMP,         { 60, 5,  10}, 0, ISOTP_STD },
  POLL_LIST_END
};


OvmsVehicleBydAtto3::OvmsVehicleBydAtto3()
  {
  ESP_LOGI(TAG, "BYD Atto3 vehicle module");

  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, vehicle_atto3_polls);
  PollSetState(POLL_STATE_CAR_OFF);
  }

OvmsVehicleBydAtto3::~OvmsVehicleBydAtto3()
  {
  ESP_LOGI(TAG, "Shutdown BYD Atto3 vehicle module");
  }

void OvmsVehicleBydAtto3::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch(p_frame->MsgID)
    {
    case 0x0055:
      switch(d[4])
        {
        case 0x80: // car off
          StdMetrics.ms_v_env_on->SetValue(false);
          break;
        case 0x82: // car on
          StdMetrics.ms_v_env_on->SetValue(true);
          break;
        }
      break;

    case 0x0151:
      StdMetrics.ms_v_pos_speed->SetValue(d[0], Kph);
      break;

    case 0x0218:
        StdMetrics.ms_v_env_handbrake->SetValue(!get_bit<4>(d[5]));
      break;

    case 0x0242:
      {
        uint8_t gear = d[5] & 0x0f;
        switch(gear)
          {
          case 0x01: // park
          case 0x03: // neutral
            StdMetrics.ms_v_env_gear->SetValue(0);
            break;
          case 0x02: // reverse
            StdMetrics.ms_v_env_gear->SetValue(-1);
            break;
          case 0x04: // drive
            StdMetrics.ms_v_env_gear->SetValue(1);
            break;
          }
      }
      break;

    case 0x0294:
      StdMetrics.ms_v_door_fl->SetValue(get_bit<3>(d[0]));
      StdMetrics.ms_v_door_fr->SetValue(get_bit<1>(d[0]));
      StdMetrics.ms_v_door_rl->SetValue(get_bit<5>(d[0]));
      StdMetrics.ms_v_door_rr->SetValue(get_bit<7>(d[0]));

      if(d[2] == 0x55)
        {
        StdMetrics.ms_v_door_trunk->SetValue(false);
        }
      else if(d[2] == 0x59)
        {
        StdMetrics.ms_v_door_trunk->SetValue(true);
        }
      break;

    case 0x0323:
      // the charge port door is managed here as the car doesn't report if it is open or not
      // and the apps assume you can't be charging if its not true
      switch(d[0])
        {
        case 0x00: // not charging
        case 0x02: // nearly ready to charge? only stays in this mode momentarily
          StdMetrics.ms_v_charge_power->SetValue(0);
          StdMetrics.ms_v_charge_inprogress->SetValue(false);
          StdMetrics.ms_v_door_chargeport->SetValue(false);
          StdMetrics.ms_v_charge_mode->SetValue("");
          StdMetrics.ms_v_charge_state->SetValue("stopped");
          StdMetrics.ms_v_charge_substate->SetValue("");

          StdMetrics.ms_v_charge_duration_full->Clear();
          StdMetrics.ms_v_charge_mode->SetValue("");
          StdMetrics.ms_v_charge_type->SetValue("");
          break;
        case 0x03: // charging
          {
          // these bytes both go to ff when somewhere less than an hour remaining
          // roughly relative to charge speed
          uint8_t charge_remaining_hours = d[4];
          if(charge_remaining_hours == 0xff) {
            charge_remaining_hours = 0;
          }
          uint8_t charge_remaining_mins = d[5];
          if(charge_remaining_mins == 0xff)
            {
            charge_remaining_mins = 0;
            }

          int16_t lsb = d[2];
          int16_t msb = (d[3] & 0x1f) * 256;

          bool ac_charging = (d[3] & 0x20) > 0;

          StdMetrics.ms_v_charge_power->SetValue((msb + lsb - 500) / 10.0);

          StdMetrics.ms_v_charge_inprogress->SetValue(true);
          StdMetrics.ms_v_door_chargeport->SetValue(true);
          StdMetrics.ms_v_charge_mode->SetValue("standard");
          StdMetrics.ms_v_charge_state->SetValue("charging");
          StdMetrics.ms_v_charge_duration_full->SetValue(charge_remaining_hours * 60 + charge_remaining_mins, Minutes);
          StdMetrics.ms_v_charge_mode->SetValue(ac_charging ? "standard" : "performance");
          StdMetrics.ms_v_charge_type->SetValue(ac_charging ? "type2" : "ccs");
          }
          break;
        case 0x07: // waiting for scheduled charge
          StdMetrics.ms_v_charge_power->SetValue(0);
          StdMetrics.ms_v_charge_inprogress->SetValue(false);
          StdMetrics.ms_v_door_chargeport->SetValue(true);
          StdMetrics.ms_v_charge_state->SetValue("timerwait");
          StdMetrics.ms_v_charge_substate->SetValue("timerwait");
          StdMetrics.ms_v_charge_mode->SetValue("");
          StdMetrics.ms_v_charge_duration_full->Clear();
          StdMetrics.ms_v_charge_mode->SetValue("");
          StdMetrics.ms_v_charge_type->SetValue("");
          break;
      }
      break;

    case 0x03D9:
      switch(d[0])
      {
        case 0x04:
        {
          uint16_t lsb = d[1];
          uint16_t msb = d[2] & 0x0f;

          StdMetrics.ms_v_bat_range_ideal->SetValue(lsb + (msb * 256), Kilometers);
          StdMetrics.ms_v_bat_range_est->SetValue(lsb + (msb * 256), Kilometers);
          break;
        }
        case 0x05:
        {
          uint32_t res;
          if(!get_bytes_uint_le<2>(d, 1, 3, res)) {
            ESP_LOGE(TAG, "Odometer: Bad Buffer");
          } else {
            StdMetrics.ms_v_pos_odometer->SetValue( (float)res / 10.0, Kilometers);
          }
        }
      }
      break;
    }
  }

void OvmsVehicleBydAtto3::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
    uint32_t res;
    switch(job.pid)
      {
      case POLL_FOR_BATTERY_VOLTAGE:
        if(!get_bytes_uint_le<2>(data, 0, 2, res))
          {
          ESP_LOGE(TAG, "Battery voltage: Bad Buffer");
          }
        else
          {
          StdMetrics.ms_v_bat_voltage->SetValue( (float)res, Volts);
          }
        break;

      case POLL_FOR_BATTERY_CURRENT:
        if(!get_bytes_uint_le<2>(data, 0, 2, res))
          {
          ESP_LOGE(TAG, "Battery current: Bad Buffer");
          }
        else
          {
          StdMetrics.ms_v_bat_current->SetValue( ((float)res - 5000) / 10.0, Amps);
          }
        break;

      case POLL_FOR_BATTERY_SOC:
        if(!get_bytes_uint_le<1>(data, 0, 1, res))
          {
          ESP_LOGE(TAG, "Battery nominal SOC: Bad Buffer");
          }
        else
          {
          StdMetrics.ms_v_bat_soc->SetValue( (float)res, Percentage);
          }
        break;

      case POLL_FOR_LOWEST_TEMP_CELL:
        if(!get_bytes_uint_le<1>(data, 0, 1, res))
          {
          ESP_LOGE(TAG, "Battery lowest temperature cell: Bad Buffer");
          }
          else
          {
          StdMetrics.ms_v_bat_pack_tmin->SetValue( (float)res - 40.0, Celcius);
          }
        break;

      case POLL_FOR_HIGHEST_TEMP_CELL:
        if(!get_bytes_uint_le<1>(data, 0, 1, res))
          {
          ESP_LOGE(TAG, "Battery highest temperature cell: Bad Buffer");
          }
        else
          {
          StdMetrics.ms_v_bat_pack_tmax->SetValue( (float)res - 40.0, Celcius);
          }
        break;

      case POLL_FOR_BATTERY_PACK_AVG_TEMP:
        if(!get_bytes_uint_le<1>(data, 0, 1, res))
          {
          ESP_LOGE(TAG, "Battery pack averge temp: Bad Buffer");
          }
        else
          {
          StdMetrics.ms_v_bat_pack_tavg->SetValue( (float)res - 40.0, Celcius);
          StdMetrics.ms_v_bat_temp->SetValue( (float)res - 40.0, Celcius);
          }
        break;
      }
  }

void OvmsVehicleBydAtto3::Ticker1(uint32_t ticker)
  {
    if(StdMetrics.ms_v_env_on->AsBool())
      {
      PollSetState(POLL_STATE_CAR_ON);
      }
    else if (StdMetrics.ms_v_charge_inprogress->AsBool())
      {
      PollSetState(POLL_STATE_CAR_CHARGING);
      }
    else
      {
      PollSetState(POLL_STATE_CAR_OFF);
      }

    float current = StdMetrics.ms_v_bat_current->AsFloat();
    float voltage = StdMetrics.ms_v_bat_voltage->AsFloat();
    float batPowerW = current * voltage;
    StdMetrics.ms_v_bat_power->SetValue(batPowerW, Watts);
  }

#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "vehicle_byd_atto3.h"
#include "ovms_webserver.h"

void OvmsVehicleBydAtto3::GetDashboardConfig(DashboardConfig& cfg)
  {
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 160, 5);
  speed_dash.AddBand("green", 0, 110);
  speed_dash.AddBand("yellow", 110, 130);
  speed_dash.AddBand("red", 130, 160);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(300, 465);
  voltage_dash.AddBand("red", 300, 310);
  voltage_dash.AddBand("yellow", 310, 440);
  voltage_dash.AddBand("green", 440, 465);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 12.5);
  soc_dash.AddBand("yellow", 12.5, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 400);
  eff_dash.AddBand("green", 0, 200);
  eff_dash.AddBand("yellow", 200, 300);
  eff_dash.AddBand("red", 300, 400);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-90, 200);
  power_dash.AddBand("violet", -90, 0);
  power_dash.AddBand("green", 0, 100);
  power_dash.AddBand("yellow", 100, 150);
  power_dash.AddBand("red", 150, 200);

  // Charger temperature:
  dash_gauge_t charget_dash("CHG ",Celcius);
  charget_dash.SetMinMax(20, 80);
  charget_dash.SetTick(20);
  charget_dash.AddBand("normal", 20, 65);
  charget_dash.AddBand("red", 65, 80);

  // Battery temperature:
  dash_gauge_t batteryt_dash("BAT ",Celcius);
  batteryt_dash.SetMinMax(-15, 65);
  batteryt_dash.SetTick(25);
  batteryt_dash.AddBand("red", -15, 0);
  batteryt_dash.AddBand("normal", 0, 50);
  batteryt_dash.AddBand("red", 50, 65);

  // Inverter temperature:
  dash_gauge_t invertert_dash("PEM ",Celcius);
  invertert_dash.SetMinMax(20, 80);
  invertert_dash.SetTick(20);
  invertert_dash.AddBand("normal", 20, 70);
  invertert_dash.AddBand("red", 70, 80);

  // Motor temperature:
  dash_gauge_t motort_dash("MOT ",Celcius);
  motort_dash.SetMinMax(50, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 50, 110);
  motort_dash.AddBand("red", 110, 125);

  std::ostringstream str;
  str << "yAxis: ["
      << speed_dash << "," // Speed
      << voltage_dash << "," // Voltage
      << soc_dash << "," // SOC
      << eff_dash << "," // Efficiency
      << power_dash << "," // Power
      << charget_dash << "," // Charger temperature
      << batteryt_dash << "," // Battery temperature
      << invertert_dash << "," // Inverter temperature
      << motort_dash // Motor temperature
      << "]";
  cfg.gaugeset1 = str.str();
  }

#endif //CONFIG_OVMS_COMP_WEBSERVER

class OvmsVehicleBydAtto3Init
  {
  public: OvmsVehicleBydAtto3Init();
  } MyOvmsVehicleBydAtto3Init  __attribute__ ((init_priority (9000)));

OvmsVehicleBydAtto3Init::OvmsVehicleBydAtto3Init()
  {
  ESP_LOGI(TAG, "Registering Vehicle: BYD Atto3 (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleBydAtto3>("ATTO3","BYD Atto 3");
  }


