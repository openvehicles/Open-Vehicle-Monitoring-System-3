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


/**
 * CANopenClient provides the user API to the CANopen framework.
 * 
 * See CANopen shell commands for usage examples.
 * 
 * On creation the client automatically connects to the worker or starts a
 *   new worker if necessary.
 * 
 * The client provides a queue for asynchronous CANopenJob processing
 *   and methods for synchronous CANopenJob execution.
 * 
 * CANopenClient uses the CiA DS301 default IDs for node addressing, i.e.
 *   NMT request     → 0x000
 *   NMT response    → 0x700 + nodeid
 *   SDO request     → 0x600 + nodeid
 *   SDO response    → 0x580 + nodeid
 * 
 * If you need another address scheme, create a sub class of CANopenClient
 *   and override the Init…() methods as necessary.
 * 
 * Hint: sub classes can also be used to add device specific methods, see
 *   Twizy implementation for a SEVCON Gen4 client example.
 */

CANopenAsyncClient::CANopenAsyncClient(CANopenWorker* worker, int queuesize /*=20*/)
  {
  m_worker = worker;
  m_worker->Open(this);
  m_done_queue = xQueueCreate(queuesize, sizeof(CANopenJob));
  }

CANopenAsyncClient::CANopenAsyncClient(canbus *bus, int queuesize /*=20*/)
  {
  m_worker = MyCANopen.Start(bus);
  m_worker->Open(this);
  m_done_queue = xQueueCreate(queuesize, sizeof(CANopenJob));
  }

CANopenAsyncClient::~CANopenAsyncClient()
  {
  m_worker->Close(this);
  vQueueDelete(m_done_queue);
  }


/**
 * SubmitDoneCallback: called by the worker to send us a job result
 *    - this should normally not be called by the application
 */
CANopenResult_t CANopenAsyncClient::SubmitDoneCallback(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueSend(m_done_queue, &job, maxqueuewait) != pdTRUE)
    return COR_ERR_QueueFull;
  return COR_OK;
  }


/**
 * [Queue API]
 * SubmitJob: send a job to the worker
 *    - use this to submit jobs for asynchronous execution
 *    - use ReceiveDone() to check for results
 * 
 * See CANopenClient::ExecuteJob() for synchronous operation.
 */
CANopenResult_t CANopenAsyncClient::SubmitJob(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  job.client = this;
  CANopenResult_t res = m_worker->SubmitJob(job, maxqueuewait);
  if (res == COR_ERR_QueueFull)
    ESP_LOGW(TAG, "Job submission failed: Worker queue is full");
  return res;
  }

/**
 * [Queue API]
 * ReceiveDone: check for / retrieve next job result
 *    - returns COR_ERR_QueueEmpty if no job results are pending
 *    - call with maxqueuewait=0 (default) to return immediately if queue is empty
 */
CANopenResult_t CANopenAsyncClient::ReceiveDone(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueReceive(m_done_queue, &job, maxqueuewait) != pdTRUE)
    return COR_ERR_QueueEmpty;
  return job.result;
  }


/**
 * InitSendNMT: prepare NMT command
 * Hint: override for customisation
 */
void CANopenAsyncClient::InitSendNMT(CANopenJob& job,
    uint8_t nodeid, CANopenNMTCommand_t command,
    bool wait_for_state /*=false*/, int resp_timeout_ms /*=1000*/, int max_tries /*=3*/)
  {
  memset(&job, 0, sizeof(job));
  
  job.type = COJT_SendNMT;
  job.nmt.nodeid = nodeid;
  job.nmt.command = command;
  
  job.txid = 0x000;
  if (wait_for_state)
    {
    job.rxid = 0x700 + nodeid;
    job.timeout_ms = resp_timeout_ms;
    job.maxtries = max_tries;
    }
  }

/**
 * InitReceiveHB: prepare receive heartbeat command
 * Hint: override for customisation
 */
void CANopenAsyncClient::InitReceiveHB(CANopenJob& job,
    uint8_t nodeid, CANopenNMTState_t* statebuf /*=NULL*/,
    int recv_timeout_ms /*=1000*/, int max_tries /*=1*/)
  {
  memset(&job, 0, sizeof(job));
  
  job.type = COJT_ReceiveHB;
  job.hb.nodeid = nodeid;
  job.hb.statebuf = statebuf;
  
  job.txid = 0;
  job.rxid = 0x700 + nodeid;
  job.timeout_ms = recv_timeout_ms;
  job.maxtries = max_tries;
  }

/**
 * InitReadSDO: prepare ReadSDO command
 * Hint: override for customisation
 */
void CANopenAsyncClient::InitReadSDO(CANopenJob& job,
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  memset(&job, 0, sizeof(job));
  
  job.type = COJT_ReadSDO;
  job.sdo.nodeid = nodeid;
  job.sdo.index = index;
  job.sdo.subindex = subindex;
  job.sdo.buf = buf;
  job.sdo.bufsize = bufsize;
  
  job.txid = 0x600 + nodeid;
  job.rxid = 0x580 + nodeid;
  job.timeout_ms = resp_timeout_ms;
  job.maxtries = max_tries;
  }

/**
 * InitWriteSDO: prepare WriteSDO command
 * Hint: override for customisation
 */
void CANopenAsyncClient::InitWriteSDO(CANopenJob& job,
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  memset(&job, 0, sizeof(job));
  
  job.type = COJT_WriteSDO;
  job.sdo.nodeid = nodeid;
  job.sdo.index = index;
  job.sdo.subindex = subindex;
  job.sdo.buf = buf;
  job.sdo.bufsize = bufsize;
  
  job.txid = 0x600 + nodeid;
  job.rxid = 0x580 + nodeid;
  job.timeout_ms = resp_timeout_ms;
  job.maxtries = max_tries;
  }


/**
 * [Main API]
 * SendNMT: send NMT request and optionally wait for NMT state change
 *  a.k.a. heartbeat message.
 * 
 * Note: NMT responses are not a part of the CANopen NMT protocol, and
 *  sending "heartbeat" NMT state updates is optional for CANopen nodes.
 *  If the node sends no state info, waiting for it will result in timeout
 *  even though the state has in fact changed -- there's no way to know
 *  if the node doesn't tell.
 */
CANopenResult_t CANopenAsyncClient::SendNMT(
    uint8_t nodeid, CANopenNMTCommand_t command,
    bool wait_for_state /*=false*/, int resp_timeout_ms /*=1000*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitSendNMT(job, nodeid, command, wait_for_state, resp_timeout_ms, max_tries);
  return SubmitJob(job);
  }


/**
 * [Main API]
 * ReceiveHB: wait for next heartbeat message of a node,
 *  return state received.
 * 
 * Use this to read the current state or synchronize to the heartbeat.
 * Note: heartbeats are optional in CANopen.
 */
CANopenResult_t CANopenAsyncClient::ReceiveHB(
    uint8_t nodeid, CANopenNMTState_t* statebuf /*=NULL*/,
    int recv_timeout_ms /*=1000*/, int max_tries /*=1*/)
  {
  CANopenJob job;
  InitReceiveHB(job, nodeid, statebuf, recv_timeout_ms, max_tries);
  return SubmitJob(job);
  }


/**
 * [Main API]
 * ReadSDO: read bytes from SDO server into buffer
 *   - reads data into buf (up to bufsize bytes)
 *   - returns data length read in job.sdo.xfersize
 *   - … and data length available in job.sdo.contsize (if known)
 *   - remaining buffer space will be zeroed
 *   - on result COR_ERR_BufferTooSmall, the buffer has been filled up to bufsize
 *   - on abort, the CANopen error code can be retrieved from job.sdo.error
 * 
 * Note: result interpretation is up to caller (check device object dictionary for data types & sizes).
 *   As CANopen is little endian as ESP32, we don't need to check lengths on numerical results,
 *   i.e. anything from int8_t to uint32_t can simply be read into a uint32_t buffer.
 */
CANopenResult_t CANopenAsyncClient::ReadSDO(
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitReadSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return SubmitJob(job);
  }


/**
 * [Main API]
 * WriteSDO: write bytes from buffer into SDO server
 *   - sends bufsize bytes from buf
 *   - … or 4 bytes from buf if bufsize is 0 (use for integer SDOs of unknown type)
 *   - returns data length sent in job.sdo.xfersize
 *   - on abort, the CANopen error code can be retrieved from job.sdo.error
 * 
 * Note: the caller needs to know data type & size of the SDO register (check device object dictionary).
 *   As CANopen servers normally are intelligent, anything from int8_t to uint32_t can simply be
 *   sent as a uint32_t with bufsize=0, the server will know how to convert it.
 */
CANopenResult_t CANopenAsyncClient::WriteSDO(
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitWriteSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return SubmitJob(job);
  }





/**
 * CANopenClient provides the synchronous API to the CANopen framework.
 * 
 * The API methods will block until the job is done, job detail results
 *   are returned in the caller provided job.
 * 
 * See CANopen shell commands for usage examples.
 */

CANopenClient::CANopenClient(CANopenWorker* worker)
  : CANopenAsyncClient(worker, 1)
  {
  m_mutex = xSemaphoreCreateMutex();
  }

CANopenClient::CANopenClient(canbus *bus)
  : CANopenAsyncClient(bus, 1)
  {
  m_mutex = xSemaphoreCreateMutex();
  }

CANopenClient::~CANopenClient()
  {
  vSemaphoreDelete(m_mutex);
  }


/**
 * [Queue API]
 * ExecuteJob: send a job to the worker, wait for the result
 *    - use this for synchronous execution of jobs
 *    - see CANopenAsyncClient::SubmitJob() for asynchronous operation
 */
CANopenResult_t CANopenClient::ExecuteJob(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xSemaphoreTake(m_mutex, maxqueuewait) != pdTRUE)
    return COR_ERR_QueueFull;
  
  CANopenResult_t res;
  if (SubmitJob(job, maxqueuewait) != COR_WAIT)
    res = COR_ERR_QueueFull;
  else
    res = ReceiveDone(job, portMAX_DELAY);
  
  xSemaphoreGive(m_mutex);
  return res;
  }


/**
 * [Main API]
 * SendNMT: send NMT request and optionally wait for NMT state change
 *  a.k.a. heartbeat message.
 * 
 * Note: NMT responses are not a part of the CANopen NMT protocol, and
 *  sending "heartbeat" NMT state updates is optional for CANopen nodes.
 *  If the node sends no state info, waiting for it will result in timeout
 *  even though the state has in fact changed -- there's no way to know
 *  if the node doesn't tell.
 */
CANopenResult_t CANopenClient::SendNMT(CANopenJob& job,
    uint8_t nodeid, CANopenNMTCommand_t command,
    bool wait_for_state /*=false*/, int resp_timeout_ms /*=1000*/, int max_tries /*=3*/)
  {
  InitSendNMT(job, nodeid, command, wait_for_state, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }


/**
 * [Main API]
 * ReceiveHB: wait for next heartbeat message of a node,
 *  return state received.
 * 
 * Use this to read the current state or synchronize to the heartbeat.
 * Note: heartbeats are optional in CANopen.
 */
CANopenResult_t CANopenClient::ReceiveHB(CANopenJob& job,
    uint8_t nodeid, CANopenNMTState_t* statebuf /*=NULL*/,
    int recv_timeout_ms /*=1000*/, int max_tries /*=1*/)
  {
  InitReceiveHB(job, nodeid, statebuf, recv_timeout_ms, max_tries);
  return ExecuteJob(job);
  }


/**
 * [Main API]
 * ReadSDO: read bytes from SDO server into buffer
 *   - reads data into buf (up to bufsize bytes)
 *   - returns data length read in job.sdo.xfersize
 *   - … and data length available in job.sdo.contsize (if known)
 *   - remaining buffer space will be zeroed
 *   - on result COR_ERR_BufferTooSmall, the buffer has been filled up to bufsize
 *   - on abort, the CANopen error code can be retrieved from job.sdo.error
 * 
 * Note: result interpretation is up to caller (check device object dictionary for data types & sizes).
 *   As CANopen is little endian as ESP32, we don't need to check lengths on numerical results,
 *   i.e. anything from int8_t to uint32_t can simply be read into a uint32_t buffer.
 */
CANopenResult_t CANopenClient::ReadSDO(CANopenJob& job,
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  InitReadSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }


/**
 * [Main API]
 * WriteSDO: write bytes from buffer into SDO server
 *   - sends bufsize bytes from buf
 *   - … or 4 bytes from buf if bufsize is 0 (use for integer SDOs of unknown type)
 *   - returns data length sent in job.sdo.xfersize
 *   - on abort, the CANopen error code can be retrieved from job.sdo.error
 * 
 * Note: the caller needs to know data type & size of the SDO register (check device object dictionary).
 *   As CANopen servers normally are intelligent, anything from int8_t to uint32_t can simply be
 *   sent as a uint32_t with bufsize=0, the server will know how to convert it.
 */
CANopenResult_t CANopenClient::WriteSDO(CANopenJob& job,
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  InitWriteSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }

