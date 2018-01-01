/**
 * Project:      Open Vehicle Monitor System
 * Module:       CANopen
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#include "ovms_log.h"
static const char *TAG = "canopen";

#include "canopen.h"
#include "ovms_events.h"

#define MIN(n,m) ((n) < (m) ? (n) : (m))
#define MAX(n,m) ((n) > (m) ? (n) : (m))


/**
 * CANopen master shell commands
 */


// Shell command:
//    co canX start
void canopen_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  if (MyCANopen.Start(bus))
    writer->printf("CANopen worker started, %d worker(s) running.\n", MyCANopen.m_workercnt);
  else
    writer->puts("Error: CANopen worker cannot be started!");
  }


// Shell command:
//    co canX stop
void canopen_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  if (MyCANopen.Stop(bus))
    writer->printf("CANopen worker stopped, %d worker(s) running.\n", MyCANopen.m_workercnt);
  else
    writer->printf("Error: CANopen worker still in use by %d clients!\n", MyCANopen.GetWorker(bus)->m_clientcnt);
  }


// Shell command:
//    co status
void canopen_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyCANopen.StatusReport(verbosity, writer);
  }


// Shell command:
//    co canX nmt <start|stop|preop|reset|commreset> [nodeid=0] [resp_timeout_ms=0]
void canopen_nmt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  std::string cmdname = cmd->GetName();
  CANopenNMTCommand_t command = CONC_Start;
  if (cmdname == "stop")
    command = CONC_Stop;
  else if (cmdname == "preop")
    command = CONC_PreOp;
  else if (cmdname == "reset")
    command = CONC_Reset;
  else if (cmdname == "commreset")
    command = CONC_CommReset;
  
  uint8_t nodeid = 0;
  if (argc >= 1)
    nodeid = atoi(argv[0]);
  
  if (nodeid > 127)
    {
    writer->puts("Error: invalid nodeid, allowed range 0-127");
    return;
    }
  
  int response_timeout = 0;
  if (argc >= 2)
    response_timeout = atoi(argv[1]);
  
  CANopenClient client(bus);
  CANopenResult_t res = client.SendNMT(nodeid, command, (response_timeout!=0), response_timeout);
  
  writer->printf("NMT command result: %s\n", MyCANopen.GetResultString(res).c_str());
  }


// Shell command:
//    co canX readsdo <nodeid> <index_hex> <subindex_hex> [timeout_ms=50]
void canopen_readsdo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  // parse args:
  uint8_t nodeid = strtol(argv[0], NULL, 10);
  uint16_t index = strtol(argv[1], NULL, 16);
  uint8_t subindex = strtol(argv[2], NULL, 16);
  int timeout = 50;
  if (argc >= 4)
    timeout = strtol(argv[3], NULL, 10);
  
  if (nodeid < 1 || nodeid > 127)
    {
    writer->puts("Error: invalid nodeid, allowed range 1-127");
    return;
    }
  
  // execute:
  union
    {
    uint32_t num;
    char txt[128];
    } buffer;
  
  CANopenClient client(bus);
  CANopenResult_t res = client.ReadSDO(nodeid, index, subindex, (uint8_t*)&buffer, sizeof(buffer)-1, timeout);
  
  // output result:
  if (res != COR_OK)
    {
    writer->printf("ReadSDO #%d 0x%04x.%02x failed, error code 0x%08x, %s\n",
      nodeid, index, subindex, client.m_jobdone.sdo.error, MyCANopen.GetResultString(res).c_str());
    }
  else
    {
    // test for printable text:
    bool show_txt = true;
    int i;
    for (i=0; i < client.m_jobdone.sdo.xfersize && buffer.txt[i]; i++)
      {
      if (!isprint(buffer.txt[i]))
        {
        show_txt = false;
        break;
        }
      }
    // terminate string:
    if (i == client.m_jobdone.sdo.xfersize && buffer.txt[i])
      buffer.txt[i] = 0;
    
    writer->printf("ReadSDO #%d 0x%04x.%02x: contsize=%d xfersize=%d dec=%d hex=0x%0*x str='%s'\n",
      nodeid, index, subindex, client.m_jobdone.sdo.contsize, client.m_jobdone.sdo.xfersize,
      buffer.num, MIN(client.m_jobdone.sdo.xfersize,4)*2, buffer.num, show_txt ? buffer.txt : "(binary)");
    }
  }


// Shell command:
//    co canX writesdo <nodeid> <index_hex> <subindex_hex> <value> [timeout_ms=50]
// <value>:
// - interpreted as hexadecimal if begins with "0x"
// - interpreted as decimal if only contains digits
// - else interpreted as string
void canopen_writesdo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  // parse args:
  uint8_t nodeid = strtol(argv[0], NULL, 10);
  uint16_t index = strtol(argv[1], NULL, 16);
  uint8_t subindex = strtol(argv[2], NULL, 16);
  
  if (nodeid < 1 || nodeid > 127)
    {
    writer->puts("Error: invalid nodeid, allowed range 1-127");
    return;
    }
  
  // value:
  union
    {
    uint32_t num;
    char txt[128];
    } buffer;
  size_t bufsize = 0;
  char buftype = '?';
  char* endptr = NULL;
  
  if (argv[3][0]=='0' && argv[3][1]=='x')
    {
    // try hexadecimal:
    buffer.num = strtol(argv[3]+2, &endptr, 16);
    if (*endptr == 0)
      {
      bufsize = 0; // let SDO server figure out type&size
      buftype = 'H';
      }
    }
  if (buftype == '?')
    {
    // try decimal:
    buffer.num = strtol(argv[3], &endptr, 10);
    if (*endptr == 0)
      {
      bufsize = 0; // let SDO server figure out type&size
      buftype = 'D';
      }
    }
  if (buftype == '?')
    {
    // string:
    strncpy(buffer.txt, argv[3], sizeof(buffer.txt)-1);
    buffer.txt[sizeof(buffer.txt)-1] = 0;
    bufsize = strlen(buffer.txt);
    buftype = 'S';
    }
  
  int timeout = 50;
  if (argc >= 5)
    timeout = strtol(argv[4], NULL, 10);
  
  // execute:
  
  CANopenClient client(bus);
  CANopenResult_t res = client.WriteSDO(nodeid, index, subindex, (uint8_t*)&buffer, bufsize, timeout);
  
  // output result:
  switch (buftype)
    {
    case 'H':
      writer->printf("WriteSDO #%d 0x%04x.%02x: hex=0x%08x => ", nodeid, index, subindex, buffer.num);
      break;
    case 'D':
      writer->printf("WriteSDO #%d 0x%04x.%02x: dec=%d => ", nodeid, index, subindex, buffer.num);
      break;
    default:
      writer->printf("WriteSDO #%d 0x%04x.%02x: len=%d str='%s' => ", nodeid, index, subindex, bufsize, buffer.txt);
      break;
    }
  if (res != COR_OK)
    {
    writer->printf("%s, CANopen error code 0x%08x, %d bytes sent\n",
      MyCANopen.GetResultString(res).c_str(), client.m_jobdone.sdo.error, client.m_jobdone.sdo.xfersize);
    }
  else
    {
    writer->puts("OK");
    }
  }


// Shell utility:
//    read and display CANopen node core attributes
int canopen_print_nodeinfo(int capacity, OvmsWriter* writer, canbus* bus, int nodeid, int timeout_ms=50, bool brief=false, bool quiet=false)
  {
  #define _readnum(idx, sub, var) (res = client.ReadSDO(nodeid, idx, sub, (uint8_t*)&var, sizeof(var), timeout_ms))
  #define _readstr(idx, sub, var) (res = client.ReadSDO(nodeid, idx, sub, (uint8_t*)&var, sizeof(var)-1, timeout_ms), \
    var[client.m_jobdone.sdo.xfersize]=0, res)
  
  CANopenClient client(bus);
  CANopenResult_t res;
  int written = 0;
  
  uint32_t device_type = 0;
  uint8_t error_register = 0;
  uint32_t vendor_id = 0;
  char device_name[50];
  char hardware_version[50];
  char software_version[50];
  uint32_t product_code = 0;
  uint32_t revision_number = 0;
  uint32_t serial_number = 0;
  
  // mandatory entries:
  if (_readnum(0x1000, 0x00, device_type) != COR_OK)
    {
    if (!quiet || res != COR_ERR_Timeout)
      {
      if (written < capacity) written += writer->printf(
        brief ? "#%d: %-.20s\n" : "Node #%d: %s (%08x)\n"
        , nodeid, MyCANopen.GetResultString(res).c_str(), client.m_jobdone.sdo.error);
      }
    return written;
    }
  _readnum(0x1001, 0x00, error_register);
  _readnum(0x1018, 0x01, vendor_id);
  
  // optional entries:
  _readstr(0x1008, 0x00, device_name);
  _readstr(0x1009, 0x00, hardware_version);
  _readstr(0x100a, 0x00, software_version);
  _readnum(0x1018, 0x02, product_code);
  _readnum(0x1018, 0x03, revision_number);
  _readnum(0x1018, 0x04, serial_number);
  
  if (brief)
    {
    if (written < capacity) written += writer->printf(
      "#%d: DT=%08x ER=%02x VI=%08x\n"
      , nodeid, device_type, error_register, vendor_id);
    }
  else
    {
    if (written < capacity) written += writer->printf(
      "Node #%d:\n"
      " Device type: 0x%08x\n"
      " Error state: 0x%02x\n"
      " Vendor ID: 0x%08x\n"
      , nodeid, device_type, error_register, vendor_id);
    if (written < capacity) written += writer->printf(
      " Product: 0x%08x\n"
      " Revision: 0x%08x\n"
      " S/N: 0x%08x\n"
      , product_code, revision_number, serial_number);
    if (written < capacity) written += writer->printf(
      " Device name: %s\n"
      " HW version: %s\n"
      " SW version: %s\n"
      , device_name, hardware_version, software_version);
    }
  
  return written;
  
  #undef _readnum
  #undef _readstr
  }


// Shell command:
//    co canX info <nodeid> [timeout_ms=50]
void canopen_info(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  uint8_t nodeid = strtol(argv[0], NULL, 10);
  int timeout = 50;
  if (argc >= 2)
    timeout = strtol(argv[1], NULL, 10);
  
  if (nodeid < 1 || nodeid > 127)
    {
    writer->puts("Error: invalid nodeid, allowed range 1-127");
    return;
    }
  
  canopen_print_nodeinfo(verbosity, writer, bus, nodeid, timeout);
  }


// Shell command:
//    co canX scan [[startid=1][-][endid=127]] [timeout_ms=50]
void canopen_scan(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  const char* busname = cmd->GetParent()->GetName();

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName(busname);
  if (bus == NULL)
    {
    writer->puts("Error: Cannot find named CAN bus");
    return;
    }

  // parse args:
  int id_start=1, id_end=127, timeout=50;
  
  if (argc >= 1)
    {
    char *endptr;
    id_start = strtol(argv[0], &endptr, 10);
    if (id_start < 0)
      {
      id_end = -id_start;
      id_start = 1;
      }
    else if(*endptr)
      {
      id_end = strtol(endptr+1, NULL, 10);
      }
    if (id_start == 0)
      id_start = 1;
    else if (id_start > 127)
      id_start = 127;
    if (id_end == 0 || id_end > 127)
      id_end = 127;
    if (id_end < id_start)
      {
      int x = id_end;
      id_end = id_start;
      id_start = x;
      }
    }
  
  if (argc >= 2)
    timeout = strtol(argv[1], NULL, 10);
  
  // execute:
  int capacity = verbosity;
  capacity -= writer->printf("Scan #%d-%d...\n", id_start, id_end);
  for (int nodeid=id_start; nodeid <= id_end; nodeid++)
    {
    capacity -= canopen_print_nodeinfo(capacity, writer, bus, nodeid, timeout, (verbosity<COMMAND_RESULT_NORMAL), true);
    }
  writer->printf("Done.\n");
  }



/**
 * The CANopen master manages all CANopenWorkers, provides shell commands
 *   and the CAN rx task for all workers.
 */

CANopen MyCANopen __attribute__ ((init_priority (7000)));

CANopen::CANopen()
  {
  ESP_LOGI(TAG, "Initialising CANopen (7000)");
  
  m_rxtask = NULL;
  m_rxqueue = NULL;
  
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    m_worker[i] = NULL;
  m_workercnt = 0;
  
  // register commands:
  
  OvmsCommand* cmd_co = MyCommandApp.RegisterCommand("co", "CANopen framework", NULL, "", 0, 0, true);
  cmd_co->RegisterCommand("status", "Show CANopen status", canopen_status, "", 0, 0, true);
  
  for (int k=1; k <= CAN_INTERFACE_CNT; k++)
    {
    static const char* name[4] = {"can1", "can2", "can3"};
    OvmsCommand* cmd_canx = cmd_co->RegisterCommand(name[k-1], "select bus", NULL, "", 0, 0, true);
    
    cmd_canx->RegisterCommand("start", "Start CANopen worker", canopen_start, "", 0, 0, true);
    cmd_canx->RegisterCommand("stop", "Stop CANopen worker", canopen_stop, "", 0, 0, true);
    
    OvmsCommand* cmd_nmt = cmd_canx->RegisterCommand("nmt", "Send NMT request", NULL, "", 0, 0, true);
    cmd_nmt->RegisterCommand("start", "Start node", canopen_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2, true);
    cmd_nmt->RegisterCommand("stop", "Stop node", canopen_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2, true);
    cmd_nmt->RegisterCommand("preop", "Enter pre-operational state", canopen_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2, true);
    cmd_nmt->RegisterCommand("reset", "Reset node", canopen_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2, true);
    cmd_nmt->RegisterCommand("commreset", "Reset communication layer", canopen_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2, true);
    
    cmd_canx->RegisterCommand("readsdo", "Read SDO register", canopen_readsdo, "<nodeid> <index_hex> <subindex_hex> [timeout_ms=50]", 3, 4, true);
    cmd_canx->RegisterCommand("writesdo", "Write SDO register", canopen_writesdo, "<nodeid> <index_hex> <subindex_hex> <value> [timeout_ms=50]", 4, 5, true);
    
    cmd_canx->RegisterCommand("info", "Show node info", canopen_info, "<nodeid> [timeout_ms=50]", 1, 2, true);
    cmd_canx->RegisterCommand("scan", "Scan nodes", canopen_scan, "[[startid=1][-][endid=127]] [timeout_ms=50]", 0, 2, true);
    }
  }

CANopen::~CANopen()
  {
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (m_worker[i])
      delete m_worker[i];
    }
  if (m_rxtask)
    {
    vQueueDelete(m_rxqueue);
    vTaskDelete(m_rxtask);
    }
  }


/**
 * CanRxTask: CAN frame receiver for all workers
 */

static void CANopenRxTask(void *pvParameters)
  {
  CANopen *me = (CANopen*)pvParameters;
  me->CanRxTask();
  }

void CANopen::CanRxTask()
  {
  CAN_frame_t frame;

  while(1)
    {
    if (xQueueReceive(m_rxqueue, &frame, (portTickType)portMAX_DELAY) == pdTRUE)
      {
      for (int i=0; i < CAN_INTERFACE_CNT; i++)
        {
        if (m_worker[i] && m_worker[i]->m_bus == frame.origin)
          {
          m_worker[i]->IncomingFrame(&frame);
          break;
          }
        }
      }
    }
  }


/**
 * Start: get / start CANopenWorker for a CAN bus
 */
CANopenWorker* CANopen::Start(canbus* bus)
  {
  // check for running worker:
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (m_worker[i] && m_worker[i]->m_bus == bus)
      return m_worker[i];
    }
  
  // start CAN rx task:
  if (m_rxtask == NULL)
    {
    m_rxqueue = xQueueCreate(20, sizeof(CAN_frame_t));
    xTaskCreatePinnedToCore(CANopenRxTask, "COrx Task", 4096, (void*)this, 5, &m_rxtask, 1);
    MyCan.RegisterListener(m_rxqueue);
    }
  
  // start worker:
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (!m_worker[i])
      {
      m_worker[i] = new CANopenWorker(bus);
      m_workercnt++;
      ESP_LOGI(TAG, "Worker started on %s", bus->GetName());
      MyEvents.SignalEvent("canopen.worker.start", (void*) m_worker[i]);
      return m_worker[i];
      }
    }
  
  return NULL;
  }


/**
 * Stop: stop CANopenWorker for a CAN bus
 *    - fails if the worker still has clients
 */
bool CANopen::Stop(canbus* bus)
  {
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (m_worker[i] && m_worker[i]->m_bus == bus)
      {
      // check if worker still in use:
      if (m_worker[i]->m_clientcnt > 0)
        return false;
      
      // shutdown worker:
      MyEvents.SignalEvent("canopen.worker.stop", (void*) m_worker[i]);
      delete m_worker[i];
      m_worker[i] = NULL;
      ESP_LOGI(TAG, "Worker stopped on %s", bus->GetName());
      
      if (--m_workercnt == 0)
        {
        // last worker stopped, stop CAN rx task:
        MyCan.DeregisterListener(m_rxqueue);
        vQueueDelete(m_rxqueue);
        vTaskDelete(m_rxtask);
        m_rxqueue = NULL;
        m_rxtask = NULL;
        }
      
      return true; // stopped
      }
    }
  
  return true; // not found = already stopped
  }


/**
 * GetWorker: find running CANopenWorker for a CAN bus
 */
CANopenWorker* CANopen::GetWorker(canbus* bus)
  {
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (m_worker[i] && m_worker[i]->m_bus == bus)
      return m_worker[i];
    }
  return NULL;
  }


/**
 * StatusReport:
 */
void CANopen::StatusReport(int verbosity, OvmsWriter* writer)
  {
  writer->printf(
    "CANopen status:\n"
    "  Active workers: %d\n"
    , m_workercnt);
  
  for (int i=0; i < CAN_INTERFACE_CNT; i++)
    {
    if (m_worker[i])
      m_worker[i]->StatusReport(verbosity, writer);
    }
  }


/**
 * GetResultString: translate CANopenResult_t to std::string
 */
const std::string CANopen::GetResultString(const CANopenResult_t result)
  {
  std::string name;
  switch (result)
    {
    case COR_OK:                        name = "OK"; break;
    case COR_WAIT:                      name = "Waiting"; break;
    
    case COR_ERR_UnknownJobType:        name = "Unknown job type"; break;
    case COR_ERR_QueueFull:             name = "Queue full"; break;
    case COR_ERR_QueueEmpty:            name = "Queue empty"; break;
    case COR_ERR_NoCANWrite:            name = "CAN bus not in active mode"; break;
    case COR_ERR_ParamRange:            name = "Parameter out of range"; break;
    case COR_ERR_BufferTooSmall:        name = "Buffer too small"; break;
    
    case COR_ERR_Timeout:               name = "Timeout"; break;
    case COR_ERR_SDO_Access:            name = "SDO access failed"; break;
    case COR_ERR_SDO_SegMismatch:       name = "SDO segment mismatch"; break;
    
    case COR_ERR_DeviceOffline:         name = "Device offline"; break;
    case COR_ERR_UnknownDevice:         name = "Unknown device"; break;
    case COR_ERR_LoginFailed:           name = "Login failed"; break;
    case COR_ERR_PreOpFailed:           name = "PreOp mode failed"; break;
    
    default:
      char val[10];
      sprintf(val, "%d", (int) result);
      name = val;
    }
  return name;
  }

