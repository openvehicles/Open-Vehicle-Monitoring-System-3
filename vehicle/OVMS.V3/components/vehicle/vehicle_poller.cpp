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
  m_bus = can;
  m_can_number = can_number;
  m_parent_signal = parent_signal;
  m_poll_txcallback = polltxcallback;

  m_poll_mode = PollMode::Standard;
  m_poll_once = {};

  m_poll_state = 0;

  m_poll_plist = NULL;
  m_poll_plcur = NULL;

  m_poll_entry = {};
  m_poll_vwtp = {};
  m_poll_ticker = 0;

  m_poll_default_bus = 0;

  m_poll_single_rxbuf = NULL;
  m_poll_single_rxerr = 0;
  m_poll_moduleid_sent = 0;
  m_poll_moduleid_low = 0;
  m_poll_moduleid_high = 0;
  m_poll_type = 0;
  m_poll_pid = 0;
  m_poll_ml_remain = 0;
  m_poll_ml_offset = 0;
  m_poll_ml_frame = 0;
  m_poll_wait = 0;
  m_poll_sequence_max = 1;
  m_poll_sequence_cnt = 0;
  m_poll_fc_septime = 25;       // response default timing: 25 milliseconds
  m_poll_ch_keepalive = 60;     // channel keepalive default: 60 seconds

  }


void OvmsPoller::Incoming(const CAN_frame_t &frame, bool success)
  {
  // Pass frame to poller protocol handlers:
  if (frame.origin == m_poll_vwtp.bus && frame.MsgID == m_poll_vwtp.rxid)
    {
    PollerVWTPReceive(&frame, frame.MsgID);
    }
  else if (frame.origin == m_bus)
    {
    if (!m_poll_wait)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]Poller: Incoming - giving up", m_can_number);
      return;
      }
    if (m_poll_entry.txmoduleid == 0)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]Poller: Incoming - Poll Entry Cleared", m_can_number);
      return;
      }
    uint32_t msgid;
    if (m_poll_protocol == ISOTP_EXTADR)
      msgid = frame.MsgID << 8 | frame.data.u8[0];
    else
      msgid = frame.MsgID;
    ESP_LOGD(TAG, "[%" PRIu8 "]Poller: FrameRx(bus=%s, msg=%" PRIx32 " )", m_can_number, frame.origin == m_bus ? "Self" : "Other", msgid );
    if (msgid >= m_poll_moduleid_low && msgid <= m_poll_moduleid_high)
      {
      PollerISOTPReceive(&frame, msgid);
      }
    }
    // pass to parent
  m_parent_signal->IncomingPollRxFrame(m_bus, &frame, success);
  }

OvmsPoller::~OvmsPoller()
  {
  }

/**
 * IncomingPollReply: Calls Vehicle poll response handler
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
 *  @member m_poll_entry
 *    Copy of the currently processed poll entry
 */
void OvmsPoller::IncomingPollReply(canbus* bus, uint32_t msgid, uint16_t type, uint16_t pid, const uint8_t* data, uint8_t length, uint16_t mlremain)
  {
    // Call back on Vehicle.
  m_parent_signal->IncomingPollReply(bus, m_poll_moduleid_sent, msgid, type, pid,
    data, m_poll_ml_offset, length, mlremain, m_poll_ml_frame, m_poll_entry);
  }


/**
 * IncomingPollError: Calls Vehicle poll response error handler
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
 *  @member m_poll_entry
 *    Copy of the currently processed poll entry
 */
void OvmsPoller::IncomingPollError(canbus* bus, uint32_t msgid, uint16_t type, uint16_t pid, uint16_t code)
  {
  // Call back on Vehicle.
  m_parent_signal->IncomingPollError(bus, m_poll_moduleid_sent, msgid, type, pid, code, m_poll_entry);
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
void OvmsPoller::IncomingPollTxCallback(canbus* bus, uint32_t txid, uint16_t type, uint16_t pid, bool success)
  {
  // Call Back on vehicle
  m_parent_signal->IncomingPollTxCallback(bus, txid, type, pid, success);
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
void OvmsPoller::PollSetPidList(uint8_t defaultbus, const poll_pid_t* plist)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_default_bus = defaultbus;
  m_poll_plist = plist;
  m_poll_plcur = NULL;
  m_poll_ticker = 0;
  m_poll_sequence_cnt = 0;

  if (m_poll_mode == PollMode::Standard)
    {
    m_poll_wait = 0;
    ResetPollEntry();
    }
  }

void  OvmsPoller::Do_PollSetState(uint8_t state)
  {
  if ((state < VEHICLE_POLL_NSTATES)&&(state != m_poll_state))
    {
    m_poll_state = state;
    m_poll_ticker = 0;
    m_poll_sequence_cnt = 0;
    m_poll_plcur = NULL;
    if (m_poll_mode == PollMode::Standard)
      {
      m_poll_wait = 0;
      m_poll_entry = {};
      m_poll_txmsgid = 0;
      }
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

void OvmsPoller::ResetPollEntry()
  {
  m_poll_plcur = NULL;
  m_poll_entry = {};
  m_poll_txmsgid = 0;
  }

bool OvmsPoller::HasPollList()
  {
  switch (m_poll_mode)
    {
    case PollMode::Standard:
      return (m_poll_plist != NULL) && (m_poll_plist->txmoduleid != 0);
    case PollMode::OnceOff:
    case PollMode::OnceOffDone:
      return m_poll_once.txmoduleid != 0;
    }
  return false;
  }

bool OvmsPoller::CanPoll()
  {
  // Check Throttle
  return (!m_poll_sequence_max || m_poll_sequence_cnt < m_poll_sequence_max);
  }

OvmsPoller::OvmsNextPollResult OvmsPoller::NextPollEntry(poller_source_t source, OvmsPoller::poll_pid_t *entry)
  {
  *entry = {};

  switch (m_poll_mode)
    {
    case PollMode::OnceOff:
      {
        if (source != poller_source_t::OnceOff)
          return OvmsNextPollResult::Ignore;

        if (m_poll_once.txmoduleid == 0) {
          m_poll_mode = PollMode::OnceOffDone;
          return OvmsNextPollResult::ReachedEnd;
        }
        *entry = m_poll_once;
        m_poll_once.txmoduleid = 0;
        return OvmsNextPollResult::FoundEntry;
      }
    case PollMode::OnceOffDone:
      return OvmsNextPollResult::StillAtEnd;

    default:
      break;
    }

  OvmsRecMutexLock lock(&m_poll_mutex);

  // Restart poll list cursor:
  if (m_poll_plcur == NULL)
    m_poll_plcur = m_poll_plist;
  else if (m_poll_plcur->txmoduleid == 0)
    return OvmsNextPollResult::StillAtEnd;
  else
    ++m_poll_plcur;

  while (m_poll_plcur->txmoduleid != 0)
    {
    auto checkbus = m_poll_plcur->pollbus;
    if (checkbus == 0)
      checkbus = m_poll_default_bus;
    if (checkbus == m_can_number)
      {
      if ((m_poll_plcur->polltime[m_poll_state] > 0) &&
          ((m_poll_ticker % m_poll_plcur->polltime[m_poll_state]) == 0))
        {
        *entry = *m_poll_plcur;
        return OvmsNextPollResult::FoundEntry;
        }
      }
    // Poll entry is not due, check next
    ++m_poll_plcur;
    }
  return OvmsNextPollResult::ReachedEnd;
  }

void OvmsPoller::PollerNextTick(poller_source_t source)
  {
  // Completed checking all poll entries for the current m_poll_ticker

  ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend(%s): cycle complete for ticker=%u", m_can_number, PollerSource(source), m_poll_ticker);

  // Allow POLL to restart.
  m_poll_plcur = NULL;

  m_poll_ticker++;
  if (m_poll_ticker > 3600) m_poll_ticker -= 3600;
  }

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
  // ESP_LOGD(TAG, "PollerSend(%d): entry at[type=%02X, pid=%X], ticker=%u, wait=%u, cnt=%u/%u",
  //          fromTicker, m_poll_plcur->type, m_poll_plcur->pid,
  //          m_poll_ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);

  bool fromPrimaryTicker = false, fromOnceOffTicker = false;
  switch (source)
    {
    case poller_source_t::OnceOff:
      fromOnceOffTicker = true;
      break;
    case poller_source_t::Primary:
      fromPrimaryTicker = true;
      break;
    default:
      ;
    }
  if (fromPrimaryTicker)
    {
    // Only reset the list when 'from Ticker' and it's at the end.
    if (m_poll_plcur && m_poll_plcur->txmoduleid == 0)
      {
      PollerNextTick(source);
      }
    }
  if (fromPrimaryTicker || fromOnceOffTicker)
    {
    // Timer ticker call: check response timeout
    if (m_poll_wait > 0) m_poll_wait--;

    // Protocol specific ticker calls:
    PollerVWTPTicker();
    }
  if (m_poll_wait > 0)
    {
    ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Waiting %" PRIu8, m_can_number, m_poll_wait);
    return;
    }

  // Check poll bus & list:
  if (!HasPollList())
    {
    ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Empty", m_can_number);
    return;
    }

  ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend: Get Next", m_can_number);
  switch (NextPollEntry(source, &m_poll_entry))
    {
    case OvmsNextPollResult::Ignore:
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend: Ignore", m_can_number);
      break;
    case OvmsNextPollResult::ReachedEnd:
      ESP_LOGV(TAG, "[%" PRIu8 "]PollerSend: Finished", m_can_number);
      PollRunFinished();
      // fall through
    case OvmsNextPollResult::StillAtEnd:
      {
      if (m_poll_mode == PollMode::Standard)
        PollerNextTick(source);
      break;
      }
    case OvmsNextPollResult::FoundEntry:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerSend(%s)[%d]: entry at[type=%02X, pid=%X], ticker=%u, wait=%u, cnt=%u/%u",
             m_can_number, PollerSource(source), m_poll_state, m_poll_entry.type, m_poll_entry.pid,
             m_poll_ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);
      // We need to poll this one...
      m_poll_protocol = m_poll_entry.protocol;
      m_poll_type = m_poll_entry.type;
      m_poll_pid = m_poll_entry.pid;

      // Dispatch transmission start to protocol handler:
      if (m_poll_protocol == VWTP_20)
        PollerVWTPStart(fromPrimaryTicker);
      else
        PollerISOTPStart(fromPrimaryTicker);

      m_poll_sequence_cnt++;
      break;
      }
    }
  }

void OvmsPoller::Outgoing(const CAN_frame_t &frame, bool success)
  {

  // Check for a late callback:
  if (!m_poll_wait || !m_poll_entry.txmoduleid || frame.origin != m_bus || frame.MsgID != m_poll_txmsgid)
    return;

  // Forward to protocol handler:
  if (m_poll_protocol == VWTP_20)
    PollerVWTPTxCallback(&frame, success);

  // On failure, try to speed up the current poll timeout:
  if (!success)
    {
    m_poll_wait = 0;
    if (m_poll_single_rxbuf)
      {
      m_poll_single_rxerr = POLLSINGLE_TXFAILURE;
      m_poll_single_rxbuf = NULL;
      m_poll_single_rxdone.Give();
      }
    }

  // Forward to application:
  IncomingPollTxCallback(m_bus, m_poll_moduleid_sent, m_poll_type, m_poll_pid, success);
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
  OvmsPoller::poll_pid_t poll[] =
    {
      { txid, rxid, 0, 0, { 999, 999, 999, 999 }, 0, protocol },
      POLL_LIST_END
    };

  assert(request.size() > 0);
  poll[0].type = request[0];
  poll[0].xargs.tag = POLL_TXDATA;

  if (POLL_TYPE_HAS_16BIT_PID(poll[0].type))
    {
    assert(request.size() >= 3);
    poll[0].xargs.pid = request[1] << 8 | request[2];
    poll[0].xargs.datalen = LIMIT_MAX(request.size()-3, 4095);
    poll[0].xargs.data = (const uint8_t*)request.data()+3;
    }
  else if (POLL_TYPE_HAS_8BIT_PID(poll[0].type))
    {
    assert(request.size() >= 2);
    poll[0].xargs.pid = request.at(1);
    poll[0].xargs.datalen = LIMIT_MAX(request.size()-2, 4095);
    poll[0].xargs.data = (const uint8_t*)request.data()+2;
    }
  else
    {
    poll[0].xargs.pid = 0;
    poll[0].xargs.datalen = LIMIT_MAX(request.size()-1, 4095);
    poll[0].xargs.data = (const uint8_t*)request.data()+1;
    }

  // acquire poller access:
  if (!m_poll_mutex.Lock(pdMS_TO_TICKS(timeout_ms)))
    return -1;
  m_poll_once = poll[0];
  m_poll_mode = PollMode::OnceOff;
  m_poll_mutex.Unlock();
  ESP_LOGD(TAG, "[%" PRIu8 "]PollSingleRequest: Request sent ", m_can_number);

  m_poll_single_rxdone.Take(0);
  m_poll_single_rxbuf = &response;
  Queue_PollerSend(poller_source_t::OnceOff);

  // wait for response:
  bool rxok = m_poll_single_rxdone.Take(pdMS_TO_TICKS(timeout_ms));
  ESP_LOGD(TAG, "[%" PRIu8 "]PollSingleRequest: Response done ", m_can_number);
  auto error = m_poll_single_rxerr;
  {
    OvmsRecMutexLock lock(&m_poll_mutex);

    // restore poller state:
    m_poll_single_rxbuf = NULL;
    m_poll_entry = {};
    m_poll_txmsgid = 0;
    m_poll_mode = PollMode::Standard;
  }
  ESP_LOGD(TAG, "[%" PRIu8 "]PollSingleRequest: Poll Mode Standard", m_can_number);
  return (rxok == pdFALSE) ? -1 : error;
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
  m_parent_signal->PollerSend(m_can_number, source);
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

OvmsPollers::OvmsPollers( OvmsPoller::ParentSignal *parent)
  : m_parent_callback(parent),
    m_poll_state(0),
    m_poll_sequence_max(0),
    m_poll_fc_septime(25),
    m_poll_ch_keepalive(60),
    m_pollqueue(nullptr), m_polltask(nullptr)
  {
  for (int idx = 0; idx < VEHICLE_MAXBUSSES; ++idx)
    m_pollers[idx] = nullptr;

  m_poll_txcallback = std::bind(&OvmsPollers::PollerTxCallback, this, _1, _2);
  MyCan.RegisterCallback("CAN Poller", std::bind(&OvmsPollers::PollerRxCallback, this, _1, _2));
  }
OvmsPollers::~OvmsPollers()
  {
  if (m_pollqueue)
    vQueueDelete(m_pollqueue);
  if (m_polltask)
    vTaskDelete(m_polltask);
  MyCan.DeregisterCallback("CAN Poller");
  delete m_parent_callback;
  for (int i = 0 ; i < VEHICLE_MAXBUSSES; ++i)
    {
    if (m_pollers[i])
      delete m_pollers[i];
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
void OvmsPollers::CheckStartPollTask()
  {
  if (!m_pollqueue)
    m_pollqueue = xQueueCreate(CONFIG_OVMS_VEHICLE_CAN_RX_QUEUE_SIZE,sizeof(OvmsPoller::poll_queue_entry_t));
  if (!m_polltask)
    {
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
  while (true)
    {
    if (xQueueReceive(m_pollqueue, &entry, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      switch (entry.entry_type)
        {
        case OvmsPoller::OvmsPollEntryType::FrameRx:
          {
          auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
          ESP_LOGD(TAG, "Pollers: FrameRx(bus=%d)", m_parent_callback->GetBusNo(entry.entry_FrameRxTx.frame.origin));
          if (poller)
            poller->Incoming(entry.entry_FrameRxTx.frame, entry.entry_FrameRxTx.success);
          }
          break;
        case OvmsPoller::OvmsPollEntryType::FrameTx:
          {
          auto poller = GetPoller(entry.entry_FrameRxTx.frame.origin);
          ESP_LOGD(TAG, "Pollers: FrameTx(bus=%d)", m_parent_callback->GetBusNo(entry.entry_FrameRxTx.frame.origin));
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

  CheckStartPollTask();

  return (gap<0) ? nullptr : m_pollers[gap];
  }

void OvmsPollers::QueuePollerSend(OvmsPoller::poller_source_t src, uint8_t busno)
  {
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
      uint8_t busno = poller->m_can_number;
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
