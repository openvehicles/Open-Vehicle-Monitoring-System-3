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
static const char *TAG = "vehicle";

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

#undef LIMIT_MIN
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#undef LIMIT_MAX
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))


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
 * PollSetPidList: set the default bus and the polling list to process
 */
void OvmsVehicle::PollSetPidList(canbus* bus, const poll_pid_t* plist)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  m_poll_bus = bus;
  m_poll_bus_default = bus;
  m_poll_plist = plist;
  m_poll_ticker = 0;
  m_poll_sequence_cnt = 0;
  m_poll_wait = 0;
  m_poll_plcur = NULL;
  }


/**
 * PollSetPidList: set the polling state
 */
void OvmsVehicle::PollSetState(uint8_t state)
  {
  if ((state < VEHICLE_POLL_NSTATES)&&(state != m_poll_state))
    {
    OvmsRecMutexLock lock(&m_poll_mutex);
    m_poll_state = state;
    m_poll_ticker = 0;
    m_poll_sequence_cnt = 0;
    m_poll_wait = 0;
    m_poll_plcur = NULL;
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
 * PollerSend: internal: start next due request
 */
void OvmsVehicle::PollerSend(bool fromTicker)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);

  // Don't do anything with no bus, no list or an empty list
  if (!m_poll_bus_default || !m_poll_plist || m_poll_plist->txmoduleid == 0) return;

  if (m_poll_plcur == NULL) m_poll_plcur = m_poll_plist;

  // ESP_LOGD(TAG, "PollerSend(%d): entry at[type=%02X, pid=%X], ticker=%u, wait=%u, cnt=%u/%u",
  //          fromTicker, m_poll_plcur->type, m_poll_plcur->pid,
  //          m_poll_ticker, m_poll_wait, m_poll_sequence_cnt, m_poll_sequence_max);

  if (fromTicker)
    {
    // Timer ticker call: reset throttling counter, check response timeout
    m_poll_sequence_cnt = 0;
    if (m_poll_wait > 0) m_poll_wait--;
    }
  if (m_poll_wait > 0) return;

  while (m_poll_plcur->txmoduleid != 0)
    {
    if ((m_poll_plcur->polltime[m_poll_state] > 0) &&
        ((m_poll_ticker % m_poll_plcur->polltime[m_poll_state]) == 0))
      {
      // We need to poll this one...
      m_poll_protocol = m_poll_plcur->protocol;
      m_poll_type = m_poll_plcur->type;
      m_poll_pid = m_poll_plcur->pid;
      if (m_poll_plcur->rxmoduleid != 0)
        {
        // send to <moduleid>, listen to response from <rmoduleid>:
        m_poll_moduleid_sent = m_poll_plcur->txmoduleid;
        m_poll_moduleid_low = m_poll_plcur->rxmoduleid;
        m_poll_moduleid_high = m_poll_plcur->rxmoduleid;
        }
      else
        {
        // broadcast: send to 0x7df, listen to all responses:
        m_poll_moduleid_sent = 0x7df;
        m_poll_moduleid_low = 0x7e8;
        m_poll_moduleid_high = 0x7ef;
        }

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

      ESP_LOGD(TAG, "PollerSend(%d): send [bus=%d, type=%02X, pid=%X], expecting %03x/%03x-%03x",
               fromTicker, m_poll_plcur->pollbus, m_poll_type, m_poll_pid, m_poll_moduleid_sent,
               m_poll_moduleid_low, m_poll_moduleid_high);

      CAN_frame_t txframe;
      uint8_t* txdata;
      memset(&txframe,0,sizeof(txframe));
      txframe.origin = m_poll_bus;
      txframe.FIR.B.FF = CAN_frame_std;
      txframe.FIR.B.DLC = 8;

      if (m_poll_protocol == ISOTP_EXTADR)
        {
        txframe.MsgID = m_poll_moduleid_sent >> 8;
        txframe.data.u8[0] = m_poll_moduleid_sent & 0xff;
        txdata = &txframe.data.u8[1];
        }
      else
        {
        txframe.MsgID = m_poll_moduleid_sent;
        txdata = &txframe.data.u8[0];
        }

      if (POLL_TYPE_HAS_16BIT_PID(m_poll_plcur->type))
        {
        uint8_t datalen = LIMIT_MAX(m_poll_plcur->args.datalen, 4);
        txdata[0] = (ISOTP_FT_SINGLE << 4) + 3 + datalen;
        txdata[1] = m_poll_type;
        txdata[2] = m_poll_pid >> 8;
        txdata[3] = m_poll_pid & 0xff;
        memcpy(&txdata[4], m_poll_plcur->args.data, datalen);
        }
      else if (POLL_TYPE_HAS_8BIT_PID(m_poll_plcur->type))
        {
        uint8_t datalen = LIMIT_MAX(m_poll_plcur->args.datalen, 5);
        txdata[0] = (ISOTP_FT_SINGLE << 4) + 2 + datalen;
        txdata[1] = m_poll_type;
        txdata[2] = m_poll_pid;
        memcpy(&txdata[3], m_poll_plcur->args.data, datalen);
        }
      else
        {
        uint8_t datalen = LIMIT_MAX(m_poll_plcur->args.datalen, 6);
        txdata[0] = (ISOTP_FT_SINGLE << 4) + 1 + datalen;
        txdata[1] = m_poll_type;
        memcpy(&txdata[2], m_poll_plcur->args.data, datalen);
        }

      m_poll_bus->Write(&txframe);
      m_poll_ml_frame = 0;
      m_poll_ml_offset = 0;
      m_poll_ml_remain = 0;
      m_poll_wait = 2;
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
 * PollerSend: internal: process poll response frame
 */
void OvmsVehicle::PollerReceive(CAN_frame_t* frame, uint32_t msgid)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  char *hexdump = NULL;

  // After locking the mutex, check again for poll expectance match:
  if (!m_poll_wait || !m_poll_plist || frame->origin != m_poll_bus ||
      msgid < m_poll_moduleid_low || msgid > m_poll_moduleid_high)
    {
    ESP_LOGD(TAG, "PollerReceive[%03X]: dropping expired poll response", msgid);
    return;
    }


  // 
  // Get & validate ISO-TP meta data
  // 

  uint8_t* fr_data;               // Frame data address
  uint8_t  fr_maxlen;             // Frame data max length
  uint8_t  tp_frametype;          // ISO-TP frame type (0…3)
  uint8_t  tp_frameindex;         // TP cyclic frame index (0…15)
  uint16_t tp_len;                // TP remaining payload length including this frame (0…4095)
  uint8_t* tp_data;               // TP frame data section address
  uint8_t  tp_datalen;            // TP frame data section length (0…7)

  if (m_poll_protocol == ISOTP_EXTADR)
    {
    fr_data = &frame->data.u8[1];
    fr_maxlen = 7;
    }
  else
    {
    fr_data = &frame->data.u8[0];
    fr_maxlen = 8;
    }

  tp_frametype = fr_data[0] >> 4;

  switch (tp_frametype)
    {
    case ISOTP_FT_SINGLE:
      tp_frameindex = 0;
      tp_len = fr_data[0] & 0x0f;
      tp_data = &fr_data[1];
      tp_datalen = tp_len;
      break;
    case ISOTP_FT_FIRST:
      tp_frameindex = 0;
      tp_len = (fr_data[0] & 0x0f) << 8 | fr_data[1];
      tp_data = &fr_data[2];
      tp_datalen = (tp_len > fr_maxlen-2) ? fr_maxlen-2 : tp_len;
      break;
    case ISOTP_FT_CONSECUTIVE:
      tp_frameindex = fr_data[0] & 0x0f;
      tp_len = m_poll_ml_remain;
      tp_data = &fr_data[1];
      tp_datalen = (tp_len > fr_maxlen-1) ? fr_maxlen-1 : tp_len;
      break;
    default:
      {
      // This is most likely an indication there is a non ISO-TP device sending
      // in our expected RX ID range, so we log the frame and abort:
      FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
      ESP_LOGW(TAG, "PollerReceive[%03X]: ignoring unknown/invalid ISO TP frame: %s",
               msgid, hexdump ? hexdump : "-");
      if (hexdump) free(hexdump);
      return;
      }
    }

  // Check frame index:
  if (tp_frametype == ISOTP_FT_CONSECUTIVE)
    {
    // Note: we tolerate an index less than the expected one, as some devices
    //  begin counting at the first consecutive frame
    if (m_poll_ml_remain == 0 || tp_frameindex > (m_poll_ml_frame & 0x0f))
      {
      FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
      ESP_LOGW(TAG, "PollerReceive[%03X]: unexpected/out of sequence ISO TP frame (%d vs %d), aborting poll %02X(%X): %s",
              msgid, tp_frameindex, m_poll_ml_frame & 0x0f, m_poll_type, m_poll_pid,
              hexdump ? hexdump : "-");
      if (hexdump) free(hexdump);
      m_poll_moduleid_low = m_poll_moduleid_high = 0; // ignore further frames
      m_poll_wait = 2; // give the bus time to let remaining frames pass
      return;
      }
    }


  // 
  // Get & validate OBD/UDS meta data
  // 

  uint8_t  response_type = 0;         // OBD/UDS response type tag (expected: 0x40 + request type)
  uint16_t response_pid = 0;          // OBD/UDS response PID (expected: request PID)
  uint8_t* response_data = NULL;      // OBD/UDS frame payload address
  uint16_t response_datalen = 0;      // OBD/UDS frame payload length (0…7)
  uint8_t  error_type = 0;            // OBD/UDS error response service type (expected: request type)
  uint8_t  error_code = 0;            // OBD/UDS error response code (see ISO 14229 Annex A.1)

  if (tp_frametype == ISOTP_FT_CONSECUTIVE)
    {
    response_type = 0x40+m_poll_type;
    response_pid = m_poll_pid;
    response_data = tp_data;
    response_datalen = tp_datalen;
    }
  else // ISOTP_FT_FIRST || ISOTP_FT_SINGLE
    {
    response_type = tp_data[0];
    if (response_type == UDS_RESP_TYPE_NRC)
      {
      error_type = tp_data[1];
      error_code = tp_data[2];
      }
    else if (POLL_TYPE_HAS_16BIT_PID(response_type-0x40))
      {
      response_pid = tp_data[1] << 8 | tp_data[2];
      response_data = &tp_data[3];
      response_datalen = tp_datalen - 3;
      }
    else if (POLL_TYPE_HAS_8BIT_PID(response_type-0x40))
      {
      response_pid = tp_data[1];
      response_data = &tp_data[2];
      response_datalen = tp_datalen - 2;
      }
    else
      {
      response_pid = m_poll_pid;
      response_data = &tp_data[1];
      response_datalen = tp_datalen - 1;
      }
    }


  // 
  // Process OBD/UDS payload
  // 

  if (response_type == UDS_RESP_TYPE_NRC && error_type == m_poll_type)
    {
    // Negative Response Code:
    if (error_code == UDS_RESP_NRC_RCRRP)
      {
      // Info: requestCorrectlyReceived-ResponsePending (server busy processing the request)
      ESP_LOGD(TAG, "PollerReceive[%03X]: got OBD/UDS info %02X(%X) code=%02X (pending)",
               msgid, m_poll_type, m_poll_pid, error_code);
      // add some wait time:
      m_poll_wait++;
      return;
      }
    else
      {
      // Error: forward to application:
      ESP_LOGD(TAG, "PollerReceive[%03X]: process OBD/UDS error %02X(%X) code=%02X",
               msgid, m_poll_type, m_poll_pid, error_code);
      // Running single poll?
      if (m_poll_single_rxbuf)
        {
        m_poll_single_rxerr = error_code;
        m_poll_single_rxbuf = NULL;
        m_poll_single_rxdone.Give();
        }
      // Forward:
      IncomingPollError(frame->origin, m_poll_type, m_poll_pid, error_code);
      // abort:
      m_poll_ml_remain = 0;
      }
    }
  else if (response_type == 0x40+m_poll_type && response_pid == m_poll_pid)
    {
    // Normal matching poll response, forward to application:
    m_poll_ml_remain = tp_len - tp_datalen;
    ESP_LOGD(TAG, "PollerReceive[%03X]: process OBD/UDS response %02X(%X) frm=%u len=%u off=%u rem=%u",
             msgid, m_poll_type, m_poll_pid,
             m_poll_ml_frame, response_datalen, m_poll_ml_offset, m_poll_ml_remain);
    // Running single poll?
    if (m_poll_single_rxbuf)
      {
      if (m_poll_ml_frame == 0)
        {
        m_poll_single_rxbuf->clear();
        m_poll_single_rxbuf->reserve(response_datalen + m_poll_ml_remain);
        }
      m_poll_single_rxbuf->append((char*)response_data, response_datalen);
      if (m_poll_ml_remain == 0)
        {
        m_poll_single_rxerr = 0;
        m_poll_single_rxbuf = NULL;
        m_poll_single_rxdone.Give();
        }
      }
    // Forward:
    IncomingPollReply(frame->origin, m_poll_type, m_poll_pid, response_data, response_datalen, m_poll_ml_remain);
    }
  else
    {
    // This is most likely a late response to a previous poll, log & skip:
    FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
    ESP_LOGW(TAG, "PollerReceive[%03X]: OBD/UDS response type/PID mismatch, got %02X(%X) vs %02X(%X) => ignoring: %s",
             msgid, response_type, response_pid, 0x40+m_poll_type, m_poll_pid, hexdump ? hexdump : "-");
    if (hexdump) free(hexdump);
    return;
    }


  // Do we expect more data?
  if (m_poll_ml_remain)
    {
    if (tp_frametype == ISOTP_FT_FIRST)
      {
      // First frame; send flow control frame:
      CAN_frame_t txframe;
      uint8_t* txdata;
      uint32_t txid;
      memset(&txframe,0,sizeof(txframe));
      txframe.origin = frame->origin;
      txframe.FIR.B.FF = CAN_frame_std;
      txframe.FIR.B.DLC = 8;

      if (m_poll_moduleid_sent == 0x7df)
        {
        // broadcast request: derive module ID from response ID:
        // (Note: this only works for the SAE standard ID scheme)
        txid = frame->MsgID - 8;
        }
      else
        {
        // use known module ID:
        txid = m_poll_moduleid_sent;
        }

      if (m_poll_protocol == ISOTP_EXTADR)
        {
        txframe.MsgID = txid >> 8;
        txframe.data.u8[0] = txid & 0xff;
        txdata = &txframe.data.u8[1];
        }
      else
        {
        txframe.MsgID = txid;
        txdata = &txframe.data.u8[0];
        }

      txdata[0] = 0x30;                // flow control frame type
      txdata[1] = 0x00;                // request all frames available
      txdata[2] = m_poll_fc_septime;   // with configured separation timing (default 25 ms)
      txframe.Write();
      m_poll_ml_frame = 1;
      }
    else
      {
      m_poll_ml_frame++;
      }

    m_poll_ml_offset += response_datalen; // next frame application payload offset
    m_poll_wait = 2;
    }
  else
    {
    // Request response complete:
    m_poll_wait = 0;
    }


  // Immediately send the next poll for this tick if…
  // - we are not waiting for another frame
  // - the poll was no broadcast (with potential further responses from other devices)
  // - poll throttling is unlimited or limit isn't reached yet
  if (m_poll_wait == 0 &&
      m_poll_moduleid_sent != 0x7df &&
      (!m_poll_sequence_max || m_poll_sequence_cnt < m_poll_sequence_max))
    {
    PollerSend(false);
    }
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
 *  @param request      Request to send (binary string) (only single frame requests supported)
 *  @param response     Response buffer (binary string) (multiple response frames assembled)
 *  @param timeout_ms   Timeout for poller/response in milliseconds
 *  @param protocol     Protocol variant: ISOTP_STD / ISOTP_EXTADR
 *  
 *  @return             0 = OK, -1 = timeout/poller unavailable, else UDS NRC detail code
 *                      Note: response is only valid with return value 0
 */
int OvmsVehicle::PollSingleRequest(canbus* bus, uint32_t txid, uint32_t rxid,
                                   std::string request, std::string& response,
                                   int timeout_ms /*=3000*/, uint8_t protocol /*=ISOTP_STD*/)
  {
  if (!m_ready)
    return -1;
  OvmsMutexLock slock(&m_poll_single_mutex, pdMS_TO_TICKS(timeout_ms));
  if (!slock.IsLocked())
    return -1;

  // prepare single poll:
  OvmsVehicle::poll_pid_t poll[] =
    {
      { txid, rxid, 0, 0, { 999, 999, 999, 999 }, 0, protocol },
      POLL_LIST_END
    };

  assert(request.size() > 0);
  poll[0].type = request[0];

  if (POLL_TYPE_HAS_16BIT_PID(poll[0].type))
    {
    assert(request.size() >= 3);
    poll[0].args.pid = request[1] << 8 | request[2];
    poll[0].args.datalen = LIMIT_MAX(request.size()-3, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+3, poll[0].args.datalen);
    }
  else if (POLL_TYPE_HAS_8BIT_PID(poll[0].type))
    {
    assert(request.size() >= 2);
    poll[0].args.pid = request.at(1);
    poll[0].args.datalen = LIMIT_MAX(request.size()-2, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+2, poll[0].args.datalen);
    }
  else
    {
    poll[0].args.pid = 0;
    poll[0].args.datalen = LIMIT_MAX(request.size()-1, sizeof(poll[0].args.data));
    memcpy(poll[0].args.data, request.data()+1, poll[0].args.datalen);
    }

  // acquire poller access:
  if (!m_poll_mutex.Lock(pdMS_TO_TICKS(timeout_ms)))
    return -1;

  // save poller state:
  canbus*           p_bus    = m_poll_bus_default;
  const poll_pid_t* p_list   = m_poll_plist;
  const poll_pid_t* p_plcur  = m_poll_plcur;
  uint32_t          p_ticker = m_poll_ticker;

  // start single poll:
  m_poll_single_rxbuf = &response;
  PollSetPidList(bus, poll);
  PollerSend(true);
  m_poll_mutex.Unlock();

  // wait for response:
  bool rxok = m_poll_single_rxdone.Take(pdMS_TO_TICKS(timeout_ms));

  // restore poller state:
  m_poll_mutex.Lock();
  PollSetPidList(p_bus, p_list);
  m_poll_plcur = p_plcur;
  m_poll_ticker = p_ticker;
  m_poll_mutex.Unlock();

  return (rxok == pdFALSE) ? -1 : (int)m_poll_single_rxerr;
  }
