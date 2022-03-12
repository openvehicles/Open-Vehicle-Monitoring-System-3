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

#include <sys/param.h>

#include "ovms_log.h"
static const char *TAG = "canopen";

#include "canopen.h"
#include "ovms_events.h"


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

  OvmsCommand* cmd_co = MyCommandApp.RegisterCommand("copen", "CANopen framework", shell_status, "", 0, 0, false);
  cmd_co->RegisterCommand("status", "Show CANopen status", shell_status);

  for (int k=1; k <= CAN_INTERFACE_CNT; k++)
    {
    static const char* name[4] = { "can1", "can2", "can3", "can4" };
    OvmsCommand* cmd_canx = cmd_co->RegisterCommand(name[k-1], "select bus");

    cmd_canx->RegisterCommand("start", "Start CANopen worker", shell_start);
    cmd_canx->RegisterCommand("stop", "Stop CANopen worker", shell_stop);

    OvmsCommand* cmd_nmt = cmd_canx->RegisterCommand("nmt", "Send NMT request");
    cmd_nmt->RegisterCommand("start", "Start node", shell_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2);
    cmd_nmt->RegisterCommand("stop", "Stop node", shell_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2);
    cmd_nmt->RegisterCommand("preop", "Enter pre-operational state", shell_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2);
    cmd_nmt->RegisterCommand("reset", "Reset node", shell_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2);
    cmd_nmt->RegisterCommand("commreset", "Reset communication layer", shell_nmt, "[nodeid=0] [timeout_ms=0]", 0, 2);

    cmd_canx->RegisterCommand("readsdo", "Read SDO register", shell_readsdo, "<nodeid> <index_hex> <subindex_hex> [timeout_ms=50]", 3, 4);
    cmd_canx->RegisterCommand("writesdo", "Write SDO register", shell_writesdo, "<nodeid> <index_hex> <subindex_hex> <value> [timeout_ms=50]", 4, 5);

    cmd_canx->RegisterCommand("info", "Show node info", shell_info, "<nodeid> [timeout_ms=50]", 1, 2);
    cmd_canx->RegisterCommand("scan", "Scan nodes", shell_scan, "[[startid=1][-][endid=127]] [timeout_ms=50]", 0, 2);
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
    xTaskCreatePinnedToCore(CANopenRxTask, "OVMS COrx",
      CONFIG_OVMS_COMP_CANOPEN_RX_STACK, (void*)this, 15, &m_rxtask, CORE(0));
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
 * GetJobName: translate CANopenJob_t to std::string
 */
const std::string CANopen::GetJobName(const CANopenJob_t jobtype)
  {
  std::string name;
  switch (jobtype)
    {
    case COJT_None:                     name = "NoOp"; break;
    case COJT_SendNMT:                  name = "SendNMT"; break;
    case COJT_ReceiveHB:                name = "ReceiveHB"; break;
    case COJT_ReadSDO:                  name = "ReadSDO"; break;
    case COJT_WriteSDO:                 name = "WriteSDO"; break;
    default:
      char val[10];
      sprintf(val, "%d", (int) jobtype);
      name = val;
    }
  return name;
  }

const std::string CANopen::GetJobName(const CANopenJob& job)
  {
  return GetJobName(job.type);
  }

/**
 * GetCommandName: translate CANopenNMTCommand_t to std::string
 */
const std::string CANopen::GetCommandName(const CANopenNMTCommand_t command)
  {
  std::string name;
  switch (command)
    {
    case CONC_Start:                    name = "Start"; break;
    case CONC_Stop:                     name = "Stop"; break;
    case CONC_PreOp:                    name = "PreOp"; break;
    case CONC_Reset:                    name = "Reset"; break;
    case CONC_CommReset:                name = "CommReset"; break;
    default:
      char val[10];
      sprintf(val, "%d", (int) command);
      name = val;
    }
  return name;
  }

/**
 * GetStateName: translate CANopenNMTState_t to std::string
 */
const std::string CANopen::GetStateName(const CANopenNMTState_t state)
  {
  std::string name;
  switch (state)
    {
    case CONS_Booting:                  name = "Booting"; break;
    case CONS_Stopped:                  name = "Stopped"; break;
    case CONS_Operational:              name = "Operational"; break;
    case CONS_PreOperational:           name = "PreOperational"; break;
    default:
      char val[10];
      sprintf(val, "%d", (int) state);
      name = val;
    }
  return name;
  }


/**
 * GetAbortCodeName: translate CANopen abort error code to std::string
 */
const std::string CANopen::GetAbortCodeName(uint32_t abortcode)
  {
  std::string name;
  switch (abortcode)
    {
    case 0x05030000:    name = "Toggle bit not alternated"; break;

    case 0x05040000:    name = "SDO protocol timed out"; break;
    case 0x05040001:    name = "Client/server command specifier invalid"; break;
    case 0x05040002:    name = "Invalid block size"; break;
    case 0x05040003:    name = "Invalid sequence number"; break;
    case 0x05040004:    name = "CRC error"; break;
    case 0x05040005:    name = "Out of memory"; break;

    case 0x06010000:    name = "Unsupported access to an object"; break;
    case 0x06010001:    name = "Attempt to read a write only object"; break;
    case 0x06010002:    name = "Attempt to write a read only object"; break;

    case 0x06020000:    name = "Object does not exist in the dictionary"; break;

    case 0x06040041:    name = "Object cannot be mapped to the PDO"; break;
    case 0x06040042:    name = "Objects to be mapped would exceed PDO length"; break;
    case 0x06040043:    name = "General parameter incompatibility"; break;
    case 0x06040047:    name = "General internal incompatibility in device"; break;

    case 0x06060000:    name = "Access failed due to an hardware error"; break;

    case 0x06070010:    name = "Data type mismatch, length does not match"; break;
    case 0x06070012:    name = "Data type mismatch, length too high"; break;
    case 0x06070013:    name = "Data type mismatch, length too low"; break;

    case 0x06090011:    name = "Sub-index does not exist"; break;
    case 0x06090030:    name = "Value range of parameter exceeded"; break;
    case 0x06090031:    name = "Value of parameter written too high"; break;
    case 0x06090032:    name = "Value of parameter written too low"; break;
    case 0x06090036:    name = "Maximum value is less than minimum value"; break;

    case 0x08000000:    name = "General error"; break;
    case 0x08000020:    name = "Can't xfer data to application"; break;
    case 0x08000021:    name = "Can't xfer data to application (local control)"; break;
    case 0x08000022:    name = "Can't xfer data to application (device state)"; break;
    case 0x08000023:    name = "Object dictionary not generated or present"; break;

    case 0xffffffff:    name = "Master collision / non-CANopen frame"; break;

    default:
      char val[12];
      sprintf(val, "0x%08x", (int) abortcode);
      name = val;
    }
  return name;
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
    case COR_ERR_StateChangeFailed:     name = "State change failed"; break;

    default:
      char val[10];
      sprintf(val, "%d", (int) result);
      name = val;
    }
  return name;
  }

const std::string CANopen::GetResultString(const CANopenResult_t result, uint32_t abortcode)
  {
  std::string name = GetResultString(result);
  if (abortcode)
    {
    name.append("; ");
    name.append(GetAbortCodeName(abortcode));
    }
  return name;
  }

const std::string CANopen::GetResultString(const CANopenJob& job)
  {
  if (job.type == COJT_ReadSDO || job.type == COJT_WriteSDO)
    return GetResultString(job.result, job.sdo.error);
  else
    return GetResultString(job.result, 0);
  }
