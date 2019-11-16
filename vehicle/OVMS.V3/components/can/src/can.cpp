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
#include "canlog.h"
#include "canplay.h"
#include "dbc.h"
#include "dbc_app.h"
#include <algorithm>
#include <ctype.h>
#include <string.h>
#include <iomanip>
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"

can MyCan __attribute__ ((init_priority (4510)));

////////////////////////////////////////////////////////////////////////
// CAN command processing
////////////////////////////////////////////////////////////////////////

void can_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  int baud = atoi(argv[0]);
  esp_err_t res;

  CAN_mode_t smode = CAN_MODE_LISTEN;
  if (strcmp(mode, "active")==0) smode = CAN_MODE_ACTIVE;

  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  dbcfile *dbcfile = NULL;
  if (argc>1)
    {
    dbcfile = MyDBC.Find(argv[1]);
    if (dbcfile == NULL)
      {
      writer->printf("Error: Could not find dbc file %s\n",argv[1]);
      return;
      }
    if (baud == 0)
      {
      baud = dbcfile->m_bittiming.GetBaudRate();
      }
    }

  switch (baud)
    {
    case 33333:
      res = sbus->Start(smode,CAN_SPEED_33KBPS,dbcfile);
      break;
    case 83333:
      res = sbus->Start(smode,CAN_SPEED_83KBPS,dbcfile);
      break;
    case 100000:
      res = sbus->Start(smode,CAN_SPEED_100KBPS,dbcfile);
      break;
    case 125000:
      res = sbus->Start(smode,CAN_SPEED_125KBPS,dbcfile);
      break;
    case 250000:
      res = sbus->Start(smode,CAN_SPEED_250KBPS,dbcfile);
      break;
    case 500000:
      res = sbus->Start(smode,CAN_SPEED_500KBPS,dbcfile);
      break;
    case 1000000:
      res = sbus->Start(smode,CAN_SPEED_1000KBPS,dbcfile);
      break;
    default:
      writer->puts("Error: Unrecognised speed (33333, 83333, 100000, 125000, 250000, 500000, 1000000 are accepted)");
      return;
    }
  if (res == ESP_OK)
    writer->printf("Can bus %s started in mode %s at speed %dbps\n",
                 bus, mode, baud);
  else
    writer->printf("Failed to start can bus %s\n",bus);
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
  writer->printf("Can bus %s stopped\n",bus);
  }

void can_dbc_attach(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  dbcfile *dbcfile = MyDBC.Find(argv[0]);
  if (dbcfile == NULL)
    {
    writer->printf("Error: Could not find dbc file %s\n",argv[0]);
    return;
    }

  sbus->AttachDBC(dbcfile);
  writer->printf("DBC %s attached to %s\n",argv[0],bus);
  }

void can_dbc_detach(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  sbus->DetachDBC();
  writer->printf("DBC detached from %s\n",bus);
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
  frame.origin = sbus;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  frame.MsgID = (int)strtol(argv[0],NULL,16);
  frame.callback = NULL;
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
  frame.callback = NULL;
  for(int k=0;k<(argc-1);k++)
    {
    frame.data.u8[k] = strtol(argv[k+1],NULL,16);
    }
  MyCan.IncomingFrame(&frame);
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

  sbus->LogStatus(CAN_LogStatus_Statistics);

  writer->printf("CAN:       %s\n",sbus->GetName());
  writer->printf("Mode:      %s\n",(sbus->m_mode==CAN_MODE_OFF)?"Off":
                                   ((sbus->m_mode==CAN_MODE_LISTEN)?"Listen":"Active"));
  writer->printf("Speed:     %d\n",MAP_CAN_SPEED(sbus->m_speed));
  writer->printf("DBC:       %s\n",(sbus->GetDBC())?sbus->GetDBC()->GetName().c_str():"none");
  writer->printf("Interrupts:%20d\n",sbus->m_status.interrupts);
  writer->printf("Rx pkt:    %20d\n",sbus->m_status.packets_rx);
  writer->printf("Rx err:    %20d\n",sbus->m_status.errors_rx);
  writer->printf("Rx ovrflw: %20d\n",sbus->m_status.rxbuf_overflow);
  writer->printf("Tx pkt:    %20d\n",sbus->m_status.packets_tx);
  writer->printf("Tx delays: %20d\n",sbus->m_status.txbuf_delay);
  writer->printf("Tx err:    %20d\n",sbus->m_status.errors_tx);
  writer->printf("Tx ovrflw: %20d\n",sbus->m_status.txbuf_overflow);
  writer->printf("Wdg Resets:%20d\n",sbus->m_status.watchdog_resets);
  writer->printf("Err Resets:%20d\n",sbus->m_status.error_resets);
  if (sbus->m_watchdog_timer>0)
    {
    writer->printf("Wdg Timer: %20d sec(s)\n",monotonictime-sbus->m_watchdog_timer);
    }
  writer->printf("Err flags: 0x%08x\n",sbus->m_status.error_flags);
  }

void can_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (int k=1;k<5;k++)
    {
    static const char* name[4] = {"can1", "can2", "can3", "can4"};
    canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(name[k-1]);
    if (sbus != NULL)
      {
      writer->printf("%s: %s/%d dbc %s\n",
        sbus->GetName(),
        (sbus->m_mode==CAN_MODE_OFF)?"Off":
          ((sbus->m_mode==CAN_MODE_LISTEN)?"Listen":"Active"),
        MAP_CAN_SPEED(sbus->m_speed),
        (sbus->GetDBC())?sbus->GetDBC()->GetName().c_str():"none");
      }
    }
  }

void can_clearstatus(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  sbus->ClearStatus();
  writer->puts("Status cleared");
  }

void can_view_registers(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  if (sbus->ViewRegisters() == ESP_ERR_NOT_SUPPORTED)
    writer->puts("Error: Not implemented");
  else
    writer->puts("See logs for details");
  }

void can_set_register(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (argc<2)
    {
    writer->puts("Error: register address and value must be given");
    return;
    }
  uint32_t addr = (int)strtol(argv[0],NULL,16);
  uint32_t value = (int)strtol(argv[1],NULL,16);
  if (addr > 255)
    {
    writer->puts("Error: Register address out of range");
    return;
    }
  if (value > 255)
    {
    writer->puts("Error: Register value out of range");
    return;
    }

  sbus->WriteReg(addr,value);
  }

////////////////////////////////////////////////////////////////////////
// CAN Filtering (software based filter)
// The canfilter object encapsulates the filtering of CAN frames
////////////////////////////////////////////////////////////////////////

canfilter::canfilter()
  {
  }

canfilter::~canfilter()
  {
  ClearFilters();
  }

void canfilter::ClearFilters()
  {
  for (CAN_filter_t* filter : m_filters)
    {
    delete filter;
    }
  m_filters.clear();
  }

void canfilter::AddFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  CAN_filter_t* f = new CAN_filter_t;
  f->bus = bus;
  f->id_from = id_from;
  f->id_to = id_to;
  m_filters.push_back(f);
  }

void canfilter::AddFilter(const char* filterstring)
  {
  char* fs = (char*)filterstring;
  if (fs[1] == 0)
    {
    AddFilter((uint8_t)fs[0]);
    }
  else
    {
    uint8_t bus = 0;
    uint32_t id_from = 0;
    uint32_t id_to = UINT32_MAX;
    if (fs[1] == ':')
      {
      bus = fs[0];
      fs += 2;
      }
    id_from = strtol(fs, &fs, 16);
    if (*fs)
      id_to = strtol(fs+1, NULL, 16); // id range
    else
      id_to = id_from; // single id
    AddFilter(bus,id_from,id_to);
    }
  }

bool canfilter::RemoveFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  for (CAN_filter_t* filter : m_filters)
    {
    if ((filter->bus == bus)&&
        (filter->id_from == id_from)&&
        (filter->id_to == id_to))
      {
      delete filter;
      return true;
      }
    }
  return false;
  }

bool canfilter::IsFiltered(const CAN_frame_t* p_frame)
  {
  if (m_filters.size() == 0) return true;
  if (! p_frame) return false;

  char buskey = '0';
  if (p_frame->origin) buskey = p_frame->origin->m_busnumber + '1';

  for (CAN_filter_t* filter : m_filters)
    {
    if ((filter->bus)&&(filter->bus != buskey)) continue;
    if ((p_frame->MsgID >= filter->id_from) && (p_frame->MsgID <= filter->id_to))
      return true;
    }

  return false;
  }

bool canfilter::IsFiltered(canbus* bus)
  {
  if (m_filters.size() == 0) return true;
  if (bus == NULL) return true;

  char buskey = bus->GetName()[3];

  for (CAN_filter_t* filter : m_filters)
    {
    if ((filter->bus)&&(filter->bus == buskey)) return true;
    }

  return false;
  }

std::string canfilter::Info()
  {
  std::ostringstream buf;

  for (CAN_filter_t* filter : m_filters)
    {
    if (filter->bus > 0) buf << std::setfill(' ') << std::dec << filter->bus << ':';
    buf << std::setfill('0') << std::setw(3) << std::hex;
    if (filter->id_from == filter->id_to)
      { buf << filter->id_from << ' '; }
    else
      { buf << filter->id_from << '-' << filter->id_to << ' '; }
    }

  return buf.str();
  }

////////////////////////////////////////////////////////////////////////
// CAN logging and tracing
// These structures are involved in formatting, logging and tracing of
// CAN messages (both frames and status/control messages)
////////////////////////////////////////////////////////////////////////

static const char* const CAN_log_type_names[] = {
  "RX",
  "TX",
  "TX_Queue",
  "TX_Fail",
  "Error",
  "Status",
  "Comment",
  "Info",
  "Event"
  };

const char* GetCanLogTypeName(CAN_log_type_t type)
  {
  return CAN_log_type_names[type];
  }

void can::LogFrame(canbus* bus, CAN_log_type_t type, const CAN_frame_t* frame)
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end(); ++it)
    {
    it->second->LogFrame(bus, type, frame);
    }
  }

void can::LogStatus(canbus* bus, CAN_log_type_t type, const CAN_status_t* status)
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end(); ++it)
    {
    it->second->LogStatus(bus, type, status);
    }
  }

void can::LogInfo(canbus* bus, CAN_log_type_t type, const char* text)
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end(); ++it)
    {
    it->second->LogInfo(bus, type, text);
    }
  }

void canbus::LogFrame(CAN_log_type_t type, const CAN_frame_t* frame)
  {
  MyCan.LogFrame(this, type, frame);
  }

void canbus::LogStatus(CAN_log_type_t type)
  {
  if (type==CAN_LogStatus_Error)
    {
    if (!StatusChanged())
      return;
    ESP_LOGE(TAG,
      "%s: intr=%d rxpkt=%d txpkt=%d errflags=%#x rxerr=%d txerr=%d rxovr=%d txovr=%d"
      " txdelay=%d wdgreset=%d errreset=%d",
      m_name, m_status.interrupts, m_status.packets_rx, m_status.packets_tx,
      m_status.error_flags, m_status.errors_rx, m_status.errors_tx,
      m_status.rxbuf_overflow, m_status.txbuf_overflow, m_status.txbuf_delay,
      m_status.watchdog_resets, m_status.error_resets);
    }
  if (MyCan.HasLogger())
    MyCan.LogStatus(this, type, &m_status);
  }

void canbus::LogInfo(CAN_log_type_t type, const char* text)
  {
  MyCan.LogInfo(this, type, text);
  }

bool canbus::StatusChanged()
  {
  // simple checksum to prevent log flooding:
  uint32_t chksum = m_status.errors_rx + m_status.errors_tx
    + m_status.rxbuf_overflow + m_status.txbuf_overflow
    + m_status.error_flags + m_status.txbuf_delay
    + m_status.watchdog_resets + m_status.error_resets;
  if (chksum != m_status_chksum)
    {
    m_status_chksum = chksum;
    return true;
    }
  return false;
  }

uint32_t can::AddLogger(canlog* logger, int filterc, const char* const* filterv)
  {
  if (filterc>0)
    {
    canfilter *filter = new canfilter();
    for (int k=0;k<filterc;k++)
      {
      filter->AddFilter(filterv[k]);
      }
    logger->SetFilter(filter);
    }

  OvmsMutexLock lock(&m_loggermap_mutex);
  uint32_t id = m_logger_id++;
  m_loggermap[id] = logger;

  return id;
  }

bool can::HasLogger()
  {
  return (m_loggermap.size() != 0);
  }

canlog* can::GetLogger(uint32_t id)
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  auto k = m_loggermap.find(id);
  if (k != m_loggermap.end())
    return k->second;
  else
    return NULL;
  }

bool can::RemoveLogger(uint32_t id)
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  auto k = m_loggermap.find(id);
  if (k != m_loggermap.end())
    {
    k->second->Close();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    delete k->second;
    m_loggermap.erase(k);
    return true;
    }
  return false;
  }

void can::RemoveLoggers()
  {
  OvmsMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end();)
    {
    it->second->Close();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    delete it->second;
    it = m_loggermap.erase(it);
    }
  }

uint32_t can::AddPlayer(canplay* player, int filterc, const char* const* filterv)
  {
  if (filterc>0)
    {
    canfilter *filter = new canfilter();
    for (int k=0;k<filterc;k++)
      {
      filter->AddFilter(filterv[k]);
      }
    player->SetFilter(filter);
    }

  OvmsMutexLock lock(&m_playermap_mutex);
  uint32_t id = m_player_id++;
  m_playermap[id] = player;

  return id;
  }

bool can::HasPlayer()
  {
  OvmsMutexLock lock(&m_playermap_mutex);

  return (m_playermap.size() > 0);
  }

canplay* can::GetPlayer(uint32_t id)
  {
  OvmsMutexLock lock(&m_playermap_mutex);

  auto k = m_playermap.find(id);
  if (k != m_playermap.end())
    return k->second;
  else
    return NULL;
  }

bool can::RemovePlayer(uint32_t id)
  {
  OvmsMutexLock lock(&m_playermap_mutex);

  auto k = m_playermap.find(id);
  if (k != m_playermap.end())
    {
    k->second->Close();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    delete k->second;
    m_playermap.erase(k);
    return true;
    }
  return false;
  }

void can::RemovePlayers()
  {
  OvmsMutexLock lock(&m_playermap_mutex);

  for (canplay_map_t::iterator it=m_playermap.begin(); it!=m_playermap.end();)
    {
    it->second->Close();
    vTaskDelay(pdMS_TO_TICKS(100)); // give logger task time to finish
    delete it->second;
    it = m_playermap.erase(it);
    }
  }

////////////////////////////////////////////////////////////////////////
// CAN controller task
////////////////////////////////////////////////////////////////////////

static void CAN_rxtask(void *pvParameters)
  {
  can *me = (can*)pvParameters;
  CAN_queue_msg_t msg;

  while(1)
    {
    if (xQueueReceive(me->m_rxqueue,&msg, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      switch(msg.type)
        {
        case CAN_frame:
          me->IncomingFrame(&msg.body.frame);
          break;
        case CAN_asyncinterrupthandler:
          {
          bool loop;
          // Loop until all interrupts are handled
          do {
            bool receivedFrame;
            loop = msg.body.bus->AsynchronousInterruptHandler(&msg.body.frame, &receivedFrame);
            if (receivedFrame)
              me->IncomingFrame(&msg.body.frame);
            } while (loop);
          break;
          }
        case CAN_txcallback:
          msg.body.bus->TxCallback(&msg.body.frame, true);
          break;
        case CAN_txfailedcallback:
          msg.body.bus->TxCallback(&msg.body.frame, false);
          msg.body.bus->LogStatus(CAN_LogStatus_Error);
          break;
        case CAN_logerror:
          msg.body.bus->LogStatus(CAN_LogStatus_Error);
          break;
        default:
          break;
        }
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// can - the CAN system controller
////////////////////////////////////////////////////////////////////////

can::can()
  {
  ESP_LOGI(TAG, "Initialising CAN (4510)");

  m_logger_id = 1;
  m_player_id = 1;

  MyConfig.RegisterParam("can", "CAN Configuration", true, true);

  OvmsCommand* cmd_can = MyCommandApp.RegisterCommand("can","CAN framework");

  for (int k=0;k<CAN_MAXBUSES;k++) m_buslist[k] = NULL;

  for (int k=1;k<5;k++)
    {
    static const char* name[4] = {"can1", "can2", "can3", "can4"};
    OvmsCommand* cmd_canx = cmd_can->RegisterCommand(name[k-1],"CANx framework");
    OvmsCommand* cmd_canstart = cmd_canx->RegisterCommand("start","CAN start framework");
    cmd_canstart->RegisterCommand("listen","Start CAN bus in listen mode",can_start,"<baud> [<dbc>]", 1, 2);
    cmd_canstart->RegisterCommand("active","Start CAN bus in active mode",can_start,"<baud> [<dbc>]", 1, 2);
    cmd_canx->RegisterCommand("stop","Stop CAN bus",can_stop);
    OvmsCommand* cmd_candbc = cmd_canx->RegisterCommand("dbc","CAN dbc framework");
    cmd_candbc->RegisterCommand("attach","Attach a DBC file to a CAN bus",can_dbc_attach,"<dbc>", 1, 1);
    cmd_candbc->RegisterCommand("detach","Detach the DBC file from a CAN bus",can_dbc_detach);
    OvmsCommand* cmd_cantx = cmd_canx->RegisterCommand("tx","CAN tx framework");
    cmd_cantx->RegisterCommand("standard","Transmit standard CAN frame",can_tx,"<id> <data...>", 1, 9);
    cmd_cantx->RegisterCommand("extended","Transmit extended CAN frame",can_tx,"<id> <data...>", 1, 9);
    OvmsCommand* cmd_canrx = cmd_canx->RegisterCommand("rx","CAN rx framework");
    cmd_canrx->RegisterCommand("standard","Simulate reception of standard CAN frame",can_rx,"<id> <data...>", 1, 9);
    cmd_canrx->RegisterCommand("extended","Simulate reception of extended CAN frame",can_rx,"<id> <data...>", 1, 9);
    cmd_canx->RegisterCommand("status","Show CAN status",can_status);
    cmd_canx->RegisterCommand("clear","Clear CAN status",can_clearstatus);
    cmd_canx->RegisterCommand("viewregisters","view can controller registers",can_view_registers);
    cmd_canx->RegisterCommand("setregister","set can controller register",can_set_register,"<reg> <value>",2,2);
    }

  cmd_can->RegisterCommand("list", "List CAN buses", can_list);

  m_rxqueue = xQueueCreate(CONFIG_OVMS_HW_CAN_RX_QUEUE_SIZE,sizeof(CAN_queue_msg_t));
  xTaskCreatePinnedToCore(CAN_rxtask, "OVMS CanRx", 2*2048, (void*)this, 23, &m_rxtask, CORE(0));
  }

can::~can()
  {
  }

canbus* can::GetBus(int busnumber)
  {
  if ((busnumber<0)||(busnumber>=CAN_MAXBUSES)) return NULL;

  canbus* found = m_buslist[busnumber];
  if (found != NULL) return found;

  char cbus[5];
  strcpy(cbus,"can");
  cbus[3] = busnumber+'1';
  cbus[4] = 0;
  found = (canbus*)MyPcpApp.FindDeviceByName(cbus);
  if (found)
    {
    m_buslist[busnumber] = found;
    }
  return found;
  }

void can::IncomingFrame(CAN_frame_t* p_frame)
  {
  p_frame->origin->m_status.packets_rx++;
  p_frame->origin->m_watchdog_timer = monotonictime;

  ExecuteCallbacks(p_frame, false, true /*ignored*/);
  p_frame->origin->LogFrame(CAN_LogFrame_RX, p_frame);
  NotifyListeners(p_frame, false);
  }

void can::RegisterListener(QueueHandle_t queue, bool txfeedback)
  {
  m_listeners[queue] = txfeedback;
  }

void can::DeregisterListener(QueueHandle_t queue)
  {
  auto it = m_listeners.find(queue);
  if (it != m_listeners.end())
    m_listeners.erase(it);
  }

void can::NotifyListeners(const CAN_frame_t* frame, bool tx)
  {
  for (CanListenerMap_t::iterator it = m_listeners.begin(); it != m_listeners.end(); ++it)
    {
    if (!tx || (tx && it->second))
      xQueueSend(it->first,frame,0);
    }
  }

void can::RegisterCallback(const char* caller, CanFrameCallback callback, bool txfeedback)
  {
  if (txfeedback)
    m_txcallbacks.push_back(new CanFrameCallbackEntry(caller, callback));
  else
    m_rxcallbacks.push_back(new CanFrameCallbackEntry(caller, callback));
  }

void can::DeregisterCallback(const char* caller)
  {
  m_rxcallbacks.remove_if([caller](CanFrameCallbackEntry* entry){ return strcmp(entry->m_caller, caller)==0; });
  m_txcallbacks.remove_if([caller](CanFrameCallbackEntry* entry){ return strcmp(entry->m_caller, caller)==0; });
  }

void can::ExecuteCallbacks(const CAN_frame_t* frame, bool tx, bool success)
  {
  if (tx)
    {
    if (frame->callback)
      {
      (*(frame->callback))(frame, success); // invoke frame-specific callback function
      }
    for (auto entry : m_txcallbacks) {      // invoke generic tx callbacks
      entry->m_callback(frame, success);
      }
    }
  else
    {
    for (auto entry : m_rxcallbacks)
      entry->m_callback(frame, success);
    }
  }

////////////////////////////////////////////////////////////////////////
// canbus - the definition of a CAN bus
////////////////////////////////////////////////////////////////////////

canbus::canbus(const char* name)
  : pcp(name)
  {
  m_busnumber = name[strlen(name)-1] - '1';
  m_txqueue = xQueueCreate(CONFIG_OVMS_HW_CAN_TX_QUEUE_SIZE, sizeof(CAN_frame_t));
  m_mode = CAN_MODE_OFF;
  m_speed = CAN_SPEED_1000KBPS;
  m_dbcfile = NULL;
  ClearStatus();

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.10", std::bind(&canbus::BusTicker10, this, _1, _2));
  }

canbus::~canbus()
  {
  vQueueDelete(m_txqueue);
  }

esp_err_t canbus::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  ClearStatus();
  return ESP_FAIL;
  }

esp_err_t canbus::Start(CAN_mode_t mode, CAN_speed_t speed, dbcfile *dbcfile)
  {
  if (m_dbcfile) DetachDBC();
  if (dbcfile) AttachDBC(dbcfile);
  return Start(mode, speed);
  }

esp_err_t canbus::Stop()
  {
  if (m_dbcfile) DetachDBC();

  return ESP_FAIL;
  }

esp_err_t canbus::ViewRegisters()
  {
  return ESP_ERR_NOT_SUPPORTED;
  }

esp_err_t canbus::WriteReg( uint8_t reg, uint8_t value )
  {
  return ESP_FAIL;
  }

void canbus::ClearStatus()
  {
  memset(&m_status, 0, sizeof(m_status));
  m_status_chksum = 0;
  m_watchdog_timer = monotonictime;
  }

void canbus::AttachDBC(dbcfile *dbcfile)
  {
  if (m_dbcfile) DetachDBC();
  m_dbcfile = dbcfile;
  m_dbcfile->LockFile();
  }

bool canbus::AttachDBC(const char *name)
  {
  if (m_dbcfile) DetachDBC();

  dbcfile *dbcfile = MyDBC.Find(name);
  if (dbcfile == NULL) return false;

  m_dbcfile = dbcfile;
  m_dbcfile->LockFile();
  return true;
  }

void canbus::DetachDBC()
  {
  if (m_dbcfile)
    {
    m_dbcfile->UnlockFile();
    m_dbcfile = NULL;
    }
  }

dbcfile* canbus::GetDBC()
  {
  return m_dbcfile;
  }

void canbus::BusTicker10(std::string event, void* data)
  {
  if ((m_powermode==On)&&(StandardMetrics.ms_v_env_on->AsBool()))
    {
    // Bus is powered on, and vehicle is ON
    if ((monotonictime-m_watchdog_timer) > 60)
      {
      // We have a watchdog timeout
      // We need to reset the CAN bus...
      m_status.watchdog_resets++;
      CAN_mode_t m = m_mode;
      CAN_speed_t s = m_speed;
      CAN_status_t t; memcpy(&t,&m_status,sizeof(CAN_status_t));
      ESP_LOGE(TAG, "%s watchdog inactivity timeout - resetting bus",m_name);
      Stop();
      Start(m, s);
      memcpy(&m_status,&t,sizeof(CAN_status_t));
      m_watchdog_timer = monotonictime;
      }
    }
  else
    {
    // Vehicle is OFF, so just tickle the watchdog timer
    m_watchdog_timer = monotonictime;
    }
  }

bool canbus::AsynchronousInterruptHandler(CAN_frame_t* frame, bool * frameReceived)
  {
  return false;
  }

void canbus::TxCallback(CAN_frame_t* p_frame, bool success)
  {
  if (success)
    {
    m_status.packets_tx++;
    MyCan.NotifyListeners(p_frame, true);
    LogFrame(CAN_LogFrame_TX, p_frame);
    }
  else
    {
    LogFrame(CAN_LogFrame_TX_Fail, p_frame);
    }
  MyCan.ExecuteCallbacks(p_frame, true, success);
  }

/**
 * canbus::Write -- main TX API
 *    - returns ESP_OK, ESP_QUEUED or ESP_FAIL
 *      … ESP_OK = frame delivered to CAN transceiver (not necessarily sent!)
 *      … ESP_FAIL = TX queue is full (TX overflow)
 *    - actual TX implementation in driver override
 */
esp_err_t canbus::Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait /*=0*/)
  {
  tx_frame = *p_frame; // save a local copy of this frame to be used later in txcallback
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
    m_status.txbuf_delay++;
    LogFrame(CAN_LogFrame_TX_Queue, p_frame);
    return ESP_QUEUED;
    }
  else
    {
    m_status.txbuf_overflow++;
    LogFrame(CAN_LogFrame_TX_Fail, p_frame);
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
