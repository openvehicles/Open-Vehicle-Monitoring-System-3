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
static const char *TAG = "v-teslaroadster";

#include <stdio.h>
#include <string.h>
#include "pcp.h"
#include "vehicle_teslaroadster.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

float TeslaRoadsterLatLon(int v)
  {
  return (((float)v) / 2048 / 3600);
  }

OvmsVehicleTeslaRoadster::OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Tesla Roadster v1.x, v2.x and v3.0 vehicle module");

  memset(m_vin,0,sizeof(m_vin));

  RegisterCanBus(1,CAN_MODE_ACTIVE,CAN_SPEED_1000KBPS);
  }

OvmsVehicleTeslaRoadster::~OvmsVehicleTeslaRoadster()
  {
  ESP_LOGI(TAG, "Shutdown Tesla Roadster vehicle module");
  }

void OvmsVehicleTeslaRoadster::IncomingFrameCan1(CAN_frame_t* p_frame)
  {
  uint8_t *d = p_frame->data.u8;

  switch (p_frame->MsgID)
    {
    case 0x100: // VMS->VDS
      {
      switch (d[0])
        {
        case 0x06: // Timer/Plugin setting and timer confirmation
          {
          if (d[0] == 0x1b)
            {
            StandardMetrics.ms_v_charge_timermode->SetValue(d[4] != 0);
            }
          else if (d[0] == 0x1a)
            {
            StandardMetrics.ms_v_charge_timerstart->SetValue(((int)d[4]<<8)+d[5]);
            }
          break;
          }
        case 0x80: // Range / State of Charge
          {
          StandardMetrics.ms_v_bat_soc->SetValue(d[1]);
          int idealrange = (int)d[2] + ((int)d[3]<<8);
          if (idealrange>6000) idealrange=0; // Sanity check (limit rng->std)
          StandardMetrics.ms_v_bat_range_ideal->SetValue(idealrange, Miles);
          int estrange = (int)d[6] + ((int)d[7]<<8);
          if (estrange>6000) estrange=0; // Sanity check (limit rng->std)
          StandardMetrics.ms_v_bat_range_est->SetValue(estrange, Miles);
          break;
          }
        case 0x81: // Time/Date UTC
          {
          StandardMetrics.ms_m_timeutc->SetValue(((int)d[7]<<24)+
                                                 ((int)d[6]<<16)+
                                                 ((int)d[5]<<8)+
                                                 d[4]);
          break;
          }
        case 0x82: // Ambient Temperature
          {
          StandardMetrics.ms_v_env_temp->SetValue(d[1]);
          break;
          }
        case 0x83: // GPS Latitude
          {
          StandardMetrics.ms_v_pos_latitude->SetValue(
            TeslaRoadsterLatLon(((int)d[7]<<24)+
                                ((int)d[6]<<16)+
                                ((int)d[5]<<8)+
                                d[4]));
          break;
          }
        case 0x84: // GPS Longitude
          {
          StandardMetrics.ms_v_pos_longitude->SetValue(
            TeslaRoadsterLatLon(((int)d[7]<<24)+
                                ((int)d[6]<<16)+
                                ((int)d[5]<<8)+
                                d[4]));
          break;
          }
        case 0x85: // GPS direction and altitude
          {
          StandardMetrics.ms_v_pos_gpslock->SetValue(d[1] == 1);
          StandardMetrics.ms_v_pos_direction->SetValue((((int)d[3]<<8)+d[2])%360);
          if (d[5] & 0xf0)
            StandardMetrics.ms_v_pos_altitude->SetValue(0);
          else
            StandardMetrics.ms_v_pos_altitude->SetValue(((int)d[5]<<8)+d[4]);
          break;
          }
        case 0x88: // Charging Current / Duration
          {
          StandardMetrics.ms_v_charge_current->SetValue((float)d[1]);
          StandardMetrics.ms_v_charge_climit->SetValue((float)d[6]);
          StandardMetrics.ms_v_charge_minutes->SetValue(((int)d[3]<<8)+d[2]);
          break;
          }
        case 0x89:  // Charging Voltage / Iavailable
          {
          StandardMetrics.ms_v_pos_speed->SetValue((float)d[1],Mph);
          // https://en.wikipedia.org/wiki/Tesla_Roadster#Transmission
          // Motor RPM given as 14,000rpm for top speed 125mph (201kph) = 112rpm/mph
          StandardMetrics.ms_v_mot_rpm->SetValue((int)d[1]*112);
          StandardMetrics.ms_v_charge_voltage->SetValue(((float)d[3]*256)+d[2]);
          break;
          }
        case 0x8f: // HVAC#1 message
          {
          StandardMetrics.ms_v_env_hvac->SetValue((((int)d[7]<<8)+d[6]) > 0);
          break;
          }
        case 0x95: // Charging mode
          {
          switch (d[1])
            {
            case 0x01: // Charging
              StandardMetrics.ms_v_charge_state->SetValue("charging"); break;
            case 0x02: // Top-off
              StandardMetrics.ms_v_charge_state->SetValue("topoff"); break;
            case 0x04: // done
              StandardMetrics.ms_v_charge_state->SetValue("done"); break;
            case 0x0c: // interrupted
            case 0x15: // interrupted
            case 0x16: // interrupted
            case 0x17: // interrupted
            case 0x18: // interrupted
            case 0x19: // interrupted
              StandardMetrics.ms_v_charge_state->SetValue("stopped"); break;
            case 0x0d: // preparing
              StandardMetrics.ms_v_charge_state->SetValue("prepare"); break;
            case 0x0e: // timer wait
              StandardMetrics.ms_v_charge_state->SetValue("timerwait"); break;
            case 0x0f: // heating
              StandardMetrics.ms_v_charge_state->SetValue("heating"); break;
            default:
              break;
            }
          switch (d[2])
            {
            case 0x02: // Scheduled start
              StandardMetrics.ms_v_charge_substate->SetValue("scheduledstart"); break;
            case 0x03: // On request
              StandardMetrics.ms_v_charge_substate->SetValue("onrequest"); break;
            case 0x05: // Timer wait
              StandardMetrics.ms_v_charge_substate->SetValue("timerwait"); break;
            case 0x07: // Power wait
              StandardMetrics.ms_v_charge_substate->SetValue("powerwait"); break;
            case 0x0d: // interrupted
              StandardMetrics.ms_v_charge_substate->SetValue("stopped"); break;
            case 0x09: // xxMinutes - yykWh
              break;
            default:
              break;
            }
          StandardMetrics.ms_v_charge_kwh->SetValue((float)d[7]*10);
          break;
          }
        case 0x96: // Doors / Charging yes/no
          {
          StandardMetrics.ms_v_door_fl->SetValue(d[1] & 0x01);
          StandardMetrics.ms_v_door_fr->SetValue(d[1] & 0x02);
          StandardMetrics.ms_v_door_chargeport->SetValue(d[1] & 0x04);
          StandardMetrics.ms_v_charge_pilot->SetValue(d[1] & 0x08);
          StandardMetrics.ms_v_charge_inprogress->SetValue(d[1] & 0x10);
          StandardMetrics.ms_v_env_handbrake->SetValue(d[1] & 0x40);
          StandardMetrics.ms_v_env_on->SetValue(d[1] & 0x80);
          StandardMetrics.ms_v_env_locked->SetValue(d[2] & 0x08);
          StandardMetrics.ms_v_env_valet->SetValue(d[2] & 0x10);
          StandardMetrics.ms_v_env_headlights->SetValue(d[2] & 0x20);
          StandardMetrics.ms_v_door_hood->SetValue(d[2] & 0x40);
          StandardMetrics.ms_v_door_trunk->SetValue(d[2] & 0x80);
          StandardMetrics.ms_v_env_awake->SetValue(d[3] & 0x01);
          StandardMetrics.ms_v_env_cooling->SetValue(d[3] & 0x02);
          StandardMetrics.ms_v_env_alarm->SetValue(d[4] & 0x02);
          break;
          }
        case 0x9e: // CAC
          {
          StandardMetrics.ms_v_bat_cac->SetValue((float)d[3] + ((float)d[2]/256));
          break;
          }
        case 0xA3: // PEM, MOTOR, BATTERY temperatures
          {
          StandardMetrics.ms_v_inv_temp->SetValue(d[1]);
          StandardMetrics.ms_v_charge_temp->SetValue(d[1]);
          StandardMetrics.ms_v_mot_temp->SetValue(d[2]);
          StandardMetrics.ms_v_bat_temp->SetValue(d[6]);
          break;
          }
        case 0xA4: // 7 bytes start of VIN bytes i.e. "SFZRE2B"
          {
          memcpy(m_vin,d+1,7);
          break;
          }
        case 0xA5: // 7 bytes mid of VIN bytes i.e. "39A3000"
          {
          memcpy(m_vin+7,d+1,7);
          m_type[0] = 'T';
          m_type[1] = 'R';
          if ((d[3]=='A')||(d[3]=='B')) m_type[2] = '2'; else m_type[2] = '1';
          if (d[1] == '3') m_type[3] = 'S'; else m_type[3] = 'N';
          m_type[4] = 0;
          StandardMetrics.ms_v_type->SetValue(m_type);
          break;
          }
        case 0xA6: // 3 bytes last of VIN bytes i.e. "359"
          {
          if ((m_vin[0] != 0)&&(m_vin[7] != 0))
            {
            memcpy(m_vin+14,p_frame->data.u8+1,3);
            m_vin[17] = 0;
            StandardMetrics.ms_v_vin->SetValue(m_vin);
            }
          break;
          }
        default:
          break;
        }
      break;
      }
    case 0x102: // Messages from VDS
      {
      switch (d[0])
        {
        case 0x0e: // Lock/Unlock state on ID#102
          {
          StandardMetrics.ms_v_env_locked->SetValue(d[1] == 0x04);
          break;
          }
        }
      break;
      }
    case 0x344: // TPMS
      {
      if (d[1]>0) // Front-left
        {
        StandardMetrics.ms_v_tpms_fl_p->SetValue((float)d[0] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_fl_t->SetValue((float)d[1] - 40);
        }
      if (d[3]>0) // Front-right
        {
        StandardMetrics.ms_v_tpms_fr_p->SetValue((float)d[2] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_fr_t->SetValue((float)d[3] - 40);
        }
      if (d[5]>0) // Rear-left
        {
        StandardMetrics.ms_v_tpms_rl_p->SetValue((float)d[4] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_rl_p->SetValue((float)d[5] - 40);
        }
      if (d[7]>0) // Rear-right
        {
        StandardMetrics.ms_v_tpms_rr_p->SetValue((float)d[6] / 2.755, PSI);
        StandardMetrics.ms_v_tpms_rr_p->SetValue((float)d[7] - 40);
        }
      break;
      }
    case 0x400:
      {
      break;
      }
    case 0x402:
      {
      unsigned int odometer = (unsigned int)d[3]
                            + ((unsigned int)d[4]<<8)
                            + ((unsigned int)d[5]<<16);
      StandardMetrics.ms_v_pos_odometer->SetValue((float)odometer/10, Miles);
      unsigned int trip = (unsigned int)d[6]
                        + ((unsigned int)d[7]<<8);
      StandardMetrics.ms_v_pos_trip->SetValue((float)trip/10, Miles);
      break;
      }
    }
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeMode(vehicle_mode_t mode)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x19;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = (uint8_t)mode;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeCurrent(uint16_t limit)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x02;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = (uint8_t)limit;
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandStartCharge()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x03;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 1;    // Start charge
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandStopCharge()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x05;
  frame.data.u8[1] = 0x03;
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = 0;    // Stop charge
  frame.data.u8[5] = 0x00;
  frame.data.u8[6] = 0x00;
  frame.data.u8[7] = 0x00;
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandSetChargeTimer(bool timeron, uint16_t timerstart)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandCooldown(bool cooldownon)
  {
  return NotImplemented;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandWakeup()
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 1;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0a;
  m_can1->Write(&frame);

  frame.origin = m_can1;
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
  m_can1->Write(&frame);
  
  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandLock(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 2;   // Lock car
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandUnlock(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 3;   // Unlock car
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandActivateValet(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 0;   // Activate valet mode
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandDeactivateValet(const char* pin)
  {
  long lpin = atol(pin);

  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x0b;
  frame.data.u8[1] = 1;   // Deactivate valet mode
  frame.data.u8[2] = 0x00;
  frame.data.u8[3] = 0x00;
  frame.data.u8[4] = lpin & 0xff;
  frame.data.u8[5] = (lpin>>8) & 0xff;
  frame.data.u8[6] = (lpin>>16) & 0xff;
  frame.data.u8[7] = (strlen(pin)<<4) + ((lpin>>24) & 0x0f);
  m_can1->Write(&frame);

  return Success;
  }

OvmsVehicle::vehicle_command_t OvmsVehicleTeslaRoadster::CommandHomelink(uint8_t button)
  {
  CAN_frame_t frame;
  memset(&frame,0,sizeof(frame));

  frame.origin = m_can1;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 3;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = 0x102;
  frame.data.u8[0] = 0x09;
  frame.data.u8[1] = 0x00;
  frame.data.u8[2] = button;
  m_can1->Write(&frame);

  return Success;
  }

class OvmsVehicleTeslaRoadsterInit
  {
  public: OvmsVehicleTeslaRoadsterInit();
} MyOvmsVehicleTeslaRoadsterInit  __attribute__ ((init_priority (9000)));

OvmsVehicleTeslaRoadsterInit::OvmsVehicleTeslaRoadsterInit()
  {
  ESP_LOGI(TAG, "Registering Vehicle: Tesla Roadster (9000)");

  MyVehicleFactory.RegisterVehicle<OvmsVehicleTeslaRoadster>("TR","Tesla Roadster");
  }
