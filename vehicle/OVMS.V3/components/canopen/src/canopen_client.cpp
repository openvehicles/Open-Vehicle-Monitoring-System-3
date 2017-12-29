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

CANopenClient::CANopenClient(CANopenWorker* worker)
  {
  m_worker = worker;
  m_worker->Open(this);
  m_done_queue = xQueueCreate(20, sizeof(CANopenJob));
  memset(&m_jobdone, 0, sizeof(m_jobdone));
  }

CANopenClient::CANopenClient(canbus *bus)
  {
  m_worker = MyCANopen.Start(bus);
  m_worker->Open(this);
  m_done_queue = xQueueCreate(20, sizeof(CANopenJob));
  memset(&m_jobdone, 0, sizeof(m_jobdone));
  }

CANopenClient::~CANopenClient()
  {
  m_worker->Close(this);
  vQueueDelete(m_done_queue);
  }


/**
 * [Main user API]
 * SendNMT: send NMT request and optionally wait for NMT state change
 *  a.k.a. heartbeat message.
 * 
 * Note: NMT responses are not a part of the CANopen NMT protocol, and
 *  sending "heartbeat" NMT state updates is optional for CANopen nodes.
 *  If the node sends no state info, waiting for it will result in timeout
 *  even though the state has in fact changed -- there's no way to know
 *  if the node doesn't tell.
 */
CANopenResult_t CANopenClient::SendNMT(
    uint8_t nodeid, CANopenNMTCommand_t command,
    bool wait_for_state /*=false*/, int resp_timeout_ms /*=1000*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitSendNMT(job, nodeid, command, wait_for_state, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }

/**
 * InitSendNMT: prepare NMT command
 * Hint: overload for customisation
 */
void CANopenClient::InitSendNMT(CANopenJob& job,
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
 * [Main user API]
 * ReadSDO: read bytes from SDO server into buffer
 *   - reads data into buf (up to bufsize bytes)
 *   - returns data length read in m_jobdone.sdo.xfersize
 *   - … and data length available in m_jobdone.sdo.contsize (if known)
 *   - remaining buffer space will be zeroed
 *   - on result COR_ERR_BufferTooSmall, the buffer has been filled up to bufsize
 *   - on abort, the CANopen error code can be retrieved from m_jobdone.sdo.error
 * 
 * Note: result interpretation is up to caller (check device object dictionary for data types & sizes).
 *   As CANopen is little endian as ESP32, we don't need to check lengths on numerical results,
 *   i.e. anything from int8_t to uint32_t can simply be read into a uint32_t buffer.
 */
CANopenResult_t CANopenClient::ReadSDO(
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitReadSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }

/**
 * InitReadSDO: prepare ReadSDO command
 * Hint: overload for customisation
 */
void CANopenClient::InitReadSDO(CANopenJob& job,
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
 * [Main user API]
 * WriteSDO: write bytes from buffer into SDO server
 *   - sends bufsize bytes from buf
 *   - … or 4 bytes from buf if bufsize is 0 (use for integer SDOs of unknown type)
 *   - returns data length sent in m_jobdone.sdo.xfersize
 *   - on abort, the CANopen error code can be retrieved from m_jobdone.sdo.error
 * 
 * Note: the caller needs to know data type & size of the SDO register (check device object dictionary).
 *   As CANopen servers normally are intelligent, anything from int8_t to uint32_t can simply be
 *   sent as a uint32_t with bufsize=0, the server will know how to convert it.
 */
CANopenResult_t CANopenClient::WriteSDO(
    uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
    int resp_timeout_ms /*=50*/, int max_tries /*=3*/)
  {
  CANopenJob job;
  InitWriteSDO(job, nodeid, index, subindex, buf, bufsize, resp_timeout_ms, max_tries);
  return ExecuteJob(job);
  }

/**
 * InitWriteSDO: prepare WriteSDO command
 * Hint: overload for customisation
 */
void CANopenClient::InitWriteSDO(CANopenJob& job,
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
 * [Advanced usage API]
 * SubmitJob: send a job to the worker
 *    - use this to submit jobs for asynchronous execution
 *    - use ReceiveDone() to check for results
 * 
 * See ExecuteJob() for synchronous operation.
 */
CANopenResult_t CANopenClient::SubmitJob(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  job.client = this;
  CANopenResult_t res = m_worker->SubmitJob(job, maxqueuewait);
  if (res == COR_ERR_QueueFull)
    ESP_LOGW(TAG, "Job submission failed: Worker queue is full");
  return res;
  }

/**
 * SubmitDone: called by the worker to send us a job result
 *    - this should normally not be called by the application
 */
CANopenResult_t CANopenClient::SubmitDone(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueSend(m_done_queue, &job, maxqueuewait) != pdTRUE)
    return COR_ERR_QueueFull;
  return COR_OK;
  }

/**
 * [Advanced usage API]
 * ReceiveDone: check for / retrieve next job result
 *    - returns COR_ERR_QueueEmpty if no job results are pending
 *    - call with maxqueuewait=0 (default) to return immediately if queue is empty
 *    - the job is copied into m_jobdone for easy access to the last job result
 */
CANopenResult_t CANopenClient::ReceiveDone(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (xQueueReceive(m_done_queue, &job, maxqueuewait) != pdTRUE)
    return COR_ERR_QueueEmpty;
  m_jobdone = job;
  return job.result;
  }

/**
 * [Advanced usage API]
 * ExecuteJob: send a job to the worker, wait for the result
 *    - use this for synchronous execution
 *    - see SubmitJob() for asynchronous operation
 */
CANopenResult_t CANopenClient::ExecuteJob(CANopenJob& job, TickType_t maxqueuewait /*=0*/)
  {
  if (SubmitJob(job, maxqueuewait) != COR_WAIT)
    return COR_ERR_QueueFull;
  return ReceiveDone(job, portMAX_DELAY);
  }


