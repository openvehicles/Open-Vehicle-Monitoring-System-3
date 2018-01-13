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
    {
    if (argc >= 1)
      {
      sbus->m_trace_id_from = strtol(argv[0], NULL, 16);
      if (argc >= 2)
        sbus->m_trace_id_to = strtol(argv[1], NULL, 16);
      else
        sbus->m_trace_id_to = sbus->m_trace_id_from;
      writer->printf("Tracing for CAN bus %s is now ON for ID range %#x - %#x\n", bus, sbus->m_trace_id_from, sbus->m_trace_id_to);
      }
    else
      {
      sbus->m_trace_id_from = 0;
      sbus->m_trace_id_to = 0xffffffff;
      writer->printf("Tracing for CAN bus %s is now ON\n", bus);
      }
    writer->puts("Use 'log level verbose can' to enable frame output");
    sbus->m_trace = true;
    }
  else
    {
    writer->printf("Tracing for CAN bus %s is now OFF\n", bus);
    sbus->m_trace = false;
    }
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
  writer->printf("Rx ovrflw: %20d\n",sbus->m_errors_rxbuf_overflow);
  writer->printf("Tx pkt:    %20d\n",sbus->m_packets_tx);
  writer->printf("Tx delays: %20d\n",sbus->m_txbuf_delay);
  writer->printf("Tx err:    %20d\n",sbus->m_errors_tx);
  writer->printf("Tx ovrflw: %20d\n",sbus->m_errors_txbuf_overflow);
  writer->printf("Err flags: %#x\n",sbus->m_error_flags);
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
        case CAN_logerror:
          me->LogStatus(CAN_Log_Error, msg.body.bus);
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
    cmd_cantrace->RegisterCommand("on","Turn CAN bus tracing ON",can_trace,"[fromid] [toid]", 0, 2, true);
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
  xTaskCreatePinnedToCore(CAN_rxtask, "CanRxTask", 2048, (void*)this, 10, &m_rxtask, 0);
  }

can::~can()
  {
  }

void can::IncomingFrame(CAN_frame_t* p_frame)
  {
  p_frame->origin->m_packets_rx++;
  
  LogFrame(CAN_Log_RX, p_frame);
  
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
  m_txqueue = xQueueCreate(20, sizeof(CAN_frame_t));
  m_mode = CAN_MODE_OFF;
  m_speed = CAN_SPEED_1000KBPS;
  m_trace = false;
  m_trace_id_from = 0;
  m_trace_id_to = 0xffffffff;
  m_packets_rx = 0;
  m_errors_rx = 0;
  m_packets_tx = 0;
  m_errors_tx = 0;
  m_errors_rxbuf_overflow = 0;
  m_errors_txbuf_overflow = 0;
  m_txbuf_delay = 0;
  m_error_flags = 0;
  m_status_chksum = 0;
  }

canbus::~canbus()
  {
  vQueueDelete(m_txqueue);
  }

esp_err_t canbus::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  // clear statistics:
  m_packets_rx = 0;
  m_errors_rx = 0;
  m_packets_tx = 0;
  m_errors_tx = 0;
  m_errors_rxbuf_overflow = 0;
  m_errors_txbuf_overflow = 0;
  m_txbuf_delay = 0;
  m_error_flags = 0;
  m_status_chksum = 0;
  return ESP_FAIL;
  }

esp_err_t canbus::Stop()
  {
  return ESP_FAIL;
  }

bool canbus::RxCallback(CAN_frame_t* frame)
  {
  return false;
  }

void canbus::TxCallback()
  {
  }

bool canbus::StatusChanged()
  {
  // simple checksum to prevent log flooding:
  uint32_t chksum = m_packets_rx + m_errors_rx + m_packets_tx + m_errors_tx
    + m_errors_rxbuf_overflow + m_errors_txbuf_overflow + m_error_flags
    + m_txbuf_delay;
  if (chksum != m_status_chksum)
    {
    m_status_chksum = chksum;
    return true;
    }
  return false;
  }

/**
 * canbus::Write -- main TX API
 *    - returns ESP_OK, ESP_QUEUED or ESP_FAIL
 *      … ESP_OK = frame delivered to CAN transceiver (not necessarily sent!)
 *      … ESP_FAIL = TX queue is full (TX overflow)
 *    - actual TX implementation in driver override
 *    - here: counting & logging of frames sent (called by driver after a successful TX)
 */
esp_err_t canbus::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  m_packets_tx++;
  MyCan.LogFrame(CAN_Log_TX, p_frame);
  return ESP_OK;
  }

/**
 * canbus::QueueWrite -- add a frame to the TX queue for later delivery
 *    - internal method, called by driver if no TX buffer is available
 */
esp_err_t canbus::QueueWrite(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueSend(m_txqueue, p_frame, maxqueuewait) == pdTRUE)
    {
    m_txbuf_delay++;
    MyCan.LogFrame(CAN_Log_TX_Queue, p_frame);
    return ESP_QUEUED;
    }
  else
    {
    m_errors_txbuf_overflow++;
    MyCan.LogFrame(CAN_Log_TX_Fail, p_frame);
    return ESP_FAIL;
    }
  }

/**
 * canbus::WriteExtended -- application TX utility
 */
esp_err_t canbus::WriteExtended(uint32_t id, uint8_t length, uint8_t *data, TickType_t maxqueuewait /*=0*/)
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
  return this->Write(&frame, maxqueuewait);
  }

/**
 * canbus::WriteStandard -- application TX utility
 */
esp_err_t canbus::WriteStandard(uint16_t id, uint8_t length, uint8_t *data, TickType_t maxqueuewait /*=0*/)
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
  return this->Write(&frame, maxqueuewait);
  }

/**
 * CAN_frame_t::Write -- main TX API
 *    - returns ESP_OK, ESP_QUEUED or ESP_FAIL
 *      … ESP_OK = frame delivered to CAN transceiver (not necessarily sent!)
 *      … ESP_FAIL = TX queue is full (TX overflow)
 */
esp_err_t CAN_frame_t::Write(canbus* bus /*=NULL*/, TickType_t maxqueuewait /*=0*/)
  {
  if (!bus)
    bus = origin;
  return bus ? bus->Write(this, maxqueuewait) : ESP_FAIL;
  }


/*********************************************************************
 * Logging
 *********************************************************************/

const char* const CAN_LogEntryTypeName[] = {
  "RX",
  "TX",
  "TX_Queue",
  "TX_Fail",
  "Error",
  "Status",
  "Comment",
  "Event"
};

const char* can::GetLogEntryTypeName(CAN_LogEntry_t type)
  {
  return CAN_LogEntryTypeName[type];
  }

void can::LogFrame(CAN_LogEntry_t type, const CAN_frame_t* p_frame)
  {
  canbus *bus = p_frame->origin;
  if (bus->m_trace
      && bus->m_trace_id_from <= p_frame->MsgID
      && bus->m_trace_id_to   >= p_frame->MsgID)
    {
    char prefix[60];
    snprintf(prefix, sizeof(prefix), "%s %s id %03x len %d",
      CAN_LogEntryTypeName[type], bus->GetName(), p_frame->MsgID, p_frame->FIR.B.DLC);
    MyCommandApp.HexDump(TAG, prefix, (const char*)p_frame->data.u8, p_frame->FIR.B.DLC, 8);
    }
  }

void can::LogStatus(CAN_LogEntry_t type, canbus* bus)
  {
  if (bus->m_trace && bus->StatusChanged())
    {
    switch (type)
      {
      case CAN_Log_Error:
        ESP_LOGE(TAG, "%s %s rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d",
          CAN_LogEntryTypeName[type], bus->GetName(), bus->m_packets_rx, bus->m_packets_tx, bus->m_error_flags,
          bus->m_errors_rx, bus->m_errors_tx, bus->m_errors_rxbuf_overflow, bus->m_errors_txbuf_overflow,
          bus->m_txbuf_delay);
        break;
      case CAN_Log_Status:
        ESP_LOGD(TAG, "%s %s rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d txdelay=%d",
          CAN_LogEntryTypeName[type], bus->GetName(), bus->m_packets_rx, bus->m_packets_tx, bus->m_error_flags,
          bus->m_errors_rx, bus->m_errors_tx, bus->m_errors_rxbuf_overflow, bus->m_errors_txbuf_overflow,
          bus->m_txbuf_delay);
        break;
      default:
        break;
      }
    }
  }
