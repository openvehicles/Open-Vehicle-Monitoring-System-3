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

#define CAN_INTERFACE_CNT         4

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
  COR_ERR_DeviceOffline = 0x80,
  COR_ERR_UnknownDevice,
  COR_ERR_LoginFailed,
  COR_ERR_StateChangeFailed
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

typedef struct CANopenNMTEvent          // event "canopen.node.state"
  {
  canbus*             origin;
  uint8_t             nodeid;
  CANopenNMTState_t   state;
  } CANopenNMTEvent_t;

typedef struct CANopenEMCYEvent         // event "canopen.node.emcy"
  {
  canbus*             origin;
  uint8_t             nodeid;
  uint16_t            code;             // see CiA DS301, Table 21: Emergency Error Codes
  uint8_t             type;             // see CiA DS301, Table 48: Structure of the Error Register
  uint8_t             data[5];          // device specific
  } CANopenEMCYEvent_t;

typedef struct CANopenNodeMetrics
  {
  OvmsMetricString*     m_state;        // metric "co.<bus>.nd<nodeid>.state"
  OvmsMetricInt*        m_emcy_code;    // metric "co.<bus>.nd<nodeid>.emcy.code"
  OvmsMetricInt*        m_emcy_type;    // metric "co.<bus>.nd<nodeid>.emcy.type"
  } CANopenNodeMetrics_t;

typedef std::map<uint8_t, CANopenNodeMetrics*> CANopenNodeMetricsMap;

class CANopenAsyncClient;


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
  COJT_ReceiveHB,
  COJT_ReadSDO,
  COJT_WriteSDO
  } CANopenJob_t;

struct __attribute__ ((__packed__)) CANopenJob
  {
  CANopenAsyncClient*   client;
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
      uint8_t               nodeid;
      CANopenNMTState_t     state;          // state received
      CANopenNMTState_t*    statebuf;       // state received
      } hb;
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
  
  void Init()
    {
    memset(this, 0, sizeof(*this));
    }
  
  CANopenResult_t SetResult(CANopenResult_t p_result, uint32_t p_error=0)
    {
    sdo.error = p_error;
    return result = p_result;
    }
  };

typedef union __attribute__ ((__packed__))
  {
  uint8_t       byte[8];        // raw access
  struct __attribute__ ((__packed__))
    {
    uint8_t     state;          // heartbeat state
    uint8_t     unused[7];
    } hb;
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

typedef std::forward_list<CANopenAsyncClient*> CANopenClientList;

class CANopenWorker
  {
  public:
    CANopenWorker(canbus* canbus);
    ~CANopenWorker();
  
  public:
    void JobTask();
    void IncomingFrame(CAN_frame_t* frame);
    void Open(CANopenAsyncClient* client);
    void Close(CANopenAsyncClient* client);
    bool IsClient(CANopenAsyncClient* client);
    void StatusReport(int verbosity, OvmsWriter* writer);
    CANopenNodeMetrics *GetNodeMetrics(uint8_t nodeid);
  
  public:
    CANopenResult_t SubmitJob(CANopenJob& job, TickType_t maxqueuewait=0);
  
  protected:
    CANopenResult_t ProcessSendNMTJob();
    CANopenResult_t ProcessReceiveHBJob();
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
    
    uint32_t              m_nmt_rxcnt;
    uint32_t              m_emcy_rxcnt;
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
 * CANopenAsyncClient provides the asynchronous API to the CANopen framework.
 * 
 * Jobs done will be sent to the m_done_queue and need to be fetched
 *   by looping ReceiveDone() until it returns COR_ERR_QueueEmpty.
 * 
 * On creation the client automatically connects to the worker or starts a
 *   new worker if necessary.
 * 
 * CANopenAsyncClient uses the CiA DS301 default IDs for node addressing, i.e.
 *   NMT request     → 0x000
 *   NMT response    → 0x700 + nodeid
 *   SDO request     → 0x600 + nodeid
 *   SDO response    → 0x580 + nodeid
 * 
 * If you need another address scheme, create a sub class of CANopenAsyncClient
 *   or CANopenClient and override the Init…() methods as necessary.
 */
class CANopenAsyncClient
  {
  public:
    CANopenAsyncClient(canbus* canbus, int queuesize=20);
    CANopenAsyncClient(CANopenWorker* worker, int queuesize=20);
    virtual ~CANopenAsyncClient();

  public:
    // CANopenWorker callback:
    virtual CANopenResult_t SubmitDoneCallback(CANopenJob& job, TickType_t maxqueuewait=0);

  public:
    // Queue API:
    virtual CANopenResult_t SubmitJob(CANopenJob& job, TickType_t maxqueuewait=0);
    virtual CANopenResult_t ReceiveDone(CANopenJob& job, TickType_t maxqueuewait=0);
  
  public:
    // Customisation:
    virtual void InitSendNMT(CANopenJob& job, uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);
    virtual void InitReceiveHB(CANopenJob& job, uint8_t nodeid, CANopenNMTState_t* statebuf=NULL,
      int recv_timeout_ms=1000, int max_tries=1);
    virtual void InitReadSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
    virtual void InitWriteSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
  
  public:
    // Main API:
    virtual CANopenResult_t SendNMT(uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);
    virtual CANopenResult_t ReceiveHB(uint8_t nodeid, CANopenNMTState_t* statebuf=NULL,
      int recv_timeout_ms=1000, int max_tries=1);
    virtual CANopenResult_t ReadSDO(uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
    virtual CANopenResult_t WriteSDO(uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
  
  public:
    CANopenWorker*        m_worker;
    QueueHandle_t         m_done_queue;
  };


/**
 * CANopenClient provides the synchronous API to the CANopen framework.
 * 
 * The API methods will block until the job is done, job detail results
 *   are returned in the caller provided job.
 * 
 * See CANopen shell commands for usage examples.
 */
class CANopenClient : public CANopenAsyncClient
  {
  public:
    CANopenClient(canbus* canbus);
    CANopenClient(CANopenWorker* worker);
    virtual ~CANopenClient();

  public:
    // Queue API:
    virtual CANopenResult_t ExecuteJob(CANopenJob& job, TickType_t maxqueuewait=0);
  
  public:
    // Main API:
    virtual CANopenResult_t SendNMT(CANopenJob& job, uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);
    virtual CANopenResult_t ReceiveHB(CANopenJob& job, uint8_t nodeid, CANopenNMTState_t* statebuf=NULL,
      int recv_timeout_ms=1000, int max_tries=1);
    virtual CANopenResult_t ReadSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
    virtual CANopenResult_t WriteSDO(CANopenJob& job, uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);
  
  public:
    SemaphoreHandle_t m_mutex;              // thread mutex
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
    static const std::string GetJobName(const CANopenJob_t jobtype);
    static const std::string GetJobName(const CANopenJob& job);
    static const std::string GetCommandName(const CANopenNMTCommand_t command);
    static const std::string GetStateName(const CANopenNMTState_t state);
    static const std::string GetAbortCodeName(const uint32_t abortcode);
    static const std::string GetResultString(const CANopenResult_t result);
    static const std::string GetResultString(const CANopenResult_t result, const uint32_t abortcode);
    static const std::string GetResultString(const CANopenJob& job);
    static int PrintNodeInfo(int capacity, OvmsWriter* writer, canbus* bus, int nodeid,
      int timeout_ms=100, bool brief=false, bool quiet=false);

  public:
    static void shell_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_nmt(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_readsdo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_writesdo(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_info(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);
    static void shell_scan(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  public:
    QueueHandle_t         m_rxqueue;    // CAN rx queue
    TaskHandle_t          m_rxtask;     // CAN rx task

    CANopenWorker*        m_worker[CAN_INTERFACE_CNT];
    int                   m_workercnt;
  };

extern CANopen MyCANopen;


#endif // __CANOPEN_H__
