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

#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "ovms_events.h"
#include "canopen.h"


// SDO commands:

#define SDO_CommandMask             0b11100000
#define SDO_ExpeditedSizeMask       0b00001100
#define SDO_Expedited               0b00000010
#define SDO_SizeIndicated           0b00000001

#define SDO_Abort                   0b10000000

#define SDO_InitUploadRequest       0b01000000
#define SDO_InitUploadResponse      0b01000000
#define SDO_UploadSegmentRequest    0b01100000
#define SDO_UploadSegmentResponse   0b00000000

#define SDO_InitDownloadRequest     0b00100000
#define SDO_InitDownloadResponse    0b01100000
#define SDO_DownloadSegmentRequest  0b00000000
#define SDO_DownloadSegmentResponse 0b00100000

#define SDO_SegmentToggle           0b00010000
#define SDO_SegmentUnusedMask       0b00001110
#define SDO_SegmentEnd              0b00000001

// SDO abort reasons:

#define SDO_Abort_SegMismatch       0x05030000
#define SDO_Abort_Timeout           0x05040000
#define SDO_Abort_OutOfMemory       0x05040005


static void CANopenWorkerJobTask(void *pvParameters);


/**
 * A CANopenWorker processes CANopenJobs on a specific bus.
 * 
 * CANopenClients create and submit Jobs to be processed to a CANopenWorker.
 * After finish/abort, the Worker sends the Job to the clients done queue.
 * 
 * A CANopenWorker also monitors the bus for emergency and heartbeat
 * messages, and translates these into events and metrics updates.
 */

CANopenWorker::CANopenWorker(canbus* bus)
  {
  m_bus = bus;
  
  m_clientcnt = 0;
  
  m_nmt_rxcnt = 0;
  m_emcy_rxcnt = 0;
  
  m_jobcnt = 0;
  m_jobcnt_timeout = 0;
  m_jobcnt_error = 0;
  
  m_jobqueue = xQueueCreate(20, sizeof(CANopenJob));
  snprintf(m_taskname, sizeof(m_taskname), "OVMS COwrk %s", bus->GetName());
  xTaskCreatePinnedToCore(CANopenWorkerJobTask, m_taskname,
    CONFIG_OVMS_COMP_CANOPEN_WRK_STACK, (void*)this, 7, &m_jobtask, CORE(0));
  
  memset(&m_job, 0, sizeof(m_job));
  m_job.type = COJT_None;
  
  memset(&m_request, 0, sizeof(m_request));
  memset(&m_response, 0, sizeof(m_response));
  }

CANopenWorker::~CANopenWorker()
  {
  vQueueDelete(m_jobqueue);
  vTaskDelete(m_jobtask);
  }


void CANopenWorker::Open(CANopenAsyncClient* client)
  {
  m_clients.push_front(client);
  ++m_clientcnt;
  }

void CANopenWorker::Close(CANopenAsyncClient* client)
  {
  m_clients.remove(client);
  m_clientcnt ? --m_clientcnt : 0;
  }

bool CANopenWorker::IsClient(CANopenAsyncClient* client)
  {
  for (auto c : m_clients)
    {
    if (c == client)
      return true;
    }
  return false;
  }


void CANopenWorker::StatusReport(int verbosity, OvmsWriter* writer)
  {
  writer->printf(
    "  %s:\n"
    "    Active clients: %d\n"
    "    Jobs waiting  : %d\n"
    "    Jobs processed: %d\n"
    "    - timeouts    : %d\n"
    "    - other errors: %d\n"
    "    NMT received  : %d\n"
    "    EMCY received : %d\n"
    , m_bus->GetName()
    , m_clientcnt
    , (int)uxQueueMessagesWaiting(m_jobqueue)
    , m_jobcnt
    , m_jobcnt_timeout
    , m_jobcnt_error
    , m_nmt_rxcnt
    , m_emcy_rxcnt);
  }


/**
 * GetNodeMetrics: find / create metrics bundle for a nodeid
 */
CANopenNodeMetrics *CANopenWorker::GetNodeMetrics(uint8_t nodeid)
  {
  CANopenNodeMetrics *nm = m_nodemetrics[nodeid];
  
  if (!nm)
    {
    // add node metrics:
    nm = new CANopenNodeMetrics();
    char name[50];
    
    snprintf(name, sizeof(name), "co.%s.nd%d.state", m_bus->GetName(), nodeid);
    if (!(nm->m_state = (OvmsMetricString*) MyMetrics.Find(name)))
      nm->m_state = new OvmsMetricString(strdup(name), SM_STALE_MIN);
    
    snprintf(name, sizeof(name), "co.%s.nd%d.emcy.code", m_bus->GetName(), nodeid);
    if (!(nm->m_emcy_code = (OvmsMetricInt*) MyMetrics.Find(name)))
      nm->m_emcy_code = new OvmsMetricInt(strdup(name), SM_STALE_MIN);
    
    snprintf(name, sizeof(name), "co.%s.nd%d.emcy.type", m_bus->GetName(), nodeid);
    if (!(nm->m_emcy_type = (OvmsMetricInt*) MyMetrics.Find(name)))
      nm->m_emcy_type = new OvmsMetricInt(strdup(name), SM_STALE_MIN);
    
    m_nodemetrics[nodeid] = nm;
    }
  
  return nm;
  }


/**
 * SubmitJob: post a new job to the JobTask queue
 */
CANopenResult_t CANopenWorker::SubmitJob(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueSend(m_jobqueue, &job, maxqueuewait) != pdTRUE)
    job.result = COR_ERR_QueueFull;
  else
    job.result = COR_WAIT;
  return job.result;
  }


/**
 * JobTask: process CANopenJobs, send results back to clients
 */

static void CANopenWorkerJobTask(void *pvParameters)
  {
  CANopenWorker *me = (CANopenWorker*)pvParameters;
  me->JobTask();
  }

void CANopenWorker::JobTask()
  {
  while(1)
    {
    // get next job:
    if (xQueueReceive(m_jobqueue, &m_job, (portTickType)portMAX_DELAY) == pdTRUE)
      {
        // check client:
        if (!IsClient(m_job.client))
          {
          ESP_LOGW(TAG, "Job dropped: Client vanished");
          continue;
          }
        
        // process job:
        switch (m_job.type)
          {
          case COJT_None:
            m_job.result = COR_OK;
            break;
          case COJT_SendNMT:
            ESP_LOGV(TAG, "SendNMT: %s node=%d, command=%d", m_bus->GetName(), m_job.nmt.nodeid, m_job.nmt.command);
            m_job.result = ProcessSendNMTJob();
            ESP_LOGV(TAG, "SendNMT result: %s", CANopen::GetResultString(m_job).c_str());
            break;
          case COJT_ReceiveHB:
            ESP_LOGV(TAG, "ReceiveHB: %s node=%d", m_bus->GetName(), m_job.hb.nodeid);
            m_job.result = ProcessReceiveHBJob();
            ESP_LOGV(TAG, "ReceiveHB result: %s", CANopen::GetResultString(m_job).c_str());
            break;
          case COJT_ReadSDO:
            ESP_LOGV(TAG, "ReadSDO: %s node=%d adr=%04x.%02x", m_bus->GetName(), m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex);
            m_job.result = ProcessReadSDOJob();
            ESP_LOGV(TAG, "ReadSDO result: %s", CANopen::GetResultString(m_job).c_str());
            break;
          case COJT_WriteSDO:
            ESP_LOGV(TAG, "WriteSDO: %s node=%d adr=%04x.%02x", m_bus->GetName(), m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex);
            m_job.result = ProcessWriteSDOJob();
            ESP_LOGV(TAG, "WriteSDO result: %s", CANopen::GetResultString(m_job).c_str());
            break;
          default:
            ESP_LOGW(TAG, "Unknown job type: %d", (int)m_job.type);
            m_job.result = COR_ERR_UnknownJobType;
          }
        
        // return job to client if still valid:
        if (!IsClient(m_job.client))
          {
          ESP_LOGW(TAG, "Job result lost: Client vanished");
          }
        else
          {
          if (m_job.client->SubmitDoneCallback(m_job, 0) != COR_OK)
            ESP_LOGW(TAG, "Job result lost: Client queue is full");
          }
        
        // statistics:
        m_jobcnt++;
        if (m_job.result == COR_ERR_Timeout)
          m_jobcnt_timeout++;
        else if (m_job.result != COR_OK)
          m_jobcnt_error++;
        
        m_job.type = COJT_None;
      }
    }
  }


/**
 * IncomingFrame: process EMCY and Heartbeat messages, forward job frames to job task
 */
void CANopenWorker::IncomingFrame(CAN_frame_t* p_frame)
  {
  // Message matching our current job?
  if (m_job.type != COJT_None && p_frame->MsgID == m_job.rxid)
    {
    // copy payload into m_response:
    int i;
    for (i=0; i < p_frame->FIR.B.DLC; i++)
      m_response.byte[i] = p_frame->data.u8[i];
    for (; i < 8; i++)
      m_response.byte[i] = 0;
    
    // signal job task:
    xTaskNotifyGive(m_jobtask);
    }
  
  
  // EMCY (Emergency) message?
  if (p_frame->MsgID > 0x080 && p_frame->MsgID < 0x100 && p_frame->FIR.B.DLC == 8)
    {
    m_emcy_rxcnt++;
    
    CANopenEMCYEvent ev;
    ev.origin = p_frame->origin;
    ev.nodeid = p_frame->MsgID - 0x080;
    ev.code = p_frame->data.u8[0] | (p_frame->data.u8[1] << 8);
    ev.type = p_frame->data.u8[2];
    memcpy(ev.data, p_frame->data.u8+3, 5);
    
    CANopenNodeMetrics *nm = GetNodeMetrics(ev.nodeid);
    
    // Note: logging is very expensive if file logging to SD card is enabled (→ issue #107).
    //  Workaround: level down to verbose (should be info):
    ESP_LOGV(TAG, "%s node %d emergency: code=0x%04x type=0x%02x data: %02x %02x %02x %02x %02x",
      m_bus->GetName(), ev.nodeid, ev.code, ev.type, ev.data[0], ev.data[1], ev.data[2], ev.data[3], ev.data[4]);
    MyEvents.SignalEvent("canopen.node.emcy", &ev, sizeof(ev));
    nm->m_emcy_code->SetValue(ev.code);
    nm->m_emcy_type->SetValue(ev.type);
    }
  
  // NMT (Heartbeat/State) message?
  else if (p_frame->MsgID > 0x700 && p_frame->MsgID < 0x780 && p_frame->FIR.B.DLC == 1)
    {
    m_nmt_rxcnt++;
    
    CANopenNMTEvent ev;
    ev.origin = p_frame->origin;
    ev.nodeid = p_frame->MsgID - 0x700;
    ev.state = (CANopenNMTState_t) p_frame->data.u8[0];
    
    CANopenNodeMetrics *nm = GetNodeMetrics(ev.nodeid);
    
    std::string newstate = CANopen::GetStateName(ev.state);
    if (nm->m_state->AsString() != newstate)
      {
      // Note: logging is very expensive if file logging to SD card is enabled (→ issue #107).
      //  Workaround: level down to verbose (should be info):
      ESP_LOGV(TAG, "%s node %d new state: %s", m_bus->GetName(), ev.nodeid, newstate.c_str());
      MyEvents.SignalEvent("canopen.node.state", &ev, sizeof(ev));
      nm->m_state->SetValue(newstate);
      }
    else
      {
      nm->m_state->SetStale(false);
      }
    }
  
  } // IncomingFrame()


/**
 * ProcessSendNMTJob: send NMT request and optionally wait for NMT state change
 *  a.k.a. heartbeat message.
 * 
 * Note: NMT responses are not a part of the CANopen NMT protocol, and
 *  sending "heartbeat" NMT state updates is optional for CANopen nodes.
 *  If the node sends no state info, waiting for it will result in timeout
 *  even though the state has in fact changed -- there's no way to know
 *  if the node doesn't tell.
 */
CANopenResult_t CANopenWorker::ProcessSendNMTJob()
  {
  // check bus:
  if (m_bus->m_mode != CAN_MODE_ACTIVE)
    return COR_ERR_NoCANWrite;
  
  // check parameters:
  if (m_job.nmt.nodeid > 127)
    return COR_ERR_ParamRange;
  
  // init request:
  CAN_frame_t txframe;
  memset(&txframe, 0, sizeof(txframe));
  
  txframe.origin = m_bus;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 2;
  txframe.MsgID = m_job.txid;
  txframe.data.u8[0] = (uint8_t) m_job.nmt.command;
  txframe.data.u8[1] = m_job.nmt.nodeid;
  
  TickType_t maxwait = pdMS_TO_TICKS(m_job.timeout_ms);
  
  do
    {
    // send request:
    m_job.trycnt++;
    txframe.Write();
    
    // immediate return?
    if (m_job.rxid == 0)
      return COR_OK;
    
    // wait for response signal from IncomingFrame():
    if (ulTaskNotifyTake(pdTRUE, maxwait))
      {
      // expected response for command?
      if ( (m_job.nmt.command == CONC_Start      && m_response.hb.state >= 5)
        || (m_job.nmt.command == CONC_Stop       && m_response.hb.state == 4)
        || (m_job.nmt.command == CONC_PreOp      && m_response.hb.state == 127)
        || (m_job.nmt.command == CONC_Reset      && m_response.hb.state != 4)
        || (m_job.nmt.command == CONC_CommReset  && m_response.hb.state != 4) )
        return COR_OK;
      }

    // timeout / wrong state → retry:
    if (m_job.trycnt < m_job.maxtries)
      vTaskDelay(pdMS_TO_TICKS(10));

    } while (m_job.trycnt < m_job.maxtries);

  return COR_ERR_Timeout;
  }


/**
 * ProcessReceiveHBJob: wait for next heartbeat message of a node,
 *  return state received.
 * 
 * Use this to read the current state or synchronize to the heartbeat.
 * Note: heartbeats are optional in CANopen.
 */
CANopenResult_t CANopenWorker::ProcessReceiveHBJob()
  {
  // check parameters:
  if (m_job.hb.nodeid < 1 || m_job.hb.nodeid > 127)
    return COR_ERR_ParamRange;
  
  TickType_t maxwait = pdMS_TO_TICKS(m_job.timeout_ms);
  
  do
    {
    m_job.trycnt++;
    
    // wait for receive signal from IncomingFrame():
    if (ulTaskNotifyTake(pdTRUE, maxwait))
      {
      // return state received:
      m_job.hb.state = (CANopenNMTState_t) m_response.hb.state;
      if (m_job.hb.statebuf)
        *m_job.hb.statebuf = m_job.hb.state;
      return COR_OK;
      }

    // timeout → retry:
    if (m_job.trycnt < m_job.maxtries)
      vTaskDelay(pdMS_TO_TICKS(10));

    } while (m_job.trycnt < m_job.maxtries);

  return COR_ERR_Timeout;
  }


/**
 * SendSDORequest: asynchronous tx of prepared CANopen SDO request
 */
void CANopenWorker::SendSDORequest()
  {
  // init tx frame:
  CAN_frame_t txframe;
  memset(&txframe, 0, sizeof(txframe));
  
  // load SDO request:
  txframe.origin = m_bus;
  txframe.FIR.B.FF = CAN_frame_std;
  txframe.FIR.B.DLC = 8;
  txframe.MsgID = m_job.txid;
  memcpy(txframe.data.u8, m_request.byte, 8);
  
  // send:
  txframe.Write();
  }


/**
 * AbortSDORequest: send SDO abort command
 */
void CANopenWorker::AbortSDORequest(uint32_t reason)
  {
  // backup request:
  uint8_t control = m_request.ctl.control;
  uint32_t data = m_request.ctl.data;
  
  // send abort:
  m_request.ctl.control = SDO_Abort;
  m_request.ctl.data = reason;
  SendSDORequest();
  
  // restore request:
  m_request.ctl.control = control;
  m_request.ctl.data = data;
  }


/**
 * ExecuteSDORequest: send SDO request and wait for response
 */
CANopenResult_t CANopenWorker::ExecuteSDORequest()
  {
  TickType_t maxwait = pdMS_TO_TICKS(m_job.timeout_ms);
  m_job.trycnt = 0;
  
  do
    {
    // send request:
    m_job.trycnt++;
    SendSDORequest();

    // wait for reply:
    if (ulTaskNotifyTake(pdTRUE, maxwait))
      return COR_OK;

    // timeout:
    AbortSDORequest(SDO_Abort_Timeout);
    if (m_job.trycnt < m_job.maxtries)
      vTaskDelay(pdMS_TO_TICKS(10));

    } while (m_job.trycnt < m_job.maxtries);
  
  return COR_ERR_Timeout;
  }


/**
 * ProcessReadSDOJob: read bytes from SDO server into buffer
 *   - reads data into m_job.sdo.buf (up to m_job.sdo.bufsize bytes)
 *   - returns data length read in m_job.sdo.xfersize
 *   - remaining buffer space will be zeroed
 *   - on result COR_ERR_BufferTooSmall, the buffer has been filled up to m_job.sdo.bufsize
 *   - on abort, the CANopen error code will be written into m_job.sdo.error
 * 
 * Note: result interpretation is up to caller (check device object dictionary for data types & sizes).
 *   As CANopen is little endian as ESP32, we don't need to check lengths on numerical results,
 *   i.e. anything from int8_t to uint32_t can simply be read into a uint32_t buffer.
 */
CANopenResult_t CANopenWorker::ProcessReadSDOJob()
  {
  // check for CAN write access:
  if (m_bus->m_mode != CAN_MODE_ACTIVE)
    return COR_ERR_NoCANWrite;

  // check parameters:
  if (m_job.sdo.nodeid < 1 || m_job.sdo.nodeid > 127)
    return COR_ERR_ParamRange;
  if (m_job.sdo.buf == NULL || m_job.sdo.bufsize == 0)
    return COR_ERR_ParamRange;
  
  uint8_t n, toggle, dlen;

  // init receive buffer:
  memset(m_job.sdo.buf, 0, m_job.sdo.bufsize);
  uint8_t *buf = m_job.sdo.buf;
  m_job.sdo.xfersize = 0;
  
  // request upload:
  memset(&m_request, 0, sizeof(m_request));
  m_request.exp.index = m_job.sdo.index;
  m_request.exp.subindex = m_job.sdo.subindex;
  m_request.exp.control = SDO_InitUploadRequest;
  if (ExecuteSDORequest() != COR_OK)
    {
    m_job.sdo.error = SDO_Abort_Timeout;
    return COR_ERR_Timeout;
    }

  // check response:
  if ((m_response.exp.control & SDO_CommandMask) != SDO_InitUploadResponse
    || m_response.exp.index != m_request.exp.index
    || m_response.exp.subindex != m_request.exp.subindex)
    {
    if ((m_response.exp.control & SDO_CommandMask) == SDO_Abort)
      m_job.sdo.error = m_response.ctl.data;
    else
      m_job.sdo.error = CANopen_BusCollision;
    ESP_LOGD(TAG, "ReadSDO #%d 0x%04x.%02x: InitUpload failed, CANopen error code 0x%08x",
      m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.error);
    return COR_ERR_SDO_Access;
    }

  // check for expedited xfer:
  if (m_response.exp.control & SDO_Expedited)
    {
    m_job.sdo.contsize = 4 - ((m_response.exp.control & SDO_ExpeditedSizeMask) >> 2);
    
    // copy response data to buffer:
    dlen = 8 - ((m_response.exp.control & SDO_ExpeditedSizeMask) >> 2);
    for (n = 4; n < dlen && m_job.sdo.xfersize < m_job.sdo.bufsize; n++, m_job.sdo.xfersize++)
      *buf++ = m_response.byte[n];
    if (n < dlen)
      {
      ESP_LOGD(TAG, "ReadSDO #%d 0x%04x.%02x: buffer too small, readlen=%d",
        m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.xfersize);
      m_job.sdo.error = SDO_Abort_OutOfMemory;
      return COR_ERR_BufferTooSmall;
      }
    
    return COR_OK;
    }

  // do segmented xfer:
  
  if (m_response.exp.control & SDO_SizeIndicated)
    m_job.sdo.contsize = m_response.ctl.data;
  else
    m_job.sdo.contsize = 0; // unknown size
  
  toggle = 0;
  memset(&m_request, 0, sizeof(m_request));
  do
    {
    // request next segment:
    m_request.seg.control = (SDO_UploadSegmentRequest|toggle);
    if (ExecuteSDORequest() != COR_OK)
      {
      m_job.sdo.error = SDO_Abort_Timeout;
      return COR_ERR_Timeout;
      }

    // check response:
    if ((m_response.seg.control & (SDO_CommandMask|SDO_SegmentToggle)) != (SDO_UploadSegmentResponse|toggle))
      {
      // mismatch → abort:
      ESP_LOGD(TAG, "ReadSDO #%d 0x%04x.%02x: segment mismatch, readlen=%d",
        m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.xfersize);
      AbortSDORequest(SDO_Abort_SegMismatch);
      m_job.sdo.error = SDO_Abort_SegMismatch;
      return COR_ERR_SDO_SegMismatch;
      }

    // ok, copy response data to buffer:
    dlen = 8 - ((m_response.seg.control & SDO_SegmentUnusedMask) >> 1);
    for (n = 1; n < dlen && m_job.sdo.xfersize < m_job.sdo.bufsize; n++, m_job.sdo.xfersize++)
      *buf++ = m_response.byte[n];
    if (n < dlen)
      {
      ESP_LOGD(TAG, "ReadSDO #%d 0x%04x.%02x: buffer too small, readlen=%d",
        m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.xfersize);
      if (!(m_response.seg.control & SDO_SegmentEnd))
        AbortSDORequest(SDO_Abort_OutOfMemory);
      m_job.sdo.error = SDO_Abort_OutOfMemory;
      return COR_ERR_BufferTooSmall;
      }

    // last segment fetched? => success!
    if (m_response.seg.control & SDO_SegmentEnd)
      return COR_OK;

    // toggle toggle bit:
    toggle = toggle ? 0 : SDO_SegmentToggle;

    } while(1);
  // not reached
  }


/**
 * ProcessWriteSDOJob: write bytes from buffer into SDO server
 *   - sends m_job.sdo.bufsize bytes from m_job.sdo.buf
 *   - … or 4 bytes from m_job.sdo.buf if bufsize is 0 (use for integer SDOs of unknown type)
 *   - returns data length sent in m_job.sdo.xfersize
 *   - on abort, the CANopen error code will be written into m_job.sdo.error
 * 
 * Note: the caller needs to know data type & size of the SDO register (check device object dictionary).
 *   As CANopen servers normally are intelligent, anything from int8_t to uint32_t can simply be
 *   sent as a uint32_t with bufsize=0, the server will know how to convert it.
 */
CANopenResult_t CANopenWorker::ProcessWriteSDOJob()
  {
  // check for CAN write access:
  if (m_bus->m_mode != CAN_MODE_ACTIVE)
    return COR_ERR_NoCANWrite;

  // check parameters:
  if (m_job.sdo.nodeid < 1 || m_job.sdo.nodeid > 127)
    return COR_ERR_ParamRange;
  if (m_job.sdo.buf == NULL)
    return COR_ERR_ParamRange;
  
  uint8_t n, toggle;
  
  // init send buffer:
  uint8_t *buf = m_job.sdo.buf;
  m_job.sdo.xfersize = 0;
  
  // request download:
  memset(&m_request, 0, sizeof(m_request));
  m_request.exp.index = m_job.sdo.index;
  m_request.exp.subindex = m_job.sdo.subindex;
  if (m_job.sdo.bufsize == 0)
    {
    // …expedited without size indication:
    m_request.exp.control = SDO_InitDownloadRequest | SDO_Expedited;
    for (n=0; n < 4; n++)
      m_request.exp.data[n] = *buf++;
    }
  else if (m_job.sdo.bufsize <= 4)
    {
    // …expedited with size indication:
    m_request.exp.control = SDO_InitDownloadRequest | SDO_Expedited | SDO_SizeIndicated | ((4-m_job.sdo.bufsize) << 2);
    for (n=0; n < m_job.sdo.bufsize; n++)
      m_request.exp.data[n] = *buf++;
    }
  else
    {
    // …segmented:
    m_request.exp.control = SDO_InitDownloadRequest | SDO_SizeIndicated;
    m_request.ctl.data = m_job.sdo.bufsize;
    }
  if (ExecuteSDORequest() != COR_OK)
    {
    m_job.sdo.error = SDO_Abort_Timeout;
    return COR_ERR_Timeout;
    }

  // check response:
  if ((m_response.exp.control & SDO_CommandMask) != SDO_InitDownloadResponse
    || m_response.exp.index != m_request.exp.index
    || m_response.exp.subindex != m_request.exp.subindex)
    {
    if ((m_response.exp.control & SDO_CommandMask) == SDO_Abort)
      m_job.sdo.error = m_response.ctl.data;
    else
      m_job.sdo.error = CANopen_BusCollision;
    ESP_LOGD(TAG, "WriteSDO #%d 0x%04x.%02x: InitDownload failed, CANopen error code 0x%08x",
      m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.error);
    return COR_ERR_SDO_Access;
    }
  
  // done?
  if (m_request.exp.control & SDO_Expedited)
    {
    m_job.sdo.xfersize = m_job.sdo.bufsize;
    return COR_OK;
    }

  // do segmented transfer:
  toggle = 0;
  do
    {
    // send next segment:
    for (n=0; n < 7 && m_job.sdo.xfersize < m_job.sdo.bufsize; n++, m_job.sdo.xfersize++)
      m_request.seg.data[n] = *buf++;
    m_request.seg.control = SDO_DownloadSegmentRequest | toggle | ((7-n) << 1);
    for (; n < 7; n++)
      m_request.seg.data[n] = 0;
    if (m_job.sdo.xfersize == m_job.sdo.bufsize)
      m_request.seg.control |= SDO_SegmentEnd;
    
    if (ExecuteSDORequest() != COR_OK)
      {
      m_job.sdo.error = SDO_Abort_Timeout;
      return COR_ERR_Timeout;
      }

    // check response:
    if ((m_response.seg.control & (SDO_CommandMask|SDO_SegmentToggle)) != (SDO_DownloadSegmentResponse|toggle))
      {
      // mismatch → abort:
      ESP_LOGD(TAG, "WriteSDO #%d 0x%04x.%02x: segment mismatch, readlen=%d",
        m_job.sdo.nodeid, m_job.sdo.index, m_job.sdo.subindex, m_job.sdo.xfersize);
      AbortSDORequest(SDO_Abort_SegMismatch);
      m_job.sdo.error = SDO_Abort_SegMismatch;
      return COR_ERR_SDO_SegMismatch;
      }

    // last segment sent? => success!
    if (m_request.seg.control & SDO_SegmentEnd)
      return COR_OK;

    // toggle toggle bit:
    toggle = toggle ? 0 : SDO_SegmentToggle;

    } while(1);
  // not reached
  }


