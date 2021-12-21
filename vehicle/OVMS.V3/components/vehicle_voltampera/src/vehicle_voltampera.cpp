/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
;    (C) 2019       Marko Juhanne
;    (C) 2021       Alexander Kiiyashko
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
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include "ovms_config.h"
#include "vehicle_voltampera.h"
#include "canutils.h"

#define VA_CANDATA_TIMEOUT 10

#define VA_CHARGING_12V_THRESHOLD (float)12.7


#define VA_BCM 0x241
#define VA_TESTER_PRESENT_TIMEOUT 30*60 //  seconds

#define VA_POLLING_START_DELAY 2 // seconds
#define VA_POLLING_NORMAL_THROTTLING 1 
#define VA_POLLING_HIGH_THROTTLING 20 


// Use states:
// 0 = bus is idle, car sleeping
// 1 = car is on and ready to Drive
// 2 = car wakeup or powertrain off, one request with high polling speed. After swith to state 1.

static const OvmsVehicle::poll_pid_t va_polls[]
  =
  {
    { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x002F, {  0, 600,  0 },   0, ISOTP_STD }, // Fuel Level
    { 0x7e2, 0x7ea, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1940, {  0, 60,   60 },  0, ISOTP_STD }, // Transmission temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369, {  0, 10,   10 },  0, ISOTP_STD }, // On-board charger current
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4368, {  0, 10,   10 },  0, ISOTP_STD }, // On-board charger voltage
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x434f, {  0, 10,   10 },  0, ISOTP_STD }, // High-voltage Battery temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1c43, {  0, 10,   10 },  0, ISOTP_STD }, // PEM temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8334, {  0, 10,   10 },  0, ISOTP_STD }, // SOC
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x41a3, {  0, 600,  0 },   0, ISOTP_STD }, // High-voltage Battery capacity
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40d7, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 1 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40d9, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 2 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40db, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 3 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40dd, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 4 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40df, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 5 temperature
    { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x40e1, {  0, 0,    60 },  0, ISOTP_STD }, // High-voltage Battery Section 6 temperature
    POLL_LIST_END
  };
// These are not polled anymore but instead received passively
// { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x000d, {  0, 10,  0 } }, // Vehicle speed
// { 0x7e1, 0x7e9, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2487, {  0, 100,  100 }, 0, ISOTP_STD }, // Distance Traveled on Battery Energy This Drive Cycle
// { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801f, {  0, 10,  0 } }, // Outside temperature (filtered)
// { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801e, {  0, 10,  0 } }, // Outside temperature (raw)


OvmsVehicleVoltAmpera::OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Volt/Ampera vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  m_type[0] = 'V';
  m_type[1] = 'A';
  m_type[2] = 0;
  m_type[3] = 0;
  m_type[4] = 0;
  m_type[5] = 0;
  m_modelyear = 0;
  m_charge_timer = 0;
  m_charge_wm = 0;
  m_candata_timer = VA_CANDATA_TIMEOUT;
  m_tx_retry_counter = 0;
  m_tester_present_timer = 0;
  m_controlled_lights = 0;
  m_startPolling_timer = 0;
  m_pPollingList = NULL;

  BmsSetCellArrangementVoltage(96, 16);
  BmsSetCellArrangementTemperature(6, 1);
  BmsSetCellLimitsVoltage(2,5);
  BmsSetCellLimitsTemperature(-35,90);
  BmsSetCellDefaultThresholdsVoltage(0.020, 0.030);
  BmsSetCellDefaultThresholdsTemperature(4.0, 8.0);

  // VA metrics
  mt_charging_limits = MyMetrics.InitVector<int>("xva.v.b.charging_limits", SM_STALE_HIGH, "0");
  mt_coolant_temp = new OvmsMetricInt("xva.v.e.coolant_temp", SM_STALE_MIN, Celcius);
  mt_coolant_heater_pwr = new OvmsMetricFloat("xva.v.e.coolant_heater_pwr", SM_STALE_MIN, kWh);
  mt_fuel_level = new OvmsMetricInt("xva.v.e.fuel", SM_STALE_HIGH, Percentage, true);
  mt_v_trip_ev = new OvmsMetricFloat("xva.v.p.trip.ev", SM_STALE_HIGH, Kilometers);

  // Config parameters
  MyConfig.RegisterParam("xva", "Volt/Ampera", true, true);
  ConfigChanged(NULL);

  ClimateControlInit();

  // Register CAN bus and polling requests
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // powertrain bus
  //RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // chassis bus 
  
  //Add cell requests to polling list
  const int nCellListSize = 96; 
  const int va_pollsSize = sizeof(va_polls)/sizeof(va_polls[0]);
  m_pPollingList = new poll_pid_t[va_pollsSize + nCellListSize];
  uint16_t pid = 0x4181;
  
  int iPoll=0;
  for(iPoll=0; iPoll<nCellListSize; iPoll++)
    {
    if(iPoll==31)
      pid+=96;

    //Updating of the cells takes significant time. To prevent fake deviation, start updates in state 2 only (without HV battery load)
    m_pPollingList[iPoll] = { 0x7e7, 0x7ef, VEHICLE_POLL_TYPE_OBDIIEXTENDED, pid, {0, 0, 10}, 0, ISOTP_STD };
    pid++;
    }

  for(int i=0; i<va_pollsSize; i++)
    {
      m_pPollingList[iPoll] = va_polls[i];
      iPoll++;
    }

  PollSetPidList(m_can1, m_pPollingList);
  PollSetState(0);
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGI(TAG, "Volt/Ampera vehicle module: Register SWCAN using external CAN module");
  RegisterCanBus(4,CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);
  p_swcan = m_can4;
  p_swcan_if = (swcan*)MyPcpApp.FindDeviceByName("can4");
#else
  ESP_LOGI(TAG, "Volt/Ampera vehicle module: Register 2nd MCP2515 as SWCAN");
  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);  // single wire can
  p_swcan = m_can3;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

  // register tx callback
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleVoltAmpera::TxCallback, this, _1, _2), true);

  wakeup_frame_sent = std::bind(&OvmsVehicleVoltAmpera::CommandWakeupComplete, this, _1, _2);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebInit();
#endif  
  }

OvmsVehicleVoltAmpera::~OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Shutdown Volt/Ampera vehicle module");
  MyCan.DeregisterCallback(TAG);
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  WebCleanup();
#endif
  PollSetPidList(m_can1, NULL); 
  delete m_pPollingList; 

  MyMetrics.DeregisterMetric(mt_coolant_temp);
  MyMetrics.DeregisterMetric(mt_coolant_heater_pwr);
  MyMetrics.DeregisterMetric(mt_fuel_level);
  MyMetrics.DeregisterMetric(mt_v_trip_ev);
  }

/**
 * ConfigChanged: reload single/all configuration variables
 */
void OvmsVehicleVoltAmpera::ConfigChanged(OvmsConfigParam* param)
  {
  if (param && param->GetName() != "xva")
    return;

  ESP_LOGD(TAG, "Volt/Ampera reload configuration");

  m_range_rated_km = MyConfig.GetParamValueInt("xva", "range.km", 0);
  m_extended_wakeup = MyConfig.GetParamValueBool("xva", "extended_wakeup", false);
  m_notify_metrics = MyConfig.GetParamValueBool("xva", "notify_va_metrics", false);
  }

void OvmsVehicleVoltAmpera::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle:  Volt Ampera (%s %d)\n", m_type, m_modelyear+2000);
  writer->printf("VIN:      %s\n",m_vin);
  writer->puts("");
  writer->printf("Ranges:   %dkm (rated) %dkm (estimated)\n",
    m_range_rated_km, StandardMetrics.ms_v_bat_range_est->AsInt());
  writer->printf("Charge:   Timer %d (%d wm)\n", m_charge_timer, m_charge_wm);
  writer->printf("Can Data: Timer %d (poll state %d)\n",m_candata_timer,m_poll_state);
  ClimateControlPrintStatus(verbosity,writer);
  }


void OvmsVehicleVoltAmpera::TxCallback(const CAN_frame_t* p_frame, bool success)
  {
  const uint8_t *d = p_frame->data.u8;
  if (p_frame->MsgID == 0x7e4)
    {
    if (!success)
      ESP_LOGE(TAG, "TxCallback. Error sending poll request. MsgId: 0x%x", p_frame->MsgID);
    return;
    }

  if (success) 
    {
    ESP_LOGD(TAG,"TxCallback. Success. Frame %08x: [%02x %02x %02x %02x %02x %02x %02x %02x]", 
      p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
    } 
  else
    {
    ESP_LOGE(TAG,"TxCallback. Failed! Frame %08x: [%02x %02x %02x %02x %02x %02x %02x %02x]", 
      p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
    }
  }


void OvmsVehicleVoltAmpera::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  int k;

  m_candata_timer = VA_CANDATA_TIMEOUT;

  ESP_LOGV(TAG,"CAN1 message received: %08x: [%02x %02x %02x %02x %02x %02x %02x %02x]", 
    p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  if ((p_frame->MsgID & 0xff8) == 0x7e8)
    return; // Ignore poll responses
    
  // Activity on the bus, so resume polling
  if (m_poll_state == 0 && m_startPolling_timer == 0)
    {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    m_startPolling_timer = VA_POLLING_START_DELAY;
    }

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    case 0x0c9: 
      {
      StandardMetrics.ms_v_env_on->SetValue((d[0] & 0xC0) != 0);  // true for Ign ON, remote preheating
      StandardMetrics.ms_v_mot_rpm->SetValue( GET16(p_frame, 1) >> 2 );
      break;
      }

    /*
    case 0x1ef: {
      // Which is better source of engine RPM?
      StandardMetrics.ms_v_mot_rpm->SetValue( GET16(p_frame, 2) );
      break;
      }
    */

    /*
    // Which is more reliable indicator of Engine On?
    case 0x0bc: {
      bool powertrain_enabled = (d[0] & 0x80) != 0);
      if (StandardMetrics.ms_v_env_on->GetValueAsBool() != powertrain_enabled)
        {
        ESP_LOGI(TAG,"Powertrain %s",powertrain_enabled ? "enabled", "disabled");
        StandardMetrics.ms_v_env_on->SetValue(powertrain_enabled);
        }
      break;
      }
    */

    case 0x120: 
      {
      StandardMetrics.ms_v_pos_odometer->SetValue(GET32(p_frame, 0) / 64);
      break;
      } 

    case 0x3e9:
      {
      StandardMetrics.ms_v_pos_speed->SetValue(GET16(p_frame, 0)/100, Mph);
      break;
      } 

    case 0x1a1:
      {
      StandardMetrics.ms_v_env_throttle->SetValue( d[7]*100/256 );
      break;
      } 

    case 0xd1:
      {
      StandardMetrics.ms_v_env_footbrake->SetValue( d[4] );
      break;
      } 

    case 0x4c1: 
      {
      // Outside temperature (filtered) (aka ambient temperature)
      StandardMetrics.ms_v_env_temp->SetValue((int)d[4]/2 - 0x28);
      // Coolant temp
      mt_coolant_temp->SetValue((int)d[2] - 0x28);
      break;
      }

    // VIN digits 10-17
    case 0x4e1:
      {
      for (k=0;k<8;k++)
        m_vin[k+9] = d[k];
      break;
      }

    // VIN digits 2-9
    case 0x514:
      {
      for (k=0;k<8;k++)
        m_vin[k+1] = d[k];
      if (m_vin[9] != 0)
        {
        m_vin[0] = '1';
        m_vin[17] = 0;
        if (m_vin[2] == '1')
          m_type[2] = 'V';
        else if (m_vin[2] == '0')
          m_type[2] = 'A';
        else
          m_type[2] = m_vin[2];
        m_modelyear = (m_vin[9]-'A')+10;
        m_type[3] = (m_modelyear / 10) + '0';
        m_type[4] = (m_modelyear % 10) + '0';
        StandardMetrics.ms_v_vin->SetValue(m_vin);
        StandardMetrics.ms_v_type->SetValue(m_type);
        if (m_range_rated_km == 0)
          {
          switch (m_modelyear)
            {
            case 11:
            case 12:
              m_range_rated_km = 56;
              break;
            case 13:
            case 14:
            case 15:
              m_range_rated_km = 61;
              break;
            default:
              m_range_rated_km = 85;
              break;
            }
          }
        }
      if (mt_charging_limits->AsString()=="0")
        {
        // Set defaults
        if (m_type[2]=='V') 
          {
          mt_charging_limits->SetValue("12,8");
          }
        else if (m_type[2]=='A') 
          {
          mt_charging_limits->SetValue("10,6");
          }
        }
      break;
      }

    case 0x1f5: // Byte 4: Shift Position PRNDL 1=Park, 2=Reverse, 3=Neutral, 4=Drive, 5=Low
      {
      if (d[3]==1) 
        StandardMetrics.ms_v_env_gear->SetValue(-2); // Park
      else if (d[3]==2)
        StandardMetrics.ms_v_env_gear->SetValue(-1); // Reverse
      else if (d[3]==3)
        StandardMetrics.ms_v_env_gear->SetValue(0); // Neutral
      else if (d[3]==4)
        StandardMetrics.ms_v_env_gear->SetValue(1); // Drive
      else if (d[3]==5)
        StandardMetrics.ms_v_env_gear->SetValue(2); // L
      break;
      }

    default:
      break;
    }
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  //uint8_t *d = p_frame->data.u8;
  //ESP_LOGI(TAG,"CAN2 message received: %08x: %02x %02x %02x %02x %02x %02x %02x %02x", 
  //  p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  IncomingFrameCan4(p_frame);  // assume third can bus messages coming from SWCAN bus
  }

void OvmsVehicleVoltAmpera::IncomingFrameCan4(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  m_candata_timer = VA_CANDATA_TIMEOUT;

  ESP_LOGV(TAG,"SW CAN message received: %08x: [%02x %02x %02x %02x %02x %02x %02x %02x]", 
    p_frame->MsgID, d[0], d[1], d[2], d[3], d[4], d[5], d[6], d[7] );

  // Activity on the bus, so resume polling
  if (m_poll_state == 0 && m_startPolling_timer == 0)
    {
    ESP_LOGI(TAG,"Car has woken (SWCAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    m_startPolling_timer = VA_POLLING_START_DELAY;
    }

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    // Door lock command
    case 0x0C414040: 
      {
        const char *lock_src;
        switch(d[3]){
          case 0x01:
            lock_src = "panel"; //with console panel button
            break;
          case 0x05:
            lock_src = "fob";
            break;
          case 0x06:
            lock_src = "keyless"; 
            break;
          case 0x07:
            lock_src = "OnStar"; 
            break;
          case 0x0A:
            lock_src = "auto"; //door is open when lock command activated, etc
            break;
          default:
            lock_src = "unknown";
            break;
        }

        // d[1]=0x05/0x04; for Volt MY14 d[1]=0x01/0x00
        if(d[1] & 1) {
            ESP_LOGI(TAG,"Car locked via %s(%u)", lock_src, d[3]);
            StandardMetrics.ms_v_env_locked->SetValue(1); 
            break;
        }
        else{
          ESP_LOGI(TAG,"Car unlocked via %s(%u)", lock_src, d[3]);
          StandardMetrics.ms_v_env_locked->SetValue(0); 
          break;
        }
      break;
      }

    /*
    // Read gear data from high speed CAN bus      
    case 0x102CA040: 
      {
      int ngear = d[6] & 7;
      if (ngear > 0 && ngear < 6) {
        StandardMetrics.ms_v_env_gear->SetValue(ngear - 1);
        }
      break;
      }
    */

    // High Volt Time Based Chrg (Current and available charging levels)
    case 0x1086C0CB:
      {
      int set_level = (GET8(p_frame, 3) >> 4) & 0x07; 

      int levels[4];
      levels[0] = (GET16(p_frame, 3) >> 7) & 0x1f; // normal (maximum) charging level
      levels[1] = (GET8(p_frame, 4) >> 2) & 0x1f;
      levels[2] = (GET16(p_frame, 4) >> 5) & 0x1f; 
      levels[3] = (GET8(p_frame, 5) >> 0) & 0x1f;
      int i=1;
      while ((i<4) && (levels[i]>0) )
        i++;

      mt_charging_limits->SetElemValues(0,i,levels);

      if (set_level>=i)
        {
        ESP_LOGE(TAG,"Invalid charging level index %d. Available levels: %s",set_level, mt_charging_limits->AsString().c_str());
        }
      else
        {
        ESP_LOGI(TAG,"Current charging limit: %d amps. Available levels: %s", levels[set_level], mt_charging_limits->AsString().c_str());
        StandardMetrics.ms_v_charge_climit->SetValue( (float)levels[set_level]);
        }
      break;
      }

    // Tire pressure
    case 0x103D4040: 
      {
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL,  d[2] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL,  d[3] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR,  d[4] << 2 );
      StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR,  d[5] << 2 );

      // Tire temperature is not sent via CAN. For now set bogus tire temperature, 
      // because iOS app does not show tire pressure unless tempereature is set and >0 ..
      int temp;
      if (StandardMetrics.ms_v_env_temp->IsDefined())
        temp = StandardMetrics.ms_v_env_temp->AsInt();
      else
        temp = 255;
      if (temp<1)
        temp=1;
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, temp);
      StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, temp);
      break;
      } 

    // Content theft sensor
    case 0x10260040:
      {
      switch (d[2] & 0x7) // alarm status
        {
        case 0x2:
          {
          ESP_LOGI(TAG,"Alarm unarmed");
          MyEvents.SignalEvent("vehicle.alarm.unarmed",NULL);
          // If previously alarm sounded, notify it as off
          if (StandardMetrics.ms_v_env_alarm->AsBool())
            StandardMetrics.ms_v_env_alarm->SetValue(false);
          break;
          }
        case 0x1:
          {
          ESP_LOGI(TAG,"Alarm armed");
          MyEvents.SignalEvent("vehicle.alarm.armed",NULL);
          break;
          }
        case 0x4:
          {
          // This is the first status after doors are locked. After about 30 secs it is transitioned to 0x1 (armed)
          ESP_LOGI(TAG,"Alarm standby(?)");
          break;
          }
        case 0x3:
          {
          ESP_LOGW(TAG,"Alarm sounded!");
          StandardMetrics.ms_v_env_alarm->SetValue(true);  // Event is signaled here
          // TODO: Parse alarm trigger source (door, window break, charger removed, tilt sensor etc..)
          break;
          }
        default:
          {
          ESP_LOGW(TAG,"Alarm: unhandled status %d", d[2] & 0x7);
          break;
          }
        }
      break;
      } 
    
    // Door status FR
    case 0x0C2F6040: 
      {
      StdMetrics.ms_v_door_fr->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status RL
    case 0x0C2F8040: 
      {
      StdMetrics.ms_v_door_rl->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status RR
    case 0x0C2FA040: 
      {
      StdMetrics.ms_v_door_rr->SetValue(d[0] & 1<<0);
      break;
      } 

    // Door status FL
    case 0x0C630040: 
      {
      StdMetrics.ms_v_door_fl->SetValue(d[0] & 1<<0);
      break;
      } 

    // Hood
    case 0x10728040: 
      {
      StdMetrics.ms_v_door_hood->SetValue(d[0] & 1<<1);
      break;
      } 

    // Trunk
    case 0x0C6AA040: 
      {
      StdMetrics.ms_v_door_trunk->SetValue(d[0] & 1<<0);
      break;
      } 

    // External Lighting
    case 0x1020C040: 
      {
      StdMetrics.ms_v_env_headlights->SetValue(d[2] & 1<<2); // Parking Light
      break;
      } 

    // This Charge miles statistic
    case 0x1044A0CB: 
      {
      StdMetrics.ms_v_pos_trip->SetValue((float)((d[4]<<8 | d[5]) & 0x3fff)/8, Kilometers); // total km this charge
      mt_v_trip_ev->SetValue((float)((d[0]<<6 | d[1]>>2))/8, Kilometers); // ev km this charge
      break;
      } 

    // This Charge kWh statistic
    case 0x102820CB: 
      {
      StdMetrics.ms_v_bat_energy_used->SetValue((float)((d[2]<<8 | d[3]) & 0x3fff)/10, kWh);
      break;
      } 

    // Battery range estimated (value on cluster)
    case 0x102EC0CB: 
      {
      StandardMetrics.ms_v_bat_range_est->SetValue(((d[2] & 1)<<2 | d[1]<<1 | d[2]>>7), Kilometers); 
      break;
      } 
 
    default:
      break;
    }

    // Handle the rest (AC / Preheating related CAN frames) here
    ClimateControlIncomingSWCAN(p_frame);
  }

void OvmsVehicleVoltAmpera::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  uint8_t value = *data;

  //Cell voltage
  const uint16_t pid_cellv1 = 0x4181;
  if((pid>=pid_cellv1 && pid<=pid_cellv1+30) || (pid>=pid_cellv1+127 && pid<=pid_cellv1+191)){
    if(length <2)
      return;

    int nCellNum = 0;
    if(pid <= pid_cellv1+30){
      nCellNum = pid - pid_cellv1;
    }
    else{
      nCellNum = pid - pid_cellv1 - 96;
    }

    if(nCellNum == 0)
      BmsResetCellVoltages();

    uint32_t uCellv =  (data[0]<<8 | data[1]);
    BmsSetCellVoltage(nCellNum, (float)5 * uCellv / 65535);
    return;
  }

  switch (pid)
    {
    case 0x002f:  // Fuel level
      if(mt_fuel_level->SetValue((int)value * 100 / 255))
        NotifyFuel();
      break;
    case 0x4369:  // On-board charger current
      StandardMetrics.ms_v_charge_current->SetValue((unsigned int)value / 5);
      break;
    case 0x4368:  // On-board charger voltage
      StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)value <<1);
      break;
    case 0x801f:  // Outside temperature (filtered) (aka ambient temperature)
      StandardMetrics.ms_v_env_temp->SetValue((int)value/2 - 0x28);
      break;
    case 0x801e:  // Outside temperature (raw)
      break;
    case 0x434f:  // High-voltage Battery temperature
      StandardMetrics.ms_v_bat_temp->SetValue((int)value - 0x28);
      break;
    case 0x1c43:  // PEM temperature
      StandardMetrics.ms_v_inv_temp->SetValue((int)value - 0x28);
      break;
    case 0x8334:  // SOC
      {
      int soc = ((int)value * 39)/99;
      StandardMetrics.ms_v_bat_soc->SetValue(soc);
      if (m_range_rated_km != 0)
        StandardMetrics.ms_v_bat_range_ideal->SetValue((soc * m_range_rated_km)/100, Kilometers);
      break;
      }
    case 0x000d:  // Vehicle speed
      StandardMetrics.ms_v_pos_speed->SetValue(value,Kilometers);
      break;
    /* 
    //ms_v_bat_range_est set from swcan message
    case 0x2487:  //Distance Traveled on Battery Energy This Drive Cycle
      {
      unsigned int edriven = (((int)data[4])<<8) + data[5];
      if ((edriven > m_range_estimated_km)&&
          (StandardMetrics.ms_v_charge_state->AsString().compare("done")))
        m_range_estimated_km = edriven;
      break;
      }
    */
    case 0x41a3:  // High-voltage Battery Capacity  
      StandardMetrics.ms_v_bat_cac->SetValue((float)(data[0]<<8 | data[1]) / 10, AmpHours);
      break;
    case 0x40d7:  // High-voltage Battery Section 1 temperature  
      BmsResetCellTemperatures();
      BmsSetCellTemperature(0, data[0] - 40);
      break;
    case 0x40d9:  // High-voltage Battery Section 2 temperature  
      BmsSetCellTemperature(1, data[0] - 40);
      break;    
    case 0x40db:  // High-voltage Battery Section 3 temperature  
      BmsSetCellTemperature(2, data[0] - 40);
      break;
    case 0x40dd:  // High-voltage Battery Section 4 temperature  
      BmsSetCellTemperature(3, data[0] - 40);
      break;
    case 0x40df:  // High-voltage Battery Section 5 temperature  
      BmsSetCellTemperature(4, data[0] - 40);
      break;
    case 0x40e1:  // High-voltage Battery Section 6 temperature  
      BmsSetCellTemperature(5, data[0] - 40);
      break;   
    case 0x1940:  // Transmission temperature
      StandardMetrics.ms_v_mot_temp->SetValue((int)value - 0x28);
      break;   
    default:
      break;
    }
  }

  void OvmsVehicleVoltAmpera::Ticker300(uint32_t ticker)
    {
    if (StandardMetrics.ms_v_env_on->AsBool())
      {
        NotifyMetrics();
      }
    }

void OvmsVehicleVoltAmpera::Ticker10(uint32_t ticker)
  {
  if (m_tx_retry_counter>0) 
    {
    ESP_LOGI(TAG, "Resetting tx_retry_counter");
    m_tx_retry_counter=0;
    }
  }

void OvmsVehicleVoltAmpera::Ticker1(uint32_t ticker)
  {
  // Check if the car has gone to sleep
  if (m_candata_timer > 0)
    {
    if (--m_candata_timer == 0)
      {
      // Car has gone to sleep
      ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
      StandardMetrics.ms_v_env_gear->SetValue(-2);
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_env_awake->SetValue(false);
      PollSetState(0);
      }
    }

  if (m_startPolling_timer > 0)
    {
    if (--m_startPolling_timer == 0)
      {
      if(m_poll_state == 0)
        {
        // Start polling with delay (battery module need time to wake up)
        PollSetThrottling(VA_POLLING_HIGH_THROTTLING);  // get all cells info before sleep
        PollSetState(2);
        }
      }
    }
  
  if((m_poll_state == 2) && (m_poll_plcur == m_poll_plist))
    {
    // polling complete one time for state 2. Switch to state 1.
    PollSetState(1);
    PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
    }

  PreheatWatchdog();

  if (m_controlled_lights)
    {
    m_tester_present_timer++;
    if ( (m_tester_present_timer > VA_TESTER_PRESENT_TIMEOUT) || (StandardMetrics.ms_v_env_gear->AsInt(-2) > -2) )
      {
      // Lights have been on too long or the car isn't parked anymore -> prevent Tester Present message (lights will go off after few secs)
      ESP_LOGI(TAG,"Lights on timeout OR car not parked -> turning off");
      m_tester_present_timer = 0;
      m_controlled_lights = (va_light_t)0;
      }
    else
      SendTesterPresentMessage(VA_BCM);
    }

  int cc = StandardMetrics.ms_v_charge_current->AsInt();
  int cv = StandardMetrics.ms_v_charge_voltage->AsInt();

  if ((cc != 0)&&(cv != 0))
    {
    // The car is charging
    StandardMetrics.ms_v_env_charging12v->SetValue(true);
    if (StandardMetrics.ms_v_charge_inprogress->AsBool() == false)
      {
      // A charge has started
      ESP_LOGI(TAG,"Car has started a charge");
      StandardMetrics.ms_v_charge_pilot->SetValue(true);
      StandardMetrics.ms_v_charge_inprogress->SetValue(true);
      StandardMetrics.ms_v_door_chargeport->SetValue(true);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      StandardMetrics.ms_v_charge_state->SetValue("charging");
      StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
      StandardMetrics.ms_v_charge_climit->SetValue(16);
      NotifyChargeState();
      m_charge_timer = 0;
      m_charge_wm = 0;
      }
    else
      {
      // A charge is ongoing
      m_charge_timer++;
      if (m_charge_timer >= 60)
        {
        m_charge_timer -= 60;
        m_charge_wm += (StandardMetrics.ms_v_charge_voltage->AsInt()
                      * StandardMetrics.ms_v_charge_current->AsInt());
        if (m_charge_wm > 60000)
          {
          StandardMetrics.ms_v_charge_kwh->SetValue(
            StandardMetrics.ms_v_charge_kwh->AsInt() + 10);
          m_charge_wm -= 60000;
          }
        }
      }
    }
  else if ((cc == 0)&&(cv == 0))
    {
    // The car is not charging
    if (StandardMetrics.ms_v_charge_inprogress->AsBool())
      {
      // The charge has completed/stopped
      StandardMetrics.ms_v_charge_pilot->SetValue(false);
      StandardMetrics.ms_v_charge_inprogress->SetValue(false);
      StandardMetrics.ms_v_door_chargeport->SetValue(false);
      StandardMetrics.ms_v_charge_mode->SetValue("standard");
      if (StandardMetrics.ms_v_bat_soc->AsInt() < 95)
        {
        // Assume the charge was interrupted
        ESP_LOGI(TAG,"Car charge session was interrupted");
        StandardMetrics.ms_v_charge_state->SetValue("stopped");
        StandardMetrics.ms_v_charge_substate->SetValue("interrupted");
        }
      else
        {
        // Assume the charge completed normally
        ESP_LOGI(TAG,"Car charge session completed");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        MyEvents.SignalEvent("vehicle.charge.finish",NULL);
        }
        NotifyChargeState();

      m_charge_timer = 0;
      m_charge_wm = 0;
      }

    // 12V battery may be charging via High Voltage battery when car is on, or via external battery charger
    if (StandardMetrics.ms_v_bat_12v_voltage->AsFloat() > VA_CHARGING_12V_THRESHOLD)
      StandardMetrics.ms_v_env_charging12v->SetValue(true);
    else
      StandardMetrics.ms_v_env_charging12v->SetValue(false);
    }

  }

void OvmsVehicleVoltAmpera::NotifiedVehicleOn()
  {
  ESP_LOGI(TAG,"Powertrain enabled");
  PollSetState(1); // abort state 2 if not complete yet
  PollSetThrottling(VA_POLLING_NORMAL_THROTTLING);
  }

void OvmsVehicleVoltAmpera::NotifiedVehicleOff()
  {
  ESP_LOGI(TAG,"Powertrain disabled");
  
  // update all cells info without HV load
  PollSetThrottling(VA_POLLING_HIGH_THROTTLING);  
  PollSetState(2);
  }

void OvmsVehicleVoltAmpera::NotifiedVehicleAwake()
  {
  NotifyMetrics();
  }

void OvmsVehicleVoltAmpera::CommandWakeupComplete( const CAN_frame_t* p_frame, bool success )
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGI(TAG,"CommandWakeupComplete. Success: %d", success);
  // Return from high voltage wakeup mode after wakeup CAN messages have been sent

  // Switch to normal mode no matter if the wakeup msg was sent successfully or not
  vTaskDelay(20 / portTICK_PERIOD_MS);  
  p_swcan_if->SetTransceiverMode(tmode_normal);
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }


#define WAKEUP_DELAY_1 220
#define WAKEUP_DELAY_2 360
#define WAKEUP_DELAY_3 180

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandWakeup()
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  CAN_frame_t txframe;

  p_swcan_if->SetTransceiverMode(tmode_highvoltagewakeup);

  // Send the 0x100 message with callback so that High Voltage Wakeup mode can be exited
  FRAME_FILL(0, p_swcan, txframe,   0x100, 0, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)
  txframe.callback = &wakeup_frame_sent;
  p_swcan->Write(&txframe);

  vTaskDelay(WAKEUP_DELAY_1 / portTICK_PERIOD_MS);

  // Body Control Module (BCM)
  SEND_STD_FRAME(p_swcan, txframe,  0x621, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

  if (m_extended_wakeup)
    {
    ESP_LOGI(TAG,"Sending extended wake up messages...");
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x621, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    SEND_STD_FRAME(p_swcan, txframe,  0x100, 8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)

    // ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x620, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x620, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);

    // Theft Deterrent Module (TDM) ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x622, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x622, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x100, 8, 0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00)

    // Sensing and Diagmostic module (SDM) ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x627, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x627, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // Instrument Panel Cluster (IPC)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62c, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62c, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // ?
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62d, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x62d, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)

    // Heating Ventilation Air Conditioning (HVAC)
    vTaskDelay(WAKEUP_DELAY_2 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x631, 8, 0x00,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
    vTaskDelay(WAKEUP_DELAY_3 / portTICK_PERIOD_MS);
    SEND_STD_FRAME(p_swcan, txframe,  0x631, 8, 0x01,0xff,0xff,0xff,0xff,0xff,0x00,0x00)
  }

  FlashLights(Interior_lamp);

  ESP_LOGI(TAG,"CommandWakeup End");
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }



OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandLock(const char* pin)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  CommandWakeup();

  CAN_frame_t txframe;

  // Onstar lock command emulation
  // Does not seem to work on MY2014 Ampera

  // Telematics control
  vTaskDelay(720 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x01,0xff)
  vTaskDelay(180 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x0c,0x00,0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)
  /*
  Alternative: Diagnostic lock command. But will not arm alarm!
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8,  0x01,0x3e,0x00,0x00,0x00,0x00,0x00,0x00)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8,  0x07,0xae,0x01,0x01,0x01,0x00,0x00,0x00)
  */

  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandUnlock(const char* pin)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  CommandWakeup();

  CAN_frame_t txframe;
  // Onstar unlock command emulation
  // Does not seem to work on MY2014 Ampera

  // Telematics control
  vTaskDelay(720 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x03,0xff)
  vTaskDelay(180 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x0c,0x00,0xff)
  vTaskDelay(1250 / portTICK_PERIOD_MS);  
  SEND_EXT_FRAME(p_swcan, txframe, 0x1024E097, 3, 0x00,0x00,0xff)

  /*
  Alternative: Diagnostic unlock command. WILL NOT DISARM ALARM! 
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x01,0x3e,0x00,0x00,0x00,0x00,0x00,0x00)
  vTaskDelay(100 / portTICK_PERIOD_MS);
  // Driver door unlock
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x07,0xae,0x01,0x04,0x04,0x00,0x00,0x00)
  // Passengers door unlock
  SEND_STD_FRAME(m_can1, txframe,  0x241, 8, 0x07,0xae,0x01,0x02,0x02,0x00,0x00,0x00)
  */

  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandLights(va_light_t lights, bool turn_on)
  {
  ESP_LOGI(TAG,"CommandLights: lights 0x%x:%d",(uint32_t)lights,turn_on);
  SendTesterPresentMessage(VA_BCM);
  vTaskDelay(200 / portTICK_PERIOD_MS);  

  CAN_frame_t txframe;
  uint8_t *d = txframe.data.u8;
  if (lights & Park)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x07, 0xff, 0x2f, 0xff, 0x2f, 0xff)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x07, 0xff, 0x00, 0x00, 0x00, 0x00)
      }

  if (lights & Charging_indicator)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x14, 0x00, 0x00, 0x02, 0x00, 0x02)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x14, 0x00, 0x00, 0x02, 0x00, 0x00)
      }

  if (lights & Interior_lamp)
      {
      if (turn_on)
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x08, 0x01, 0x7f, 0xff, 0x00, 0x00)
      else
        SEND_STD_FRAME(m_can1, txframe, VA_BCM, 8, 0x07, 0xae, 0x08, 0x01, 0x00, 0x00, 0x00, 0x00)
      }

  if ( (lights & Left_rear_signal) || (lights & Right_rear_signal) || (lights & Driver_front_signal) || (lights & Passenger_front_signal) 
      || (lights & Center_stop_lamp) )
    {
    // Light group 0x02
    FRAME_FILL(0, m_can1, txframe, VA_BCM, 8, 0x07,0xae,0x02,0x00,0x00,0x00,0x00,0x00)

    if (lights & Left_rear_signal)
        {
        d[3] |= 0x20;
        if (turn_on)
          d[4] |= 0x20;
        }

    if (lights & Right_rear_signal)
        {
        d[3] |= 0x80;
        if (turn_on)
          d[4] |= 0x80;
        }

    if (lights & Driver_front_signal)
        {
        d[3] |= 0x10;
        if (turn_on)
          d[4] |= 0x10;
        }

    if (lights & Passenger_front_signal)
        {
        d[3] |= 0x40;
        if (turn_on)
          d[4] |= 0x40;
        }

    if (lights & Center_stop_lamp)
        {
        d[5] |= 0x20;
        if (turn_on)
          d[6] |= 0x20;
        }

    m_can1->Write(&txframe);
    return Success;
    }

  // Set the bitwise status of the lights that we control and are now ON. If any of these are set, we send periodic Tester Present messages
  m_controlled_lights = (m_controlled_lights & ~lights) | (lights*turn_on);
  ESP_LOGI(TAG,"CommandLights: controlled_lights 0x%x",m_controlled_lights);
  return Success;
  }

void OvmsVehicleVoltAmpera::FlashLights(va_light_t light, int interval, int count)
  {
  for (int i=0;i<count;i++)
    {
    CommandLights(light, true);
    vTaskDelay(interval / portTICK_PERIOD_MS);
    CommandLights(light, false);      
    if (i+1 < count)
      vTaskDelay(interval / portTICK_PERIOD_MS);
    }
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandHomelink(int button, int durationms)
  {
  switch (button)
    {
    case 0:
      return CommandClimateControl(true);
      break;
    case 1:
      return CommandClimateControl(false);
      break;
    case 2:
      return CommandWakeup();
      break;
    default:
      break;
    }
  return NotImplemented;
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandSetChargeCurrent(uint16_t limit)
  {
  ESP_LOGI(TAG,"CommandSetChargeCurrent: %d amps",limit);
  CommandWakeup();

  int highest=0;
  int highest_index=-1;
  // Find the highest possible available charging limit
  for (int i=0;i<mt_charging_limits->GetSize();i++)
    {
    int lim = mt_charging_limits->GetElemValue(i);
    ESP_LOGI(TAG,"CommandSetChargeCurrent: %d:%d",i,lim);
    if ( (lim<=limit) && (lim > highest) )
      {
      highest = mt_charging_limits->GetElemValue(i);
      highest_index = i;
      }
    }
  if (highest_index == -1)
    {
    ESP_LOGE(TAG,"CommandSetChargeCurrent: No valid current limit found!");
    return Fail;      
    }

  ESP_LOGI(TAG,"CommandSetChargeCurrent: Selected charging limit %d:%d amps",highest_index,highest);
  CAN_frame_t txframe;
  SEND_EXT_FRAME(p_swcan, txframe, 0x10864080, 8, (highest_index+1) << 4, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  return Success;
  }

void OvmsVehicleVoltAmpera::SendTesterPresentMessage( uint32_t id )
  {
  CAN_frame_t txframe;
  SEND_STD_FRAME(m_can1, txframe, id, 8, 0x01, 0x3e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00)
  }


class OvmsVehicleVoltAmperaInit
  {
  public: OvmsVehicleVoltAmperaInit();
} MyOvmsVehicleVoltAmperaInit  __attribute__ ((init_priority (9000)));

OvmsVehicleVoltAmperaInit::OvmsVehicleVoltAmperaInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Volt/Ampera (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleVoltAmpera>("VA","Volt/Ampera");
  }
