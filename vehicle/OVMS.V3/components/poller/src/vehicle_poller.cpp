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
#include "vehicle_poller.h"
#include "can.h"
#include "ovms_boot.h"
#include "dbc.h"

using namespace std::placeholders;

OvmsPollers MyPollers __attribute__ ((init_priority (7000)));

// Runtime control for logging:
#define IFTRACE(x) if (MyPollers.m_trace &  OvmsPollers::tracetype_t::trace_##x)

OvmsPoller::OvmsPoller(canbus* can, uint8_t can_number, OvmsPollers *parent,
    const CanFrameCallback &polltxcallback)
  : m_parent(parent)
  {
  m_poll.bus = can;
  m_poll.bus_no = can_number;
  m_poll_txcallback = polltxcallback;

  m_poll_state = 0;

  m_poll.entry = {};
  m_poll_vwtp = {};
  m_poll.ticker = init_ticker;

  m_poll.moduleid_sent = 0;
  m_poll.moduleid_low = 0;
  m_poll.moduleid_high = 0;
  m_poll.type = VEHICLE_POLL_TYPE_NONE;
  m_poll.pid = 0;
  m_poll.mlremain = 0;
  m_poll.mloffset = 0;
  m_poll.mlframe = 0;
  m_poll.format = CAN_frame_std;
  m_poll.raw_data = nullptr;
  m_poll.raw_data_len = 0;
  m_poll_wait = 0;
  m_poll_sequence_max = 1;
  m_poll_sequence_cnt = 0;
  m_poll_fc_septime = 25;       // response default timing: 25 milliseconds
  m_poll_ch_keepalive = 60;     // channel keepalive default: 60 seconds
  m_poll_repeat_count = 0;
  m_poll_sent_last = 0;
  m_poll_between_success = 0;
  }

/** Handle incoming frame.
 * @return true if the frame was handled by the poller.
 */
bool OvmsPoller::Incoming(CAN_frame_t &frame, bool success)
  {

  // No multiframe request is active.
  if (m_poll.type == VEHICLE_POLL_TYPE_NONE)
    return false;

  // Pass frame to poller protocol handlers:
  if (frame.origin == m_poll_vwtp.bus && frame.MsgID == m_poll_vwtp.rxid)
    {
    // Keep raw Data for DBC
    m_poll.format = frame.FIR.B.FF;
    m_poll.raw_data = &frame.data.u8[0];
    m_poll.raw_data_len = 8;

    auto ret = PollerVWTPReceive(&frame, frame.MsgID);
    m_poll.raw_data = nullptr;
    m_poll.raw_data_len = 0;
    return ret;
    }
  else if (frame.origin != m_poll.bus)
    {
    return false;
    }
  else
    {
    if (m_poll.entry.txmoduleid == 0)
      {
      IFTRACE(TXRX) ESP_LOGV(TAG, "[%" PRIu8 "]Poller: Incoming - dropped (no poll entry)", m_poll.bus_no);
      return false;
      }
    if (!m_poll_wait)
      {
      IFTRACE(TXRX) ESP_LOGV(TAG, "[%" PRIu8 "]Poller: Incoming - timed out", m_poll.bus_no);
      return false;
      }
    uint32_t msgid;
    if (m_poll.protocol == ISOTP_EXTADR)
      msgid = frame.MsgID << 8 | frame.data.u8[0];
    else
      msgid = frame.MsgID;
    IFTRACE(TXRX) ESP_LOGV(TAG, "[%" PRIu8 "]Poller: FrameRx(bus=%s, msg=%" PRIx32 " )", m_poll.bus_no, frame.origin == m_poll.bus ? "Self" : "Other", msgid );
    if (msgid >= m_poll.moduleid_low && msgid <= m_poll.moduleid_high)
      {
      // Keep raw Data for DBC
      m_poll.format = frame.FIR.B.FF;
      m_poll.raw_data = &frame.data.u8[0];
      m_poll.raw_data_len = 8;
      auto ret = PollerISOTPReceive(&frame, msgid);
      m_poll.raw_data = nullptr;
      m_poll.raw_data_len = 0;
      return ret;
      }
    }
  return false;
  }

OvmsPoller::~OvmsPoller()
  {
  }


/**
 * IncomingPollTxCallback: poller TX callback
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
  m_polls.IncomingTxReply(job, success);
  }

bool OvmsPoller::Ready() const
  {
  return m_parent->Ready();
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
void OvmsPoller::PollSetPidList(uint8_t defaultbus, const OvmsPoller::poll_pid_t* plist, VehicleSignal *signal)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  if (m_poll_series == nullptr)
    {
    if (!plist) // Don't add if not necessary.
      return;
    m_poll_series = std::shared_ptr<StandardPollSeries>(new StandardVehiclePollSeries(this, signal));
    m_polls.SetEntry("!v.standard", m_poll_series);
    }

  m_poll_series->PollSetPidList(defaultbus, plist);

  m_poll_run_finished = true;
  m_poll.ticker = init_ticker;
  m_poll_sequence_cnt = 0;
  }

void OvmsPoller::Do_PollSetState(uint8_t state)
  {
  if ( state != m_poll_state)
    {
    m_poll_state = state;
    m_poll_run_finished = true;
    m_poll.ticker = init_ticker;
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

void OvmsPoller::PollSetTimeBetweenSuccess(uint16_t time_between_ms)
  {
  m_poll_between_success = pdMS_TO_TICKS(time_between_ms);
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

bool OvmsPoller::HasPollList() const
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  return m_polls.HasPollList();
  }

void OvmsPoller::ClearPollList()
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  return m_polls.Clear();
  }

bool OvmsPoller::CanPoll() const
  {
  // Check Throttle
  return (!m_poll_sequence_max || m_poll_sequence_cnt < m_poll_sequence_max);
  }

void OvmsPoller::PollerNextTick(poller_source_t source)
  {
  // Completed checking all poll entries for the current m_poll.ticker

  IFTRACE(Poller) ESP_LOGD(TAG, "[%" PRIu8 "]PollerNextTick(%s): cycle complete for ticker=%" PRIu32, m_poll.bus_no, PollerSource(source), m_poll.ticker);
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

    if (m_poll.ticker < max_ticker)
      m_poll.ticker++;
    else
      m_poll.ticker = (m_poll.ticker == init_ticker) ? 0 : 1;
    }
  }

/** Called after reaching the end of available POLL entries.
 */
void OvmsPoller::PollRunFinished()
  {
  // Call back on vehicle PollRunFinished()  with bus number / bus
  m_parent->PollRunFinished(m_poll.bus);
  }

/**
 * PollerSend: internal: start next due request
 */
void OvmsPoller::PollerSend(poller_source_t source)
  {

  bool curIsBlocking;
  {
    OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(50));
    if (!lock.IsLocked())
      {
      IFTRACE(Poller) ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend - Failed to lock ", m_poll.bus_no);
      return; // Something is blocking .. don't bind things up.
      }
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
  if (fromPrimaryTicker)
    {
    // Call back on vehicle PollerStateTicker()  with bus number / bus
    m_parent->PollerStateTicker(m_poll.bus);
    }

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
    IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Waiting %" PRIu8, m_poll.bus_no, m_poll_wait);
    return;
    }

  if (!curIsBlocking && m_poll_ticked)
    {
    if (!m_poll_run_finished && m_poll_repeat_count > 0)
      {
      IFTRACE(Poller) ESP_LOGD(TAG, "[%" PRIu8 "]Poller finished primary run", m_poll.bus_no);
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
 // Clear. If it got to here we are ready to send a new item.
  m_poll.type = VEHICLE_POLL_TYPE_NONE;

  if (m_poll.ticker == init_ticker)
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Wait for next primary poll", m_poll.bus_no );
    return;
    }

  // Check poll bus & list:
  if (!HasPollList())
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Empty", m_poll.bus_no);
    return;
    }

  if (! CanPoll())
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Throttled", m_poll.bus_no);
    return;
    }

  OvmsPoller::OvmsNextPollResult res;
  {
    OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(50));
    if (!lock.IsLocked())
      {
      IFTRACE(Poller) ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend - Failed to lock for NextPollEntry", m_poll.bus_no);
      return; // Something is blocking .. don't bind things up.
      }
    res = m_polls.NextPollEntry(m_poll.entry, m_poll.bus_no, m_poll.ticker, m_poll_state);
  }
  if (res == OvmsNextPollResult::ReachedEnd && m_polls.HasRepeat())
    {
    ++m_poll_repeat_count;
    if (m_poll_repeat_count > max_poll_repeat)
      {
      IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Poller Retry Exceeded - Finishing", m_poll.bus_no);
      m_poll_run_finished = true;
      res = OvmsNextPollResult::StillAtEnd;
      }
    else
      {
      IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Poller Reset for Repeat (%s)", m_poll.bus_no, OvmsPoller::PollerSource(source));
      m_polls.RestartPoll(OvmsPoller::ResetMode::LoopReset);
      // If this poll is from a ISOTP success, don't overwhelm the ECU,
      // wait until a Secondary tick.
      if (source == poller_source_t::Successful)
        {
        IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Poller Restart: Wait for secondary", m_poll.bus_no);
        return;
        }
      res = m_polls.NextPollEntry(m_poll.entry, m_poll.bus_no, m_poll.ticker, m_poll_state);
      }
    }
  switch (res)
    {
    case OvmsNextPollResult::Ignore:
      IFTRACE(Poller) ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend: Ignore", m_poll.bus_no);
      break;
    case OvmsNextPollResult::NotReady:
      {
      IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Poller Not Ready", m_poll.bus_no);
      m_poll_run_finished = true;
      break;
      }
    case OvmsNextPollResult::ReachedEnd:
      {
      IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Poller Reached End", m_poll.bus_no);
      m_poll_run_finished = true;
      break;
      }
    case OvmsNextPollResult::StillAtEnd:
      if (!m_poll_run_finished)
        {
        IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "Poller Reached End(!)", m_poll.bus_no);
        m_poll_run_finished = true;
        }
      break;
    case OvmsNextPollResult::FoundEntry:
      {
      IFTRACE(Poller) 
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend(%s)[%" PRIu8 "]: entry at[type=%02X, pid=%X], ticker=%" PRIu32 ", wait=%u, cnt=%u/%u",
             m_poll.bus_no, PollerSource(source), m_poll_state, m_poll.entry.type, m_poll.entry.pid,
             m_poll.ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);
      // We need to poll this one...
      m_poll.protocol = m_poll.entry.protocol;
      m_poll.type = m_poll.entry.type;
      m_poll.pid = m_poll.entry.pid;

      m_poll_sent_last = monotonictime;
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
    OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(10));
    if (lock.IsLocked())
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
    m_polls.SetEntry("!v.single", poller, true);
    }

  IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Single Request Sending", m_poll.bus_no);
  Queue_PollerSend(poller_source_t::OnceOff);

  // wait for response:
  IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Single Request Waiting for response", m_poll.bus_no);
  bool rxok = single_rxdone.Take(pdMS_TO_TICKS(timeout_ms));
  IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]PollSingleRequest: Response done ", m_poll.bus_no);
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
  m_parent->QueuePollerSend(source, m_poll.bus_no);
  }

void OvmsPoller::DoPollerSendSuccess( void * pvParamCan, uint32_t ticker ) // Static
  {
  uint8_t can_number = uint32_t(pvParamCan);
  MyPollers.QueuePollerSend(OvmsPoller::poller_source_t::Successful, can_number, ticker);
  }

//: Call when poll Succeeded and no more is expected.
void OvmsPoller::PollerSucceededPollNext()
  {
  // Not expecting any more .. so short-cut the poll receive.
  m_poll.type = VEHICLE_POLL_TYPE_NONE;

  // Immediately send the next poll for this tick if…
  // - we are not waiting for another frame
  // - poll throttling is unlimited or limit isn't reached yet
  if (m_poll_wait == 0 && CanPoll())
    {
    Queue_PollerSendSuccess();
    }
  }

void OvmsPoller::Queue_PollerSendSuccess()
  {
  if (m_poll_between_success == 0)
    m_parent->QueuePollerSend(OvmsPoller::poller_source_t::Successful, m_poll.bus_no);
  else
    {
    xTimerPendFunctionCall(OvmsPoller::DoPollerSendSuccess,(void *)(uint32_t)m_poll.bus_no, m_poll.ticker, m_poll_between_success);
    }
  }

/** Add a polling requester to list. Replaces any existing entry with that name.
  @param name Unique name/identifier of poll series.
  @param series Shared pointer to poller instance.
  */
bool OvmsPoller::PollRequest(const std::string &name, const std::shared_ptr<PollSeriesEntry> &series, int timeout_ms )
  {
  OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(timeout_ms));
  if (!lock.IsLocked())
    {
    ESP_LOGE(TAG, "[%" PRIu8 "] Failed to add Poll Request '%s'", m_poll.bus_no, name.c_str());
    return false;
    }
  series->SetParentPoller(this);
  series->ResetList(OvmsPoller::ResetMode::PollReset);
  m_polls.SetEntry(name, series);
  return true;
  }

/** Remove a polling requester from the list if it exists.
  @param name Unique name/identifier of poll series.
  */
void OvmsPoller::RemovePollRequest(const std::string &name)
  {
  OvmsRecMutexLock lock(&m_poll_mutex, pdMS_TO_TICKS(3000));
  if (!lock.IsLocked())
    {
    ESP_LOGE(TAG, "Failed to Remove Poll Request '%s'", name.c_str());
    return;
    }
  m_polls.RemoveEntry(name);
  }
void OvmsPoller::RemovePollRequestStarting(const std::string &name)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_polls.RemoveEntry(name, true);
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
const char *OvmsPoller::PollerCommand(OvmsPollCommand src, bool brief)
  {
  switch(src)
    {
    case OvmsPollCommand::Pause:       return                   "Pause";
    case OvmsPollCommand::Resume:      return brief ? "Resme" : "Resume";
    case OvmsPollCommand::Throttle:    return brief ? "Thrtl" : "Throttle";
    case OvmsPollCommand::ResponseSep: return brief ? "RspSp" : "RespSep";
    case OvmsPollCommand::Keepalive:   return brief ? "KpAlv" : "Keepalive";
    case OvmsPollCommand::SuccessSep:  return brief ? "SucSp" : "SuccSep";
    case OvmsPollCommand::Shutdown:    return brief ? "Shtdn" : "Shutdown";
    case OvmsPollCommand::ResetTimer:  return brief ? "RstTm" : "ResetTimer";
    }
  return "??";
  }

OvmsPollers::OvmsPollers()
  : m_poll_state(0),
    m_poll_sequence_max(0),
    m_poll_fc_septime(25),
    m_poll_ch_keepalive(60),
    m_poll_between_success(0),
    m_poll_last(0),
    m_pollqueue(nullptr), m_polltask(nullptr),
    m_timer_poller(nullptr),
    m_poll_subticker(0),
    m_poll_tick_ms(1000),
    m_poll_tick_secondary(0),
    m_shut_down(false),
    m_ready(false),
    m_paused(false),
    m_user_paused(false),
    m_trace(trace_Off),
    m_filtered(false)

  {
  ESP_LOGI(TAG, "Initialising Poller (7000)");
  for (int idx = 0; idx < VEHICLE_MAXBUSSES; ++idx)
    {
    m_pollers[idx] = nullptr;
    m_canbusses[idx].can = nullptr;
    m_canbusses[idx].auto_poweroff = BusPoweroff::None;
    }

  m_overflow_count[0] = 0;
  m_overflow_count[1] = 0;

  m_poll_txcallback = std::bind(&OvmsPollers::PollerTxCallback, this, _1, _2);

  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsPollers::Ticker1, this, _1, _2));
  MyCan.RegisterCallback(TAG, std::bind(&OvmsPollers::PollerRxCallback, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.shuttingdown",std::bind(&OvmsPollers::EventSystemShuttingDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsPollers::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsPollers::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "vehicle.charge.start", std::bind(&OvmsPollers::VehicleChargeStart, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "vehicle.on", std::bind(&OvmsPollers::VehicleOn, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "vehicle.charge.stop", std::bind(&OvmsPollers::VehicleChargeStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "vehicle.off", std::bind(&OvmsPollers::VehicleOff, this, _1, _2));

  OvmsCommand* cmd_poller = MyCommandApp.RegisterCommand("poller","OBD polling framework",vehicle_poller_status);
  cmd_poller->RegisterCommand("status","OBD Polling Status",vehicle_poller_status);
  cmd_poller->RegisterCommand("pause","Pause OBD Polling",vehicle_pause_on);
  cmd_poller->RegisterCommand("resume","Resume OBD Polling",vehicle_pause_off);

  OvmsCommand* cmd_trace = cmd_poller->RegisterCommand("trace","OBD Poller Verbose Logging");
  cmd_trace->RegisterCommand("on","Turn verbose logging ON for poller loop",vehicle_poller_trace);
  cmd_trace->RegisterCommand("txrx","Turn verbose logging ON for txrx queuing",vehicle_poller_trace);
  cmd_trace->RegisterCommand("all","Turn verbose logging ON",vehicle_poller_trace);
  cmd_trace->RegisterCommand("off","Turn verbose logging OFF",vehicle_poller_trace);
  cmd_trace->RegisterCommand("status","Show status for poller loop logging",vehicle_poller_trace);
  OvmsCommand* cmd_times = cmd_poller->RegisterCommand("times","OBD Poll-Time Tracing");
  cmd_times->RegisterCommand("on","Turn on Poll-Time Tracing",poller_times);
  cmd_times->RegisterCommand("off","Turn off Poll-Time Tracing",poller_times);
  cmd_times->RegisterCommand("status","Show timing status",poller_times);
  cmd_times->RegisterCommand("reset","Reset Poll-Time Tracing",poller_times);

#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DuktapeObjectRegistration* dto = new DuktapeObjectRegistration("OvmsPoller");

  dto->RegisterDuktapeFunction(DukOvmsPollerPaused, 0, "GetPaused");
  dto->RegisterDuktapeFunction(DukOvmsPollerUserPaused, 0, "GetUserPaused");
  dto->RegisterDuktapeFunction(DukOvmsPollerPause,  0, "Pause");
  dto->RegisterDuktapeFunction(DukOvmsPollerResume, 0, "Resume");

  dto->RegisterDuktapeFunction(DukOvmsPollerSetTrace, 1, "Trace");
  dto->RegisterDuktapeFunction(DukOvmsPollerGetTrace, 0, "GetTraceStatus");

  // OvmsPoller.Times Sub-object
  DuktapeObjectRegistration* dto_times =
    new DuktapeObjectRegistration("OvmsPollerTimes");

  dto_times->RegisterDuktapeFunction(DukOvmsPollerTimesGetStarted, 0, "GetStarted");
  dto_times->RegisterDuktapeFunction(DukOvmsPollerTimesStart, 0, "Start");
  dto_times->RegisterDuktapeFunction(DukOvmsPollerTimesStop, 0, "Stop");
  dto_times->RegisterDuktapeFunction(DukOvmsPollerTimesReset, 0, "Reset");
  dto_times->RegisterDuktapeFunction(DukOvmsPollerTimesGetStatus, 0, "GetStatus");

  dto->RegisterDuktapeObject(dto_times, "Times");

  MyDuktape.RegisterDuktapeObject(dto);
#endif

  if (MyConfig.ismounted())
    LoadPollerTimerConfig();
  }

OvmsPollers::~OvmsPollers()
  {
  MyEvents.DeregisterEvent(TAG);

  auto pqueue = Atomic_GetAndNull(m_pollqueue);
  if (pqueue)
    vQueueDelete(pqueue);

  // Shouldn't be needed.. but just in case.
  auto ptask = Atomic_GetAndNull(m_polltask);
  if (ptask)
    {
    vTaskDelete(ptask);
    ESP_LOGE(TAG, "Poller Shutdown Force-Deleting task");
    }
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      {
      delete m_pollers[i];
      m_pollers[i] = nullptr;
      }
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
  ESP_LOGD(TAG, "Poller Shutdown Sending Shut-Down");

  MyCan.DeregisterCallback(TAG);

  if (m_timer_poller)
    {
    xTimerDelete( m_timer_poller, 0);
    m_timer_poller = NULL;
    }

  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      m_pollers[i]->ClearPollList();
    }

  bool autoOff = MyConfig.GetParamValueBool("vehicle", "can.autooff", true);
  ESP_LOGV(TAG, "Poller Shutdown Powering Down Busses%s", autoOff ? "" : " (disabled)");
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    auto &can_info = m_canbusses[i];
    switch (can_info.auto_poweroff)
      {
      case BusPoweroff::Vehicle:
      case BusPoweroff::System:
        if (can_info.can != nullptr)
          {
          if (autoOff)
            can_info.can->SetPowerMode(Off);
          can_info.auto_poweroff = BusPoweroff::None;
          }
        break;

      case BusPoweroff::None:
        break;
      }
    }

  ESP_LOGV(TAG, "Poller Shutdown Finished");
  }

void OvmsPollers::ShuttingDownVehicle()
  {
  bool autoOff = MyConfig.GetParamValueBool("vehicle", "can.autooff", true);
  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
      // Remove All pollers starting with "!v."
    if (m_pollers[i])
      m_pollers[i]->RemovePollRequestStarting("!v.");
    // Power down pollers started from the 'vehicle'
    bus_info_t &info = m_canbusses[i];
    if (info.can && info.auto_poweroff == BusPoweroff::Vehicle )
      {
      if (autoOff)
        info.can->SetPowerMode(Off);
      info.auto_poweroff = BusPoweroff::None;
      }
    }
  }

uint8_t OvmsPollers::GetBusNo(canbus* bus) const
  {
  for (int i = 0; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (bus == m_canbusses[i].can)
      return i+1;
    }
  return 0;
  }
canbus* OvmsPollers::GetBus(uint8_t busno) const
  {
  if (busno <1 || busno > VEHICLE_MAXBUSSES)
    return nullptr;
  return m_canbusses[busno-1].can;
  }
canbus* OvmsPollers::RegisterCanBus(int busno, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile, BusPoweroff autoPower)
  {

  canbus *bus = nullptr;
  OvmsPollers::RegisterCanBus(busno, mode, speed, dbcfile, autoPower, bus,0, nullptr);
  return bus;
  }

esp_err_t OvmsPollers::RegisterCanBus(int busno, CAN_mode_t mode, CAN_speed_t speed, dbcfile* dbcfile, BusPoweroff autoPower, canbus*& bus,int verbosity, OvmsWriter* writer )
  {
  bus = nullptr;
  if (m_shut_down)
    {
    if (writer)
      writer->puts("Error: Pollers shut down");
    return ESP_FAIL;
    }
  if (busno <1 || busno > VEHICLE_MAXBUSSES)
    {
    if (writer)
      writer->puts("Error: CAN bus number out of range");
    return ESP_FAIL;
    }

  bus_info_t &info = m_canbusses[busno-1];
  if (!info.can)
    {
    std::string busname = string_format("can%d", busno);
    info.can = (canbus*)MyPcpApp.FindDeviceByName(busname.c_str());
    if (!info.can)
      {
      if (writer)
        writer->puts("Error: Cannot find named CAN bus");
      return ESP_FAIL;
      }
    }
  bus = info.can;
  info.auto_poweroff = autoPower;
  info.can->SetPowerMode(On);
  return info.can->Start(mode,speed,dbcfile);
  }

void OvmsPollers::PowerDownCanBus(int busno)
  {
  canbus *bus = GetBus(busno);
  if(bus)
    bus->SetPowerMode(Off);
  }

void OvmsPollers::Ticker1(std::string event, void* data)
  {
  PollerResetThrottle();
  }

void OvmsPollers::EventSystemShuttingDown(std::string event, void* data)
  {
  if (m_shut_down)
    return;
  m_shut_down = true;

  // Close down standard events.
  MyEvents.DeregisterEvent(TAG);
  MyBoot.ShutdownPending(TAG);
  // Register a special shut-down event to check shut-down
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&OvmsPollers::Ticker1_Shutdown, this, _1, _2));
  if (Atomic_Get(m_polltask))
    {

    // Send a Shutdown command to the front of the queue
    OvmsPoller::poll_queue_entry_t entry;
    entry.entry_type = OvmsPoller::OvmsPollEntryType::Command;
    entry.entry_Command.cmd = OvmsPoller::OvmsPollCommand::Shutdown;
    entry.entry_Command.parameter = 0;
    xQueueSendToFront(m_pollqueue, &entry, 0);
    }
  }
void OvmsPollers::Ticker1_Shutdown(std::string event, void* data)
  {
  if (Atomic_Get(m_polltask) == nullptr)
    {
    MyEvents.DeregisterEvent(TAG);
    MyBoot.ShutdownReady(TAG);
    }
  }

void OvmsPollers::VehicleOn(std::string event, void* data)
  {
  IFTRACE(Times)
    MyPollers.PollerTimesReset();
  }
void OvmsPollers::VehicleChargeStart(std::string event, void* data)
  {
  IFTRACE(Times)
    MyPollers.PollerTimesReset();
  }
void OvmsPollers::VehicleOff(std::string event, void* data)
  {
  NotifyPollerTrace();
  }
void OvmsPollers::VehicleChargeStop(std::string event, void* data)
  {
  NotifyPollerTrace();
  }

void OvmsPollers::NotifyPollerTrace()
  {
  if (!IsTracingTimes())
    return;

  times_trace_t trace;
  if (!LoadTimesTrace( Permille, trace))
    return;

  // *-LOG-PollStats,0,"type","count_hz","avg_util_pm","peak_util_pm","avg_time_ms","peak_time_ms"

  uint32_t ident = 0;
  for (auto it = trace.items.begin(); it != trace.items.end(); ++it)
    {
    ++ident;
    std::string s = string_format(
      "*-LOG-PollStats,%" PRIu32 ",86400,\"%s\",%.2f,%.3f,%.3f,%.3f,%.3f\n",
        ident, it->desc.c_str(), it->avg_n, it->avg_utlzn_ms,  it->max_time, it->avg_time, it->max_val);
    MyNotify.NotifyString("data", "log.pollstats", s.c_str());
    }
  }

void OvmsPollers::ConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* param = (OvmsConfigParam*) data;
  if (!param || param->GetName() == "log")
    LoadPollerTimerConfig();
  }

void OvmsPollers::LoadPollerTimerConfig()
  {
  if (MyConfig.GetParamValueBool("log", "poller.timers", false))
    MyPollers.m_trace |= trace_Times;
  else
    MyPollers.m_trace &= ~trace_Times;
  }

/**
 * PollerTxCallback: internal: process poll request callbacks
 */
void OvmsPollers::PollerTxCallback(const CAN_frame_t* frame, bool success)
  {
  Queue_PollerFrame(*frame, success, true);
  }

/**
 * PollerRxCallback: internal: process poll request callbacks
 */
void OvmsPollers::PollerRxCallback(const CAN_frame_t* frame, bool success)
  {
  if (m_filtered)
    {
    OvmsMutexLock lock(&m_filter_mutex, 0); // Don't block! (ever)
    // If not locked, just let it through.
    if (lock.IsLocked() && !m_filter.IsFiltered(frame))
      return;
    }
  Queue_PollerFrame(*frame, success, false);
  }

void OvmsPollers::ClearFilters()
  {
  m_filtered = false;
  OvmsMutexLock lock(&m_filter_mutex);
  m_filter.ClearFilters();
  }
void OvmsPollers::AddFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  OvmsMutexLock lock(&m_filter_mutex);
  m_filter.AddFilter(bus, id_from, id_to);
  m_filtered = true;
  }
void OvmsPollers::AddFilter(const char* filterstring)
  {
  OvmsMutexLock lock(&m_filter_mutex);
  m_filter.AddFilter(filterstring);
  m_filtered = true;
  }
bool OvmsPollers::RemoveFilter(uint8_t bus, uint32_t id_from, uint32_t id_to)
  {
  OvmsMutexLock lock(&m_filter_mutex);
  auto res = m_filter.RemoveFilter(bus, id_from, id_to);
  m_filtered = m_filter.HasFilters();
  return res;
  }

static void OvmsVehiclePollTicker(TimerHandle_t xTimer )
  {
  OvmsPollers *pollers = (OvmsPollers *)pvTimerGetTimerID(xTimer);
  if (pollers)
    pollers->VehiclePollTicker();
  }

void OvmsPollers::VehiclePollTicker()
  {
  int32_t cur_ticker = ++m_poll_subticker;
  if (cur_ticker >= m_poll_tick_secondary)
    m_poll_subticker = 0;

  OvmsPoller::poller_source_t src;

  // So first tick is Primary.
  src = (cur_ticker == 1) ? OvmsPoller::poller_source_t::Primary : OvmsPoller::poller_source_t::Secondary;

  QueuePollerSend(src);
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

  if (!Atomic_Get(m_pollqueue))
    {
    OvmsRecMutexLock lock(&m_poller_mutex);
    if (!m_pollqueue)
      m_pollqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(OvmsPoller::poll_queue_entry_t));
    }
  if (!Atomic_Get(m_polltask))
    {
    if (!Ready())
      return;
    OvmsRecMutexLock lock(&m_poller_mutex);
    if (!m_polltask)
      xTaskCreatePinnedToCore(OvmsPollerTask, "OVMS Vehicle Poll",
        CONFIG_OVMS_VEHICLE_RXTASK_STACK, (void*)this, 10, &m_polltask, CORE(1));
    }
  if (!m_timer_poller)
    {
    // default to 1 second
    OvmsRecMutexLock lock(&m_poller_mutex);
    if (!m_timer_poller)
      {
      m_timer_poller = xTimerCreate("Vehicle OBD Poll Ticker",
          pdMS_TO_TICKS(m_poll_tick_ms),pdTRUE,this,
          OvmsVehiclePollTicker);
      xTimerStart(m_timer_poller, 0);
      }
    }
  }

void OvmsPollers::PollSetTicker(uint16_t tick_time_ms, uint8_t secondary_ticks)
  {
  ESP_LOGD(TAG, "Set Poll Ticker Timer %dms * %d", tick_time_ms, secondary_ticks);
  if (!m_timer_poller)
    m_poll_tick_ms = tick_time_ms;
  else if (m_poll_tick_ms != tick_time_ms)
    {
    if (xTimerChangePeriod(m_timer_poller, pdMS_TO_TICKS(tick_time_ms), 0) == pdPASS)
      {
      m_poll_tick_ms = tick_time_ms;
      }
    else
      ESP_LOGE(TAG, "Poll Ticker Timer period not changed");
    xTimerReset(m_timer_poller, 0);
    }
  m_poll_tick_secondary = secondary_ticks;
  m_poll_subticker = 0;
  }

void OvmsPollers::OvmsPollerTask(void *pvParameters)
  {
  OvmsPollers *me = (OvmsPollers*)pvParameters;
  me->PollerTask();
  }

void OvmsPollers::average_value_t::catchup(uint64_t time_added)
  {
  if (time_added > next_end)
    {
    if (next_end == 0)
      {
      next_end = time_added + average_sep_mic_s;
      return;
      }
    do
      {
      next_end += average_sep_mic_s;
      avg_n.push();
      avg_utlzn.push();
      } while (time_added > next_end);
    }
  }
void OvmsPollers::average_value_t::add_time(uint32_t time_spent, uint64_t time_added)
  {
  uint32_t timesum = avg_utlzn.sum();
  catchup(time_added);
  avg_time.add(time_spent);
  avg_utlzn.add(time_spent);
  avg_n.add(100);
  if (time_spent > max_val)
    max_val = time_spent;
  if (timesum > max_time )
    max_time = timesum;
  }

bool OvmsPollers::poller_key_less_t::operator()(const poller_key_t &lhs, const poller_key_t &rhs) const
  {
  if (lhs.entry_type != rhs.entry_type)
    return lhs.entry_type < rhs.entry_type;
  switch (lhs.entry_type)
    {
    case OvmsPoller::OvmsPollEntryType::FrameRx:
    case OvmsPoller::OvmsPollEntryType::FrameTx:
      {
      if (lhs.busnumber != rhs.busnumber)
        return lhs.busnumber < rhs.busnumber;
      return lhs.Frame_MsgId < rhs.Frame_MsgId;
      }
    case OvmsPoller::OvmsPollEntryType::Poll:
      {
      if (lhs.busnumber != rhs.busnumber)
        return lhs.busnumber < rhs.busnumber;
      return lhs.Poll_src < rhs.Poll_src;
      }
    case OvmsPoller::OvmsPollEntryType::Command:
      return lhs.Command_cmd < rhs.Command_cmd;
    case OvmsPoller::OvmsPollEntryType::PollState:
      return false;
    default:
      return false;
    }
  }

OvmsPollers::poller_key_st::poller_key_st( const OvmsPoller::poll_queue_entry_t &entry)
  {
  busnumber = 0;
  entry_type = entry.entry_type;
  switch (entry.entry_type)
    {
    case OvmsPoller::OvmsPollEntryType::FrameRx:
    case OvmsPoller::OvmsPollEntryType::FrameTx:
      busnumber = entry.entry_FrameRxTx.frame.origin->m_busnumber+1;
      Frame_MsgId = entry.entry_FrameRxTx.frame.MsgID;
      break;
    case OvmsPoller::OvmsPollEntryType::Poll:
      busnumber = entry.entry_Poll.busno;
      Poll_src = entry.entry_Poll.source;
      break;
    case OvmsPoller::OvmsPollEntryType::Command:
      Command_cmd = entry.entry_Command.cmd;
      break;
    default:
      ;
    }
  }

void OvmsPollers::PollerTask()
  {
  OvmsPoller::poll_queue_entry_t entry;
  while (true)
    {
    if ( m_shut_down )
      {
      ShuttingDown();
      break;
      }
    if (xQueueReceive(m_pollqueue, &entry, (portTickType)portMAX_DELAY)!=pdTRUE)
      continue;

    IFTRACE(Times)
      {
      if (entry.entry_type == OvmsPoller::OvmsPollEntryType::PollState)
        {
        // Make sure the every second (ish), we have caught up on averages-by-period measurements.
        uint64_t curtime = esp_timer_get_time();
        OvmsMutexLock lock(&m_stats_mutex);
        for (auto it = m_poll_time_stats.begin(); it != m_poll_time_stats.end(); ++it)
          it->second.catchup(curtime);
        }
      }
    // A couple of special cases.
    if (entry.entry_type == OvmsPoller::OvmsPollEntryType::Command)
      {
      if (entry.entry_Command.cmd == OvmsPoller::OvmsPollCommand::Shutdown)
        {
        ShuttingDown();
        break;
        }
      if (entry.entry_Command.cmd == OvmsPoller::OvmsPollCommand::ResetTimer)
        {
        OvmsMutexLock lock(&m_stats_mutex);
        if (entry.entry_Command.parameter == 2)
          {
          for (auto it = m_poll_time_stats.begin(); it != m_poll_time_stats.end(); ++it)
            it->second.paused();
          }
        else
          {
          for (auto it = m_poll_time_stats.begin(); it != m_poll_time_stats.end(); ++it)
            it->second.reset();
          if (entry.entry_Command.parameter == 1)
            {
            // Tracing back on.
            MyPollers.m_trace |= trace_Times;
            }
          }
        continue;
        }
      }
    if (m_shut_down)
      {
      ShuttingDown();
      break;
      }

    std::unique_ptr<timer_util_t> timer;

    IFTRACE(Times)
      timer = std::unique_ptr<timer_util_t>(new timer_util_t(
        [&entry,this](uint64_t start, uint64_t finish)
          {
            {
            int32_t diff = finish-start;
            poller_key_t key(entry);
            OvmsMutexLock lock(&m_stats_mutex);
            m_poll_time_stats[key].add_time(diff, finish);
            }
          }
        ));

    m_poll_last = monotonictime;
    switch (entry.entry_type)
      {
      case OvmsPoller::OvmsPollEntryType::FrameRx:
        {
        bool processed = false;
        canbus* bus = entry.entry_FrameRxTx.frame.origin;
        auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
        IFTRACE(Poller) ESP_LOGV(TAG, "Pollers: FrameRx(bus=%d)", GetBusNo(entry.entry_FrameRxTx.frame.origin));
        if (poller)
          processed = poller->Incoming(entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
        PollerFrameRx(entry.entry_FrameRxTx.frame);

        if (!processed) // Not processed by poller.
          {
          dbcfile* dbc = bus->GetDBC();
          if (dbc != nullptr)
            {
            CAN_frame_t &frame = entry.entry_FrameRxTx.frame;
            dbc->DecodeSignal(frame.FIR.B.FF, frame.MsgID, frame.data.u8, 8);
            }
          }
        }
        break;
      case OvmsPoller::OvmsPollEntryType::FrameTx:
        {
        auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
        IFTRACE(Poller) ESP_LOGV(TAG, "Pollers: FrameTx(bus=%d)", GetBusNo(entry.entry_FrameRxTx.frame.origin));
        if (poller)
          poller->Outgoing(entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
        }
        break;
      case OvmsPoller::OvmsPollEntryType::Poll:
        {
        for (int istx = 0; istx < 2; ++istx)
          {
          uint32_t ovf_count = Atomic_Get(m_overflow_count[istx]);
          if (ovf_count > 0)
            {
            ESP_LOGI(TAG, "Poller[Frame]: %s Task Queue Overflow Run %" PRIu32, ( istx ? "TX" : "RX"), ovf_count);
            Atomic_Subtract( m_overflow_count[istx], ovf_count);
            }
          }
        if (!m_user_paused && !m_paused)
          {
          // Must not lock mutex while calling.
          IFTRACE(Poller) ESP_LOGV(TAG, "[%" PRIu8 "]Poller: Send(%s)", entry.entry_Poll.busno, OvmsPoller::PollerSource(entry.entry_Poll.source));
          if (entry.entry_Poll.busno != 0)
            {
            auto bus = GetBus(entry.entry_Poll.busno);
            auto poller = GetPoller(bus);
            if (poller)
              {
              if ((entry.entry_Poll.poll_ticker == 0) || (poller->m_poll.ticker == entry.entry_Poll.poll_ticker))
                poller->PollerSend(entry.entry_Poll.source);
              }
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
          }
        }
        break;
      case OvmsPoller::OvmsPollEntryType::Command:
        switch (entry.entry_Command.cmd)
          {
          case OvmsPoller::OvmsPollCommand::Shutdown:
            break; // Shouldn't get here.
          case OvmsPoller::OvmsPollCommand::Pause:
            {
            if (entry.entry_Command.parameter)
              {
              ESP_LOGD(TAG, "Pollers: Command Pause (user)");
              m_user_paused = true;
              }
              else
              {
              ESP_LOGD(TAG, "Pollers: Command Pause (system)");
              m_paused = true;
              }
            continue;
            }
          case OvmsPoller::OvmsPollCommand::Resume:
            {
            if (entry.entry_Command.parameter)
              {
              ESP_LOGD(TAG, "Pollers: Command Resume (user)");
              m_user_paused = false;
              }
              else
              {
              ESP_LOGD(TAG, "Pollers: Command Resume (system)");
              m_paused = false;
              }
            continue;
            }
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
          case OvmsPoller::OvmsPollCommand::SuccessSep:
            if (entry.entry_Command.parameter != m_poll_between_success)
              {
              m_poll_between_success = entry.entry_Command.parameter;
              OvmsRecMutexLock lock(&m_poller_mutex);
              for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
                {
                if (m_pollers[i])
                  m_pollers[i]->PollSetTimeBetweenSuccess(m_poll_between_success);
                }
              }
            break;
          case OvmsPoller::OvmsPollCommand::ResetTimer:
            break;//triggered above
          }
        break;
      case OvmsPoller::OvmsPollEntryType::PollState:
        {
        if (entry.entry_PollState.bus)
          {
          ESP_LOGD(TAG, "Pollers: PollState(%" PRIu8 ",%" PRIu8 ")", entry.entry_PollState.new_state, GetBusNo(entry.entry_PollState.bus));
          OvmsRecMutexLock lock(&m_poller_mutex);
          for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
            {
            auto cur = m_pollers[i];
            if (cur && cur->HasBus(entry.entry_PollState.bus) )
              cur->Do_PollSetState(entry.entry_PollState.new_state);
            }
          }
        else
          {
          ESP_LOGD(TAG, "Pollers: PollState(%" PRIu8 ")", entry.entry_PollState.new_state);
          OvmsRecMutexLock lock(&m_poller_mutex);
          for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
            {
            auto cur = m_pollers[i];
            if (cur)
              cur->Do_PollSetState(entry.entry_PollState.new_state);
            }
          }
        }
        break;
      }
    }

  auto task = Atomic_GetAndNull(m_polltask);
  ESP_LOGD(TAG, "Pollers: Shutdown %s", task ? "null" : "OK");

  if (task)
    vTaskDelete(task);
  // Wait to die.
  vTaskSuspend(nullptr);
  }

OvmsPoller *OvmsPollers::GetPoller(canbus *can, bool force)
  {
  if (m_shut_down || !can)
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

    int busno = GetBusNo(can);
    if (!busno)
      return nullptr;

    if (m_shut_down)
      return nullptr;
    IFTRACE(Poller) ESP_LOGD(TAG, "GetPoller - create Poller ( busno=%" PRIu8 ")", busno);
    auto newpoller =  new OvmsPoller(can, busno, this, m_poll_txcallback);
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

void OvmsPollers::QueuePollerSend(OvmsPoller::poller_source_t src, uint8_t busno, uint32_t pollticker)
  {
  if (m_shut_down)
    return;
  if (!Ready())
    return;
  if (!m_pollqueue)
    return;
  // Sends poller send message
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = OvmsPoller::OvmsPollEntryType::Poll;
  entry.entry_Poll.busno = busno;
  entry.entry_Poll.source = src;
  entry.entry_Poll.poll_ticker = pollticker;

  IFTRACE(TXRX) ESP_LOGV(TAG, "Pollers: Queue PollerSend(%s, %" PRIu8 ")", OvmsPoller::PollerSource(src), busno);
  if (xQueueSend(m_pollqueue, &entry, pdMS_TO_TICKS(50)) != pdPASS)
    ESP_LOGI(TAG, "Pollers[Send]: Task Queue Overflow");
  }

void OvmsPollers::PollSetPidList(canbus* defbus, const OvmsPoller::poll_pid_t* plist,
  OvmsPoller::VehicleSignal *signal)
  {
  if (m_shut_down)
    return;
  uint8_t defbusno = !defbus ? 0 : GetBusNo(defbus);
  bool hasbus[1+VEHICLE_MAXBUSSES];
  for (int i = 0 ; i <= VEHICLE_MAXBUSSES; ++i)
    hasbus[i] = false;

  // Load up any bus pollers required.
  if (!plist)
    ESP_LOGD(TAG, "PollSetPidList - Setting NULL List");
  else if (plist->txmoduleid == 0) // Check for an Empty list.
    {
    plist = nullptr;
    ESP_LOGD(TAG, "PollSetPidList - Setting Empty List");
    }
  else
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
        GetPoller(GetBus(usebusno), true);
        }
      }
    }

  if (m_shut_down)
    return;
  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    auto poller = m_pollers[i];
    if (poller)
      {
      uint8_t busno = poller->m_poll.bus_no;
      if (busno > VEHICLE_MAXBUSSES)
        continue;
      if (hasbus[busno])
        {
        ESP_LOGD(TAG, "PollSetPidList - Setting for bus=%" PRIu8 " defbus=%" PRIu8, busno, defbusno);
        poller->PollSetPidList(defbusno, plist, signal);
        }
      else
        {
        ESP_LOGD(TAG, "PollSetPidList - Clearing for bus=%" PRIu8, busno);
        poller->PollSetPidList(0, nullptr, nullptr );
        }
      }
    }
  }

bool OvmsPollers::PollRequest(canbus* defbus, const std::string &name, const std::shared_ptr<OvmsPoller::PollSeriesEntry> &series, int timeout_ms )
  {
  if (defbus == nullptr)
    {
    ESP_LOGE(TAG, "Pollers: Bus not specified getting poller to add '%s'", name.c_str());
    return false;
    }

  if (m_shut_down)
    return false;

  OvmsRecMutexLock lock(&m_poller_mutex, pdMS_TO_TICKS(timeout_ms) );
  if (!lock.IsLocked())
    {
    ESP_LOGE(TAG, "Pollers: Failed to add Poll Request '%s'", name.c_str());
    return false;
    }
  auto poller = GetPoller(defbus, true);
  if (poller == nullptr)
    {
    ESP_LOGE(TAG, "Pollers: Failed to get poller for bus %d to add '%s'", defbus->m_busnumber, name.c_str());
    return false;
    }
  return poller->PollRequest(name, series, timeout_ms);
  }

void OvmsPollers::PollRemove(canbus* defbus, const std::string &name)
  {
  OvmsRecMutexLock lock(&m_poller_mutex);
  if (defbus)
    {
    auto poller = GetPoller(defbus, false);
    if (poller)
      poller->RemovePollRequest(name);
    }
  else
    {
    for (int i = 0 ; i < VEHICLE_MAXBUSSES ; ++i)
      {
      auto poller = m_pollers[i];
      if (poller)
        poller->RemovePollRequest(name);
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
  if (!m_pollqueue || m_shut_down)
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
  if (m_shut_down)
    return;
  if (!Ready())
    return;
  if (!m_pollqueue)
    return;
  // Queues the frame
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = istx ? OvmsPoller::OvmsPollEntryType::FrameTx : OvmsPoller::OvmsPollEntryType::FrameRx;
  entry.entry_FrameRxTx.frame = frame;
  entry.entry_FrameRxTx.success = success;

  IFTRACE(TXRX) ESP_LOGV(TAG, "Poller: Queue PollerFrame(%s, %s)", (success ? "OK" : "Fail"), ( istx ? "TX" : "RX") );
  if (xQueueSend(m_pollqueue, &entry, 0) != pdPASS)
    {
    volatile uint32_t &count = m_overflow_count[istx ? 1 : 0];
    Atomic_Increment(count, (uint32_t)1);
    }
  }

void OvmsPollers::PollSetState(uint8_t state, canbus* bus)
  {
  if (m_shut_down)
    return;

  if (!bus)
    {
    if (m_poll_state == state)
      return;
    m_poll_state = state;
    }

  if (!m_pollqueue)
    return;

  // Queues the command
  OvmsPoller::poll_queue_entry_t entry;
  memset(&entry, 0, sizeof(entry));
  entry.entry_type = OvmsPoller::OvmsPollEntryType::PollState;
  entry.entry_PollState.new_state = state;
  entry.entry_PollState.bus = bus;

  ESP_LOGD(TAG, "Pollers: Queue SetState()");
  if (xQueueSend(m_pollqueue, &entry, pdMS_TO_TICKS(500)) != pdPASS)
    ESP_LOGI(TAG, "Pollers[SetState]: Task Queue Overflow");
  }

// signal poller (private)
void OvmsPollers::PollerResetThrottle()
  {
  if (m_shut_down)
    return;
  OvmsRecMutexLock lock(&m_poller_mutex);
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      m_pollers[i]->ResetThrottle();
    }
  }
void OvmsPollers::PollerStatus(int verbosity, OvmsWriter* writer)
  {
  auto curmon = monotonictime;
  if (!HasPollTask())
    {
    writer->puts("OBD Polling task is not running");
    return;
    }
  bool found_list = false;
  for (uint8_t busno = 1; busno <= VEHICLE_MAXBUSSES; ++busno)
    {
    auto bus = GetBus(busno);
    if (!bus)
      continue;
    OvmsPoller *poller = GetPoller(bus, false);
    if (!poller)
      continue;
    bool has_active = poller->HasPollList();
    uint8_t state = poller->PollState();
    found_list = true;
    writer->printf("Poller on Can%" PRIu8 "\n", busno);
    writer->printf("  List: %s\n", (has_active ? "active" : "not active") );
    if (has_active)
      writer->printf("  State: %" PRIu8 "\n", state);

    writer->printf("  Last Request: ");
    auto last = poller->m_poll_sent_last;
    if (last == 0)
      writer->puts("None");
    else
      writer->printf("%" PRIu32 "s (ticks)\n", (curmon - last));
    }
  if (!found_list)
    {
    writer->puts("No OBD Pollers active.");
    return;
    }
  writer->printf("Time between polling ticks: %" PRIu16 "ms\n", m_poll_tick_ms);
 if (m_poll_tick_secondary > 0)
   writer->printf("Secondary ticks: %" PRIu8 ".\n",  m_poll_tick_secondary);
  auto last = LastPollCmdReceived();
  writer->printf("Last Poll Received: ");
  if (last == 0)
    writer->puts("none");
  else
    writer->printf("%" PRIu32 "s (ticks) ago\n", (curmon-last));
  if (m_pollqueue)
    {
    auto waiting = uxQueueMessagesWaiting(m_pollqueue);
    writer->printf("Poll Queue Length: %d\n", waiting);
    }

  if (IsPaused() || IsUserPaused())
    writer->printf("OBD polling is Paused %s%s\n", IsPaused() ? "[system]":"", IsUserPaused() ? "[user]" : "");
  else
    writer->puts("OBD polling is Resumed");
  }

void OvmsPollers::SetUserPauseStatus(bool paused, int verbosity, OvmsWriter* writer)
  {
  if (paused)
    {
    PausePolling(true);
    writer->puts("Vehicle OBD Polling Pause Requested");
    }
  else
    {
    ResumePolling(true);
    writer->puts("Vehicle OBD Polling Resume Requested");
    }
  }

void OvmsPollers::vehicle_poller_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPollers.PollerStatus(verbosity, writer);
  }
void OvmsPollers::vehicle_pause_on(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPollers.SetUserPauseStatus(true, verbosity, writer);
  }

void OvmsPollers::vehicle_pause_off(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyPollers.SetUserPauseStatus(false, verbosity, writer);
  }

void OvmsPollers::vehicle_poller_trace(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(), "on") == 0)
    MyPollers.m_trace |= trace_Poller;
  else if (strcmp(cmd->GetName(), "txrx") == 0)
    MyPollers.m_trace |= trace_TXRX;
  else if (strcmp(cmd->GetName(), "all") == 0)
    MyPollers.m_trace |= trace_All;
  else if (strcmp(cmd->GetName(), "off") == 0)
    MyPollers.m_trace &= ~trace_All;
  //"status" falls through here.

  writer->printf("Vehicle OBD poller tracing: %s%s%s\n",
      (MyPollers.m_trace & trace_Poller) ? "+poll" : "",
      (MyPollers.m_trace & trace_TXRX) ? "+txrx" : "",
      (MyPollers.m_trace & trace_All) ? "" : "off");
  }

void OvmsPollers::poller_times(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (strcmp(cmd->GetName(), "on") == 0)
    {
    MyConfig.SetParamValueBool("log", "poller.timers", true);
    writer->puts("Poller timing is now on");
    }
  else if (strcmp(cmd->GetName(), "off") == 0)
    {
    MyConfig.SetParamValueBool("log", "poller.timers", false);
    writer->puts("Poller timing is now off");
    // Turn off the 'last' value.
    MyPollers.Queue_Command(OvmsPoller::OvmsPollCommand::ResetTimer, 3);
    }
  else if (strcmp(cmd->GetName(), "status") == 0)
    {
    writer->printf("Poller timing is: %s\n",
      (MyPollers.m_trace & trace_Times) ? "on" : "off");
    MyPollers.PollerTimesTrace(writer);
    }
  else if (strcmp(cmd->GetName(), "reset") == 0)
    {
    MyPollers.PollerTimesReset();
    writer->puts("Poller times Reset");
    writer->printf("Poller timing is: %s\n",
      (MyPollers.m_trace & trace_Times) ? "on" : "off");
    }
  }
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
// OvmsPoller.GetPaused
duk_ret_t OvmsPollers::DukOvmsPollerPaused(duk_context *ctx)
  {
  duk_push_boolean(ctx, (MyPollers.IsPaused() || MyPollers.IsUserPaused()) ? 1 : 0);
  return 1;
  }
// OvmsPoller.GetUserPaused
duk_ret_t OvmsPollers::DukOvmsPollerUserPaused(duk_context *ctx)
  {
  duk_push_boolean(ctx, MyPollers.IsUserPaused() ? 1 : 0);
  return 1;
  }
// OvmsPoller.Pause
duk_ret_t OvmsPollers::DukOvmsPollerPause(duk_context *ctx)
  {
  MyPollers.PausePolling(true);
  return 0;
  }
// OvmsPoller.Resume
duk_ret_t OvmsPollers::DukOvmsPollerResume(duk_context *ctx)
  {
  MyPollers.ResumePolling(true);
  return 0;
  }

// OvmsPoller.SetTrace
duk_ret_t OvmsPollers::DukOvmsPollerSetTrace(duk_context *ctx)
  {
  uint8_t newval;
  if (duk_is_string(ctx, 0))
    {
    std::string mode(duk_to_string(ctx,0));
    if (mode == "on")
      newval = trace_Poller;
    else if (mode == "off")
      newval = 0;
    else if (mode == "txrx")
      newval = trace_TXRX;
    else if (mode == "all")
      newval = trace_All;
    else
      return 0;
    }
  else if (duk_is_object(ctx, 0))
    {
    newval = 0;
    if (duk_get_prop_string(ctx, 0, "poller"))
      {
      if (duk_get_boolean(ctx, -1))
        newval |= trace_Poller;
      duk_pop(ctx);
      }
    if (duk_get_prop_string(ctx, 0, "txrx"))
      {
      if (duk_get_boolean(ctx, -1))
        newval |= trace_TXRX;
      duk_pop(ctx);
      }
    }
  else
    return 0;
  // Set the pollers log only
  MyPollers.m_trace = (MyPollers.m_trace & ~trace_All) | newval;
  return 0;

  }
// OvmsPoller.GetTrace
duk_ret_t OvmsPollers::DukOvmsPollerGetTrace(duk_context *ctx)
  {
  uint8_t val = MyPollers.m_trace & trace_All;

  duk_push_object(ctx);
  duk_push_boolean(ctx, val & trace_Poller ? 1 : 0);
  duk_put_prop_string(ctx, -2, "poller");
  duk_push_boolean(ctx, val & trace_TXRX ? 1 : 0);
  duk_put_prop_string(ctx, -2, "txrx");

  return 1;
  }

// OvmsPoller.Times.GetStarted
duk_ret_t OvmsPollers::DukOvmsPollerTimesGetStarted(duk_context *ctx)
  {
  bool enabled = MyConfig.GetParamValueBool("log", "poller.timers", false);
  duk_push_boolean(ctx, enabled?1:0);
  return 1;
  }
// OvmsPoller.Times.Start
duk_ret_t OvmsPollers::DukOvmsPollerTimesStart(duk_context *ctx)
  {
  MyConfig.SetParamValueBool("log", "poller.timers", true);
  return 0;
  }
// OvmsPoller.Times.Stop
duk_ret_t OvmsPollers::DukOvmsPollerTimesStop(duk_context *ctx)
  {
  MyConfig.SetParamValueBool("log", "poller.timers", false);
  // Turn off the 'last' value.
  MyPollers.Queue_Command(OvmsPoller::OvmsPollCommand::ResetTimer, 3);
  return 0;
  }
// OvmsPoller.Times.Reset
duk_ret_t OvmsPollers::DukOvmsPollerTimesReset(duk_context *ctx)
  {
  MyPollers.PollerTimesReset();
  return 0;
  }
// OvmsPoller.Times.GetStatus
duk_ret_t OvmsPollers::DukOvmsPollerTimesGetStatus(duk_context *ctx)
  {
  // array of objects!
  times_trace_t trace;
  if (!MyPollers.LoadTimesTrace( Permille, trace))
    return 0;

  // The return object
  duk_push_object(ctx);

  bool enabled = MyConfig.GetParamValueBool("log", "poller.timers", false);
  duk_push_boolean(ctx, enabled?1:0);
  duk_put_prop_string(ctx, -2, "started");

  // The items object
  duk_push_object(ctx);

  for (auto it = trace.items.begin(); it != trace.items.end(); ++it)
    {
    // The dictionary object of items.
    duk_push_object(ctx);

    duk_push_number(ctx, it->avg_n);
    duk_put_prop_string(ctx, -2, "count_hz");

    duk_push_number(ctx, it->avg_utlzn_ms);
    duk_put_prop_string(ctx, -2, "avg_util_pm");

    duk_push_number(ctx, it->max_time);
    duk_put_prop_string(ctx, -2, "peak_util_pm");

    duk_push_number(ctx, it->avg_time);
    duk_put_prop_string(ctx, -2, "avg_time_ms");

    duk_push_number(ctx, it->max_val);
    duk_put_prop_string(ctx, -2, "peak_time_ms");

    // Add the object with the desc as the key
    duk_put_prop_string(ctx, -2, it->desc.c_str());
    }
  duk_put_prop_string(ctx, -2, "items");

  // Now the totals
  duk_push_number(ctx, trace.tot_n);
  duk_put_prop_string(ctx, -2, "tot_count_hz");

  duk_push_number(ctx, trace.tot_utlzn_ms);
  duk_put_prop_string(ctx, -2, "tot_util_pm");

  duk_push_number(ctx, trace.tot_time);
  duk_put_prop_string(ctx, -2, "tot_time_ms");

  return 1;
  }
#endif

void OvmsPollers::PollerTimesReset()
  {
  bool currLogging = ((MyPollers.m_trace & trace_Times) != 0);
  MyPollers.m_trace &= ~trace_Times;

  Queue_Command(OvmsPoller::OvmsPollCommand::ResetTimer, currLogging?1:0);
  }

bool OvmsPollers::LoadTimesTrace( metric_unit_t ratio_unit, times_trace_t &trace)
  {
  trace.items.clear();

  uint32_t avg_time_sum_us = 0;
  uint32_t avg_utlzn_sum_us = 0;
  uint32_t avg_count_sum = 0;
  std::list<std::pair<poller_key_t, average_value_t> > copy;
  // Lock for the shortest time possible... copy the current values.
  m_stats_mutex.Lock();
  for (const auto &val : m_poll_time_stats)
    copy.push_back(val);
  m_stats_mutex.Unlock();

  for (auto it = copy.begin(); it != copy.end(); ++it)
    {
    average_value_t &cur = it->second;
    uint16_t max_val = cur.max_val;
    if (max_val == 0)
      continue;
    uint16_t avg_100n = cur.avg_n.get();
    uint32_t avg_utlzn_us = cur.avg_utlzn.get();
    uint32_t avg_time = cur.avg_time.get();
    uint32_t max_time = cur.max_time;
    avg_time_sum_us += avg_time;
    avg_utlzn_sum_us += avg_utlzn_us;
    avg_count_sum += avg_100n;

    times_trace_elt_t item;
    switch (it->first.entry_type)
      {
      case OvmsPoller::OvmsPollEntryType::FrameRx:
        item.desc = string_format("RxCan%" PRIu8 "[%03" PRIx32 "]",
            it->first.busnumber, it->first.Frame_MsgId);
        break;
      case OvmsPoller::OvmsPollEntryType::FrameTx:
        item.desc = string_format("TxCan%" PRIu8 "[%03" PRIx32 "]",
            it->first.busnumber, it->first.Frame_MsgId);
        break;
      case OvmsPoller::OvmsPollEntryType::Poll:
        item.desc = string_format("Poll:%s", OvmsPoller::PollerSource(it->first.Poll_src));
        break;
      case OvmsPoller::OvmsPollEntryType::Command:
        item.desc = string_format("Cmd:%s", OvmsPoller::PollerCommand(it->first.Command_cmd, true));
        break;
      case OvmsPoller::OvmsPollEntryType::PollState:
        item.desc = "Cmd:State";
        break;
      default:
        item.desc = "Other";
      }
    // uses 100 as base for precision. cvt from 10s to 1s
    item.avg_n = avg_100n  / (average_sep_s * 100.0);
    // Convert to micro-s per 10s to ms per s (ie permille)
    item.avg_utlzn_ms = UnitConvert( Permille, ratio_unit, avg_utlzn_us / (average_sep_s * 1000.0F));
    item.max_time = UnitConvert(Permille, ratio_unit, max_time / (average_sep_s * 1000.0F));
    item.avg_time = avg_time / 10000.0;
    item.max_val  = max_val  / 1000.0;
    trace.items.push_back(item);
    }
  if (trace.items.size() == 0)
    return false;
  // uses 100 as base for precision. cvt from 10s to 1s (ie permille)
  trace.tot_n = avg_count_sum  / (average_sep_s * 100.0);
  // Convert to micro-s per 10s to ms per s
  trace.tot_utlzn_ms = UnitConvert(Permille, ratio_unit, avg_utlzn_sum_us / (average_sep_s * 1000.0F));
  trace.tot_time = avg_time_sum_us / 1000.0;
  return true;
  }

bool OvmsPollers::PollerTimesTrace( OvmsWriter* writer)
  {
  metric_unit_t ratio_unt = OvmsMetricGetUserUnit(GrpRatio, Permille);
  times_trace_t trace;
  if (!LoadTimesTrace( ratio_unt, trace))
    return false;
  int ratio_dec = (ratio_unt == Percentage) ? 4 : 3;
  writer->puts(  "Type           | count  | Utlztn | Time");
  writer->printf("               | per s  | [%s]    | [ms]\n", OvmsMetricUnitLabel(ratio_unt) );
  for (auto it = trace.items.begin(); it != trace.items.end(); ++it)
    {
      writer->puts( "---------------+--------+--------+---------");
      writer->printf("%-12sAvg|%8.2f|%8.*f|%9.3f\n",
          it->desc.c_str(), it->avg_n, ratio_dec, it->avg_utlzn_ms, it->avg_time);
      writer->printf("           Peak|        |%8.*f|%9.3f\n",
           ratio_dec, it->max_time, it->max_val);
    }
  writer->puts(  "===============+========+========+=========");
  writer->printf("      Total Avg|%8.2f|%8.*f|%9.3f\n",
      trace.tot_n, ratio_dec, trace.tot_utlzn_ms, trace.tot_time);
  return true;
  }

static const char *PollResStr( OvmsPoller::OvmsNextPollResult res)
  {
  switch(res)
    {
    case OvmsPoller::OvmsNextPollResult::NotReady: return "NotReady";
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

void OvmsPoller::PollSeriesList::SetEntry(const std::string &name, std::shared_ptr<OvmsPoller::PollSeriesEntry> series, bool blocking)
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

  IFTRACE(Poller) ESP_LOGV(TAG, "Poll List: Removing series %s", iter->name.c_str());
  if (iter->series != nullptr)
     iter->series->Removing();
  iter->series = nullptr;
  IFTRACE(Poller) ESP_LOGV(TAG, "Poll List: Deleting series %s", iter->name.c_str());
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

void OvmsPoller::PollSeriesList::RemoveEntry( const std::string &name, bool starts_with)
  {
  for (poll_series_t *it = m_first; it != nullptr; )
    {
    bool found;
    if (starts_with)
      found = startsWith(it->name, name);
    else
      found = it->name == name;

    poll_series_t *thisone=it;
    it = it->next;
    if (found)
      {
      Remove(thisone);
      if (!starts_with)
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
    IFTRACE(Poller) ESP_LOGD(TAG, "Poll List:[%s] IncomingPacket TYPE:%x PID: %03x LEN: %d REM: %d ", m_iter->name.c_str(), job.type, job.pid, length, job.mlremain);

    m_iter->series->IncomingPacket(job, data, length);
    }
  else
    {
    IFTRACE(Poller) ESP_LOGD(TAG, "Poll List:[Discard] IncomingPacket TYPE:%x PID: %03x LEN: %d REM: %d ", job.type, job.pid, length, job.mlremain);
    }
  }

// Process An Error
void OvmsPoller::PollSeriesList::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if ((m_iter != nullptr) && (m_iter->series != nullptr))
    {
    IFTRACE(Poller) ESP_LOGD(TAG, "Poll List:[%s] IncomingError TYPE:%x PID: %03x Code: %02X", m_iter->name.c_str(), job.type, job.pid, code);
    m_iter->series->IncomingError(job, code);
    }
  else
    {
    IFTRACE(Poller) ESP_LOGD(TAG, "Poll List:[Discard] IncomingError TYPE:%x PID: %03x Code: %02X", job.type, job.pid, code);
    }
  }

/// Send on an imcoming TX reply
void OvmsPoller::PollSeriesList::IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success)
  {
  if ((m_iter != nullptr) && (m_iter->series != nullptr))
    {
    IFTRACE(TXRX) ESP_LOGV(TAG, "Poll List:[%s] IncomingTXReply TYPE:%x PID: %03x Success: %s", m_iter->name.c_str(), job.type, job.pid, success ? "true" : "false");
    m_iter->series->IncomingTxReply(job, success);
    }
  else
    {
    IFTRACE(TXRX) ESP_LOGV(TAG, "Poll List:[Discard] IncomingTXReply TYPE:%x PID: %03x Success %s", job.type, job.pid, success ? "true" : "false");
    }

  }

OvmsPoller::OvmsNextPollResult OvmsPoller::PollSeriesList::NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate)
  {
  if (!m_first)
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "PollSeriesList::NextPollEntry - No List Set");
    return OvmsPoller::OvmsNextPollResult::StillAtEnd;
    }
  if (!m_iter)
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "PollSeriesList::NextPollEntry - Not Started");
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
      {
      IFTRACE(Poller) ESP_LOGV(TAG, "PollSeriesList::NextPollEntry Skipped %d", skipped);
      }

    if (m_iter == nullptr)
      return OvmsPoller::OvmsNextPollResult::ReachedEnd;
    res = m_iter->series->NextPollEntry(entry, mybus, pollticker, pollstate);

    IFTRACE(Poller) ESP_LOGV(TAG, "PollSeriesList::NextPollEntry[%s]: %s", m_iter->name.c_str(), PollResStr(res));

    switch (res)
      {
      case OvmsPoller::OvmsNextPollResult::NotReady:
        {
        m_iter = m_iter->next;
        break;
        }
      case OvmsPoller::OvmsNextPollResult::StillAtEnd:
        {
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
            IFTRACE(Poller) ESP_LOGD(TAG, "Poll Auto-Removing (next) '%s'", m_iter->name.c_str());
            Remove(m_iter);
            break;
            }
          case OvmsPoller::SeriesStatus::RemoveRestart:
            {
            // Resume - skipping over 'StillAtEnd' iterators.
            IFTRACE(Poller) ESP_LOGD(TAG, "Poll Auto-Removing (restart) '%s'", m_iter->name.c_str());
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

bool OvmsPoller::PollSeriesList::HasPollList() const
  {
  for (auto it = m_first; it != nullptr; it = it->next)
    {
    if (it->series != nullptr && it->series->HasPollList())
      return true;
    }
  return false;
  }

bool OvmsPoller::PollSeriesList::HasRepeat() const
  {
  for (auto it = m_first; it != nullptr; it = it->next)
    {
    if (it->series != nullptr && it->series->HasRepeat())
      return true;
    }
  return false;
  }

// Poll Series base
/// Send on an imcoming TX reply
void OvmsPoller::PollSeriesEntry::IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success)
  {
  // ignore
  }

bool OvmsPoller::PollSeriesEntry::Ready() const
  {
  return true;
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
  IFTRACE(Poller) ESP_LOGV(TAG, "Standard Poll Series: PID List set");
  m_poll_plcur = nullptr;
  m_poll_plist = plist;
  m_defaultbus = defaultbus;
  }

void OvmsPoller::StandardPollSeries::ResetList(OvmsPoller::ResetMode mode)
  {
  if (mode == OvmsPoller::ResetMode::PollReset)
    {
    IFTRACE(Poller) ESP_LOGV(TAG, "Standard Poll Series: List reset");
    m_poll_plcur = NULL;
    }
  }

OvmsPoller::OvmsNextPollResult OvmsPoller::StandardPollSeries::NextPollEntry(poll_pid_t &entry, uint8_t mybus, uint32_t pollticker, uint8_t pollstate)
  {

  entry = {};

  if (!Ready())
    return OvmsNextPollResult::NotReady;

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
        IFTRACE(Poller) ESP_LOGD(TAG, "Found Poll Entry for Standard Poll");
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
  }

void OvmsPoller::StandardPollSeries::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  }

OvmsPoller::SeriesStatus OvmsPoller::StandardPollSeries::FinishRun()
  {
  return OvmsPoller::SeriesStatus::Next;
  }

void OvmsPoller::StandardPollSeries::Removing()
  {
  m_poller = nullptr;
  }

bool OvmsPoller::StandardPollSeries::HasPollList() const
  {
  return (m_defaultbus != 0)
      && (m_poll_plist != NULL)
      && (m_poll_plist->txmoduleid != 0);
  }

bool OvmsPoller::StandardPollSeries::HasRepeat() const
  {
  return false;
  }

// StandardVehiclePollSeries
OvmsPoller::StandardVehiclePollSeries::StandardVehiclePollSeries(OvmsPoller *poller,
          VehicleSignal *signal,
          uint16_t stateoffset)
  : StandardPollSeries(poller, stateoffset), m_signal(signal)
  {
  }
// Process an incoming packet.
void OvmsPoller::StandardVehiclePollSeries::IncomingPacket(const OvmsPoller::poll_job_t& job, uint8_t* data, uint8_t length)
 {
 if (m_signal)
   m_signal->IncomingPollReply(job, data, length);
 }

// Process An Error. 
void OvmsPoller::StandardVehiclePollSeries::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
 {
 if (m_signal)
   m_signal->IncomingPollError(job, code);
 }

// Return true if this series is ok to run.
bool OvmsPoller::StandardVehiclePollSeries::Ready() const
  {
  return (!m_signal) || m_signal->Ready();
  }

// Send on an imcoming TX reply
void OvmsPoller::StandardVehiclePollSeries::IncomingTxReply(const OvmsPoller::poll_job_t& job, bool success)
  {
  if (m_signal)
    m_signal->IncomingPollTxCallback(job, success);
  }

// StandardPacketPollSeries

OvmsPoller::StandardPacketPollSeries::StandardPacketPollSeries( OvmsPoller *poller, int repeat_max, poll_success_func success, poll_fail_func fail, bool pack_raw_data)
  : StandardPollSeries(poller),
    m_repeat_max(repeat_max),
    m_repeat_count(0),
    m_success(success),
    m_fail(fail),
    m_format(CAN_frame_std),
    m_pack_raw_data(pack_raw_data)
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
    m_format = job.format;
    m_data.clear();
    auto len = length+job.mlremain;
    if (m_pack_raw_data) // Default to a bit more.
      len = (1+ (len >> 3)) << 3;
    m_data.reserve(len);
    }
  if (m_pack_raw_data)
    m_data.append((char*)job.raw_data, job.raw_data_len);
  else
    m_data.append((char*)data, length);
  if (job.mlremain == 0)
    {
    if (m_success)
      m_success(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, job.format, m_data);
    m_data.clear();
    }
  }

// Process An Error
void OvmsPoller::StandardPacketPollSeries::IncomingError(const OvmsPoller::poll_job_t& job, uint16_t code)
  {
  if (code == 0)
    {
    IFTRACE(Poller) ESP_LOGD(TAG, "Packet failed with zero error %.03" PRIx32 " TYPE:%x PID: %03x", job.moduleid_rec, job.type, job.pid);
    m_data.clear();
    if (m_success)
      m_success(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, job.format, m_data);
    }
  else
    {
    if (m_fail)
      m_fail(job.type, job.moduleid_sent, job.moduleid_rec, job.pid, code);
    OvmsPoller::StandardPollSeries::IncomingError(job, code);
    }
  }

// Return true if this series has entries to retry/redo.
bool OvmsPoller::StandardPacketPollSeries::HasRepeat() const
  {
  return (m_repeat_count < m_repeat_max) && HasPollList();
  }

// OvmsPoller::OnceOffPollBase class

OvmsPoller::OnceOffPollBase::OnceOffPollBase( const poll_pid_t &pollentry, std::string *rxbuf, int *rxerr, uint8_t retry_fail)
   : m_sent(status_t::Init), m_poll(pollentry), m_poll_rxbuf(rxbuf), m_poll_rxerr(rxerr),
    m_retry_fail(retry_fail), m_format(CAN_frame_std)
  {
  }
void OvmsPoller::OnceOffPollBase::SetParentPoller(OvmsPoller *poller)
  {
  }

OvmsPoller::OnceOffPollBase::OnceOffPollBase(std::string *rxbuf, int *rxerr, uint8_t retry_fail)
   : m_sent(status_t::Init),
    m_poll({ 0, 0, 0, 0, { 1, 1, 1, 1 }, 0, 0 }),
    m_poll_rxbuf(rxbuf), m_poll_rxerr(rxerr),
    m_retry_fail(retry_fail), m_format(CAN_frame_std)
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
      IFTRACE(Poller) ESP_LOGV(TAG, "Once Off Poll: List reset to start");
      break;
    case status_t::Retry:
      if (mode == OvmsPoller::ResetMode::PollReset)
        {
        if (!m_retry_fail)
          {
          m_sent = status_t::Stopping;
          return;
          }
        IFTRACE(Poller) ESP_LOGD(TAG, "Once Off Poll: Reset for retry");
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
        IFTRACE(Poller) ESP_LOGD(TAG, "Once Off Poll: Retries left %d", m_retry_fail);
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
      m_format = job.format;
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

bool OvmsPoller::OnceOffPollBase::HasPollList() const
  {
  return m_poll.txmoduleid != 0;
  }

bool OvmsPoller::OnceOffPollBase::HasRepeat() const
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
      m_success(m_poll.type, m_poll.txmoduleid, m_poll.rxmoduleid, m_poll.pid, m_format, m_data );
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
