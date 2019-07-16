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
static const char *TAG = "v-voltampera";

#include <stdio.h>
#include "ovms_config.h"
#include "vehicle_voltampera.h"

#define VA_CANDATA_TIMEOUT 10

// Use states:
// 0 = bus is idle, car sleeping
// 1 = car is on and ready to Drive
static const OvmsVehicle::poll_pid_t va_polls[]
  =
  {
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4369, {  0, 10,  0 } }, // On-board charger current
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x4368, {  0, 10,  0 } }, // On-board charger voltage
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801f, {  0, 10,  0 } }, // Outside temperature (filtered)
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x801e, {  0, 10,  0 } }, // Outside temperature (raw)
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x434f, {  0, 10,  0 } }, // High-voltage Battery temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x1c43, {  0, 10,  0 } }, // PEM temperature
    { 0x7e4, 0x7ec, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x8334, {  0, 10,  0 } }, // SOC
    { 0x7e1, 0x7e9, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x2487, {  0,100,  0 } }, // Distance Traveled on Battery Energy This Drive Cycle
    { 0, 0, 0x00, 0x00, { 0, 0, 0 } }
  };
// These are not polled anymore but instead received passively
// { 0x7e0, 0x7e8, VEHICLE_POLL_TYPE_OBDIIEXTENDED, 0x000d, {  0, 10,  0 } }, // Vehicle speed

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
  m_range_estimated_km = 0;

  // Config parameters
  MyConfig.RegisterParam("xva", "Volt/Ampera", true, true);
  m_range_rated_km = MyConfig.GetParamValueInt("xva", "range.km", 0);

  // Register CAN bus and polling requests
  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // powertrain bus
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // chassis bus 
  PollSetPidList(m_can1,va_polls);
  PollSetState(0);
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGI(TAG, "Volt/Ampera vehicle module: Register SWCAN using external CAN module");
  RegisterCanBus(4,CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);
  p_swcan = (swcan*)MyPcpApp.FindDeviceByName("swcan");
#else
  ESP_LOGI(TAG, "Volt/Ampera vehicle module: Register 2nd MCP2515 as SWCAN");
  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_33KBPS);  // single wire can
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN

  // register tx callback
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyCan.RegisterCallback(TAG, std::bind(&OvmsVehicleVoltAmpera::TxCallback, this, _1, _2), true);

  wakeup_frame_sent = std::bind(&OvmsVehicleVoltAmpera::CommandWakeupComplete, this, _1, _2);
  }

OvmsVehicleVoltAmpera::~OvmsVehicleVoltAmpera()
  {
  ESP_LOGI(TAG, "Shutdown Volt/Ampera vehicle module");
  MyCan.DeregisterCallback(TAG);
  }

void OvmsVehicleVoltAmpera::Status(int verbosity, OvmsWriter* writer)
  {
  writer->printf("Vehicle:  Volt Ampera (%s %d)\n", m_type, m_modelyear+2000);
  writer->printf("VIN:      %s\n",m_vin);
  writer->puts("");
  writer->printf("Ranges:   %dkm (rated) %dkm (estimated)\n",
    m_range_rated_km, m_range_estimated_km);
  writer->printf("Charge:   Timer %d (%d wm)\n", m_charge_timer, m_charge_wm);
  writer->printf("Can Data: Timer %d (poll state %d)\n",m_candata_timer,m_poll_state);
  }


void OvmsVehicleVoltAmpera::TxCallback(const CAN_frame_t* p_frame, bool success)
  {
  if (p_frame->MsgID == 0x7e4)
    {
    if (!success)
      ESP_LOGE(TAG, "TxCallback. Error sending poll request. MsgId: 0x%x", p_frame->MsgID);
    return;
    }

  if (success) 
    {
    ESP_LOGI(TAG, "TxCallback. MsgId: 0x%x. Success: %d", p_frame->MsgID, success);
    m_tx_retry_counter = 0;
    } 
  else
    {
    if (m_tx_retry_counter < 5)
      {
      m_tx_retry_counter++;
      ESP_LOGE(TAG, "TxCallback. Failed sending MsgId: 0x%x, global retry %d", p_frame->MsgID, m_tx_retry_counter);
      p_frame->origin->Write(p_frame);
      } 
    else
      {
      ESP_LOGE(TAG, "TxCallback. Maximum retries reached!");
      }
    }
  }


#define GET8(frame, pos) (frame->data.u8[pos])
#define GET16(frame, pos) ((frame->data.u8[pos] << 8) | frame->data.u8[pos+1])
#define GET24(frame, pos) ((frame->data.u8[pos] << 16) | (frame->data.u8[pos+1] << 8) | frame->data.u8[pos+2])
#define GET32(frame, pos) ((frame->data.u8[pos] << 24) | (frame->data.u8[pos+1] << 16) | (frame->data.u8[pos+2] << 8) | frame->data.u8[pos+3])

void OvmsVehicleVoltAmpera::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;
  int k;

  if ((p_frame->MsgID == 0x7e8)||
      (p_frame->MsgID == 0x7ec)||
      (p_frame->MsgID == 0x7e9))
    return; // Ignore poll responses

  // Activity on the bus, so resume polling
  if (m_poll_state != 1)
    {
    ESP_LOGI(TAG,"Car has woken (CAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    PollSetState(1);
    }
  m_candata_timer = VA_CANDATA_TIMEOUT;

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    case 0x0c9: {
      StandardMetrics.ms_v_mot_rpm->SetValue( GET16(p_frame, 1) >> 2 );
      break;
      }

    case 0x120: {
      StandardMetrics.ms_v_pos_odometer->SetValue( GET32(p_frame, 0) / 64);
      break;
      } 

    case 0x3e9:
      StandardMetrics.ms_v_pos_speed->SetValue(GET16(p_frame, 0), Miles);
      break;

    case 0x1a1:
      StandardMetrics.ms_v_env_throttle->SetValue( d[7] );
      break;

    case 0xd1:
      StandardMetrics.ms_v_env_footbrake->SetValue( d[4] );
      break;

    case 0x4e1:
      {
      for (k=0;k<8;k++)
        m_vin[k+9] = d[k];
      break;
      }
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
      break;
      }
    case 0x135:
      {
      if (d[0] == 0)
        {
        // Car is in PARK
        StandardMetrics.ms_v_env_gear->SetValue(0);
        StandardMetrics.ms_v_env_on->SetValue(false);
        StandardMetrics.ms_v_env_handbrake->SetValue(true);
        }
      else
        {
        // Car is not in PARK
        StandardMetrics.ms_v_env_on->SetValue(true);
        StandardMetrics.ms_v_env_handbrake->SetValue(false);
        }
      break;
      }
    case 0x241:
      {
      // Theft horn
      // TODO: TEST!
      if (d[0] == 0x07 && d[1] == 0xae && d[2] == 0x10 && d[3] == 0x03)
        {
        if (d[4] == 0x03)
          {
          StandardMetrics.ms_v_env_alarm->SetValue(true);
          }
        else
          {
          StandardMetrics.ms_v_env_alarm->SetValue(false);
          }
        }
      break;
      }
    default:
      break;
    }
  }


void OvmsVehicleVoltAmpera::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  IncomingFrameSWCan(p_frame);  // assume third can bus messages coming from SWCAN bus
  }


void OvmsVehicleVoltAmpera::IncomingFrameSWCan(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  // Activity on the bus, so resume polling
  if (m_poll_state != 1)
    {
    ESP_LOGI(TAG,"Car has woken (SWCAN bus activity)");
    StandardMetrics.ms_v_env_awake->SetValue(true);
    PollSetState(1);
    }
  m_candata_timer = VA_CANDATA_TIMEOUT;

  // Process the incoming message
  switch (p_frame->MsgID)
    {
    case 0x107220A9:  // seat heater
      {
      if ( (d[0]==0) && (d[1]==0) )
        ESP_LOGI(TAG,"Driver and passenger seat heater off");
      else
        ESP_LOGI(TAG,"Seat heaters %02x / %02x", d[1],d[3]);
      break;
      }

    case 0x0C414040: {
      bool with_fob = false;
      if (d[3]==5)
        with_fob = true;  // d[3] == 1 -> with console panel button
      switch (d[1]) {
        case 0x05: {
          ESP_LOGI(TAG,"Car locked");
          StandardMetrics.ms_v_env_locked->SetValue(1); 
          break;
          }
        case 0x04: 
          {
          ESP_LOGI(TAG,"Car unlocked");
          StandardMetrics.ms_v_env_locked->SetValue(0); 
          break;
          }
        }
      break;
      }
      
    case 0x102CA040: {
      int ngear = d[6] & 7;
      if (ngear > 0 && ngear < 6) {
        StandardMetrics.ms_v_env_gear->SetValue(ngear - 1);
        }
      break;
      }

    case 0x103D4040: {
      // tire pressure
      StandardMetrics.ms_v_tpms_fl_p->SetValue( d[2] );
      StandardMetrics.ms_v_tpms_rl_p->SetValue( d[3] );
      StandardMetrics.ms_v_tpms_fr_p->SetValue( d[4] );
      StandardMetrics.ms_v_tpms_rr_p->SetValue( d[5] );
      break;
      } 

    case 0x10424040: {
      // TODO: which temp is which? is env_temp same as when polled?
      //StandardMetrics.ms_v_env_temp->SetValue( d[1] );
      StandardMetrics.ms_v_env_cabintemp->SetValue( d[2] );
      break;
      } 

    case 0x10254040: {
      switch(d[0]) {
        case 0: StandardMetrics.ms_v_env_drivemode->SetValue(0); break;
        case 1: StandardMetrics.ms_v_env_drivemode->SetValue(1); break;
        case 4: StandardMetrics.ms_v_env_drivemode->SetValue(2); break;
        case 16: StandardMetrics.ms_v_env_drivemode->SetValue(3); break;
        }
      break;
      } 


    default:
      break;
    }
  }

void OvmsVehicleVoltAmpera::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  uint8_t value = *data;

  switch (pid)
    {
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
      if (m_range_estimated_km != 0)
        StandardMetrics.ms_v_bat_range_est->SetValue((soc * m_range_estimated_km)/100, Kilometers);
      break;
      }
    /*
    case 0x000d:  // Vehicle speed
      StandardMetrics.ms_v_pos_speed->SetValue(value,Kilometers);
      break;
    */
    case 0x2487:  //Distance Traveled on Battery Energy This Drive Cycle
      {
      unsigned int edriven = (((int)data[4])<<8) + data[5];
      if ((edriven > m_range_estimated_km)&&
          (StandardMetrics.ms_v_charge_state->AsString().compare("done")))
        m_range_estimated_km = edriven;
      break;
      }
    default:
      break;
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
      StandardMetrics.ms_v_env_gear->SetValue(0);
      StandardMetrics.ms_v_env_on->SetValue(false);
      StandardMetrics.ms_v_env_awake->SetValue(false);
      StandardMetrics.ms_v_env_handbrake->SetValue(true);
      PollSetState(0);
      }
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
    StandardMetrics.ms_v_env_charging12v->SetValue(false);
    }

  }


void OvmsVehicleVoltAmpera::CommandWakeupComplete( const CAN_frame_t* p_frame, bool success )
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  ESP_LOGI(TAG,"CommandWakeupComplete. Success: %d", success);
  // return from high voltage wakeup mode after wakeup CAN messages have been sent
  // return to normal mode no matter if the wakeup msg was sent successfully or not
  vTaskDelay(20 / portTICK_PERIOD_MS);  
  p_swcan->SetTransceiverMode(tmode_normal);
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }


OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandWakeup()
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  p_swcan->SetTransceiverMode(tmode_highvoltagewakeup);

  CAN_frame_t txframe;
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x100;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 0; //8;
  p_swcan->Write(&txframe);

  vTaskDelay(80 / portTICK_PERIOD_MS);  
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x620;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x621;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x622;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x100;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 0; //8;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);  
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x627;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);  
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x62c;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  p_swcan->Write(&txframe);

  vTaskDelay(10 / portTICK_PERIOD_MS);  
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x62d;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.data.u8[0] = 0x01;
  txframe.data.u8[1] = 0xff;
  txframe.data.u8[2] = 0xff;
  txframe.data.u8[3] = 0xff;
  txframe.data.u8[4] = 0xff;
  txframe.data.u8[5] = 0xff;
  txframe.data.u8[6] = 0x00;
  txframe.data.u8[7] = 0x00;
  // register tx callback for this frame
  // we want to set mode from high voltage wakeup mode back to normal mode only after these messages have been sent
  txframe.callback = &wakeup_frame_sent;
  p_swcan->Write(&txframe);

  ESP_LOGI(TAG,"CommandWakeup End");
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }



OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandLock(const char* pin)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  // DOES NOT WORK ON MY 2014->
  CommandWakeup();
  vTaskDelay(500 / portTICK_PERIOD_MS);  
  CAN_frame_t txframe;
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x1024E097;
  txframe.FIR.B.FF = CAN_frame_ext;
  txframe.FIR.B.DLC = 3; // 6;
  txframe.data.u8[0] = 0x00;
  txframe.data.u8[1] = 0x01;
  txframe.data.u8[2] = 0xff;
  /*
  txframe.data.u8[3] = 0x00;
  txframe.data.u8[4] = 0x00;
  txframe.data.u8[5] = 0xff;
  */
  p_swcan->Write(&txframe);
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandUnlock(const char* pin)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  // DOES NOT WORK ON MY 2014->
  CommandWakeup();
  vTaskDelay(500 / portTICK_PERIOD_MS);  
  CAN_frame_t txframe;
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x1024E097; 
  txframe.FIR.B.FF = CAN_frame_ext;
  txframe.FIR.B.DLC = 3; // 6;
  txframe.data.u8[0] = 0x00;
  txframe.data.u8[1] = 0x03;
  txframe.data.u8[2] = 0xff;
  /*
  txframe.data.u8[3] = 0x00;
  txframe.data.u8[4] = 0x00;
  txframe.data.u8[5] = 0xff;
  */
  p_swcan->Write(&txframe);
  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  }

OvmsVehicle::vehicle_command_t OvmsVehicleVoltAmpera::CommandClimateControl(bool enable)
  {
#ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
  // DOES NOT WORK ON MY 2014->
  CommandWakeup();
  vTaskDelay(50 / portTICK_PERIOD_MS);  
  CommandLock(0);
  vTaskDelay(500 / portTICK_PERIOD_MS);  

  CAN_frame_t txframe;
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x1024E097;
  txframe.FIR.B.FF = CAN_frame_ext;
  txframe.FIR.B.DLC = 3;
  if (enable)
    {
    txframe.data.u8[0] = 0x80;
    txframe.data.u8[1] = 0x01;
    } else
    {
    txframe.data.u8[0] = 0x40;
    txframe.data.u8[1] = 0x00; // or 0x01 ?
    }
  txframe.data.u8[2] = 0xff;
  p_swcan->Write(&txframe);

  vTaskDelay(1000 / portTICK_PERIOD_MS);  
  memset(&txframe,0,sizeof(txframe));
  txframe.origin = p_swcan;
  txframe.MsgID = 0x1024E097;
  txframe.FIR.B.FF = CAN_frame_ext;
  txframe.FIR.B.DLC = 3;
  txframe.data.u8[0] = 0x00;
  txframe.data.u8[1] = 0x00;
  txframe.data.u8[2] = 0xff;
  p_swcan->Write(&txframe);

  return Success;
#else
  return NotImplemented;
#endif // #ifdef CONFIG_OVMS_COMP_EXTERNAL_SWCAN
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
