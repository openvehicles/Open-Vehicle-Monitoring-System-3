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
#include <cstdio>
#include "ovms_config.h"
#include "ovms_command.h"
#include "metrics_standard.h"
#include "vehicle_poller.h"

#if defined(CONFIG_OVMS_COMP_ESP32CAN) || \
    defined(CONFIG_OVMS_COMP_MCP2515) || \
    defined(CONFIG_OVMS_COMP_EXTERNAL_SWCAN)
static const bool includeCAN = true;
#else
static const bool includeCAN = false;
#endif

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

  CAN_mode_t smode = CAN_MODE_LISTEN;
  if (strcmp(mode, "active")==0) smode = CAN_MODE_ACTIVE;

  CAN_speed_t cspeed = CAN_SPEED_33KBPS;
  switch (baud)
    {
    case 33333:
      cspeed = CAN_SPEED_33KBPS;
      break;
    case 50000:
      cspeed = CAN_SPEED_50KBPS;
      break;
    case 83333:
      cspeed = CAN_SPEED_83KBPS;
      break;
    case 100000:
      cspeed = CAN_SPEED_100KBPS;
      break;
    case 125000:
      cspeed = CAN_SPEED_125KBPS;
      break;
    case 250000:
      cspeed = CAN_SPEED_250KBPS;
      break;
    case 500000:
      cspeed = CAN_SPEED_500KBPS;
      break;
    case 1000000:
      cspeed = CAN_SPEED_1000KBPS;
      break;
    default:
      writer->puts("Error: Unrecognised speed (33333, 50000, 83333, 100000, 125000, 250000, 500000, 1000000 are accepted)");
      return;
    }
#ifdef CONFIG_OVMS_COMP_POLLER
  canbus* sbus;
  unsigned int busno;
  if (std::sscanf(bus, "can%u", &busno) != 1)
    {
    writer->puts("Error: invalid can bus number");
    return;
    }
  res = MyPollers.RegisterCanBus(busno, smode, cspeed, dbcfile, OvmsPollers::BusPoweroff::System, sbus, verbosity, writer);
#else
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  res = sbus->Start(smode,cspeed,dbcfile);
#endif

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

void can_reset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }
  if (sbus->Reset() == ESP_OK)
    writer->printf("Can bus %s has been reset\n",bus);
  else
    writer->printf("ERROR: failed to reset can bus %s\n",bus);
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
  uint32_t idmax = (1 << 11) - 1;
  if (strcmp(mode, "extended")==0)
    {
    smode = CAN_frame_ext;
    idmax = (1 << 29) - 1;
    }

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

  CAN_frame_t frame = {};
  frame.origin = sbus;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = argc-1;
  frame.FIR.B.FF = smode;
  char* ep;
  uint32_t uv = strtoul(argv[0], &ep, 16);
  if (*ep != '\0' || uv > idmax)
    {
    writer->printf("Error: Invalid CAN ID \"%s\" (0x%" PRIx32 " max)\n", argv[0], idmax);
    return;
    }
  frame.MsgID = uv;
  frame.callback = NULL;
  for(int k=0;k<(argc-1);k++)
    {
    uv = strtoul(argv[k+1], &ep, 16);
    if (*ep != '\0' || uv > 0xff)
      {
      writer->printf("Error: Invalid CAN octet \"%s\"\n", argv[k+1]);
      return;
      }
    frame.data.u8[k] = uv;
    }
  sbus->Write(&frame, pdMS_TO_TICKS(500));
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

  CAN_frame_t frame = {};
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

void can_testtx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetParent()->GetName();
  const char* mode = cmd->GetName();
  CAN_frame_format_t smode = CAN_frame_std;
  uint32_t idmax = (1 << 11) - 1;
  if (strcmp(mode, "extended")==0)
    {
    smode = CAN_frame_ext;
    idmax = (1 << 29) - 1;
    }

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

  CAN_frame_t frame = {};
  memset(&frame, 0, sizeof(frame));
  frame.origin = sbus;
  frame.FIR.U = 0;
  frame.FIR.B.DLC = 8;
  frame.FIR.B.FF = smode;
  char* ep;
  uint32_t uv = strtoul(argv[0], &ep, 16);
  if (*ep != '\0' || uv > idmax)
    {
    writer->printf("Error: Invalid CAN ID \"%s\" (0x%" PRIx32 " max)\n", argv[0], idmax);
    return;
    }
  frame.MsgID = uv;
  frame.callback = NULL;

  uint32_t count = strtoul(argv[1], &ep, 10);
  uint32_t delayms = strtoul(argv[2], &ep, 10);

  for (uint32_t k=0;k<count;k++)
    {
    frame.data.u8[0] = (uint8_t)k;
    sbus->Write(&frame, pdMS_TO_TICKS(500));
    if (delayms != 0) vTaskDelay(pdMS_TO_TICKS(delayms));
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

  sbus->LogStatus(CAN_LogStatus_Statistics);

  writer->printf("CAN:       %s\n",sbus->GetName());
  writer->printf("Mode:      %s\n",(sbus->m_mode==CAN_MODE_OFF)?"Off":
                                   ((sbus->m_mode==CAN_MODE_LISTEN)?"Listen":"Active"));
  writer->printf("Speed:     %d\n",MAP_CAN_SPEED(sbus->m_speed));
  writer->printf("DBC:       %s\n",(sbus->GetDBC())?sbus->GetDBC()->GetName().c_str():"none");

  writer->printf("\nInterrupts:%20" PRId32 "\n",sbus->m_status.interrupts);
  writer->printf("Rx pkt:    %20" PRId32 "\n",sbus->m_status.packets_rx);
  writer->printf("Rx ovrflw: %20d\n",sbus->m_status.rxbuf_overflow);
  writer->printf("Tx pkt:    %20" PRId32 "\n",sbus->m_status.packets_tx);
  writer->printf("Tx queue:  %20" PRId32 "\n",uxQueueMessagesWaiting(sbus->m_txqueue));
  writer->printf("Tx delays: %20" PRId32 "\n",sbus->m_status.txbuf_delay);
  writer->printf("Tx ovrflw: %20d\n",sbus->m_status.txbuf_overflow);
  writer->printf("Tx fails:  %20" PRId32 "\n",sbus->m_status.tx_fails);

  writer->printf("\nErr flags: 0x%08" PRIx32 "\n",sbus->m_status.error_flags);
  std::string efdesc;
  if (sbus->GetErrorFlagsDesc(efdesc, sbus->m_status.error_flags))
    {
    replace_substrings(efdesc, "\n", "\n        ");
    writer->printf("        %s\n", efdesc.c_str());
    }
  writer->printf("Rx err:    %20d\n",sbus->m_status.errors_rx);
  writer->printf("Tx err:    %20d\n",sbus->m_status.errors_tx);
  writer->printf("Rx invalid:%20d\n",sbus->m_status.invalid_rx);
  writer->printf("Wdg Resets:%20d\n",sbus->m_status.watchdog_resets);
  if (sbus->m_watchdog_timer>0)
    {
    writer->printf("Wdg Timer: %20" PRId32 " sec(s)\n",monotonictime-sbus->m_watchdog_timer);
    }
  writer->printf("Err Resets:%20d\n",sbus->m_status.error_resets);
  }

void can_explain_flags(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* bus = cmd->GetParent()->GetName();
  canbus* sbus = (canbus*)MyPcpApp.FindDeviceByName(bus);
  if (sbus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  uint32_t error_flags = 0;
  if (argc == 0)
    {
    error_flags = sbus->m_status.error_flags;
    }
  else
    {
    const char *hex = argv[0];
    if (hex[0] == '0' && hex[1] == 'x') hex += 2;
    sscanf(hex, "%x", &error_flags);
    }

  std::string efdesc;
  if (sbus->GetErrorFlagsDesc(efdesc, error_flags))
    {
    writer->printf("Bus error flags 0x%08" PRIx32 ":\n", error_flags);
    writer->printf("%s\n", efdesc.empty() ? "No error indication" : efdesc.c_str());
    }
  else
    {
    writer->puts("ERROR: bus does not provide error flag decoding");
    }
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

/** Add a filter to the list (what is allowed).
 * @param bus The CAN bus 1-3  or 0 to match any.
 * @param id_from The id to match from
 * @param id_to The id to match to
 */

bool canfilter::AddFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  if (bus > CAN_MAXBUSES)
    return false;
  if (id_from > id_to)
    return false;
  // Backwards compatible. Used to be '1' (49) for bus 1 etc
  if (bus >= (uint8_t)'0')
    bus -= '0';
  if (bus > CAN_MAXBUSES)
    return false;

  CAN_filter_t* f = new CAN_filter_t;
  f->bus = bus;
  f->id_from = id_from;
  f->id_to = id_to;
  m_filters.push_back(f);
  return true;
  }
/** Add a filter string.
 * The format is: <bus>[:<from>[-[<to>]]]
 * Where <bus> is 0 to match any or 1 to CAN_MAXBUSES,
 * and 'from' and 'to' are in hex.
 * <bus>          Matches anything on that bus.
 * <bus>:<value>  Matches 1 address on the specified bus.
 * <bus>:<from>-  Matches adressses greater than the specified value on that bus
 * <bus>:<from>-<to>  Matches addresses  'from' <= adress <= 'to' on that bus
 * <value>        Matches 1 address on any bus.
 * <from>-        Matches adressses greater than the specified value on any bus
 * <from>-<to>    Matches addresses  'from' <= adress <= 'to' on any bus
 */
bool canfilter::AddFilter(const char* filterstring)
  {
  if (!*filterstring)
    return false;
  uint32_t bus, id_from, id_to;
  char sep; // Consume '-' - but don't care exactly what it is.
  int level;

  switch (filterstring[1])
    {
    // first character is a bus
    case '\0':
    case ':':
      level = sscanf(filterstring, "%" SCNu32 ":%" SCNx32 "%c%" SCNx32, &bus, &id_from, &sep, &id_to);
      break;
    // No bus
    default:
      {
      level = sscanf(filterstring, "%" SCNx32 "%c%" SCNx32,  &id_from, &sep, &id_to);
      if (level == 0)
        return false;
      bus = 0;
      ++level;
      break;
      }
    }

  switch (level)
    {
    case 1: // eg: 1
      id_from = 0;
      id_to = UINT32_MAX;
      break;
    case 2: // eg: 1:200 or 200
      id_to = id_from;
      break;
    case 3: // eg: 1:200- or 200-
      id_to = UINT32_MAX;
      break;
    case 4: // eg: 1:200-7E0 or 200-7E0
      break;
    default:
      return false;
    }
  return AddFilter(bus, id_from, id_to);
  }

bool canfilter::RemoveFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  if (bus >= (uint8_t)'0')
    bus -= '0';
  for (CAN_filter_list_t::iterator it = m_filters.begin(); it != m_filters.end(); ++it)
    {
    CAN_filter_t* filter = *it;
    if ((filter->bus == bus) &&
        (filter->id_from == id_from) &&
        (filter->id_to == id_to))
      {
      delete filter;
      m_filters.erase(it);
      return true;
      }
    }
  return false;
  }

bool canfilter::IsFiltered(const CAN_frame_t* p_frame)
  {
  if (m_filters.size() == 0) return true;
  if (! p_frame) return false;

  uint8_t buskey = 0;
  if (p_frame->origin)
    buskey = (p_frame->origin->m_busnumber + 1);

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

  uint8_t buskey = bus->m_busnumber+1;

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
    if (filter->bus > 0) buf << std::setfill(' ') << std::dec << char('0'+ filter->bus) << ':';
    buf << std::setfill('0') << std::setw(3) << std::hex;
    if (filter->id_from == filter->id_to)
      buf << filter->id_from << ' ';
    else if (filter->id_to < UINT32_MAX)
      buf << filter->id_from << '-' << filter->id_to << ' ';
    else if (filter->id_to > 0)
      buf << filter->id_from << "- ";
    else
      buf << ' ';
    }

  return buf.str();
  }

////////////////////////////////////////////////////////////////////////
// CAN logging and tracing
// These structures are involved in formatting, logging and tracing of
// CAN messages (both frames and status/control messages)
////////////////////////////////////////////////////////////////////////

static const char* const CAN_log_type_names[] = {
  "-",
  "RX",
  "TX",
  "TX_Queue",
  "TX_Fail",
  "Error",
  "Status",
  "Comment",
  "Info",
  "Event",
  "Metric"
  };

const char* GetCanLogTypeName(CAN_log_type_t type)
  {
  return CAN_log_type_names[type];
  }

void can::LogFrame(canbus* bus, CAN_log_type_t type, const CAN_frame_t* frame)
  {
  OvmsRecMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end(); ++it)
    {
    it->second->LogFrame(bus, type, frame);
    }
  }

void can::LogStatus(canbus* bus, CAN_log_type_t type, const CAN_status_t* status)
  {
  OvmsRecMutexLock lock(&m_loggermap_mutex);

  for (canlog_map_t::iterator it=m_loggermap.begin(); it!=m_loggermap.end(); ++it)
    {
    it->second->LogStatus(bus, type, status);
    }
  }

void can::LogInfo(canbus* bus, CAN_log_type_t type, const char* text)
  {
  OvmsRecMutexLock lock(&m_loggermap_mutex);

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
      "%s: intr=%" PRId32 " rxpkt=%" PRId32 " txpkt=%" PRId32 " errflags=%#" PRIx32 " rxerr=%d txerr=%d rxinval=%d"
      " rxovr=%d txovr=%d txdelay=%" PRId32 " txfail=%" PRId32 " wdgreset=%d errreset=%d txqueue=%" PRId32,
      m_name, m_status.interrupts, m_status.packets_rx, m_status.packets_tx,
      m_status.error_flags, m_status.errors_rx, m_status.errors_tx,
      m_status.invalid_rx, m_status.rxbuf_overflow, m_status.txbuf_overflow,
      m_status.txbuf_delay, m_status.tx_fails, m_status.watchdog_resets,
      m_status.error_resets, uxQueueMessagesWaiting(m_txqueue));
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
    + m_status.invalid_rx + m_status.rxbuf_overflow + m_status.txbuf_overflow
    + m_status.error_flags + m_status.txbuf_delay + m_status.tx_fails
    + m_status.watchdog_resets + m_status.error_resets;
  if (chksum != m_status_chksum)
    {
    m_status_chksum = chksum;
    return true;
    }
  return false;
  }

static const char* const CAN_errorstate_names[] = {
  "none",
  "active",
  "warning",
  "passive",
  "busoff"
  };

const char* GetCanErrorStateName(CAN_errorstate_t error_state)
  {
  return CAN_errorstate_names[error_state];
  }

CAN_errorstate_t canbus::GetErrorState()
  {
  if (m_status.errors_tx == 0 && m_status.errors_rx == 0)
    return CAN_errorstate_none;
  else if (m_status.errors_tx < 96 && m_status.errors_rx < 96)
    return CAN_errorstate_active;
  else if (m_status.errors_tx < 128 && m_status.errors_rx < 128)
    return CAN_errorstate_warning;
  else if (m_status.errors_tx < 256 && m_status.errors_rx < 256)
    return CAN_errorstate_passive;
  else
    return CAN_errorstate_busoff;
  }

const char* canbus::GetErrorStateName()
  {
  return GetCanErrorStateName(GetErrorState());
  }

/**
 * GetErrorFlagsDesc: decode error flags into human readable text
 *  (see overrides for bus specific implementations)
 */
bool canbus::GetErrorFlagsDesc(std::string &buffer, uint32_t error_flags)
  {
  return false;
  }

uint32_t can::AddLogger(canlog* logger, int filterc, const char* const* filterv, OvmsWriter* writer)
  {
  if (filterc>0)
    {
    canfilter *filter = new canfilter();
    for (int k=0;k<filterc;k++)
      {
      if (!filter->AddFilter(filterv[k]))
        {
        if (writer)
          writer->printf("Filter '%s' is invalid\n", filterv[k] );
        }
      }
    logger->SetFilter(filter);
    }

  OvmsRecMutexLock lock(&m_loggermap_mutex);
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
  OvmsRecMutexLock lock(&m_loggermap_mutex);

  auto k = m_loggermap.find(id);
  if (k != m_loggermap.end())
    return k->second;
  else
    return NULL;
  }

bool can::RemoveLogger(uint32_t id)
  {
  OvmsRecMutexLock lock(&m_loggermap_mutex);

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
  OvmsRecMutexLock lock(&m_loggermap_mutex);

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

void can::CAN_rxtask(void *pvParameters)
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
            uint32_t receivedFrames;
            loop = msg.body.bus->AsynchronousInterruptHandler(&msg.body.frame, &receivedFrames);
            } while (loop);
          break;
          }
        case CAN_txcallback:
          msg.body.bus->TxCallback(&msg.body.frame, true);
          break;
        case CAN_txfailedcallback:
          msg.body.bus->TxCallback(&msg.body.frame, false);
          break;
        case CAN_logerror:
          msg.body.bus->LogStatus(CAN_LogStatus_Error);
          break;
        case CAN_logstatus:
          msg.body.bus->LogStatus(CAN_LogStatus_Statistics);
          break;
        default:
          break;
        }
      }
    }
  }

const char *valid_baud[] = {
  "33333", "50000", "83333", "100000", "125000", "250000", "500000", "1000000"
};

static int can_start_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  if (argc == 1)
    {
    const size_t baud_count = sizeof(valid_baud) / sizeof(valid_baud[0]);
    const char *token = argv[0];

    if (complete)
      {
      bool match = false;
      unsigned int index = 0;
      writer->SetCompletion(index, nullptr);
      int len = strlen(token);
      for (int i = 0; i < baud_count; ++i)
        {
        const char *entry = valid_baud[i];
        if (strncmp(entry, token, len) == 0)
          {
          writer->SetCompletion(index++, entry);
          match = true;
          }
        }
      return match ? 1 : -1;
      }
    else
      {
      for (int i = 0; i < baud_count; ++i)
        {
        if (strcmp(valid_baud[i], token))
          return argc;
        }
      writer->printf("Error: Invalid baud %s\n", token);
      return -1;
      }
    }
  if (argc == 2)
    {
    return MyDBC.ExpandComplete(writer, argv[1], complete) ? argc : -1;
    }
  return -1;
  }

static int can_dbc_validate(OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv, bool complete)
  {
  if (argc == 1)
    {
    return MyDBC.ExpandComplete(writer, argv[0], complete) ? argc : -1;
    }
  return -1;
  }

////////////////////////////////////////////////////////////////////////
// can - the CAN system controller
////////////////////////////////////////////////////////////////////////

can::can()
  {
  if (!includeCAN) return;

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
    cmd_canstart->RegisterCommand("listen","Start CAN bus in listen mode",can_start,"<baud> [<dbc>]", 1, 2, true, can_start_validate);
    cmd_canstart->RegisterCommand("active","Start CAN bus in active mode",can_start,"<baud> [<dbc>]", 1, 2, true, can_start_validate);
    cmd_canx->RegisterCommand("stop","Stop CAN bus",can_stop);
    cmd_canx->RegisterCommand("reset","Reset CAN bus",can_reset);
    OvmsCommand* cmd_candbc = cmd_canx->RegisterCommand("dbc","CAN dbc framework");
    cmd_candbc->RegisterCommand("attach","Attach a DBC file to a CAN bus",can_dbc_attach,"<dbc>", 1, 1, true, can_dbc_validate);
    cmd_candbc->RegisterCommand("detach","Detach the DBC file from a CAN bus",can_dbc_detach);
    OvmsCommand* cmd_cantx = cmd_canx->RegisterCommand("tx","CAN tx framework");
    cmd_cantx->RegisterCommand("standard","Transmit standard CAN frame",can_tx,"<id> <data...>", 1, 9);
    cmd_cantx->RegisterCommand("extended","Transmit extended CAN frame",can_tx,"<id> <data...>", 1, 9);
    OvmsCommand* cmd_canrx = cmd_canx->RegisterCommand("rx","CAN rx framework");
    cmd_canrx->RegisterCommand("standard","Simulate reception of standard CAN frame",can_rx,"<id> <data...>", 1, 9);
    cmd_canrx->RegisterCommand("extended","Simulate reception of extended CAN frame",can_rx,"<id> <data...>", 1, 9);
    OvmsCommand* cmd_cantesttx = cmd_canx->RegisterCommand("testtx","CAN test tx framework");
    cmd_cantesttx->RegisterCommand("standard","Transmit test standard CAN frames",can_testtx,"<id> <count> <delayms>", 3, 3);
    cmd_cantesttx->RegisterCommand("extended","Transmit test extended CAN frames",can_testtx,"<id> <count> <delayms>", 3, 3);
    cmd_canx->RegisterCommand("status","Show CAN status",can_status);
    cmd_canx->RegisterCommand("clear","Clear CAN status",can_clearstatus);
    cmd_canx->RegisterCommand("explain","Explain CAN error flags", can_explain_flags, "[<errorflags>]\n"
      "Produce a human readable decoding of the current or given error flags for the bus, if available.\n"
      "Enter <errorflags> hexadecimal as shown in the status output, with/without '0x' prefix.\n", 0, 1);
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

/**
 * RegisterListener: register an asynchronous CAN frame processor
 * 
 * This is the standard mechanism to hook into the CAN framework. All RX frames
 * and all TX results for all CAN buses get copied to all listener queues as
 * CAN_frame_t objects, to be processed asynchronously by application tasks.
 * 
 * Queue creation template:
 *   my_rx_queue = xQueueCreate(CONFIG_OVMS_HW_CAN_RX_QUEUE_SIZE, sizeof(CAN_frame_t));
 * 
 * You're free to use whatever queue size is appropriate for your task, but be
 * aware the system will silently drop queue overflows. The vehicle poller
 * takes care of registering a queue and provides an additional filter API.
 * 
 * If you need to process incoming frames or TX results as fast as possible,
 * register a synchronous CAN callback -- see below.
 */
void can::RegisterListener(QueueHandle_t queue, bool txfeedback /*=false*/)
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

/**
 * RegisterCallback: register a synchronous CAN frame processor
 * 
 * This is the mechanism to hook into the CAN framework if you need to process
 * incoming frames / transmission results as fast as possible.
 * 
 * A standard application for a CAN callback is a CAN processor that needs to
 * respond to certain incoming frames within a defined maximum time span,
 * e.g. to override control messages on the bus.
 * 
 * Callbacks are executed within the CAN task context, before the frames get
 * distributed to the listener queues (see above).
 * 
 * Callbacks need to be highly optimized, and must not block. Avoid writing
 * to metrics (as they may have listeners), avoid any kind of network or
 * file operations or complex calculations (floating point math), avoid
 * ESP_LOG* logging (as that may block). You may raise events from a callback,
 * and you may write to queues/semaphores non-blocking.
 */
void can::RegisterCallback(const char* caller, CanFrameCallback callback, bool txfeedback /*=false*/)
  {
  if (txfeedback)
    m_txcallbacks.push_back(new CanFrameCallbackEntry(caller, callback));
  else
    m_rxcallbacks.push_back(new CanFrameCallbackEntry(caller, callback));
  }

/**
 * RegisterCallbackFront: register a callback at the front of the callback list
 * 
 * Use this if you want to make sure your callback is the first to process
 * incoming frames / transmission results (until more callbacks get added to
 * the front).
 * 
 * Be aware the vehicle poller also registers a callback (at the end) when you
 * call OvmsVehicle::RegisterCanBus(), so if you need to add a critical callback
 * later on, use this API method to prioritize your callback.
 */
void can::RegisterCallbackFront(const char* caller, CanFrameCallback callback, bool txfeedback /*=false*/)
  {
  if (txfeedback)
    m_txcallbacks.push_front(new CanFrameCallbackEntry(caller, callback));
  else
    m_rxcallbacks.push_front(new CanFrameCallbackEntry(caller, callback));
  }

void can::DeregisterCallback(const char* caller)
  {
  m_rxcallbacks.remove_if([caller](CanFrameCallbackEntry* entry){ return strcmp(entry->m_caller, caller)==0; });
  m_txcallbacks.remove_if([caller](CanFrameCallbackEntry* entry){ return strcmp(entry->m_caller, caller)==0; });
  }

int can::ExecuteCallbacks(const CAN_frame_t* frame, bool tx, bool success)
  {
  int cnt = 0;
  if (tx)
    {
    if (frame->callback)
      {
      // invoke frame-specific callback function
      (*(frame->callback))(frame, success);
      cnt++;
      }
    for (auto entry : m_txcallbacks)
      {
      // invoke generic tx callbacks
      entry->m_callback(frame, success);
      cnt++;
      }
    }
  else
    {
    for (auto entry : m_rxcallbacks)
      {
      entry->m_callback(frame, success);
      cnt++;
      }
    }
  return cnt;
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
  m_tx_frame = {};
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

esp_err_t canbus::Reset()
  {
  CAN_mode_t m = m_mode;
  CAN_speed_t s = m_speed;
  CAN_status_t t;
  memcpy(&t,&m_status,sizeof(CAN_status_t));
  Stop();
  esp_err_t res = Start(m, s);
  memcpy(&m_status,&t,sizeof(CAN_status_t));
  return res;
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
      ESP_LOGE(TAG, "%s watchdog inactivity timeout - resetting bus",m_name);
      Reset();
      m_status.watchdog_resets++;
      m_watchdog_timer = monotonictime;
      }
    }
  else
    {
    // Vehicle is OFF, so just tickle the watchdog timer
    m_watchdog_timer = monotonictime;
    }
  }

bool canbus::AsynchronousInterruptHandler(CAN_frame_t* frame, uint32_t* framesReceived)
  {
  return false;
  }

void canbus::TxCallback(CAN_frame_t* p_frame, bool success)
  {
  if (success)
    {
    m_status.packets_tx++;
    MyCan.ExecuteCallbacks(p_frame, true, success);
    MyCan.NotifyListeners(p_frame, true);
    LogFrame(CAN_LogFrame_TX, p_frame);
    }
  else
    {
    m_status.tx_fails++;
    int cnt = MyCan.ExecuteCallbacks(p_frame, true, success);
    LogFrame(CAN_LogFrame_TX_Fail, p_frame);
    // log error status if no application callbacks were called for this frame:
    if (cnt == 0)
      {
      LogStatus(CAN_LogStatus_Error);
      }
    }
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
  m_tx_frame = *p_frame; // save a local copy of this frame to be used later in txcallback
  m_tx_frame.origin = this;
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
