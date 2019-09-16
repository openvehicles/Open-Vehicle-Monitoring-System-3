CANopen API Usage
=================


Synchronous API
---------------

The synchronous use is very simple, all you need is a ``CANopenClient`` instance.

On creation the client automatically connects to the active CANopen worker or starts a
new worker instance if necessary.

The ``CANopenClient`` methods will block until the job is done (or has failed).
Job results and/or error details are returned in the caller provided job.

.. caution:: Do not make synchronous calls from code that may run in a restricted context,
  e.g. within a metric update handler. Avoid using synchronous calls from time critical
  code, e.g. a CAN or event handler.


Example
^^^^^^^

.. code-block:: c++

  #include "canopen.h"

  // find CAN interface:
  canbus* bus = (canbus*) MyPcpApp.FindDeviceByName("can1");
  // …or simply use m_can1 if you're a vehicle subclass

  // create CANopen client:
  CANopenClient client(bus);
  
  // a CANopen job holds request and response data:
  CANopenJob job;

  // read value from node #1 SDO 0x1008.00:
  uint32_t value;
  if (client.ReadSDO(&job, 1, 0x1008, 0x00, (uint8_t)&value, sizeof(value)) == COR_OK) {
    // read result is now in value
  }

  // start node #2, wait for presence:
  if (client.SendNMT(&job, 2, CONC_Start, true) == COR_OK) {
    // node #2 is now started
  }

  // write value into node #3 SDO 0x2345.18:
  if (client.WriteSDO(&job, 3, 0x2345, 0x18, (uint8_t)&value, 0) == COR_OK) {
    // value has now been written into register 0x2345.18
  }


Main API methods
^^^^^^^^^^^^^^^^

.. code-block:: c++

    /**
    * SendNMT: send NMT request and optionally wait for NMT state change
    *  a.k.a. heartbeat message.
    * 
    * Note: NMT responses are not a part of the CANopen NMT protocol, and
    *  sending "heartbeat" NMT state updates is optional for CANopen nodes.
    *  If the node sends no state info, waiting for it will result in timeout
    *  even though the state has in fact changed -- there's no way to know
    *  if the node doesn't tell.
    */
    CANopenResult_t SendNMT(CANopenJob& job,
        uint8_t nodeid, CANopenNMTCommand_t command,
        bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);

    /**
     * ReceiveHB: wait for next heartbeat message of a node,
     *  return state received.
     * 
     * Use this to read the current state or synchronize to the heartbeat.
     * Note: heartbeats are optional in CANopen.
     */
    CANopenResult_t ReceiveHB(CANopenJob& job,
        uint8_t nodeid, CANopenNMTState_t* statebuf=NULL,
        int recv_timeout_ms=1000, int max_tries=1);

    /**
     * ReadSDO: read bytes from SDO server into buffer
     *   - reads data into buf (up to bufsize bytes)
     *   - returns data length read in job.sdo.xfersize
     *   - … and data length available in job.sdo.contsize (if known)
     *   - remaining buffer space will be zeroed
     *   - on result COR_ERR_BufferTooSmall, the buffer has been filled up to bufsize
     *   - on abort, the CANopen error code can be retrieved from job.sdo.error
     * 
     * Note: result interpretation is up to caller (check device object dictionary
     *   for data types & sizes). As CANopen is little endian as ESP32, we don't
     *   need to check lengths on numerical results, i.e. anything from int8_t to
     *   uint32_t can simply be read into a uint32_t buffer.
     */
    CANopenResult_t ReadSDO(CANopenJob& job,
        uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
        int resp_timeout_ms=50, int max_tries=3);

    /**
     * WriteSDO: write bytes from buffer into SDO server
     *   - sends bufsize bytes from buf
     *   - … or 4 bytes from buf if bufsize is 0 (use for integer SDOs of unknown type)
     *   - returns data length sent in job.sdo.xfersize
     *   - on abort, the CANopen error code can be retrieved from job.sdo.error
     * 
     * Note: the caller needs to know data type & size of the SDO register (check
     *   device object dictionary). As CANopen servers normally are intelligent, 
     *   anything from int8_t to uint32_t can simply be sent as a uint32_t with 
     *   bufsize=0, the server will know how to convert it.
     */
    CANopenResult_t WriteSDO(CANopenJob& job,
        uint8_t nodeid, uint16_t index, uint8_t subindex, uint8_t* buf, size_t bufsize,
        int resp_timeout_ms=50, int max_tries=3);


If you want to create custom jobs, use the low level method ``ExecuteJob()`` to execute them.


Asynchronous API
----------------

The ``CANopenAsyncClient`` class provides the asynchronous interface and the response queue.

To use the asynchronous API you need to handle asynchronous responses, which normally
means adding a dedicated task for this. A minimal handling would be to simply discard
the responses (just empty the queue), if you don't need to care about the results.


Example
^^^^^^^

Instantiate the async client for a CAN bus and a queue size like this:

.. code-block:: c++

  CANopenAsyncClient m_async(m_can1, 50);

Example response handler task:

.. code-block:: c++

  void MyAsyncTask()
  {
    CANopenJob job;
    while (true) {
      if (m_async.ReceiveDone(job, portMAX_DELAY) != COR_ERR_QueueEmpty) {
        // …process job results…
      }
    }
  }

Sending requests is following the same scheme as with the synchronous API. Standard
result code is ``COR_WAIT``, an error may occur if the queue is full.

.. code-block:: c++

  if (m_async.WriteSDO(m_nodeid, index, subindex, (uint8_t*)value, 0) != COR_WAIT) {
    // …handle error…
  }


Main API methods
^^^^^^^^^^^^^^^^

The API methods are similar to the synchronous methods (see above).

.. code-block:: c++

    CANopenResult_t SendNMT(uint8_t nodeid, CANopenNMTCommand_t command,
      bool wait_for_state=false, int resp_timeout_ms=1000, int max_tries=3);

    CANopenResult_t ReceiveHB(uint8_t nodeid, CANopenNMTState_t* statebuf=NULL,
      int recv_timeout_ms=1000, int max_tries=1);

    CANopenResult_t ReadSDO(uint8_t nodeid, uint16_t index, uint8_t subindex,
      uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);

    CANopenResult_t WriteSDO(uint8_t nodeid, uint16_t index, uint8_t subindex,
      uint8_t* buf, size_t bufsize,
      int resp_timeout_ms=100, int max_tries=3);


``CANopenJob`` objects are created automatically by these methods. Jobs done
need to be fetched by looping ``ReceiveDone()`` until it returns ``COR_ERR_QueueEmpty``.

If you want to create custom jobs, use the low level method ``SubmitJob()`` to add them
to the worker queue.


Error Handling
--------------

If an error occurs, it will be given as a ``CANopenResult_t`` other than ``COR_OK`` or
``COR_WAIT``, either by a method result or by the ``CANopenJob.result`` field.

Result codes are:

.. code-block:: c++

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

Additionally, if an SDO read/write error occurs, an abortion error code may be given
by the slave. These codes follow the CANopen standard and may be extended by device
specific codes.

To translate a ``CANopenResult_t`` and/or a known SDO abort code into a string,
use the ``CANopen`` class utility methods:

.. code-block:: c++

    std::string GetAbortCodeName(const uint32_t abortcode);
    std::string GetResultString(const CANopenResult_t result);
    std::string GetResultString(const CANopenResult_t result, const uint32_t abortcode);
    std::string GetResultString(const CANopenJob& job);


Example
^^^^^^^

.. code-block:: c++

    if (job.result != COR_OK) {
      ESP_LOGE(TAG, "Result for %s: %s",
        CANopen::GetJobName(job).c_str(),
        CANopen::GetResultString(job).c_str());
    }


Custom Address Schemes
----------------------

The standard clients use the CiA DS301 default IDs for node addressing, i.e.::

  NMT request     → 0x000
  NMT response    → 0x700 + nodeid
  SDO request     → 0x600 + nodeid
  SDO response    → 0x580 + nodeid

If you need another address scheme, create a sub class of ``CANopenAsyncClient``
or ``CANopenClient`` and override the ``Init…()`` methods as necessary.


More Code Examples
------------------

* See shell commands in ``canopen_shell.cpp``
* See classes ``SevconClient`` and ``SevconJob`` in the Twizy SEVCON module

