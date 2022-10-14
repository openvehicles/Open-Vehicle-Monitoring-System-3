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
*/

// #include "ovms_log.h"
// static const char *TAG = "vehicle";

#include <stdio.h>
#include <algorithm>
#include <ovms_command.h>
#include <ovms_script.h>
#include <ovms_metrics.h>
#include <ovms_notify.h>
#include <metrics_standard.h>
#ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_webserver.h>
#endif // #ifdef CONFIG_OVMS_COMP_WEBSERVER
#include <ovms_peripherals.h>
#include <string_writer.h>
#include "vehicle.h"


/**
 * PollerStateTicker: check for state changes (stub, override with vehicle implementation)
 *  This is called by VehicleTicker1() just before the next PollerSend().
 *  Implement your poller state transition logic in this method, so the changes
 *  will get applied immediately.
 */
void OvmsVehicle::PollerStateTicker()
  {
  }


/**
 * IncomingPollReply: poll response handler (stub, override with vehicle implementation)
 *  This is called by PollerReceive() on each valid response frame for the current request.
 *  Be aware responses may consist of multiple frames, detectable e.g. by mlremain > 0.
 *  A typical pattern is to collect frames in a buffer until mlremain == 0.
 *  
 *  @param bus
 *    CAN bus the current poll is done on
 *  @param type
 *    OBD2 mode / UDS polling type, e.g. VEHICLE_POLL_TYPE_READDTC
 *  @param pid
 *    PID addressed (depending on the request type, may be none / 8 bit / 16 bit)
 *  @param data
 *    Payload
 *  @param length
 *    Payload size
 *  @param mlremain
 *    Remaining bytes expected to complete the response after this frame
 *  
 *  @member m_poll_moduleid_sent
 *    The CAN ID addressed by the current request (txmoduleid)
 *  @member m_poll_ml_frame
 *    Frame number of the response, 0 = first frame / new response
 *  @member m_poll_ml_offset
 *    Byte position of this frame's payload part in the response, 0 = first frame
 *  @member m_poll_plcur
 *    Pointer to the currently processed poll entry
 */
void OvmsVehicle::IncomingPollReply(canbus* bus, uint16_t type, uint16_t pid, uint8_t* data, uint8_t length, uint16_t mlremain)
  {
  }


/**
 * IncomingPollError: poll response error handler (stub, override with vehicle implementation)
 *  This is called by PollerReceive() on reception of an OBD/UDS Negative Response Code (NRC),
 *  except if the code is requestCorrectlyReceived-ResponsePending (0x78), which is handled
 *  by the poller. See ISO 14229 Annex A.1 for the list of NRC codes.
 *  
 *  @param bus
 *    CAN bus the current poll is done on
 *  @param type
 *    OBD2 mode / UDS polling type, e.g. VEHICLE_POLL_TYPE_READDTC
 *  @param pid
 *    PID addressed (depending on the request type, may be none / 8 bit / 16 bit)
 *  @param code
 *    NRC detail code
 *  
 *  @member m_poll_moduleid_sent
 *    The CAN ID addressed by the current request (txmoduleid)
 *  @member m_poll_plcur
 *    Pointer to the currently processed poll entry
 */
void OvmsVehicle::IncomingPollError(canbus* bus, uint16_t type, uint16_t pid, uint16_t code)
  {
  }


/**
 * IncomingPollTxCallback: poller TX callback (stub, override with vehicle implementation)
 *  This is called by PollerTxCallback() on TX success/failure for a poller request.
 *  You can use this to detect CAN bus issues, e.g. if the car switches off the OBD port.
 *  
 *  ATT: this is executed in the main CAN task context. Keep it simple.
 *    Complex processing here will affect overall CAN performance.
 *  
 *  @param bus
 *    CAN bus the current poll is done on
 *  @param txid
 *    The module TX ID of the current poll
 *  @param type
 *    OBD2 mode / UDS polling type, e.g. VEHICLE_POLL_TYPE_READDTC
 *  @param pid
 *    PID addressed (depending on the request type, may be none / 8 bit / 16 bit)
 *  @param success
 *    Frame transmission success
 */
void OvmsVehicle::IncomingPollTxCallback(canbus* bus, uint32_t txid, uint16_t type, uint16_t pid, bool success)
  {
  }


/**
 * PollSetPidList: set the default bus and the polling list to process
 *  Call this to install a new polling list or restart the list.
 *  This won't change the polling state; you can change the list while keeping the state.
 *  The list is changed without waiting for pending responses to finish (except PollSingleRequests).
 *  
 *  @param bus
 *    CAN bus to use as the default bus (for all poll entries with bus=0) or NULL to stop polling
 *  @param plist
 *    The polling list to use or NULL to stop polling
 */
void OvmsVehicle::PollSetPidList(canbus* bus, const poll_pid_t* plist)
  {
  OvmsRecMutexLock slock(&m_poll_single_mutex);
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_bus = bus;
  m_poll_bus_default = bus;
  m_poll_plist = plist;
  m_poll_ticker = 0;
  m_poll_sequence_cnt = 0;
  m_poll_wait = 0;
  m_poll_plcur = NULL;
  m_poll_entry = {};
  m_poll_txmsgid = 0;
  }


/**
 * PollSetState: set the polling state
 *  Call this to change the polling state and restart the current polling list.
 *  This won't do anything if the state is already active. The state is changed without
 *  waiting for pending responses to finish (except PollSingleRequests).
 *  
 *  @param state
 *    The polling state to activate (0 … VEHICLE_POLL_NSTATES)
 */
void OvmsVehicle::PollSetState(uint8_t state)
  {
  if ((state < VEHICLE_POLL_NSTATES)&&(state != m_poll_state))
    {
    OvmsRecMutexLock slock(&m_poll_single_mutex);
    OvmsRecMutexLock lock(&m_poll_mutex);
    m_poll_state = state;
    m_poll_ticker = 0;
    m_poll_sequence_cnt = 0;
    m_poll_wait = 0;
    m_poll_plcur = NULL;
    m_poll_entry = {};
    m_poll_txmsgid = 0;
    }
  }


/**
 * PollSetThrottling: configure polling speed / niceness
 *  If multiple requests are due at the same poll tick (second), this controls how many of
 *  them will be sent in series without a delay, i.e. as soon as the response/timeout for
 *  the previous request occurred.
 *  
 *  @param sequence_max
 *    Polls allowed to be sent in sequence per time tick (second), default 1, 0 = no limit.
 *  
 *  The configuration is kept unchanged over calls to PollSetPidList() or PollSetState().
 */
void OvmsVehicle::PollSetThrottling(uint8_t sequence_max)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_sequence_max = sequence_max;
  }


/**
 * PollSetResponseSeparationTime: configure ISO TP multi frame response timing
 *  See: https://en.wikipedia.org/wiki/ISO_15765-2
 *  
 *  @param septime
 *    Separation Time (ST), the minimum delay time between frames. Default: 25 milliseconds
 *    ST values up to 127 (0x7F) specify the minimum number of milliseconds to delay between frames,
 *    while values in the range 241 (0xF1) to 249 (0xF9) specify delays increasing from
 *    100 to 900 microseconds.
 *  
 *  The configuration is kept unchanged over calls to PollSetPidList() or PollSetState().
 */
void OvmsVehicle::PollSetResponseSeparationTime(uint8_t septime)
  {
  assert (septime <= 127 || (septime >= 241 && septime <= 249));
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_fc_septime = septime;
  }


/**
 * PollSetChannelKeepalive: configure keepalive timeout for channel oriented protocols
 * 
 *  The VWTP poller will keep a channel connection to a module open for this
 *  many seconds after the last poll data transmission has taken place, to
 *  minimize connection overhead in case the next poll is sent to the same
 *  module.
 * 
 *  Devices may stay awake as long as the channel is open, so don't disable
 *  the timeout except for special purposes.
 * 
 *  @param keepalive_seconds
 *    Channel inactivity timeout in seconds, 0 = disable, default/init = 60
 */
void OvmsVehicle::PollSetChannelKeepalive(uint16_t keepalive_seconds)
  {
  m_poll_ch_keepalive = keepalive_seconds;
  }


/**
 * PollerSend: internal: start next due request
 */
void OvmsVehicle::PollerSend(bool fromTicker)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);

  // ESP_LOGD(TAG, "PollerSend(%d): entry at[type=%02X, pid=%X], ticker=%u, wait=%u, cnt=%u/%u",
  //          fromTicker, m_poll_plcur->type, m_poll_plcur->pid,
  //          m_poll_ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);

  if (fromTicker)
    {
    // Timer ticker call: reset throttling counter, check response timeout
    m_poll_sequence_cnt = 0;
    if (m_poll_wait > 0) m_poll_wait--;

    // Protocol specific ticker calls:
    PollerVWTPTicker();
    }
  if (m_poll_wait > 0) return;

  // Check poll bus & list:
  if (!m_poll_bus_default || !m_poll_plist || m_poll_plist->txmoduleid == 0) return;

  // Restart poll list cursor:
  if (m_poll_plcur == NULL) m_poll_plcur = m_poll_plist;

  m_poll_entry = {};

  while (m_poll_plcur->txmoduleid != 0)
    {
    if ((m_poll_plcur->polltime[m_poll_state] > 0) &&
        ((m_poll_ticker % m_poll_plcur->polltime[m_poll_state]) == 0))
      {
      // We need to poll this one...
      m_poll_entry = *m_poll_plcur;
      m_poll_protocol = m_poll_plcur->protocol;
      m_poll_type = m_poll_plcur->type;
      m_poll_pid = m_poll_plcur->pid;

      switch (m_poll_plcur->pollbus)
        {
        case 1:
          m_poll_bus = m_can1;
          break;
        case 2:
          m_poll_bus = m_can2;
          break;
        case 3:
          m_poll_bus = m_can3;
          break;
        case 4:
          m_poll_bus = m_can4;
          break;
        default:
          m_poll_bus = m_poll_bus_default;
        }

      // Dispatch transmission start to protocol handler:
      if (m_poll_protocol == VWTP_20)
        PollerVWTPStart(fromTicker);
      else
        PollerISOTPStart(fromTicker);

      m_poll_plcur++;
      m_poll_sequence_cnt++;
      return;
      }

    // Poll entry is not due, check next
    m_poll_plcur++;
    }

  // Completed checking all poll entries for the current m_poll_ticker
  // ESP_LOGD(TAG, "PollerSend(%d): cycle complete for ticker=%u", fromTicker, m_poll_ticker);
  m_poll_plcur = m_poll_plist;
  m_poll_ticker++;
  if (m_poll_ticker > 3600) m_poll_ticker -= 3600;
  }


/**
 * PollerTxCallback: internal: process poll request callbacks
 */
void OvmsVehicle::PollerTxCallback(const CAN_frame_t* frame, bool success)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);

  // Check for a late callback:
  if (!m_poll_wait || !m_poll_plist || frame->origin != m_poll_bus || frame->MsgID != m_poll_txmsgid)
    return;

  // Forward to protocol handler:
  if (m_poll_protocol == VWTP_20)
    PollerVWTPTxCallback(frame, success);

  // On failure, try to speed up the current poll timeout:
  if (!success)
    {
    m_poll_wait = 0;
    if (m_poll_single_rxbuf || m_poll_single_rx_vecbuf)
      {
      m_poll_single_rxerr = POLLSINGLE_TXFAILURE;
      m_poll_single_rxbuf = NULL;
      m_poll_single_rx_vecbuf = NULL;
      m_poll_single_rxdone.Give();
      }
    }

  // Forward to application:
  IncomingPollTxCallback(m_poll_bus, m_poll_moduleid_sent, m_poll_type, m_poll_pid, success);
  }


/**
 * PollerReceive: internal: process poll response frame
 */
void OvmsVehicle::PollerReceive(CAN_frame_t* frame, uint32_t msgid)
  {
  // obsolete
  }


/**
 * PollSingleRequest: perform prioritized synchronous single OBD2/UDS request
 *  Pass a full OBD2/UDS request (mode/type, PID, additional payload).
 *  The request is sent immediately, aborting a running poll list request. The previous
 *  poller state is automatically restored after the request has been performed, but
 *  without any guarantee for repetition or omission of an aborted poll.
 *  On success, the response buffer will contain the response payload (may be empty).
 *
 *  See OvmsVehicleFactory::obdii_request() for a usage example.
 *
 *  ATT: must not be called from within the vehicle task context -- deadlock situation!
 *
 *  @param bus          CAN bus to use for the request
 *  @param txid         CAN ID to send to (0x7df = broadcast)
 *  @param rxid         CAN ID to expect response from (broadcast: 0)
 *  @param request      Request to send (binary string) (type + pid + up to 4095 bytes payload)
 *  @param size         Size of request
 *  @param response     Response buffer (binary string) (multiple response frames assembled)
 *  @param vresponse    Response vector buffer.
 *  @param timeout_ms   Timeout for poller/response in milliseconds
 *  @param protocol     Protocol variant: ISOTP_STD / ISOTP_EXTADR / ISOTP_EXTFRAME
 *
 *  @return             POLLSINGLE_OK         (0)   -- success, response is valid
 *                      POLLSINGLE_TIMEOUT    (-1)  -- timeout/poller unavailable
 *                      POLLSINGLE_TXFAILURE  (-2)  -- CAN transmission failure
 *                      else                  (>0)  -- UDS NRC detail code
 *                      Note: response is only valid with return value 0
 */
int OvmsVehicle::DoPollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                  const uint8_t* request, uint16_t size,
                  std::string* response, std::vector<uint8_t> *vresponse,
                  int timeout_ms/*=3000*/, uint8_t protocol/*=ISOTP_STD*/)
  {
  if (!m_ready)
    return -1;
  if (response == NULL && vresponse == NULL)
    return -1;

  if (!m_registeredlistener)
    {
    m_registeredlistener = true;
    MyCan.RegisterListener(m_rxqueue);
    }

  OvmsRecMutexLock slock(&m_poll_single_mutex, pdMS_TO_TICKS(timeout_ms));
  if (!slock.IsLocked())
    return POLLSINGLE_TIMEOUT;

  // prepare single poll:
  OvmsVehicle::poll_pid_t poll[] =
    {
      { txid, rxid, 0, 0, { 999, 999, 999, 999 }, 0, protocol },
      POLL_LIST_END
    };

  assert(size > 0);
  poll[0].type = request[0];
  poll[0].xargs.tag = POLL_TXDATA;

  if (POLL_TYPE_HAS_16BIT_PID(poll[0].type))
    {
    assert(size >= 3);
    poll[0].xargs.pid = request[1] << 8 | request[2];
    poll[0].xargs.datalen = LIMIT_MAX(size-3, 4095);
    poll[0].xargs.data = (const uint8_t*)request+3;
    }
  else if (POLL_TYPE_HAS_8BIT_PID(poll[0].type))
    {
    assert(size >= 2);
    poll[0].xargs.pid = request[1];
    poll[0].xargs.datalen = LIMIT_MAX(size-2, 4095);
    poll[0].xargs.data = request+2;
    }
  else
    {
    poll[0].xargs.pid = 0;
    poll[0].xargs.datalen = LIMIT_MAX(size-1, 4095);
    poll[0].xargs.data = request+1;
    }
  canbus*           p_bus;
  const poll_pid_t* p_list;
  const poll_pid_t* p_plcur;
  uint32_t          p_ticker;

    {
    // acquire poller access:
    OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(timeout_ms));
    if (!lock.IsLocked())
      return -1;

    // save poller state:
    p_bus    = m_poll_bus_default;
    p_list   = m_poll_plist;
    p_plcur  = m_poll_plcur;
    p_ticker = m_poll_ticker;

    // start single poll:
    PollSetPidList(bus, poll);
    m_poll_single_rxdone.Take(0);
    // One should be available
    m_poll_single_rxbuf = response;
    m_poll_single_rx_vecbuf = vresponse;

    PollerSend(true);

    // Release poll mutex
    }

  // wait for response:
  bool rxok = m_poll_single_rxdone.Take(pdMS_TO_TICKS(timeout_ms));

  // restore poller state:
  {
    OvmsRecMutexLock lock(&m_poll_mutex);

    PollSetPidList(p_bus, p_list);
    m_poll_plcur = p_plcur;
    m_poll_ticker = p_ticker;
    m_poll_single_rxbuf = NULL;
    m_poll_single_rx_vecbuf = NULL;
    // Release poll mutex
  }
  /*
  ESP_LOGV(TAG, "Vehicle-PollISOTP: IPR %03x TYPE:%x PID: %03x Frames: %d Message Size: %d",
      m_poll_moduleid_low, type, pid, obd_frame, rxbuf.size());
    ESP_BUFFER_LOGD(TAG, rxbuf.data(), rxbuf.size());
*/
  return (rxok == pdFALSE) ? -1 : m_poll_single_rxerr;
  }


/**
 * PollSingleRequest: perform prioritized synchronous single OBD2/UDS request
 *  Convenience wrapper for standard PID polls, see above for main implementation.
 *  
 *  @param bus          CAN bus to use for the request
 *  @param txid         CAN ID to send to (0x7df = broadcast)
 *  @param rxid         CAN ID to expect response from (broadcast: 0)
 *  @param polltype     OBD2/UDS poll type …
 *  @param pid          … and PID to poll
 *  @param response     Response buffer (binary string) (multiple response frames assembled)
 *  @param timeout_ms   Timeout for poller/response in milliseconds
 *  @param protocol     Protocol variant: ISOTP_STD / ISOTP_EXTADR / ISOTP_EXTFRAME
 *  
 *  @return             POLLSINGLE_OK         ( 0)  -- success, response is valid
 *                      POLLSINGLE_TIMEOUT    (-1)  -- timeout/poller unavailable
 *                      POLLSINGLE_TXFAILURE  (-2)  -- CAN transmission failure
 *                      else                  (>0)  -- UDS NRC detail code
 *                      Note: response is only valid with return value 0
 */
int OvmsVehicle::PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                                   uint8_t polltype, uint16_t pid, std::string& response,
                                   int timeout_ms /*=3000*/, uint8_t protocol /*=ISOTP_STD*/)
  {
  std::string request;
  request = (char) polltype;
  if (POLL_TYPE_HAS_16BIT_PID(polltype))
    {
    request += (char) (pid >> 8);
    request += (char) (pid & 0xff);
    }
  else if (POLL_TYPE_HAS_8BIT_PID(polltype))
    {
    request += (char) (pid & 0xff);
    }
  return DoPollSingleRequest(bus, txid, rxid,
      reinterpret_cast<const uint8_t *>(request.data()), request.size(),
      &response, NULL,
      timeout_ms, protocol);
  }

int OvmsVehicle::PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                  uint8_t polltype, uint16_t pid, std::vector<uint8_t>& response,
                  int timeout_ms/*=3000*/, uint8_t protocol/*=ISOTP_STD*/)
  {
  std::vector<uint8_t> request;
  request.reserve(3);
  request.push_back(polltype);
  if (POLL_TYPE_HAS_16BIT_PID(polltype))
    {
    request.push_back(pid >> 8);
    request.push_back(pid & 0xff);
    }
  else if (POLL_TYPE_HAS_8BIT_PID(polltype))
    {
    request.push_back(pid & 0xff);
    }
  return PollSingleRequest(bus, txid, rxid, request, response, timeout_ms, protocol);
  }

/**
 * PollResultCodeName: get text representation of result code
 */
const char* OvmsVehicle::PollResultCodeName(int code)
  {
  switch (code)
    {
    case 0x10:  return "generalReject";
    case 0x11:  return "serviceNotSupported";
    case 0x12:  return "subFunctionNotSupported";
    case 0x13:  return "incorrectMessageLengthOrInvalidFormat";
    case 0x14:  return "responseTooLong";
    case 0x21:  return "busyRepeatRequest";
    case 0x22:  return "conditionsNotCorrect";
    case 0x24:  return "requestSequenceError";
    case 0x25:  return "noResponseFromSubnetComponent";
    case 0x26:  return "failurePreventsExecutionOfRequestedAction";
    case 0x31:  return "requestOutOfRange";
    case 0x33:  return "securityAccessDenied";
    case 0x35:  return "invalidKey";
    case 0x36:  return "exceedNumberOfAttempts";
    case 0x37:  return "requiredTimeDelayNotExpired";
    case 0x70:  return "uploadDownloadNotAccepted";
    case 0x71:  return "transferDataSuspended";
    case 0x72:  return "generalProgrammingFailure";
    case 0x73:  return "wrongBlockSequenceCounter";
    case 0x78:  return "requestCorrectlyReceived-ResponsePending";
    case 0x7e:  return "subFunctionNotSupportedInActiveSession";
    case 0x7f:  return "serviceNotSupportedInActiveSession";
    default:    return NULL;
    }
  }
