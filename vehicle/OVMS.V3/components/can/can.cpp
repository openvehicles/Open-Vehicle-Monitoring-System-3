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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#include "ovms_log.h"
static const char *TAG = "can";

#include "can.h"
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include "ovms_command.h"

can MyCan __attribute__ ((init_priority (4500)));;

void can_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  int baud = atoi(argv[0]);

  CAN_mode_t smode = CAN_MODE_LISTEN;
  if (strcmp(mode, "active")==0) smode = CAN_MODE_ACTIVE;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  switch (baud)
    {
    case 100000:
      sbus->Start(smode,CAN_SPEED_100KBPS);
      break;
    case 125000:
      sbus->Start(smode,CAN_SPEED_125KBPS);
      break;
    case 250000:
      sbus->Start(smode,CAN_SPEED_250KBPS);
      break;
    case 500000:
      sbus->Start(smode,CAN_SPEED_500KBPS);
      break;
    case 1000000:
      sbus->Start(smode,CAN_SPEED_1000KBPS);
      break;
    default:
      writer->puts("Error: Unrecognised speed (100000, 125000, 250000, 500000, 1000000 are accepted)");
      return;
    }
  writer->printf("Can bus %s started in mode %s at speed %dKbps\n",
                 bus, mode, baud);
  }

void can_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  sbus->Stop();
  writer->printf("Can bus %s stapped\n",bus);
  }

void can_tx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  CAN_frame_format_t smode = CAN_frame_std;
  if (strcmp(mode, "extended")==0) smode = CAN_frame_ext;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (sbus->GetPowerMode() != On)
    {
    writer->puts("Error: Can bus is not powered on");
    return;
    }

  CAN_frame_t frame;
  frame.origin = NULL;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  frame.MsgID = (int)strtol(argv[0],NULL,16);
  for(int k=0;k<(argc-1);k++)
    {
    frame.data.u8[k] = strtol(argv[k+1],NULL,16);
    }
  sbus->Write(&frame);
  }

void can_rx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  CAN_frame_format_t smode = CAN_frame_std;
  if (strcmp(mode, "extended")==0) smode = CAN_frame_ext;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (sbus->GetPowerMode() != On)
    {
    writer->puts("Error: Can bus is not powered on");
    return;
    }

  CAN_frame_t frame;
  frame.origin = sbus;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  frame.MsgID = (int)strtol(argv[0],NULL,16);
  for(int k=0;k<(argc-1);k++)
    {
    frame.data.u8[k] = strtol(argv[k+1],NULL,16);
    }
  MyCan.IncomingFrame(&frame);
  }

void can_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  if (strcmp(cmd->GetName(),"on")==0)
    sbus->m_trace = true;
  else
    sbus->m_trace = false;

  writer->printf("Tracing for CAN bus %s is now %s\n",bus,cmd->GetName());
  }

void can_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  writer->printf("CAN:       %s\n",sbus->GetName());
  writer->printf("Mode:      %s\n",(sbus->m_mode==CAN_MODE_OFF)?"Off":
                                   ((sbus->m_mode==CAN_MODE_LISTEN)?"Listen":"Active"));
  writer->printf("Speed:     %d\n",(sbus->m_speed)*1000);
  writer->printf("Rx pkt:    %20d\n",sbus->m_packets_rx);
  writer->printf("Rx err:    %20d\n",sbus->m_errors_rx);
  writer->printf("Tx pkt:    %20d\n",sbus->m_packets_tx);
  writer->printf("Tx err:    %20d\n",sbus->m_errors_tx);
  }

static void CAN_rxtask(void *pvParameters)
  {
  can *me = (can*)pvParameters;
  CAN_msg_t msg;

  while(1)
    {
    if (xQueueReceive(me->m_rxqueue,&msg, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      switch(msg.type)
        {
        case CAN_frame:
          me->IncomingFrame(&msg.body.frame);
          break;
        case CAN_rxcallback:
          while (msg.body.bus->RxCallback(&msg.body.frame))
            {
            me->IncomingFrame(&msg.body.frame);
            }
          break;
        case CAN_txcallback:
          msg.body.bus->TxCallback();
          break;
        default:
          break;
        }
      }
    }
  }

can::can()
  {
  ESP_LOGI(TAG, "Initialising CAN (4500)");

  OvmsCommand* cmd_can = MyCommandApp.RegisterCommand("can","CAN framework",NULL, "", 0, 0, true);
  for (int k=1;k<4;k++)
    {
    static const char* name[4] = {"can1", "can2", "can3"};
    OvmsCommand* cmd_canx = cmd_can->RegisterCommand(name[k-1],"CANx framework",NULL, "", 0, 0, true);
    OvmsCommand* cmd_cantrace = cmd_canx->RegisterCommand("trace","CAN trace framework", NULL, "", 0, 0, true);
    cmd_cantrace->RegisterCommand("on","Turn CAN bus tracing ON",can_trace,"", 0, 0, true);
    cmd_cantrace->RegisterCommand("off","Turn CAN bus tracing OFF",can_trace,"", 0, 0, true);
    OvmsCommand* cmd_canstart = cmd_canx->RegisterCommand("start","CAN start framework", NULL, "", 0, 0, true);
    cmd_canstart->RegisterCommand("listen","Start CAN bus in listen mode",can_start,"<baud>", 1, 1, true);
    cmd_canstart->RegisterCommand("active","Start CAN bus in active mode",can_start,"<baud>", 1, 1, true);
    cmd_canx->RegisterCommand("stop","Stop CAN bus",can_stop, "", 0, 0, true);
    OvmsCommand* cmd_cantx = cmd_canx->RegisterCommand("tx","CAN tx framework", NULL, "", 0, 0, true);
    cmd_cantx->RegisterCommand("standard","Transmit standard CAN frame",can_tx,"<id> <data...>", 1, 9, true);
    cmd_cantx->RegisterCommand("extended","Transmit extended CAN frame",can_tx,"<id> <data...>", 1, 9, true);
    OvmsCommand* cmd_canrx = cmd_canx->RegisterCommand("rx","CAN rx framework", NULL, "", 0, 0, true);
    cmd_canrx->RegisterCommand("standard","Simulate reception of standard CAN frame",can_rx,"<id> <data...>", 1, 9, true);
    cmd_canrx->RegisterCommand("extended","Simulate reception of extended CAN frame",can_rx,"<id> <data...>", 1, 9, true);
    cmd_canx->RegisterCommand("status","Show CAN status",can_status,"", 0, 0, true);
    }

  m_rxqueue = xQueueCreate(20,sizeof(CAN_msg_t));
  xTaskCreatePinnedToCore(CAN_rxtask, "CanRxTask", 2048, (void*)this, 5, &m_rxtask, 1);
  }

can::~can()
  {
  }

void can::IncomingFrame(CAN_frame_t* p_frame)
  {
  p_frame->origin->m_packets_rx++;

  if (p_frame->origin->m_trace)
    {
    MyCommandApp.Log("CAN rx origin %s id %03x (len:%d)",
      p_frame->origin->GetName(),p_frame->MsgID,p_frame->FIR.B.DLC);
    for (int k=0;k<8;k++)
      {
      if (k<p_frame->FIR.B.DLC)
        MyCommandApp.Log(" %02x",p_frame->data.u8[k]);
      else
        MyCommandApp.Log("   ");
      }
    for (int k=0;k<p_frame->FIR.B.DLC;k++)
      {
      if (isprint(p_frame->data.u8[k]))
        MyCommandApp.Log("%c",p_frame->data.u8[k]);
      else
        MyCommandApp.Log(".");
      }
    MyCommandApp.Log("\n");
    }
  for (auto n : m_listeners)
    {
    xQueueSend(n,p_frame,0);
    }
  }

void can::RegisterListener(QueueHandle_t queue)
  {
  m_listeners.push_back(queue);
  }

void can::DeregisterListener(QueueHandle_t queue)
  {
  auto it = std::find(m_listeners.begin(), m_listeners.end(), queue);
  if (it != m_listeners.end())
    {
    m_listeners.erase(it);
    }
  }

canbus::canbus(const char* name)
  : pcp(name)
  {
  m_mode = CAN_MODE_OFF;
  m_speed = CAN_SPEED_1000KBPS;
  m_trace = false;
  m_packets_rx = 0;
  m_errors_rx = 0;
  m_packets_tx = 0;
  m_errors_tx = 0;
  }

canbus::~canbus()
  {
  }

esp_err_t canbus::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  return ESP_FAIL; // Not implemented by base implementation
  }

esp_err_t canbus::Stop()
  {
  return ESP_FAIL; // Not implemented by base implementation
  }

esp_err_t canbus::Write(const CAN_frame_t* p_frame)
  {
  if (m_trace)
    {
    MyCommandApp.Log("CAN tx origin %s id %03x (len:%d)",
      GetName(),p_frame->MsgID,p_frame->FIR.B.DLC);
    for (int k=0;k<8;k++)
      {
      if (k<p_frame->FIR.B.DLC)
        MyCommandApp.Log(" %02x",p_frame->data.u8[k]);
      else
        MyCommandApp.Log("   ");
      }
    for (int k=0;k<p_frame->FIR.B.DLC;k++)
      {
      if (isprint(p_frame->data.u8[k]))
        MyCommandApp.Log("%c",p_frame->data.u8[k]);
      else
        MyCommandApp.Log(".");
      }
    MyCommandApp.Log("\n");
    }

  m_packets_tx++;

  return ESP_OK; // Not implemented by base implementation
  }

esp_err_t canbus::WriteExtended(uint32_t id, uint8_t length, uint8_t *data)
  {
  if (length > 8)
    {
    abort();
    }

  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = this;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = length;
  frame.FIR.B.FF = CAN_frame_ext;
  frame.MsgID = id;
  memcpy(frame.data.u8, data, length);
  return this->Write(&frame);
  }

esp_err_t canbus::WriteStandard(uint16_t id, uint8_t length, uint8_t *data)
  {
  if (length > 8)
    {
    abort();
    }

  CAN_frame_t frame;
  memset(&frame, 0, sizeof(frame));

  frame.origin = this;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = length;
  frame.FIR.B.FF = CAN_frame_std;
  frame.MsgID = id;
  memcpy(frame.data.u8, data, length);
  return this->Write(&frame);
  }


bool canbus::RxCallback(CAN_frame_t* frame)
  {
  return false;
  }

void canbus::TxCallback()
  {
  }

