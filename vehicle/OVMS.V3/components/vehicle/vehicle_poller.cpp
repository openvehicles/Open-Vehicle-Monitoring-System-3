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

#include "ovms_log.h"
static const char *TAG = "vehicle-poll";

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
#include "can.h"

using namespace std::placeholders;

OvmsPoller::OvmsPoller(canbus* can, uint8_t can_number, ParentSignal *parent_signal,
      const CanFrameCallback  &polltxcallback)
  {
  m_poll.bus = can;
  m_poll.bus_no = can_number;
  m_parent_signal = parent_signal;
  m_poll_txcallback = polltxcallback;

  m_poll_state = 0;

  m_poll.entry = {};
  m_poll_vwtp = {};
  m_poll.ticker = 0;

  m_poll_default_bus = 0;

  m_poll.moduleid_sent = 0;
  m_poll.moduleid_low = 0;
  m_poll.moduleid_high = 0;
  m_poll.type = 0;
  m_poll.pid = 0;
  m_poll.mlremain = 0;
  m_poll.mloffset = 0;
  m_poll.mlframe = 0;
  m_poll_wait = 0;
  m_poll_sequence_max = 1;
  m_poll_sequence_cnt = 0;
  m_poll_fc_septime = 25;       // response default timing: 25 milliseconds
  m_poll_ch_keepalive = 60;     // channel keepalive default: 60 seconds
  m_poll_repeat_count = 0;

  }


void OvmsPoller::Incoming(CAN_frame_t &frame, bool success)
  {
  // Pass frame to poller protocol handlers:
  if (frame.origin == m_poll_vwtp.bus && frame.MsgID == m_poll_vwtp.rxid)
    {
    PollerVWTPReceive(&frame, frame.MsgID);
    }
  else if (frame.origin == m_poll.bus)
    {
    if (!m_poll_wait)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]Poller: Incoming - giving up", m_poll.bus_no);
      return;
      }
    if (m_poll.entry.txmoduleid == 0)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]Poller: Incoming - Poll Entry Cleared", m_poll.bus_no);
      return;
      }
    uint32_t msgid;
    if (m_poll.protocol == ISOTP_EXTADR)
      msgid = frame.MsgID << 8 | frame.data.u8[0];
    else
      msgid = frame.MsgID;
    ESP_LOGD(TAG, "[%" PRIu8 "]Poller: FrameRx(bus=%s, msg=%" PRIx32 " )", m_poll.bus_no, frame.origin == m_poll.bus ? "Self" : "Other", msgid );
    if (msgid >= m_poll.moduleid_low && msgid <= m_poll.moduleid_high)
      {
      PollerISOTPReceive(&frame, msgid);
      }
    }
  }

OvmsPoller::~OvmsPoller()
  {
  }

/**
 * IncomingPollReply: Calls Vehicle poll response handler
 *  This is called by StandardPollSeries::IncomingPacket on each valid response frame for
 *  the current request of a Poll Series.
 *  Be aware responses may consist of multiple frames, detectable e.g. by mlremain > 0.
 *  A typical pattern is to collect frames in a buffer until mlremain == 0.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param data
 *    Payload
 *  @param length
 *    Payload size
 */
void OvmsPoller::IncomingPollReply(const OvmsPoller::poll_job_t &job, uint8_t* data, uint8_t length)
  {
    // Call back on Vehicle.
    m_parent_signal->IncomingPollReply(job, data, length);
  }

/**
 * IncomingPollError: Calls Vehicle poll response error handler
 *  This is called by PollerReceive() on reception of an OBD/UDS Negative Response Code (NRC),
 *  except if the code is requestCorrectlyReceived-ResponsePending (0x78), which is handled
 *  by the poller. See ISO 14229 Annex A.1 for the list of NRC codes.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param code
 *    NRC detail code
 */
void OvmsPoller::IncomingPollError(const OvmsPoller::poll_job_t &job, uint16_t code)
  {
  // Call back on Vehicle.
  m_parent_signal->IncomingPollError(job, code);
  }

/**
 * IncomingPollTxCallback: poller TX callback (stub, override with vehicle implementation)
 *  This is called by PollerTxCallback() on TX success/failure for a poller request.
 *  You can use this to detect CAN bus issues, e.g. if the car switches off the OBD port.
 *  
 *  ATT: this is executed in the main CAN task context. Keep it simple.
 *    Complex processing here will affect overall CAN performance.
 *
 *  @param job
 *    Status of the current Poll job
 *  @param success
 *    Frame transmission success
 */
void OvmsPoller::IncomingPollTxCallback(const OvmsPoller::poll_job_t &job, bool success)
  {
  // Call back on Vehicle.
  m_parent_signal->IncomingPollTxCallback(job, success);
  }

bool OvmsPoller::Ready()
  {
  return m_parent_signal->Ready();
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
void OvmsPoller::PollSetPidList(uint8_t defaultbus, const OvmsPoller::poll_pid_t* plist)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_default_bus = defaultbus;
  if (m_poll_series == nullptr)
    {
    if (!plist) // Don't add if not necessary.
      return;
    m_poll_series = std::shared_ptr<StandardPollSeries>(new StandardPollSeries(this));
    m_polls.SetEntry("!standard", m_poll_series);
    }

  m_poll_series->PollSetPidList(defaultbus, plist);

  m_poll.ticker = 0;
  m_poll_sequence_cnt = 0;
  }

void OvmsPoller::Do_PollSetState(uint8_t state)
  {
  if (/* (state < VEHICLE_POLL_NSTATES)&&*/ state != m_poll_state)
    {
    m_poll_state = state;
    m_poll.ticker = 0;
    m_poll_sequence_cnt = 0;
    m_poll_repeat_count = 0;
    ResetPollEntry(false);
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
void OvmsPoller::PollSetThrottling(uint8_t sequence_max)
  {
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
void OvmsPoller::PollSetResponseSeparationTime(uint8_t septime)
  {
  assert (septime <= 127 || (septime >= 241 && septime <= 249));
  m_poll_fc_septime  = septime;
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
void OvmsPoller::PollSetChannelKeepalive(uint16_t keepalive_seconds)
  {
  m_poll_ch_keepalive = keepalive_seconds;
  }

void OvmsPoller::ResetThrottle()
  {
  // Main Timer reset throttling counter,
  m_poll_sequence_cnt = 0;
  }

void OvmsPoller::ResetPollEntry(bool force)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  if (force || !m_polls.PollIsBlocking() )
    {
    m_poll_repeat_count = 0;
    m_polls.RestartPoll(OvmsPoller::ResetMode::PollReset);
    m_poll.entry = {};
    m_poll_txmsgid = 0;
    }
  }

bool OvmsPoller::HasPollList()
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  return m_polls.HasPollList();
  }

void OvmsPoller::ClearPollList()
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  return m_polls.Clear();
  }

bool OvmsPoller::CanPoll()
  {
  // Check Throttle
  return (!m_poll_sequence_max || m_poll_sequence_cnt < m_poll_sequence_max);
  }

void OvmsPoller::PollerNextTick(poller_source_t source)
  {
  // Completed checking all poll entries for the current m_poll_ticker

  ESP_LOGD(TAG, "[%" PRIu8 "]PollerNextTick(%s): cycle complete for ticker=%u", m_poll.bus_no, PollerSource(source), m_poll.ticker);
  m_poll_ticked = false;

  if (source == poller_source_t::Primary)
    {
      // Allow POLL to restart.
      {
      OvmsRecMutexLock lock(&m_poll_mutex);

      m_poll_run_finished = false;
      m_polls.RestartPoll(OvmsPoller::ResetMode::PollReset);
      m_poll_repeat_count = 0;
      }

    m_poll.ticker++;
    if (m_poll.ticker > 3600) m_poll.ticker -= 3600;
    }
  }

/** Called after reaching the end of available POLL entries.
 */
void OvmsPoller::PollRunFinished()
  {
  // Call back on vehicle PollRunFinished()  with bus number / bus
  m_parent_signal->PollRunFinished();
  }

/**
 * PollerSend: internal: start next due request
 */
void OvmsPoller::PollerSend(poller_source_t source)
  {

  bool curIsBlocking;
  {
    OvmsRecMutexLock lock(&m_poll_mutex);
    curIsBlocking = m_polls.PollIsBlocking();
  }
  bool fromPrimaryTicker = false, fromPrimaryOrOnceOffTicker = false;
  switch (source)
    {
    case poller_source_t::OnceOff:
      fromPrimaryOrOnceOffTicker = true;
      break;
    case poller_source_t::Primary:
      fromPrimaryOrOnceOffTicker = true;
      fromPrimaryTicker = true;
      break;
    default:
      ;
    }
  // Only reset the list when 'from Ticker' and it's at the end.
  if (fromPrimaryTicker )
    {
    if (!curIsBlocking)
      {
      m_poll_ticked = true;
      }
    }
  if (fromPrimaryOrOnceOffTicker)
    {
    // Timer ticker call: check response timeout
    if (m_poll_wait > 0)
      m_poll_wait--;

    // Protocol specific ticker calls:
    PollerVWTPTicker();
    }

  if (m_poll_wait > 0)
    {
    ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Waiting %" PRIu8, m_poll.bus_no, m_poll_wait);
    return;
    }

  if (!curIsBlocking && m_poll_ticked)
    {
    if (!m_poll_run_finished && m_poll_repeat_count > 0)
      {
      ESP_LOGD(TAG, "Poller finished primary run");
      m_poll_run_finished = true;
      }
    m_poll_ticked = false;

    // Force a reset
    if (m_poll_run_finished)
      {
      PollRunFinished();
      PollerNextTick(poller_source_t::Primary);
      }
    }

  // Check poll bus & list:
  if (!HasPollList())
    {
    ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Empty", m_poll.bus_no);
    return;
    }

  OvmsPoller::OvmsNextPollResult res;
  {
    OvmsRecMutexLock lock(&m_poll_mutex);
    res = m_polls.NextPollEntry(m_poll.entry, m_poll.bus_no, m_poll.ticker, m_poll_state);
  }
  if (res == OvmsNextPollResult::ReachedEnd && m_polls.HasRepeat())
    {
    ++m_poll_repeat_count;
    if (m_poll_repeat_count > max_poll_repeat)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]Poller Retry Exceeded - Finishing", m_poll.bus_no);
      m_poll_run_finished = true;
      res = OvmsNextPollResult::StillAtEnd;
      }
    else
      {
      ESP_LOGV(TAG, "[%" PRIu8 "]Poller Reset for Repeat (%s)", m_poll.bus_no, OvmsPoller::PollerSource(source));
      m_polls.RestartPoll(OvmsPoller::ResetMode::LoopReset);
      // If this poll is from a ISOTP success, don't overwhelm the ECU,
      // wait until a Secondary tick.
      if (source == poller_source_t::Successful)
        {
        ESP_LOGV(TAG, "[%" PRIu8 "]Poller Restart: Wait for secondary", m_poll.bus_no);
        return;
        }
      res = m_polls.NextPollEntry(m_poll.entry, m_poll.bus_no, m_poll.ticker, m_poll_state);
      }
    }
  switch (res)
    {
    case OvmsNextPollResult::Ignore:
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend: Ignore", m_poll.bus_no);
      break;
    case OvmsNextPollResult::ReachedEnd:
      {
      ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Poller Reached End", m_poll.bus_no);
      m_poll_run_finished = true;
      break;
      }
    case OvmsNextPollResult::StillAtEnd:
      if (!m_poll_run_finished)
        {
        ESP_LOGD(TAG, "[%" PRIu8 "Poller Reached End(!)", m_poll.bus_no);
        m_poll_run_finished = true;
        }
      break;
    case OvmsNextPollResult::FoundEntry:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend(%s)[%" PRIu8 "]: entry at[type=%02X, pid=%X], ticker=%u, wait=%u, cnt=%u/%u",
             m_poll.bus_no, PollerSource(source), m_poll_state, m_poll.entry.type, m_poll.entry.pid,
             m_poll.ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);
      // We need to poll this one...
      m_poll.protocol = m_poll.entry.protocol;
      m_poll.type = m_poll.entry.type;
      m_poll.pid = m_poll.entry.pid;

      // Dispatch transmission start to protocol handler:
      if (m_poll.protocol == VWTP_20)
        PollerVWTPStart(fromPrimaryOrOnceOffTicker);
      else
        PollerISOTPStart(fromPrimaryOrOnceOffTicker);

      m_poll_sequence_cnt++;
      break;
      }
    }
  }

void OvmsPoller::Outgoing(const CAN_frame_t &frame, bool success)
  {

  // Check for a late callback:
  if (!m_poll_wait || !m_poll.entry.txmoduleid || frame.origin != m_poll.bus || frame.MsgID != m_poll_txmsgid)
    return;

  // Forward to protocol handler:
  if (m_poll.protocol == VWTP_20)
    PollerVWTPTxCallback(&frame, success);

  // On failure, try to speed up the current poll timeout:
  if (!success)
    {
    m_poll_wait = 0;
    OvmsRecMutexLock lock(&m_poll_mutex);
    m_polls.IncomingError(m_poll, POLLSINGLE_TXFAILURE);
    }

  // Forward to application:
  m_poll.moduleid_rec = 0; // Not yet received
  IncomingPollTxCallback(m_poll, success);
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
 *  @param txid         CAN ID to send to (0x7df = broadcast)
 *  @param rxid         CAN ID to expect response from (broadcast: 0)
 *  @param request      Request to send (binary string) (type + pid + up to 4095 bytes payload)
 *  @param response     Response buffer (binary string) (multiple response frames assembled)
 *  @param timeout_ms   Timeout for poller/response in milliseconds
 *  @param protocol     Protocol variant: ISOTP_STD / ISOTP_EXTADR / ISOTP_EXTFRAME
 *  
 *  @return             POLLSINGLE_OK         (0)   -- success, response is valid
 *                      POLLSINGLE_TIMEOUT    (-1)  -- timeout/poller unavailable
 *                      POLLSINGLE_TXFAILURE  (-2)  -- CAN transmission failure
 *                      else                  (>0)  -- UDS NRC detail code
 *                      Note: response is only valid with return value 0
 */
int OvmsPoller::PollSingleRequest(uint32_t txid, uint32_t rxid,
                                   std::string request, std::string& response,
                                   int timeout_ms /*=3000*/, uint8_t protocol /*=ISOTP_STD*/)
  {
  if (!Ready())
    return -1;

  OvmsRecMutexLock slock(&m_poll_single_mutex, pdMS_TO_TICKS(timeout_ms));
  if (!slock.IsLocked())
    return -1;

  // prepare single poll:
  OvmsPoller::poll_pid_t poll =
      { txid, rxid, 0, 0, { 1, 1, 1, 1 }, 0, protocol };

  assert(request.size() > 0);
  poll.type = request[0];
  poll.xargs.tag = POLL_TXDATA;

  if (POLL_TYPE_HAS_16BIT_PID(poll.type))
    {
    assert(request.size() >= 3);
    poll.xargs.pid = request[1] << 8 | request[2];
    poll.xargs.datalen = LIMIT_MAX(request.size()-3, 4095);
    poll.xargs.data = (const uint8_t*)request.data()+3;
    }
  else if (POLL_TYPE_HAS_8BIT_PID(poll.type))
    {
    assert(request.size() >= 2);
    poll.xargs.pid = request.at(1);
    poll.xargs.datalen = LIMIT_MAX(request.size()-2, 4095);
    poll.xargs.data = (const uint8_t*)request.data()+2;
    }
  else
    {
    poll.xargs.pid = 0;
    poll.xargs.datalen = LIMIT_MAX(request.size()-1, 4095);
    poll.xargs.data = (const uint8_t*)request.data()+1;
    }

  int rx_error;
  OvmsSemaphore     single_rxdone;   // … response done (ok/error)
  std::shared_ptr<BlockingOnceOffPoll> poller( new BlockingOnceOffPoll(poll, &response, &rx_error, &single_rxdone));

  // acquire poller access:
    {
    OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(timeout_ms));
    if (!lock.IsLocked())
      return -1;
    // start single poll:
    m_polls.SetEntry("!single", poller, true);
    }

  ESP_LOGV(TAG, "[%" PRIu8 "]Single Request Sending", m_poll.bus_no);
  Queue_PollerSend(poller_source_t::OnceOff);

  // wait for response:
  ESP_LOGV(TAG, "[%" PRIu8 "]Single Request Waiting for response", m_poll.bus_no);
  bool rxok = single_rxdone.Take(pdMS_TO_TICKS(timeout_ms));
  ESP_LOGV(TAG, "[%" PRIu8 "]PollSingleRequest: Response done ", m_poll.bus_no);
  // Make sure if it is still sticking around that it's not accessing
  // stack objects!
  poller->Finished();
  return (rxok == pdFALSE) ? -1 : rx_error;
  }


/**
 * PollSingleRequest: perform prioritized synchronous single OBD2/UDS request
 *  Convenience wrapper for standard PID polls, see above for main implementation.
 *  
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
int OvmsPoller::PollSingleRequest( uint32_t txid, uint32_t rxid,
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
  return PollSingleRequest(txid, rxid, request, response, timeout_ms, protocol);
  }

void OvmsPoller::Queue_PollerSend(OvmsPoller::poller_source_t source)
  {
  m_parent_signal->PollerSend(m_poll.bus_no, source);
  }
/** Add a polling requester to list. Replaces any existing entry with that name.
  @param name Unique name/identifier of poll series.
  @param series Shared pointer to poller instance.
  */
void OvmsPoller::PollRequest(const std::string &name, const std::shared_ptr<PollSeriesEntry> &series)
  {
  series->SetParentPoller(this);
  OvmsRecMutexLock lock(&m_poll_mutex);
  series->ResetList(OvmsPoller::ResetMode::PollReset);
  m_polls.SetEntry(name, series);
  }

/** Remove a polling requester from the list if it exists.
  @param name Unique name/identifier of poll series.
  */
void OvmsPoller::RemovePollRequest(const std::string &name)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_polls.RemoveEntry(name);
  }

/**
 * PollResultCodeName: get text representation of result code
 */
const char* OvmsPoller::PollResultCodeName(int code)
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
const char *OvmsPoller::PollerSource(OvmsPoller::poller_source_t src)
  {
  switch (src)
    {
    case poller_source_t::Primary: return "PRI";
    case poller_source_t::Secondary: return "SEC";
    case poller_source_t::Successful: return "SRX";
    case poller_source_t::OnceOff: return "ONE";
    }
    return "XXX";
  }
const char *OvmsPoller::PollerCommand(OvmsPollCommand src)
  {
  switch(src)
    {
    case OvmsPollCommand::Pause: return "Pause";
    case OvmsPollCommand::Resume: return "Resume";
    case OvmsPollCommand::Throttle: return "Throttle";
    case OvmsPollCommand::ResponseSep: return "ResponseSep";
    case OvmsPollCommand::Keepalive: return "Keepalive";
    }
  return "??";
  }

static const char *PollerRegister="CAN Poller";

OvmsPollers::OvmsPollers( OvmsPoller::ParentSignal *parent)
  : m_parent_callback(parent),
    m_poll_state(0),
    m_poll_sequence_max(0),
    m_poll_fc_septime(25),
    m_poll_ch_keepalive(60),
    m_pollqueue(nullptr), m_polltask(nullptr),
    m_shut_down(false)
  {
  for (int idx = 0; idx < VEHICLE_MAXBUSSES; ++idx)
    m_pollers[idx] = nullptr;

  m_poll_txcallback = std::bind(&OvmsPollers::PollerTxCallback, this, _1, _2);
  MyCan.RegisterCallback(PollerRegister, std::bind(&OvmsPollers::PollerRxCallback, this, _1, _2));
  }
OvmsPollers::~OvmsPollers()
  {
  ShuttingDown();
  delete m_parent_callback;
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      delete m_pollers[i];
    }
  }

void OvmsPollers::StartingUp()
  {
  if (m_shut_down)
    return;
  CheckStartPollTask();
  }

void OvmsPollers::ShuttingDown()
  {
  if (m_shut_down)
    return;
  m_shut_down = true;
  if (m_polltask)
    {
    vTaskDelete(m_polltask);
    m_polltask = nullptr;
    }
  if (m_pollqueue)
    {
    vQueueDelete(m_pollqueue);
    m_pollqueue = nullptr;
    }
  MyCan.DeregisterCallback(PollerRegister);

  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      m_pollers[i]->ClearPollList();
    }
  }

/**
 * PollerTxCallback: internal: process poll request callbacks
 */
void OvmsPollers::PollerTxCallback(const CAN_frame_t* frame, bool success)
  {
  Queue_PollerFrame(*frame, success, true);
  }
/**
 * PollerTxCallback: internal: process poll request callbacks
 */
void OvmsPollers::PollerRxCallback(const CAN_frame_t* frame, bool success)
  {
  Queue_PollerFrame(*frame, success, false);
  }

/**
  * Make sure the Poll task is running.
  * Automatically called when a poller is set.
  */
void OvmsPollers::CheckStartPollTask( bool force )
  {

  if (!force)
    {
    bool require = false;
    OvmsRecMutexLock lock(&m_poller_mutex);
    for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
      {
      if (m_pollers[i])
        {
        require = true;
        break;
        }
      }

    if (!require)
      return;
    }

  if (!m_pollqueue)
    m_pollqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(OvmsPoller::poll_queue_entry_t));
  if (!m_polltask)
    {
    if (!m_parent_callback->Ready())
      return;
    xTaskCreatePinnedToCore(OvmsPollerTask, "OVMS Vehicle Poll",
      CONFIG_OVMS_VEHICLE_RXTASK_STACK, (void*)this, 10, &m_polltask, CORE(1));
    }
  }


void OvmsPollers::OvmsPollerTask(void *pvParameters)
  {
  OvmsPollers *me = (OvmsPollers*)pvParameters;
  me->PollerTask();
  }

void OvmsPollers::PollerTask()
  {
  OvmsPoller::poll_queue_entry_t entry;
  bool paused = false;
  while (!m_shut_down)
    {
    if (xQueueReceive(m_pollqueue, &entry, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      if (m_shut_down)
        break;

      switch (entry.entry_type)
        {
        case OvmsPoller::OvmsPollEntryType::FrameRx:
          {
          auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
          ESP_LOGV(TAG, "Pollers: FrameRx(bus=%d)", m_parent_callback->GetBusNo(entry.entry_FrameRxTx.frame.origin));
          if (poller)
            poller->Incoming(entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
          // pass to parent
          m_parent_callback->IncomingPollRxFrame(entry.entry_FrameRxTx.frame.origin, &entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
          }
          break;
        case OvmsPoller::OvmsPollEntryType::FrameTx:
          {
          auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
          ESP_LOGV(TAG, "Pollers: FrameTx(bus=%d)", m_parent_callback->GetBusNo(entry.entry_FrameRxTx.frame.origin));
          if (poller)
            poller->Outgoing(entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
          }
          break;
        case OvmsPoller::OvmsPollEntryType::Poll:
          if (!paused)
            {
            // Must not lock mutex while calling.
            ESP_LOGV(TAG, "[%" PRIu8 "]Poller: Send(%s)", entry.entry_Poll.busno, OvmsPoller::PollerSource(entry.entry_Poll.source));
            if (entry.entry_Poll.busno != 0)
              {
              auto bus = m_parent_callback->GetBus(entry.entry_Poll.busno);
              auto poller = GetPoller(bus);
              if (poller)
                poller->PollerSend(entry.entry_Poll.source);
              }
            else
              {
              for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
                {
                OvmsPoller *poller;
                  {
                  OvmsRecMutexLock lock(&m_poller_mutex);
                  poller = m_pollers[i];
                  }
                if (poller)
                  poller->PollerSend(entry.entry_Poll.source);
              }
            }
          break;
        case OvmsPoller::OvmsPollEntryType::Command:
          switch (entry.entry_Command.cmd)
            {
            case OvmsPoller::OvmsPollCommand::Pause:
              ESP_LOGD(TAG, "Pollers: Command Pause");
              paused = true;
              continue;
            case OvmsPoller::OvmsPollCommand::Resume:
              ESP_LOGD(TAG, "Pollers: Command Resume");
              paused = false;
              continue;
            case OvmsPoller::OvmsPollCommand::Throttle:
              if (entry.entry_Command.parameter != m_poll_sequence_max)
                {
                m_poll_sequence_max = entry.entry_Command.parameter;
                OvmsRecMutexLock lock(&m_poller_mutex);
                for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
                  {
                  if (m_pollers[i])
                    m_pollers[i]->PollSetThrottling(m_poll_sequence_max);
                  }
                }
              break;
            case OvmsPoller::OvmsPollCommand::ResponseSep:
              {
              auto septime = entry.entry_Command.parameter;
              if (septime <= 127 || (septime >= 241 && septime <= 249))
                {
                if (septime != m_poll_fc_septime)
                  {
                  m_poll_fc_septime = septime;
                  OvmsRecMutexLock lock(&m_poller_mutex);
                  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
                    {
                    if (m_pollers[i])
                      m_pollers[i]->PollSetResponseSeparationTime(septime);
                    }
                  }
                }
              }
              break;
            case OvmsPoller::OvmsPollCommand::Keepalive:
              if (entry.entry_Command.parameter != m_poll_ch_keepalive)
                {
                m_poll_ch_keepalive = entry.entry_Command.parameter;
                OvmsRecMutexLock lock(&m_poller_mutex);
                for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
                  {
                  if (m_pollers[i])
                    m_pollers[i]->PollSetChannelKeepalive(m_poll_ch_keepalive);
                  }
                }
              break;
            }
          break;

        case OvmsPoller::OvmsPollEntryType::PollState:
          ESP_LOGD(TAG, "Pollers: PollState(%" PRIu8 ")", entry.entry_PollState);
          OvmsRecMutexLock lock(&m_poller_mutex);
          for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
            {
            if (m_pollers[i])
              m_pollers[i]->Do_PollSetState(entry.entry_PollState);
            }
          }
          break;
        }

      }
    }
  }

OvmsPoller *OvmsPollers::GetPoller(canbus *can, bool force)
  {
  if (m_shut_down)
    return nullptr;

  int gap = -1;
    {
    OvmsRecMutexLock lock(&m_poller_mutex);
    for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
      {
      auto cur = m_pollers[i];
      if (!cur)
        {
        if (gap < 0)
          gap = i;
        }
      else if (cur->HasBus(can))
        {
        return cur;
        }
      }
    if (!force)
      return nullptr;
    if (gap < 0)
      return nullptr;

    int busno = m_parent_callback->GetBusNo(can);

    ESP_LOGD(TAG, "GetPoller( busno=%" PRIu8 ")", busno);
    auto newpoller =  new OvmsPoller(can, busno, m_parent_callback, m_poll_txcallback);
    newpoller->m_poll_state = m_poll_state;
    newpoller->m_poll_sequence_max = m_poll_sequence_max;
    newpoller->m_poll_fc_septime = m_poll_fc_septime;
    newpoller->m_poll_ch_keepalive = m_poll_ch_keepalive;
    m_pollers[gap] = newpoller;
    }

  if (gap >= 0)
    CheckStartPollTask(true);

  return (gap<0) ? nullptr : m_pollers[gap];
  }

void OvmsPollers::QueuePollerSend(OvmsPoller::poller_source_t src, uint8_t busno)
  {
  if (m_shut_down)
    return;
  if (!m_parent_callback->Ready())
    return;
  if (!m_pollqueue)
    return;
  // Sends poller send message
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = OvmsPoller::OvmsPollEntryType::Poll;
  entry.entry_Poll.busno = busno;
  entry.entry_Poll.source = src;

  ESP_LOGV(TAG, "Pollers: Queue PollerSend(%s, %" PRIu8 ")", OvmsPoller::PollerSource(src), busno);
  if (xQueueSend(m_pollqueue, &entry, 0) != pdPASS)
    ESP_LOGI(TAG, "Pollers[Send]: Task Queue Overflow");
  }

void OvmsPollers::PollSetPidList(canbus* defbus, const OvmsPoller::poll_pid_t* plist)
  {
  uint8_t defbusno = !defbus ? 0 : m_parent_callback->GetBusNo(defbus);
  bool hasbus[1+VEHICLE_MAXBUSSES];
  for (int i = 0 ; i <= VEHICLE_MAXBUSSES; ++i)
    hasbus[i] = false;

  // Check for an Empty list.
  if (plist->txmoduleid == 0)
    {
    plist = nullptr;
    ESP_LOGD(TAG, "PollSetPidList - Setting Empty List");
    }

  // Load up any bus pollers required.
  if (plist)
    {
    for (const OvmsPoller::poll_pid_t *plcur = plist; plcur->txmoduleid != 0; ++plcur)
      {
      uint8_t usebusno = plcur->pollbus;
      if ( usebusno > VEHICLE_MAXBUSSES)
        continue;
      if (usebusno == 0)
        {
        if (defbusno == 0)
          continue;
        usebusno = defbusno;
        }
      if (!hasbus[usebusno])
        {
        hasbus[usebusno] = true;
        GetPoller(m_parent_callback->GetBus(usebusno), true);
        }
      }
    }
  else
    ESP_LOGD(TAG, "PollSetPidList - Setting NULL List");

  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    auto poller = m_pollers[i];
    if (poller)
      {
      uint8_t busno = poller->m_poll.bus_no;
      if ((busno > VEHICLE_MAXBUSSES) || hasbus[busno])
        {
        ESP_LOGD(TAG, "PollSetPidList - Setting for bus=%" PRIu8 " defbus=%" PRIu8, busno, defbusno);
        poller->PollSetPidList(defbusno, plist);
        }
      else
        {
        ESP_LOGD(TAG, "PollSetPidList - Clearing for bus=%" PRIu8, busno);
        poller->PollSetPidList(0, nullptr);
        }
      }
    }
  }

bool OvmsPollers::HasPollList(canbus* bus)
  {
  if (bus)
    {
    auto poller = GetPoller(bus, false);
    return poller && poller->HasPollList();
    }
  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    auto poller = m_pollers[i];
    if (poller && poller->HasPollList())
      return true;
    }
  return false;
  }

void OvmsPollers::Queue_Command(OvmsPoller::OvmsPollCommand cmd, uint16_t param)
  {
  if (!m_pollqueue)
    return;
  // Queues the command
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = OvmsPoller::OvmsPollEntryType::Command;
  entry.entry_Command.cmd = cmd;
  entry.entry_Command.parameter = param;

  ESP_LOGD(TAG, "Pollers: Queue Command()");
  if (xQueueSend(m_pollqueue, &entry, 0) != pdPASS)
    ESP_LOGI(TAG, "Poller[Command]: Task Queue Overflow");
  }

void OvmsPollers::Queue_PollerFrame(const CAN_frame_t &frame, bool success, bool istx)
  {
  if (!m_parent_callback->Ready())
    return;
  if (!m_pollqueue)
    return;
  // Queues the frame
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = istx ? OvmsPoller::OvmsPollEntryType::FrameTx : OvmsPoller::OvmsPollEntryType::FrameRx;
  entry.entry_FrameRxTx.frame = frame;
  entry.entry_FrameRxTx.success = success;

  ESP_LOGD(TAG, "Poller: Queue PollerFrame()");
  if (xQueueSend(m_pollqueue, &entry, 0) != pdPASS)
    ESP_LOGI(TAG, "Poller[Frame]: Task Queue Overflow");
  }

void OvmsPollers::PollSetState(uint8_t state)
  {
  if (state >= VEHICLE_POLL_NSTATES)
    return;
  if (state == m_poll_state)
    return;

  m_poll_state = state;
  if (!m_pollqueue)
    return;

  // Queues the command
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = OvmsPoller::OvmsPollEntryType::PollState;
  entry.entry_PollState = state;

  ESP_LOGD(TAG, "Pollers: Queue SetState()");
  if (xQueueSend(m_pollqueue, &entry, 0) != pdPASS)
    ESP_LOGI(TAG, "Pollers[SetState]: Task Queue Overflow");
  }


// signal poller (private)
void OvmsPollers::PollerResetThrottle()
  {
  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      m_pollers[i]->ResetThrottle();
    }
  }

static const char *PollResStr( OvmsPoller::OvmsNextPollResult res)
{
  switch(res) {
    case OvmsPoller::OvmsNextPollResult::StillAtEnd: return "StillAtEnd";
    case OvmsPoller::OvmsNextPollResult::FoundEntry: return "FoundEntry";
    case OvmsPoller::OvmsNextPollResult::ReachedEnd: return "ReachedEnd";
    default: return "Unknown";
  }
}

// List of Poll Series

OvmsPoller::PollSeriesList::PollSeriesList()
  : m_first(nullptr), m_last(nullptr), m_iter(nullptr)
  {
  }

OvmsPoller::PollSeriesList::~PollSeriesList()
  {
  Clear();
  }

void OvmsPoller::PollSeriesList::SetEntry(const std::string &name, std::shared_ptr<PollSeriesEntry> series, bool blocking)
  {
  bool activate = blocking;
  for (poll_series_t *it = m_first; it != nullptr; it = it->next)
    {
    if (it->name == name)
      {
      if (it->series == series)
        return;

      if (it->series != nullptr)
        it->series->Removing();
      it->series = series;
      if (activate && it == m_first)
        m_iter = it;
      ESP_LOGD(TAG, "Poll List: Replaced Entry %s (%s)%s", it->name.c_str(), name.c_str(), activate ? " (active)" : "");
      return;
      }
    }
  poll_series_t *newval = new poll_series_t;
  newval->name = name;
  newval->series = series;
  newval->is_blocking = blocking;
  newval->next = nullptr;
  newval->prev = nullptr;

  poll_series_t *before = nullptr; // Default END.
  if (blocking)
    {
    before = m_first;
    while (before && before->is_blocking)
      {
      before = before->next;
      activate = false; // not first.
      }
    }

  InsertBefore(newval, before);
  if (activate)
    m_iter = newval;
  ESP_LOGD(TAG, "Poll List: Added Entry %s%s%s", newval->name.c_str(), blocking ? " (blocking)" : "", activate ? " (active)" : "");
  }

bool OvmsPoller::PollSeriesList::IsEmpty()
  {
  return m_first == nullptr;
  }

void OvmsPoller::PollSeriesList::Remove( poll_series_t *iter)
  {
  if (!iter)
    return;
  auto iternext = iter->next;
  auto iterprev = iter->prev;
  iter->next = nullptr;
  iter->prev = nullptr;

  if (m_iter == iter)
    m_iter = iternext;
  if (m_first == iter)
    m_first = iternext;
  if (m_last == iter)
    m_last = iterprev;

  if (iterprev)
    iterprev->next = iternext;
  if (iternext)
    iternext->prev = iterprev;

  ESP_LOGV(TAG, "Poll List: Removing series %s", iter->name.c_str());
  if (iter->series != nullptr)
     iter->series->Removing();
  iter->series = nullptr;
  ESP_LOGV(TAG, "Poll List: Deleting series %s", iter->name.c_str());
  delete iter;
  }

void OvmsPoller::PollSeriesList::InsertBefore( poll_series_t *iter, poll_series_t *before)
  {
  if (before == nullptr || !m_first) // End
    {
    iter->prev = m_last;
    iter->next = nullptr;
    if (m_last)
      m_last->next = iter;
    else
      m_first = iter;
    m_last = iter;
    }
  else
    {
    iter->next = before;
    iter->prev = before->prev;
    before->prev = iter;

    if (before == m_first)
      m_first = iter;
    }
  }

void OvmsPoller::PollSeriesList::RemoveEntry( const std::string &name)
  {
  for (poll_series_t *it = m_first; it != nullptr; it = it->next)
    {
    if (it->name == name)
      {
      Remove(it);
      return;
      }
    }
  }

void OvmsPoller::PollSeriesList::Clear()
  {
  while (m_first != nullptr)
    Remove(m_first);
  }

void OvmsPoller::PollSeriesList::RestartPoll(OvmsPoller::ResetMode mode)
  {
  m_iter = m_first;
  for ( auto iter = m_first; iter != nullptr; iter = iter->next)
    {
    if (iter->series != nullptr)
      iter->series->ResetList(mode);
    }
  }

// Process an incoming packet.
void OvmsPoller::PollSeriesList::IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length)
  {
  if ((m_iter != nullptr) && (m_iter->series != nullptr))
    {
    ESP_LOGD(TAG, "Poll List:[%s] IncomingPacket TYPE:%x PID: %03x LEN: %d REM: %d ", m_iter->name.c_str(), job.type, job.pid, length, job.mlremain);

    m_iter->series->IncomingPacket(job, data, length);
    }
  else
    {
    ESP_LOGD(TAG, "Poll List:[Discard] IncomingPacket TYPE:%x PID: %03x LEN: %d REM: %d ", job.type, job.pid, length, job.mlremain);
    }
  }

// Process An Error
void OvmsPoller::PollSeriesList::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if ((m_iter != nullptr) && (m_iter->series != nullptr))
    {
    ESP_LOGD(TAG, "Poll List:[%s] IncomingError TYPE:%x PID: %03x Code: %02X", m_iter->name.c_str(), job.type, job.pid, code);
    m_iter->series->IncomingError(job, code);
    }
  else
    {
    ESP_LOGD(TAG, "Poll List:[Discard] IncomingError TYPE:%x PID: %03x Code: %02X", job.type, job.pid, code);
    }
  }

OvmsPoller::OvmsNextPollResult OvmsPoller::PollSeriesList::NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate)
  {
  if (!m_first)
    {
    ESP_LOGV(TAG, "PollSeriesList::NextPollEntry - No List Set");
    return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    }
  if (!m_iter)
    {
    ESP_LOGV(TAG, "PollSeriesList::NextPollEntry - Not Started");
    return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    }

  OvmsPoller::OvmsNextPollResult res = OvmsPoller::OvmsNextPollResult::StillAtEnd;
  while (true)
    {
    int skipped = 0;
    while (m_iter != nullptr && m_iter->series == nullptr)
      {
      m_iter = m_iter->next;
      ++skipped;
      }
    if (skipped)
      ESP_LOGV(TAG, "PollSeriesList::NextPollEntry Skipped %d", skipped);

    if (m_iter == nullptr)
      return OvmsPoller::OvmsNextPollResult::ReachedEnd;
    res = m_iter->series->NextPollEntry(entry, mybus, pollticker, pollstate);

    ESP_LOGD(TAG, "PollSeriesList::NextPollEntry[%s]: %s", m_iter->name.c_str(), PollResStr(res));

    switch (res)
      {
      case OvmsPoller::OvmsNextPollResult::StillAtEnd:
        {
        ESP_LOGV(TAG, "Poll Still at end: '%s'", m_iter->name.c_str());
        if (m_iter->is_blocking)
          {
          m_iter = nullptr;
          return res;
          }

        m_iter = m_iter->next;
        break;
        }
      case OvmsPoller::OvmsNextPollResult::ReachedEnd:
        {
        switch (m_iter->series->FinishRun())
          {
          case OvmsPoller::SeriesStatus::Next:
            {
            if (m_iter->is_blocking)
              {
              m_iter = nullptr;
              return res;
              }
            m_iter = m_iter->next;
            break;// Default .. move to next
            }
          case OvmsPoller::SeriesStatus::RemoveNext:
            {
            ESP_LOGD(TAG, "Poll Auto-Removing (next) '%s'", m_iter->name.c_str());
            Remove(m_iter);
            break;
            }
          case OvmsPoller::SeriesStatus::RemoveRestart:
            {
            // Resume - skipping over 'StillAtEnd' iterators.
            ESP_LOGD(TAG, "Poll Auto-Removing (restart) '%s'", m_iter->name.c_str());
            auto cur = m_iter;
            m_iter = nullptr;
            Remove(cur);
            m_iter = m_first;
            break;
            }
          default:
            // Shouldn't happen.
            if (m_iter->is_blocking)
              {
              m_iter = nullptr;
              return res;
              }
            m_iter = m_iter->next;
          }
        break;
        }
      default:
        return res;
      }
    }
  }

bool OvmsPoller::PollSeriesList::HasPollList()
  {
  for (auto it = m_first; it != nullptr; it = it->next)
    {
    if (it->series != nullptr && it->series->HasPollList())
      return true;
    }
  return false;
  }

bool OvmsPoller::PollSeriesList::HasRepeat()
  {
  for (auto it = m_first; it != nullptr; it = it->next)
    {
    if (it->series != nullptr && it->series->HasRepeat())
      return true;
    }
  return false;
  }

// Standard Poll Series - Replaces the original functionality

// Standard Poll Series class
OvmsPoller::StandardPollSeries::StandardPollSeries(OvmsPoller *poller, uint16_t stateoffset  )
  : m_poller(poller), m_state_offset(stateoffset),  m_defaultbus(0), m_poll_plist(nullptr), m_poll_plcur(nullptr) 
  {
  }
void OvmsPoller::StandardPollSeries::SetParentPoller(OvmsPoller *poller)
  {
  m_poller = poller;
  }

// Set the PID List in use.
void OvmsPoller::StandardPollSeries::PollSetPidList(uint8_t defaultbus, const poll_pid_t* plist)
  {
  ESP_LOGV(TAG, "Standard Poll Series: PID List set");
  m_poll_plcur = nullptr;
  m_poll_plist = plist;
  m_defaultbus = defaultbus;
  }

void OvmsPoller::StandardPollSeries::ResetList(OvmsPoller::ResetMode mode)
  {
  if (mode == OvmsPoller::ResetMode::PollReset)
    {
    ESP_LOGV(TAG, "Standard Poll Series: List reset");
    m_poll_plcur = NULL;
    }
  }

OvmsPoller::OvmsNextPollResult OvmsPoller::StandardPollSeries::NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate)
  {

  entry = {};

  // Offset pollstate and check it is within polltime bounds.
  if (pollstate < m_state_offset)
    return OvmsNextPollResult::StillAtEnd;
  pollstate -= m_state_offset;
  if (pollstate >= VEHICLE_POLL_NSTATES)
    return OvmsNextPollResult::StillAtEnd;

  // Restart poll list cursor:
  if (m_poll_plcur == NULL)
    m_poll_plcur = m_poll_plist;
  else if (m_poll_plcur->txmoduleid == 0)
    return OvmsNextPollResult::StillAtEnd;
  else
    ++m_poll_plcur;

  while (m_poll_plcur->txmoduleid != 0)
    {
    uint8_t bus = m_poll_plcur->pollbus;
    if (bus == 0)
      bus = m_defaultbus;
    if (mybus == bus)
      {
      uint16_t polltime = m_poll_plcur->polltime[pollstate];
      if (( polltime > 0) && ((pollticker % polltime) == 0))
        {
        entry = *m_poll_plcur;
        ESP_LOGD(TAG, "Found Poll Entry for Standard Poll");
        return OvmsNextPollResult::FoundEntry;
        }
      }
    // Poll entry is not due, check next
    ++m_poll_plcur;
    }
  return OvmsNextPollResult::ReachedEnd;
  }

void OvmsPoller::StandardPollSeries::IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length)
  {
  if (m_poller)
    m_poller->IncomingPollReply(job, data, length);
  }

void OvmsPoller::StandardPollSeries::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if (m_poller)
    m_poller->IncomingPollError(job, code);
  }

OvmsPoller::SeriesStatus OvmsPoller::StandardPollSeries::FinishRun()
  {
  return OvmsPoller::SeriesStatus::Next;
  }

void OvmsPoller::StandardPollSeries::Removing()
  {
  m_poller = nullptr;
  }

bool OvmsPoller::StandardPollSeries::HasPollList()
  {
  return (m_defaultbus != 0)
      && (m_poll_plist != NULL)
      && (m_poll_plist->txmoduleid != 0);
  }

bool OvmsPoller::StandardPollSeries::HasRepeat()
  {
  return false;
  }

// StandardPacketPollSeries

OvmsPoller::StandardPacketPollSeries::StandardPacketPollSeries( OvmsPoller *poller, int repeat_max, poll_success_func success, poll_fail_func fail)
  : StandardPollSeries(poller),
    m_repeat_max(repeat_max),
    m_repeat_count(0),
    m_success(success),
    m_fail(fail)
  {
  }

// Move list to start.
void OvmsPoller::StandardPacketPollSeries::ResetList(OvmsPoller::ResetMode mode)
  {
  switch (mode)
    {
      case OvmsPoller::ResetMode::PollReset:
        m_repeat_count = 0;
        break;
      case OvmsPoller::ResetMode::LoopReset:
        if (++m_repeat_count >= m_repeat_max)
          return; // No more retries... don't reset the loop.
        break;
    }
  // Do the base Poll reset - ignore passed-in.
  OvmsPoller::StandardPollSeries::ResetList(OvmsPoller::ResetMode::PollReset);
  }

// Process an incoming packet.
void OvmsPoller::StandardPacketPollSeries::IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length)
  {
  if (job.mlframe == 0)
    {
    m_data.clear();
    m_data.reserve(length+job.mlremain);
    }
  m_data.append((char*)data, length);
  if (job.mlremain == 0)
    {
    if (m_success)
      m_success(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, m_data);
    m_data.clear();
    }
  }

// Process An Error
void OvmsPoller::StandardPacketPollSeries::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if (code == 0)
    {
    ESP_LOGD(TAG, "Packet failed with zero error %.03" PRIx32 " TYPE:%x PID: %03x", job.moduleid_rec, job.type, job.pid);
    m_data.clear();
    if (m_success)
      m_success(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, m_data);
    }
  else
    {
    if (m_fail)
      m_fail(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, code);
    OvmsPoller::StandardPollSeries::IncomingError(job, code);
    }
  }

// Return true if this series has entries to retry/redo.
bool OvmsPoller::StandardPacketPollSeries::HasRepeat()
  {
  return (m_repeat_count < m_repeat_max) && HasPollList();
  }

// OvmsPoller::OnceOffPollBase class

OvmsPoller::OnceOffPollBase::OnceOffPollBase( const poll_pid_t &pollentry, std::string *rxbuf, int *rxerr, uint8_t retry_fail)
   : m_sent(status_t::Init), m_poll(pollentry), m_poll_rxbuf(rxbuf), m_poll_rxerr(rxerr),
    m_retry_fail(retry_fail)
  {
  }
void OvmsPoller::OnceOffPollBase::SetParentPoller(OvmsPoller *poller)
  {
  }

OvmsPoller::OnceOffPollBase::OnceOffPollBase(std::string *rxbuf, int *rxerr, uint8_t retry_fail)
   : m_sent(status_t::Init),
    m_poll({ 0, 0, 0, 0, { 1, 1, 1, 1 }, 0, 0 }),
    m_poll_rxbuf(rxbuf), m_poll_rxerr(rxerr),
    m_retry_fail(retry_fail)
  {
  }

void OvmsPoller::OnceOffPollBase::Done(bool success)
  {
  }

void OvmsPoller::OnceOffPollBase::SetPollPid( uint32_t txid, uint32_t rxid, const std::string &request, uint8_t protocol, uint8_t pollbus, uint16_t polltime)
  {
  // prepare single poll:
  m_poll  = { txid, rxid, 0, 0, { polltime, polltime, polltime, polltime }, pollbus, protocol };

  assert(request.size() > 0);
  m_poll.type = request[0];
  m_poll.xargs.tag = POLL_TXDATA;

  m_poll_data = request;
  if (POLL_TYPE_HAS_16BIT_PID(m_poll.type))
    {
    assert(request.size() >= 3);
    m_poll.xargs.pid = request[1] << 8 | request[2];
    m_poll.xargs.datalen = LIMIT_MAX(request.size()-3, 4095);
    m_poll.xargs.data = (const uint8_t*)m_poll_data.data()+3;
    }
  else if (POLL_TYPE_HAS_8BIT_PID(m_poll.type))
    {
    assert(request.size() >= 2);
    m_poll.xargs.pid = request.at(1);
    m_poll.xargs.datalen = LIMIT_MAX(request.size()-2, 4095);
    m_poll.xargs.data = (const uint8_t*)m_poll_data.data()+2;
    }
  else
    {
    m_poll.xargs.pid = 0;
    m_poll.xargs.datalen = LIMIT_MAX(request.size()-1, 4095);
    m_poll.xargs.data = (const uint8_t*)m_poll_data.data()+1;
    }
  }

void OvmsPoller::OnceOffPollBase::SetPollPid( uint32_t txid, uint32_t rxid, uint8_t polltype, uint16_t pid,  uint8_t protocol, uint8_t pollbus, uint16_t polltime)
  {
  m_poll  = { txid, rxid, polltype, pid, { polltime, polltime, polltime, polltime }, pollbus, protocol };
  }

// Move list to start.
void OvmsPoller::OnceOffPollBase::ResetList(OvmsPoller::ResetMode mode)
  {
  switch(m_sent)
    {
    case status_t::Init:
      ESP_LOGD(TAG, "Once Off Poll: List reset to start");
      break;
    case status_t::Retry:
      if (mode == OvmsPoller::ResetMode::PollReset)
        {
        if (!m_retry_fail)
          {
          m_sent = status_t::Stopping;
          return;
          }
        ESP_LOGD(TAG, "Once Off Poll: Reset for retry");
        --m_retry_fail;
        m_sent = status_t::RetryInit;
        }
      break;
    default:
      m_sent = status_t::Stopping;
    }
  }

// Find the next poll entry.
OvmsPoller::OvmsNextPollResult OvmsPoller::OnceOffPollBase::NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate)
  {
  switch (m_sent)
    {
    case status_t::Init:
      {
      entry = m_poll;
      m_sent = status_t::Sent;
      return OvmsNextPollResult::FoundEntry;
      }
    case status_t::RetrySent:
    case status_t::Sent:
      {
      if ((m_retry_fail == 0) || (!m_poll_rxerr) || *m_poll_rxerr == 0)
        m_sent = status_t::Stopped;
      else
        {
        m_sent = status_t::Retry;
        ESP_LOGD(TAG, "Once Off Poll: Retries left %d", m_retry_fail);
        }
      return OvmsPoller::OvmsNextPollResult::ReachedEnd;
      }
    case status_t::Stopping:
      m_sent = status_t::Stopped;
      return OvmsPoller::OvmsNextPollResult::ReachedEnd;
    case status_t::Stopped:
      return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    case status_t::Retry:
      return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    case status_t::RetryInit:
      {
      entry = m_poll;
      m_sent = status_t::RetrySent;
      return OvmsNextPollResult::FoundEntry;
      }
    default:
      return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    }
  }

// Process an incoming packet.
void OvmsPoller::OnceOffPollBase::IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length)
  {
  if (m_poll_rxbuf != nullptr)
    {
    if (job.mlframe == 0 )
      {
      m_poll_rxbuf->clear();
      m_poll_rxbuf->reserve(length+job.mlremain);
      }
    m_poll_rxbuf->append((char*)data, length);
    }
  if (job.mlremain == 0)
    {
    if (m_poll_rxerr)
      *m_poll_rxerr = 0;

    m_sent = status_t::Stopping;
    Done(true);
    }
  }

// Process An Error
void OvmsPoller::OnceOffPollBase::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if (m_poll_rxerr)
    *m_poll_rxerr = code;
  if (m_poll_rxbuf)
    m_poll_rxbuf->clear();
  if (code == 0)
    m_sent = status_t::Stopping;
  else
    {
    switch (m_sent)
      {
      case status_t::Retry: return;
      case status_t::Sent:
      case status_t::RetrySent:
        {
        if (m_retry_fail > 0)
          {
          m_sent = status_t::Retry;
          return;
          }
        m_sent = status_t::Stopping;
        }
      default: break;
      }
    }
  Done(false);
  }

bool OvmsPoller::OnceOffPollBase::HasPollList()
  {
  return m_poll.txmoduleid != 0;
  }

bool OvmsPoller::OnceOffPollBase::HasRepeat()
  {
  return false; // Don't retry in same tick.
  }

// OvmsPoller::BlockingOnceOffPoll class
OvmsPoller::BlockingOnceOffPoll::BlockingOnceOffPoll(const poll_pid_t &pollentry, std::string *rxbuf, int *rxerr, OvmsSemaphore *rxdone )
   : OvmsPoller::OnceOffPollBase(pollentry, rxbuf, rxerr),  m_poll_rxdone(rxdone)
  {
  }

void OvmsPoller::BlockingOnceOffPoll::Done(bool success)
  {
  if (m_poll_rxdone)
    {
    ESP_LOGD(TAG, "Blocking Poll: Done %s", success ? "success" : "fail");
    m_poll_rxdone->Give();
    Finished();
    }
  }

// Called when run is finished to determine what happens next.
OvmsPoller::SeriesStatus OvmsPoller::BlockingOnceOffPoll::FinishRun()
  {
  return OvmsPoller::SeriesStatus::RemoveRestart;
  }


void OvmsPoller::BlockingOnceOffPoll::Removing()
  {
  Done(false);
  }

// Called To null out the call-back pointers.
void OvmsPoller::BlockingOnceOffPoll::Finished()
  {
  m_poll_rxbuf = nullptr;
  m_poll_rxdone = nullptr;
  m_poll_rxerr = nullptr;
  }

// OnceOffPoll class

// Once off poll entry with buffer and result.
OvmsPoller::OnceOffPoll::OnceOffPoll(poll_success_func success, poll_fail_func fail,
    uint32_t txid, uint32_t rxid, const std::string &request, uint8_t protocol, uint8_t pollbus, uint8_t retry_fail)
  : OvmsPoller::OnceOffPollBase(nullptr, &m_error, retry_fail),
    m_success(success), m_fail(fail), m_error(0)
  {
  m_poll_rxbuf = &m_data;
  SetPollPid(txid, rxid, request, protocol, pollbus);
  }

OvmsPoller::OnceOffPoll::OnceOffPoll( poll_success_func success, poll_fail_func fail,
            uint32_t txid, uint32_t rxid, uint8_t polltype, uint16_t pid,  uint8_t protocol, uint8_t pollbus, uint8_t retry_fail)
  : OvmsPoller::OnceOffPollBase( nullptr, &m_error, retry_fail),
    m_success(success), m_fail(fail), m_error(0)
  {
  m_poll_rxbuf = &m_data;
  SetPollPid(txid, rxid, polltype, pid, protocol, pollbus);
  }

void OvmsPoller::OnceOffPoll::Done(bool success)
  {
  ESP_LOGD(TAG, "Once-Off Poll: Done %s", success ? "success" : "fail");
  m_poll_rxbuf = nullptr;
  if (success)
    {
    if (m_success)
      m_success(m_poll.type, m_poll.txmoduleid, m_poll.rxmoduleid, m_poll.pid, m_data );
    }
  else
    {
    if (m_fail)
      m_fail(m_poll.type, m_poll.txmoduleid, m_poll.rxmoduleid, m_poll.pid, m_error);
    }
  }

// Called when run is finished to determine what happens next.
OvmsPoller::SeriesStatus OvmsPoller::OnceOffPoll::FinishRun()
  {
  if (m_sent == status_t::Retry)
    return OvmsPoller::SeriesStatus::Next;
  return OvmsPoller::SeriesStatus::RemoveNext;
  }

void OvmsPoller::OnceOffPoll::Removing()
  {
  }
