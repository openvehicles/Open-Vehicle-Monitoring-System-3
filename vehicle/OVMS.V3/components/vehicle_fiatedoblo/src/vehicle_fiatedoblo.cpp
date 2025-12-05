/*
;    Project:       Open Vehicle Monitor System
;    Date:          30th of November 2025
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2024-2025  Raphael Mack
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
static const char *TAG = "v-fiatedoblo";

#include <stdio.h>
#include "vehicle_fiatedoblo.h"
#include "ovms_webserver.h"

static const OvmsPoller::poll_pid_t vehicle_ftdo_polls[]
=
  {
    // Pollstate 0 - car is off
    // Pollstate 1 - car is on (IGN on, otherwise no DIGA possible)
    // Pollstate 2 - car is charging
    // Pollstate 3 - ping : car is off, not charging and something triggers a wake
    // OBD2 broadcast                                    Off   ON CHRG PING (cycle time in sec)
    { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIVEHICLE,   0x02, {  0,  10, 15, 25 }, 0, ISOTP_STD }, // VIN

    // Battery details
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd815, {  0,  5,   1, 999 }, 0, ISOTP_STD }, // Battery voltage
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd816, {  0,  5,   1, 999 }, 0, ISOTP_STD }, // Battery current
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd86f, {  0,  30,   1, 999 }, 0, ISOTP_STD }, // min cell voltage (mV)
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd870, {  0,  30,   1, 999 }, 0, ISOTP_STD }, // max cell voltage (mV)
    // { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd410, {  0,  30,   1, 999 }, 0, ISOTP_STD }, // SOC calibrated (/512 -> %) - not used, as the CAN frame 0x3a8 is also transmitting the value
    //                                                  0xd440 are single cell details, we don't care about them for now...
    // { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd810, {  0,  30,   1, 999 }, 0, ISOTP_STD }, // SOC, not used, using calibrated one
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd865, {  0,  30,   60, 999 }, 0, ISOTP_STD }, // kWh available
    { 0x6b4, 0x694, VEHICLE_POLL_TYPE_READDATA,      0xd860, {  0, 600,   60, 999 }, 0, ISOTP_STD }, // SOH is (3 byte value - 65536) / 16
    
    // VCU Temperature details
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd434, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // Ambient temp
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd8ef, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // Battery temp
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd8cd, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // drive motor stator temperature?
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd8ce, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // DC-DC converter temperature or Drive motor coolant inlet temperature?
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd8e1, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // on-board charger temperature?
    { 0x6a2, 0x682, VEHICLE_POLL_TYPE_READDATA, 0xd8f9, {  0,  30,   30,   0 }, 0, ISOTP_STD }, // Traction cicuit coolant temperature * 10

    // { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE,   0x01, {  0,  10, 1, 999 }, 0, ISOTP_STD }, // VIN length
    // { 0x7df, 0, VEHICLE_POLL_TYPE_OBDIIVEHICLE,   0x02, {  0,  10, 1, 999 }, 0, ISOTP_STD }, // VIN maybe like this or with other POLL type data received... or with poll below

    POLL_LIST_END
  };

/**
 * Ticker60: Called every minute
 */
void OvmsVehicleFiatEDoblo::Ticker60(uint32_t ticker)
{
  if((m_lastCanFrameTime + 60) < monotonictime)
    {
      StandardMetrics.ms_v_env_awake->SetValue(false);
      ESP_LOGD(TAG, "No CAN frame received since %d sec, so the car is off (%d %d).", ((int32_t)monotonictime - (int32_t)m_lastCanFrameTime), monotonictime, m_lastCanFrameTime);

      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_pos_speed->SetValue(0);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_env_handbrake->SetValue(true);

      PollState_Off();
    }
  else if((m_last305FrameTime + 20) < monotonictime)
    {
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_pos_speed->SetValue(0);
    }
}

OvmsVehicleFiatEDoblo::OvmsVehicleFiatEDoblo()
{
  ESP_LOGI(TAG, "Fiat e-Doblo vehicle module");
  StdMetrics.ms_v_charge_inprogress->SetValue(false);
  StdMetrics.ms_v_env_on->SetValue(false);

  // we use the BSI-CAN (on OBDII connector) as CAN1
  // the DIAG CAN - the second CAN on the OBDII connector doesn't look helpful (but proove me wrong, it's just a claim!)
  // maybe someone wants to add the Comfort CAN later as a second CAN
  RegisterCanBus(1, CAN_MODE_ACTIVE, CAN_SPEED_500KBPS);
  PollSetPidList(m_can1, vehicle_ftdo_polls);
  PollState_Charging();
}

OvmsVehicleFiatEDoblo::~OvmsVehicleFiatEDoblo()
{
  ESP_LOGI(TAG, "Shutdown Fiat e-Doblo vehicle module");
}

class OvmsVehicleFiatEDobloInit
{
  public: OvmsVehicleFiatEDobloInit();
} MyOvmsVehicleFiatEDobloInit  __attribute__ ((init_priority (9000)));

OvmsVehicleFiatEDobloInit::OvmsVehicleFiatEDobloInit()
{
  ESP_LOGI(TAG, "Registering Vehicle: Fiat e-Doblo (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleFiatEDoblo>("FTDO", "Fiat e-Doblo");
}

// TODO check if frame 0x5c3 and 0x563 are helpful in any way

// handle incoming data on StdOBD CAN
// 412 is sent also while charging (start) and with DOOR status change
void OvmsVehicleFiatEDoblo::IncomingFrameCan1(CAN_frame_t* p_frame)
{
  uint8_t *d = p_frame->data.u8;

  m_lastCanFrameTime = monotonictime;
  if(IsPollState_Off()){
    ESP_LOGI(TAG, "waking up by CAN frame %x", p_frame->MsgID);
    PollState_Charging();
  }
  
  switch (p_frame->MsgID){
  case 0x305: // steering wheel angle in byte 0 and 1 (x10) maybe not, as it's in 2eb
    {
      /* this frame seems to be transmitted only while ignition is on... */
      PollState_Running();
      StandardMetrics.ms_v_env_on->SetValue(true);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      m_last305FrameTime = monotonictime;
      break;
    }
  case 0x30d: // wheel rotation speed
    {
      break;
    }
  case 0x38d: // vehicle speed (bytes 0 and 1 x100kmh)
    {
      int speed = ((((uint32_t)d[0] << 8) + d[1]) / 100.0);
      StandardMetrics.ms_v_pos_speed->SetValue(speed);
      break;
    }
  case 0x3a8:
    {
      float newSoC = (float)d[3] + ((float)d[4] / 256.0);
      // on startup sometime we get bad frame with: 1R11 3A8 06 06 fe 00 00 00
      // d[1] == 0x06 seems to indicate an invalid state, all plausible frames we received had 0x08, 0x0a, 0x0c, 0x16 or 0x14
      if(d[1] != 0x06)
        {
          
          if(newSoC > StandardMetrics.ms_v_bat_soc->AsFloat())
            {
              PollState_Charging(); // until we find the signal with the charging status we use the indication of increasing SoC, even though it might be wrong during recuperation
              StandardMetrics.ms_v_charge_inprogress->SetValue(true);
              ESP_LOGD(TAG, "charging started SoC: %f (0x%2x 0x%2x 0x%2x 0x%2x  0x%2x 0x%2x 0x%2x 0x%2x)", newSoC, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);            
            }
          if(newSoC < 1)
            {
              ESP_LOGW(TAG, "received small SoC: %f (0x%2x 0x%2x 0x%2x 0x%2x  0x%2x 0x%2x 0x%2x 0x%2x)", newSoC, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
            }
          else
            {
              StandardMetrics.ms_v_bat_soc->SetValue(newSoC);
            }
        }
      else if(d[1] == 0x06)
        {
          // this happens regularly and doesn't contain a resonable soc
        }
      else
        {
          ESP_LOGW(TAG, "received strange SoC: %f (0x%2x 0x%2x 0x%2x 0x%2x  0x%2x 0x%2x 0x%2x 0x%2x)", newSoC, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7]);
      }
    }
    break;
  case 0x552:
    {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[4] << 16) + ((uint32_t)d[5] << 8) + d[6]));
    }
    break;
  case 0x2eb:
    {
      // steering wheel angle in bytes 0 and 1: negative: right, positive: left
      // angle = ((uint32_t)d[0] << 8) + d[1]) / 10.0);
    }
    break;
  case 0x3ad:
    {
      StandardMetrics.ms_v_env_awake->SetValue(true);
      /*
        if(d[6] & 0x40) Switch handbrake engage pressed
        if(d[6] & 0x10) Switch handbrake released pressed

        if(d[4] & 0x01) handbrake engaged
        if(d[4] & 0x02) motor of handbrake is running (state will change soon)
        d[7] is CNT/CRC
      */
      StandardMetrics.ms_v_env_handbrake->SetValue((d[4] & 0x01));
    }
    break;
  case 0x4a2:
    {
      // not fully correct, as bit 6 in byte 1 reflects the connection status (0x40 for unplugged, 0x00 for type2 connected, but maybe close enough)
      if(d[1] & 0x40)
        {
          StandardMetrics.ms_v_door_chargeport->SetValue(false);
        }
      else
        {
          StandardMetrics.ms_v_door_chargeport->SetValue(true);
        }                                                         
    }
    break;
  case 0x412:
    {
      // door status: d[6]: 0x08 driver door
      //                    0x80 trunk door
      //                    0x40 back door right
      //                    0x20 back door left
      //                    0x10 passenger door
      //                    0x0c ? windows? 

      StandardMetrics.ms_v_door_fl->SetValue(d[6] & 0x08);
      StandardMetrics.ms_v_door_fr->SetValue(d[6] & 0x10);
      StandardMetrics.ms_v_door_rl->SetValue(d[6] & 0x20);
      StandardMetrics.ms_v_door_rr->SetValue(d[6] & 0x40);
      StandardMetrics.ms_v_door_trunk->SetValue(d[6] & 0x80);

      StandardMetrics.ms_v_env_footbrake->SetValue(((d[0] & 0x20) > 0) ? 100.0 : 0.0); // we have in this bit only a binary footbrake status, no details.

      /* TODO:
         StandardMetrics.ms_v_charge_pilot->SetValue(d[1] & 0x08);
         StandardMetrics.ms_v_env_locked->SetValue(d[2] & 0x08);
         StandardMetrics.ms_v_env_valet->SetValue(d[2] & 0x10);
         StandardMetrics.ms_v_env_headlights->SetValue(d[2] & 0x20);
         StandardMetrics.ms_v_door_hood->SetValue(d[2] & 0x40);
      
         StandardMetrics.ms_v_env_aux12v->SetValue(m_aux12v);
         StandardMetrics.ms_v_env_charging12v->SetValue(d[3] & 0x01);
         StandardMetrics.ms_v_env_cooling->SetValue(d[3] & 0x02);
         StandardMetrics.ms_v_env_alarm->SetValue(d[4] & 0x02);
      */
    }
    break;
  case 0x5c3:
    {
      // Byte 2 could be related to the charge current in bits 0011 1100
      break;
    }
  }
}

void OvmsVehicleFiatEDoblo::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
{
  switch (job.moduleid_rec) {
  case 0x682:
    // temperatures and other data from VCU
    IncomingVCUPoll(job, data, length);
    break;
  case 0x694:
    // battery details
    IncomingBatteryPoll(job, data, length);
    break;
  case 0x7e8:
    // more details
    IncomingVINPoll(job, data, length);
    break;
  default:
    ESP_LOGI(TAG, "unexpected poll reply on ID 0x%4x (pid = 0x%4x, len = %d)", job.moduleid_rec, job.pid, length);
    ESP_LOGI(TAG, "unexpected poll reply 0x%4x (pid = 0x%x, len = %d): 0x%2x %c 0x%2x %c 0x%2x %c 0x%2x %c  0x%2x %c 0x%2x %c 0x%2x %c 0x%2x %c",
             job.moduleid_rec, job.pid, length,
             data[0], (char)data[0],
             data[1], (char)data[1],
             data[2], (char)data[2],
             data[3], (char)data[3],
             data[4], (char)data[4],
             data[5], (char)data[5],
             data[6], (char)data[6],
             data[7], (char)data[7]);
  }
  /*      case 0x1f:  // runtime since engine start
        ESP_LOGI(TAG, "runtime since engine start %d", value2);
        StandardMetrics.ms_v_env_drivetime->SetValue(value2);
        break;
        
  */
}

/**
 * Handle incoming messages from VIN-poll
 */
void OvmsVehicleFiatEDoblo::IncomingVINPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
{
  if(job.pid == 0x02) {
    if(job.mlremain > 13) {
      m_vin.clear();
    }
    m_vin.append((char*)data, length);
    if(job.mlremain == 0) {
      ESP_LOGV(TAG, "received VIN: %s", m_vin.c_str());
      StdMetrics.ms_v_vin->SetValue(m_vin);
    }
  }else{
    ESP_LOGI(TAG, "unexpected poll reply 0x%4x (pid = 0x%x, len = %d): 0x%2x %c 0x%2x %c 0x%2x %c 0x%2x %c  0x%2x %c 0x%2x %c 0x%2x %c 0x%2x %c",
             job.moduleid_rec, job.pid, length,
             data[0], (char)data[0],
             data[1], (char)data[1],
             data[2], (char)data[2],
             data[3], (char)data[3],
             data[4], (char)data[4],
             data[5], (char)data[5],
             data[6], (char)data[6],
             data[7], (char)data[7]);
  }
}

/**
 * Handle incoming messages from Battery-poll
 *
 */
void OvmsVehicleFiatEDoblo::IncomingBatteryPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
{
  int value2 = (int)((uint16_t)data[0] << 8) + data[1];
  float kwh;
  float soh;
  
  switch (job.pid) {
  case 0xd815: // bat voltage
    StandardMetrics.ms_v_bat_voltage->SetValue((float)value2 / 16.0);
    break;
  case 0xd816:  // battery current
    {
      /*
        uint32_t value = ((uint32_t)data[2] << 8) + data[3];
        TODO: check how to convert into current (or power)
        int32_t v;
        float fin;
        value &= 0x7ff;
        value = value << 9;
        v = (int32_t) value;
      
        fin = v / 64.0;
        //see https://www.goingelectric.de/forum/viewtopic.php?p=1903069&sid=2cadc1e2d2a3312a436b2dfed52ce479#p1903069
    
        StandardMetrics.ms_v_bat_current->SetValue(fin);
        ESP_LOGI(TAG, "received battery current: %d %d  %f  %d, %x %x %x %x", value, v, fin, (data[0] << 24) + (data[1] << 16) + (data[2] << 8) + data[3], data[0], data[1], data[2], data[3]);
      */
      break;
    }
  case 0xd860:
    soh = (((uint32_t)data[0] << 16) + ((uint32_t)data[1] << 8) + (data[2]));
    soh = (soh - 65536) / 16.0;
    StandardMetrics.ms_v_bat_soh->SetValue(soh);
    break;

  case 0xd86f: // min cell voltage ok
    StandardMetrics.ms_v_bat_pack_vmin->SetValue(value2 / 1000.0);
    break;
    
  case 0xd870: // max cell voltage ok
    StandardMetrics.ms_v_bat_pack_vmax->SetValue(value2 / 1000.0);      
    break;

  case 0xd865: // kWh - maybe wrong conversion
    kwh = (float)value2 / 50.0; //64.0;
    ESP_LOGD(TAG, "received kWh: %f (maybe %f or %f)", kwh, (float)value2 / 50.0 , (float)value2 / 64.0);
    StandardMetrics.ms_v_bat_capacity->SetValue(kwh);
    break;
    
  default:
    ESP_LOGD(TAG, "unexpected battery poll reply on ID 0x%4x (pid = 0x%4x, len = %d)", job.moduleid_rec, job.pid, length);
    ESP_LOGD(TAG, "poll reply 0x%4x (pid = 0x%x, len = %d): 0x%2x 0x%2x 0x%2x 0x%2x  0x%2x 0x%2x 0x%2x 0x%2x", job.moduleid_rec, job.pid, length, data[0], data[1], data[2], data[3], data[4], data[5], data[6], data[7]);
  }
}

/**
 * Handle incoming messages from VCU-poll
 */
void OvmsVehicleFiatEDoblo::IncomingVCUPoll(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
{
  switch (job.pid) {
  case 0xd434:  // Ambient temperature
    StandardMetrics.ms_v_env_temp->SetValue((int8_t)data[0]);
    break;
  case 0xd8ef:  // Battery temperature
    StandardMetrics.ms_v_bat_temp->SetValue((int8_t)data[0]);
    break;
  case 0xd8e1:  // on-board charger temperature ??
    StandardMetrics.ms_v_charge_temp->SetValue((int8_t)data[0]);
    break;
  case 0xd402:  // speed
    //    StandardMetrics.ms_v_pos_speed->SetValue((uint8_t)data[0]);
    // set via CAN frame
    break;
  case 0xd8cd:  // drive motor temp?
    StandardMetrics.ms_v_mot_temp->SetValue((((uint32_t)data[0] << 8) + (uint32_t)data[1]) / 2); // just a guess, TODO: check it!
    break;
    
  default:
    ESP_LOGI(TAG, "unexpected VCU poll reply on ID 0x%4x (pid = 0x%0x, len = %d) %x %x  %i", job.moduleid_rec, job.pid, length, data[0], data[1], (((uint32_t)data[1] << 8) + data[2]));
    break;
  }
}

#ifdef CONFIG_OVMS_COMP_WEBSERVER
/**
 * GetDashboardConfig:
 * see https://api.highcharts.com/highcharts/yAxis for details on options
 */
void OvmsVehicleFiatEDoblo::GetDashboardConfig(DashboardConfig& cfg)
{
  // Speed:
  dash_gauge_t speed_dash(NULL,Kph);
  speed_dash.SetMinMax(0, 150, 5); // max speed is 135 according documents
  speed_dash.AddBand("green", 0, 110);
  speed_dash.AddBand("yellow", 110, 135);
  speed_dash.AddBand("red", 135, 150);

  // Voltage:
  dash_gauge_t voltage_dash(NULL,Volts);
  voltage_dash.SetMinMax(310, 450);
  voltage_dash.AddBand("red", 310, 325);
  voltage_dash.AddBand("yellow", 325, 340);
  voltage_dash.AddBand("green", 340, 450);

  // SOC:
  dash_gauge_t soc_dash("SOC ",Percentage);
  soc_dash.SetMinMax(0, 100);
  soc_dash.AddBand("red", 0, 15);
  soc_dash.AddBand("yellow", 15, 25);
  soc_dash.AddBand("green", 25, 100);

  // Efficiency:
  dash_gauge_t eff_dash(NULL,WattHoursPK);
  eff_dash.SetMinMax(0, 400); // TODO: check
  eff_dash.AddBand("green", 0, 200);
  eff_dash.AddBand("yellow", 200, 300);
  eff_dash.AddBand("red", 300, 400);

  // Power:
  dash_gauge_t power_dash(NULL,kW);
  power_dash.SetMinMax(-50, 100);
  power_dash.AddBand("violet", -50, 0);
  power_dash.AddBand("green", 0, 50);
  power_dash.AddBand("yellow", 50, 80);
  power_dash.AddBand("red", 80, 100);

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
  motort_dash.SetMinMax(0, 125);
  motort_dash.SetTick(25);
  motort_dash.AddBand("normal", 10, 80);
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
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
