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

#ifndef __CANOPEN_H__
#define __CANOPEN_H__

#include <forward_list>

#include "can.h"

#include "ovms_log.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "ovms_command.h"

#define CAN_INTERFACE_CNT         3

#define CANopen_GeneralError      0x08000000  // check for device specific error details
#define CANopen_BusCollision      0xffffffff  // another master is active / non-CANopen frame received

typedef enum __attribute__ ((__packed__))
  {
  COR_OK = 0,
  
  // API level:
  COR_WAIT,                   // job waiting to be processed
  COR_ERR_UnknownJobType,
  COR_ERR_QueueFull,
  COR_ERR_QueueEmpty,
  COR_ERR_NoCANWrite,
  COR_ERR_ParamRange,
  COR_ERR_BufferTooSmall,
  
  // Protocol level:
  COR_ERR_Timeout,
  COR_ERR_SDO_Access,
  COR_ERR_SDO_SegMismatch,
  
  // General purpose application level:
  COR_ERR_DeviceOffline,
  COR_ERR_UnknownDevice,
  COR_ERR_LoginFailed,
  COR_ERR_PreOpFailed
  
  } CANopenResult_t;

typedef enum __attribute__ ((__packed__))
  {
  CONC_Start          = 1,
  CONC_Stop           = 2,
  CONC_PreOp          = 128,
  CONC_Reset          = 129,
  CONC_CommReset      = 130
  } CANopenNMTCommand_t;

typedef enum __attribute__ ((__packed__))
  {
  CONS_Booting        = 0,
  CONS_Stopped        = 4,
  CONS_Operational    = 5,
  CONS_PreOperational = 127
  } CANopenNMTState_t;

struct CANopenNMTEvent                  // event "canopen.node.state"
  {
  canbus*             origin;
  uint8_t             nodeid;
  CANopenNMTState_t   state;
  };

struct CANopenEMCYEvent                 // event "canopen.node.emcy"
  {
  canbus*             origin;
  uint8_t             nodeid;
  uint16_t            code;             // see CiA DS301, Table 21: Emergency Error Codes
  uint8_t             type;             // see CiA DS301, Table 48: Structure of the Error Register
  uint8_t             data[5];          // device specific
  };

struct CANopenNodeMetrics
  {
  OvmsMetricString*     m_state;        // metric "co.<bus>.nd<nodeid>.state"
  OvmsMetricInt*        m_emcy_code;    // metric "co.<bus>.nd<nodeid>.emcy.code"
  OvmsMetricInt*        m_emcy_type;    // metric "co.<bus>.nd<nodeid>.emcy.type"
  };

typedef std::map<uint8_t, CANopenNodeMetrics*> CANopenNodeMetricsMap;

class CANopenClient;
typedef std::forward_list<CANopenClient*> CANopenClientList;


/**
 * A CANopenJob is a transfer process consisting of a single send or a
 *   series of one or more requests & responses.
 * 
 * CANopenClients create and submit Jobs to be processed to a CANopenWorker.
 * After finish/abort, the Worker sends the Job to the clients done queue.
 */

typedef enum __attribute__ ((__packed__))
  {
  COJT_None = 0,
  COJT_SendNMT,
  COJT_ReadSDO,
  COJT_WriteSDO
  } CANopenJob_t;

struct __attribute__ ((__packed__)) CANopenJob
  {
  CANopenClient*        client;
  CANopenJob_t          type;
  CANopenResult_t       result;         // current job state or result
  
  uint16_t              txid;           // requests sent to ←
  uint16_t              rxid;           // expecting responses from ←
  
  uint16_t              timeout_ms;     // single response timeout
  uint8_t               maxtries;       // max try count
  uint8_t               trycnt;         // actual try count
  
  union __attribute__ ((__packed__))
    {
    struct __attribute__ ((__packed__))
      {
      uint8_t               nodeid;         // 0=broadcast to all nodes
      CANopenNMTCommand_t   command;
      } nmt;
    struct __attribute__ ((__packed__))
      {
      uint8_t               nodeid;         // no broadcast allowed
      uint16_t              index;          // SDO register address
      uint8_t               subindex;       // SDO subregister address
      uint8_t*              buf;            // tx from / rx to
      size_t                bufsize;        // rx: buffer capacity, tx: unused
      size_t                xfersize;       // byte count sent / received
      size_t                contsize;       // content size of SDO (if indicated by slave)
      uint32_t              error;          // CANopen general error code
      } sdo;
    };
  };

typedef union __attribute__ ((__packed__))
  {
  uint8_t       byte[8];        // raw access
  struct __attribute__ ((__packed__))
    {
    uint8_t     control;        // protocol request / response
    uint16_t    index;          // SDO register address (little endian)
    uint8_t     subindex;       // SDO register sub index
    uint8_t     data[4];        // expedited payload
    } exp;
  struct __attribute__ ((__packed__))
    {
    uint8_t     control;        // protocol request / response
    uint8_t     data[7];        // segmented payload
    } seg;
  struct __attribute__ ((__packed__))
    {
    uint8_t     control;        // protocol request / response
    uint16_t    index;          // SDO register address (little endian)
    uint8_t     subindex;       // SDO register sub index
    uint32_t    data;           // abort reason / error code (little endian)
    } ctl;
  } CANopenFrame_t;


/**
 * A CANopenWorker processes CANopenJobs on a specific bus.
 * 
 * CANopenClients create and submit Jobs to be processed to a CANopenWorker.
 * After finish/abort, the Worker sends the Job to the clients done queue.
 * 
 * A CANopenWorker also monitors the bus for emergency and heartbeat
 * messages, and translates these into events and metrics updates.
 */
class CANopenWorker
  {
  public:
    CANopenWorker(canbus* canbus);
    ~CANopenWorker();
  
  public:
    void JobTask();
    void IncomingFrame(CAN_frame_t* frame);
    void Open(CANopenClient* client);
    void Close(CANopenClient* client);
    bool IsClient(CANopenClient* client);
    void StatusReport(int verbosity, OvmsWriter* writer);
    CANopenNodeMetrics *GetNodeMetrics(uint8_t nodeid);
  
  public:
    CANopenResult_t SubmitJob(CANopenJob& job, TickType_t maxqueuewait=0);
  
  protected:
    CANopenResult_t ProcessSendNMTJob();
    CANopenResult_t ProcessReadSDOJob();
    CANopenResult_t ProcessWriteSDOJob();
  
  private:
    void SendSDORequest();
    void AbortSDORequest(uint32_t reason);
    CANopenResult_t ExecuteSDORequest();

  public:
    canbus*               m_bus;            // max one worker per bus
    int                   m_clientcnt;
    CANopenClientList     m_clients;
    
    char                  m_taskname[12];   // "COwrk canX"
    TaskHandle_t          m_jobtask;        // worker task
    QueueHandle_t         m_jobqueue;       // job rx queue
    
    uint32_t              m_jobcnt;
    uint32_t              m_jobcnt_timeout;
    uint32_t              m_jobcnt_error;
    
    CANopenJob            m_job;            // job currently processed
    
    CANopenNodeMetricsMap m_nodemetrics;    // map: nodeid → node metrics


  private:
    CANopenFrame_t        m_request;
    CANopenFrame_t        m_response;
  
  };


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
class CANopenClient
  {
  public:
    CANopenClient(canbus* canbus);
    CANopenClient(CANopenWorker* worker);
    ~CANopenClient();
  
  public:
    // Advanced usage API:
    CANopenResult_t SubmitJob(CANopenJob& job, TickType_t maxqueuewait=0);
    CANopenResult_t SubmitDone(CANopenJob& job, TickType_t maxqueuewait=0);
    CANopenResult_t ReceiveDone(CANopenJob& job, TickType_t maxqueuewait=0);
    CANopenResult_t ExecuteJob(CANopenJob& job, TickType_t maxqueuewait=0);
  
  public:
    // Main usage API:
    CANopenResult_t SendNMT(uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);
    CANopenResult_t ReadSDO(uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=50, int max_tries=3);
    CANopenResult_t WriteSDO(uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=50, int max_tries=3);
  
  public:
    // Customisation:
    void InitSendNMT(CANopenJob& job, uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);
    void InitReadSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=50, int max_tries=3);
    void InitWriteSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=50, int max_tries=3);
  
  public:
    CANopenWorker*        m_worker;
    QueueHandle_t         m_done_queue;
    CANopenJob            m_jobdone;        // last job done, may be used to access result details
  
  };


/**
 * The CANopen master manages all CANopenWorkers, provides shell commands
 *   and the CAN rx task for all workers.
 */
class CANopen
  {
  public:
    CANopen();
    ~CANopen();
  
  public:
    void CanRxTask();
  
  public:
    CANopenWorker* Start(canbus* bus);
    bool Stop(canbus* bus);
    CANopenWorker* GetWorker(canbus* bus);
    void StatusReport(int verbosity, OvmsWriter* writer);

  public:
    const std::string GetResultString(const CANopenResult_t result);

  public:
    QueueHandle_t         m_rxqueue;    // CAN rx queue
    TaskHandle_t          m_rxtask;     // CAN rx task

    CANopenWorker*        m_worker[CAN_INTERFACE_CNT];
    int                   m_workercnt;

  };

extern CANopen MyCANopen;


#endif // __CANOPEN_H__
