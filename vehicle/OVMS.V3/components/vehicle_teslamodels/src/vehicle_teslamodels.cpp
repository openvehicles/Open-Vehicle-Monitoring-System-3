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
static const char *TAG = "v-teslamodels";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslamodels.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include "ovms_webserver.h"
#endif

OvmsVehicleTeslaModelS::OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Tesla Model S vehicle module");

  memset(m_vin,0,sizeof(m_vin));
  memset(m_type,0,sizeof(m_type));
  m_charge_w = 0;
#ifdef CONFIG_OVMS_COMP_TPMS
  m_candata_timer = TS_CANDATA_TIMEOUT;
  m_tpms_cmd = Idle;
  m_tpms_pos = 0;
#endif // #ifdef CONFIG_OVMS_COMP_TPMS

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Tesla Model S/X CAN3: Powertrain
  RegisterCanBus(2,CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);  // Tesla Model S/X CAN6: Chassis
  RegisterCanBus(3,CAN_MODE_ACTIVE,CAN_SPEED_125KBPS);  // Tesla Model S/X CAN4: Body Fault-Tolerant (OVT1 cable)

  BmsSetCellArrangementVoltage(96, 6);
  BmsSetCellArrangementTemperature(32, 2);
  BmsSetCellLimitsVoltage(1,4.9);
  BmsSetCellLimitsTemperature(-90,90);

#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.RegisterPage("/bms/cellmon", "BMS cell monitor", OvmsWebServer::HandleBmsCellMonitor, PageMenu_Vehicle, PageAuth_Cookie);
#endif
  }

OvmsVehicleTeslaModelS::~OvmsVehicleTeslaModelS()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Model S vehicle module");
  MyCommandApp.UnregisterCommand("xts");
#ifdef CONFIG_OVMS_COMP_WEBSERVER
  MyWebServer.DeregisterPage("/bms/cellmon");
#endif
  }

void OvmsVehicleTeslaModelS::Ticker1(uint32_t ticker)
  {
  if ((m_candata_timer > 0)&&(--m_candata_timer == 0))
    {
    // Car has gone to sleep
    ESP_LOGI(TAG,"Car has gone to sleep (CAN bus timeout)");
    StandardMetrics.ms_v_env_awake->SetValue(false);
    }
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  // Tesla Model S/X CAN3: Powertrain

  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x102: // BMS current and voltage
      {
      // Don't update battery voltage too quickly (as it jumps around like crazy)
      if (StandardMetrics.ms_v_bat_voltage->Age() > 10)
        StandardMetrics.ms_v_bat_voltage->SetValue(((float)((int)d[1]<<8)+d[0])/100);
      StandardMetrics.ms_v_bat_temp->SetValue((float)((((int)d[7]&0x07)<<8)+d[6])/10);
      break;
      }
    case 0x116: // Gear selector
      {
      switch ((d[1]&0x70)>>4)
        {
        case 1: // Park
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(true);
          StandardMetrics.ms_v_env_charging12v->SetValue(false);
          break;
        case 2: // Reverse
          StandardMetrics.ms_v_env_gear->SetValue(-1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 3: // Neutral
          StandardMetrics.ms_v_env_gear->SetValue(0);
          StandardMetrics.ms_v_env_on->SetValue(false);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        case 4: // Drive
          StandardMetrics.ms_v_env_gear->SetValue(1);
          StandardMetrics.ms_v_env_on->SetValue(true);
          StandardMetrics.ms_v_env_handbrake->SetValue(false);
          StandardMetrics.ms_v_env_charging12v->SetValue(true);
          break;
        default:
          break;
        }
      break;
      }
    case 0x222: // Charging related
      {
      m_charge_w = uint16_t(d[1]<<8)+d[0];
      break;
      }
    case 0x256: // Speed
      {
      StandardMetrics.ms_v_pos_speed->SetValue( ((((int)d[3]&0x0f)<<8) + (int)d[2])/10, (d[3]&0x80)?Kph:Mph );
      break;
      }
    case 0x28C: // Charging related
      {
      float charge_v = (float)(uint16_t(d[1]<<8) + d[0])/256;
      if ((charge_v != 0)&&(m_charge_w != 0))
        { // Charging
        StandardMetrics.ms_v_charge_current->SetValue((unsigned int)(m_charge_w/charge_v));
        StandardMetrics.ms_v_charge_voltage->SetValue((unsigned int)charge_v);
        StandardMetrics.ms_v_charge_pilot->SetValue(true);
        StandardMetrics.ms_v_charge_inprogress->SetValue(true);
        StandardMetrics.ms_v_door_chargeport->SetValue(true);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("charging");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue((unsigned int)(m_charge_w/charge_v));
        }
      else
        { // Not charging
        StandardMetrics.ms_v_charge_current->SetValue(0);
        StandardMetrics.ms_v_charge_voltage->SetValue(0);
        StandardMetrics.ms_v_charge_pilot->SetValue(false);
        StandardMetrics.ms_v_charge_inprogress->SetValue(false);
        StandardMetrics.ms_v_door_chargeport->SetValue(false);
        StandardMetrics.ms_v_charge_mode->SetValue("standard");
        StandardMetrics.ms_v_charge_state->SetValue("done");
        StandardMetrics.ms_v_charge_substate->SetValue("onrequest");
        StandardMetrics.ms_v_charge_climit->SetValue(0);
        }
      break;
      }
    case 0x302: // SOC
      {
      StandardMetrics.ms_v_bat_soc->SetValue( (((int)d[1]>>2) + (((int)d[2] & 0x0f)<<6))/10 );
      break;
      }
    case 0x306: // Temperatures
      {
      StandardMetrics.ms_v_inv_temp->SetValue((int)d[1]-40);
      StandardMetrics.ms_v_mot_temp->SetValue((int)d[2]-40);
      break;
      }
    case 0x338: // MCU_range
      {
      StandardMetrics.ms_v_bat_range_est->SetValue(((uint32_t)d[3]<<8)+d[2],Miles);
      StandardMetrics.ms_v_bat_range_ideal->SetValue(((uint32_t)d[1]<<8)+d[0],Miles);
      StandardMetrics.ms_v_bat_consumption->SetValue(((uint32_t)d[5]<<8)+d[4],WattHoursPM);
      break;
      }
    case 0x398: // Country
      {
      m_type[0] = 'T';
      m_type[1] = 'S';
      m_type[2] = d[0];
      m_type[3] = d[1];
      StandardMetrics.ms_v_type->SetValue(m_type);
      break;
      }
    case 0x508: // VIN
      {
      switch(d[0])
        {
        case 0:
          memcpy(m_vin,d+1,7);
          break;
        case 1:
          memcpy(m_vin+7,d+1,7);
          break;
        case 2:
          memcpy(m_vin+14,d+1,3);
          m_vin[17] = 0;
          StandardMetrics.ms_v_vin->SetValue(m_vin);
          break;
        }
      break;
      }
    case 0x5d8: // Odometer (0x562 is battery, so this is motor or car?)
      {
      StandardMetrics.ms_v_pos_odometer->SetValue((float)(((uint32_t)d[3]<<24)
                                                + ((uint32_t)d[2]<<16)
                                                + ((uint32_t)d[1]<<8)
                                                + d[0])/1000, Miles);
      break;
      }
    case 0x6f2: // BMS brick voltage and module temperatures
      {
      int v1 = ((int)(d[2]&0x3f)<<8) + d[1];
      int v2 = (((int)d[4]&0x0f)<<10) + (((int)d[3])<<2) + (d[2]>>6);
      int v3 = (((int)d[6]&0x03)<<12) + (((int)d[5])<<4) + (d[4]>>4);
      int v4 = (((int)d[7])<<6) + (((int)d[6]>>2));
      if ((v1 != 0x3fff)&&(v2 != 0x3fff)&&(v3 != 0x3fff)&&(v4 != 0x3fff)&&
          (v1 != 0)&&(v2 != 0)&&(v3 != 0)&&(v4 != 0))
        {
        if (d[0]<24)
          {
          // Voltages
          int k = d[0]*4;
          BmsSetCellVoltage(k,   0.000305 * v1);
          BmsSetCellVoltage(k+1, 0.000305 * v2);
          BmsSetCellVoltage(k+2, 0.000305 * v3);
          BmsSetCellVoltage(k+3, 0.000305 * v4);
          }
        else
          {
          // Temperatures
          int k = (d[0]-24)*4;
          BmsSetCellTemperature(k,   0.0122 * ((v1 & 0x1FFF) - (v1 & 0x2000)));
          BmsSetCellTemperature(k+1, 0.0122 * ((v2 & 0x1FFF) - (v2 & 0x2000)));
          BmsSetCellTemperature(k+2, 0.0122 * ((v3 & 0x1FFF) - (v3 & 0x2000)));
          BmsSetCellTemperature(k+3, 0.0122 * ((v4 & 0x1FFF) - (v4 & 0x2000)));
          }
        }
      break;
      }
    default:
      break;
    }
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan2(CAN_frame_t* p_frame)
  {
  // Tesla Model S/X CAN6: Chassis

  uint8_t *d = p_frame->data.u8;
  uint8_t b;

  if (m_candata_timer < TS_CANDATA_TIMEOUT)
    {
    StandardMetrics.ms_v_env_awake->SetValue(true);
    }
  m_candata_timer = TS_CANDATA_TIMEOUT;

  switch (p_frame->MsgID)
    {
    case 0x2f8: // MCU GPS speed/heading
      StandardMetrics.ms_v_pos_gpshdop->SetValue((float)d[0] / 10);
      StandardMetrics.ms_v_pos_direction->SetValue((float)(((uint32_t)d[2]<<8)+d[1])/128.0);
      StandardMetrics.ms_v_pos_gpsspeed->SetValue((float)(((uint32_t)d[4]<<8)+d[3])/256.0,Kph);
      if (d[0] < 255)
        {
        StandardMetrics.ms_v_pos_gpslock->SetValue(true);
        StandardMetrics.ms_v_pos_gpsmode->SetValue("TESLA");
        }
      break;
    case 0x318: // GWT CAR state
      b = d[5]>>6;            if (b<2) StandardMetrics.ms_v_door_trunk->SetValue(b);
      b = (d[1] >> 4) & 0x03; if (b<2) StandardMetrics.ms_v_door_fr->SetValue(b);
      b = (d[1] >> 6);        if (b<2) StandardMetrics.ms_v_door_fl->SetValue(b);
      b = (d[2] >> 6);        if (b<2) StandardMetrics.ms_v_door_rr->SetValue(b);
      b = (d[3] >> 5) & 0x03; if (b<2) StandardMetrics.ms_v_door_rl->SetValue(b);
      b = (d[6] >> 2) & 0x03; if (b<2) StandardMetrics.ms_v_door_hood->SetValue(b);
      break;
    case 0x3d8: // MCU GPS latitude / longitude
      StandardMetrics.ms_v_pos_latitude->SetValue((double)(((uint32_t)(d[3]&0x0f) << 24) +
                                                          ((uint32_t)d[2] << 16) +
                                                          ((uint32_t)d[1] << 8) +
                                                          (uint32_t)d[0]) / 1000000.0);
      StandardMetrics.ms_v_pos_longitude->SetValue((double)(((uint32_t)d[6] << 20) +
                                                           ((uint32_t)d[5] << 12) +
                                                           ((uint32_t)d[4] << 4) +
                                                           ((uint32_t)(d[3]&0xf0) >> 4)) / 1000000.0);
      break;
    case 0x31f: // TPMS Baolong tyre pressures + temperatures
      // Note: Baolong TPMS in Model S does not specify which wheel is which, so
      // the numbers may not correspond to the correct wheel
      // (further work needs to be done with this)
      {
      if (d[1]>0) // Front-left
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FL, (float)d[0] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FL, (float)d[1] - 40);
        }
      if (d[3]>0) // Front-right
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_FR, (float)d[2] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_FR, (float)d[3] - 40);
        }
      if (d[5]>0) // Rear-left
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RL, (float)d[4] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RL, (float)d[5] - 40);
        }
      if (d[7]>0) // Rear-right
        {
        StandardMetrics.ms_v_tpms_pressure->SetElemValue(MS_V_TPMS_IDX_RR, (float)d[6] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_temp->SetElemValue(MS_V_TPMS_IDX_RR, (float)d[7] - 40);
        }
      break;
      }
#ifdef CONFIG_OVMS_COMP_TPMS
    case 0x65f: // TPMS Baolong ECU response
      {
      if (m_tpms_cmd == Reading)
        {
        if ((d[0]==0x10)&&(d[1]==0x13)&&(d[2]==0x62)&&(d[3]==0xf0)&&(d[4]==0x90))
          {
          // First frame response
          m_tpms_data[0] = d[5];
          m_tpms_data[1] = d[6];
          m_tpms_data[2] = d[7];
          m_tpms_pos = 3;
          }
        else if (d[0]==0x21)
          {
          // Second frame response
          for (int k=1;(k<8)&&(m_tpms_pos < sizeof(m_tpms_data));k++)
            { m_tpms_data[m_tpms_pos++] = d[k]; }
          }
        else if (d[0]==0x22)
          {
          // Third (last) frame response
          for (int k=1;(k<7)&&(m_tpms_pos < sizeof(m_tpms_data));k++)
            { m_tpms_data[m_tpms_pos++] = d[k]; }
          m_tpms_cmd = DoneReading;
          m_tpms_pos = 0;
          }
        }
      else if (m_tpms_cmd == Writing)
        {
        if ((m_tpms_pos==-30)&&(d[0]==0x06)&&(d[1]==0x50)&&(d[2]==0x03))
          {
          // Confirmation of UDS Diagnostic Session Control
          m_tpms_pos = -21;
          }
        else if ((m_tpms_pos==-20)&&(d[0]==0x04)&&(d[1]==0x67)&&(d[2]==0x01))
          {
          m_tpms_uds_seed = ((uint16_t)d[3]<<8) + d[4];
          m_tpms_pos = -11;
          }
        else if ((m_tpms_pos==-10)&&(d[0]==0x02)&&(d[1]==0x67)&&(d[2]==0x02))
          {
          // Successful UDS security access
          m_tpms_pos = -1;
          }
        else if ((m_tpms_pos==0)&&(d[0]==0x30)&&(d[1]==0x03)&&(d[2]==0x14))
          {
          // Confirmation (from ECU) to start transmission
          m_tpms_pos = 3;
          }
        else if ((m_tpms_pos==100)&&(d[0]==0x03)&&(d[1]==0x6e)&&(d[2]==0xf0)&&(d[3]==0x90))
          {
          // Write is complete and successful
          m_tpms_cmd = DoneWriting;
          m_tpms_pos = 0;
          }
        }
      break;
      }
#endif // #ifdef CONFIG_OVMS_COMP_TPMS
    default:
      break;
    }
  }

void OvmsVehicleTeslaModelS::IncomingFrameCan3(CAN_frame_t* p_frame)
  {
  // Tesla Model S/X CAN4: Body Fault-Tolerant (OVT1 cable)
  }

void OvmsVehicleTeslaModelS::Notify12vCritical()
  { // Not supported on Model S
  }

void OvmsVehicleTeslaModelS::Notify12vRecovered()
  { // Not supported on Model S
  }

void OvmsVehicleTeslaModelS::NotifyBmsAlerts()
  { // Not supported on Model S
  }

#ifdef CONFIG_OVMS_COMP_TPMS

bool OvmsVehicleTeslaModelS::TPMSRead(std::vector<uint32_t> *tpms)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  m_tpms_pos = 0;
  m_tpms_cmd = Reading;

  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x64f;     // TPMS ECU
  frame.data.u8[0] = 0x03; // 3 bytes
  frame.data.u8[1] = 0x22; // Extended read
  frame.data.u8[2] = 0xf0; // PID
  frame.data.u8[3] = 0x90; // PID
  frame.data.u8[4] = 0x55;
  frame.data.u8[5] = 0x55;
  frame.data.u8[6] = 0x55;
  frame.data.u8[7] = 0x55;
  m_can2->Write(&frame);

  for (int k=0; k<20; k++)
    {
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay 100ms
    if (m_tpms_cmd != Reading) break;
    if (m_tpms_pos == 3)
      {
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x30; // Continue read
      frame.data.u8[1] = 0x03; // Continue read
      frame.data.u8[2] = 0x0a; // 10ms interval
      frame.data.u8[3] = 0x00;
      frame.data.u8[4] = 0x00;
      frame.data.u8[5] = 0x00;
      frame.data.u8[6] = 0x00;
      frame.data.u8[7] = 0x00;
      m_can2->Write(&frame);
      }
    }

  if (m_tpms_cmd == DoneReading)
    {
    for (int k=0;k<4;k++)
      {
      int offset = (k*4);
      uint32_t id = ((uint32_t)m_tpms_data[offset] << 24) +
                    ((uint32_t)m_tpms_data[offset+1] << 16) +
                    ((uint32_t)m_tpms_data[offset+2] << 8) +
                    ((uint32_t)m_tpms_data[offset+3]);
      ESP_LOGD(TAG,"TPMS read ID %08x",id);
      tpms->push_back( id );
      }
    return true;
    }

  ESP_LOGE(TAG,"TPMS read timed out");
  return false;
  }

bool OvmsVehicleTeslaModelS::TPMSWrite(std::vector<uint32_t> &tpms)
  {
  CAN_frame_t frame;

  if (tpms.size() != 4)
    {
    ESP_LOGE(TAG,"Tesla Model S only supports TPMS sets with 4 IDs");
    return false;
    }

  m_tpms_cmd = Idle;
  m_tpms_pos = 0;
  for(uint32_t id : tpms)
    {
    ESP_LOGD(TAG,"TPMS write ID %08x",id);
    m_tpms_data[m_tpms_pos++] = (id>>24) & 0xff;
    m_tpms_data[m_tpms_pos++] = (id>>16) & 0xff;
    m_tpms_data[m_tpms_pos++] = (id>>8) & 0xff;
    m_tpms_data[m_tpms_pos++] = id & 0xff;
    }

  memset(&frame,0,sizeof(frame));
  frame.origin = m_can2;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x64f;     // TPMS ECU
  frame.data.u8[0] = 0x02; // 2 bytes
  frame.data.u8[1] = 0x10; // UDS Diagnostic Session Control
  frame.data.u8[2] = 0x03; // Type 3
  m_can2->Write(&frame);

  m_tpms_pos = -30;
  m_tpms_cmd = Writing;

  for (int k=0; k<20; k++)
    {
    vTaskDelay(pdMS_TO_TICKS(100)); // Delay 100ms
    if (m_tpms_cmd != Writing) break;
    if (m_tpms_pos == -21)
      {
      // Write the UDS Security Access seed request
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x02; // 2 bytes
      frame.data.u8[1] = 0x27; // UDS Security Access
      frame.data.u8[2] = 0x01; // Type 1 Seed Request
      m_can2->Write(&frame);
      m_tpms_pos = -20;
      }
    else if (m_tpms_pos == -11)
      {
      // Calculate the UDS Security Access response and send
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x04; // 4 bytes
      frame.data.u8[1] = 0x27; // UDS Security Access
      frame.data.u8[2] = 0x02; // Type 2 Seed Response
      frame.data.u8[3] = (m_tpms_uds_seed >> 8) | 0b01010010;
      frame.data.u8[4] = (m_tpms_uds_seed & 0xff) & 0b01110010;
      m_can2->Write(&frame);
      m_tpms_pos = -10;
      }
    else if (m_tpms_pos == -1)
      {
      // Write the first frame
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x10; // Extended write
      frame.data.u8[1] = 0x13; // Length
      frame.data.u8[2] = 0x2e; // Extended write
      frame.data.u8[3] = 0xf0; // PID
      frame.data.u8[4] = 0x90; // PID
      frame.data.u8[5] = m_tpms_data[0];
      frame.data.u8[6] = m_tpms_data[1];
      frame.data.u8[7] = m_tpms_data[2];
      m_can2->Write(&frame);
      m_tpms_pos = 0;
      }
    else if (m_tpms_pos == 3)
      {
      // Write the second frame
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x21; // Second frame
      frame.data.u8[1] = m_tpms_data[3];
      frame.data.u8[2] = m_tpms_data[4];
      frame.data.u8[3] = m_tpms_data[5];
      frame.data.u8[4] = m_tpms_data[6];
      frame.data.u8[5] = m_tpms_data[7];
      frame.data.u8[6] = m_tpms_data[8];
      frame.data.u8[7] = m_tpms_data[9];
      m_tpms_pos = 10;
      m_can2->Write(&frame);
      }
    else if (m_tpms_pos == 10)
      {
      // Write the third frame
      memset(&frame,0,sizeof(frame));
      frame.origin = m_can2;
      frame.FIR.U = 0;
      frame.FIR.B.DLC = 8;
      frame.FIR.B.FF = CAN_frame_std;
      frame.MsgID = 0x64f;     // TPMS ECU
      frame.data.u8[0] = 0x22; // third frame
      frame.data.u8[1] = m_tpms_data[10];
      frame.data.u8[2] = m_tpms_data[11];
      frame.data.u8[3] = m_tpms_data[12];
      frame.data.u8[4] = m_tpms_data[13];
      frame.data.u8[5] = m_tpms_data[14];
      frame.data.u8[6] = m_tpms_data[15];
      frame.data.u8[7] = 0x00; // Padding
      m_tpms_pos = 100;
      m_can2->Write(&frame);
      }
    }

  if (m_tpms_cmd == DoneWriting)
    {
    return true;
    }

  ESP_LOGE(TAG,"TPMS write timed out");

  return false;
  }

#endif // #ifdef CONFIG_OVMS_COMP_TPMS

class OvmsVehicleTeslaModelSInit
  {
  public: OvmsVehicleTeslaModelSInit();
} MyOvmsVehicleTeslaModelSInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaModelSInit::OvmsVehicleTeslaModelSInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Model S (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaModelS>("TS","Tesla Model S");
  }
