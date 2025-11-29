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
;    (C) 2021       Michael Balzer <dexter@dexters-web.de>
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
static const char *TAG = "vehicle-vwtp";

#include <stdio.h>
#include <algorithm>
#include "vehicle.h"


/**
 * PollerVWTPStart: start next VW-TP request (internal method)
 */
void OvmsPoller::PollerVWTPStart(bool fromTicker)
  {
  m_poll_vwtp.lastused = esp_log_timestamp();

  // Check connection state:
  if (m_poll_vwtp.bus != m_poll.bus ||
      m_poll_vwtp.baseid != m_poll.entry.txmoduleid ||
      m_poll_vwtp.moduleid != m_poll.entry.rxmoduleid)
    {
    // close or reconnect channel:
    if (m_poll_vwtp.state != VWTP_Closed)
      PollerVWTPEnter(VWTP_ChannelClose);
    else if (m_poll.entry.rxmoduleid != 0)
      PollerVWTPEnter(VWTP_ChannelSetup);
    else
      {
      OvmsRecMutexLock lock(&m_poll_mutex);
      m_polls.IncomingError(m_poll, POLLSINGLE_OK);
      }
    return;
    }

  // Check communication state:
  if (m_poll_vwtp.state < VWTP_Idle)
    {
    // previous channel setup/shutdown hasn't finished, start over:
    PollerVWTPEnter(VWTP_ChannelSetup);
    }
  else
    {
    // abort transfer in progress if any:
    if (m_poll_vwtp.state == VWTP_Transmit)
      {
      PollerVWTPEnter(VWTP_AbortXfer);
      }
    else if (m_poll_vwtp.state == VWTP_Receive)
      {
      PollerVWTPEnter(VWTP_AbortXfer);
      usleep(m_poll_vwtp.septime * m_poll_vwtp.blocksize);
      }
    // start new poll:
    PollerVWTPEnter(VWTP_StartPoll);
    }
  }


/**
 * PollerVWTPEnter: channel state transition (internal method)
 */
void OvmsPoller::PollerVWTPEnter(vwtp_channelstate_t state)
  {
  CAN_frame_t txframe = {};
  txframe.callback = &m_poll_txcallback;
  txframe.FIR.B.FF = CAN_frame_std;

  if (m_poll_vwtp.state == state) return;

  switch (state)
    {
    case VWTP_ChannelSetup:
      {
      // Open TP20 channel for current polling entry:
      if (m_poll.protocol != VWTP_20)
        {
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter/ChannelSetup: m_poll_protocol mismatch, abort", m_poll.bus_no);
        }
      else
        {
        m_poll_vwtp.bus = m_poll.bus;
        m_poll_vwtp.baseid = m_poll.entry.txmoduleid;
        m_poll_vwtp.moduleid = m_poll.entry.rxmoduleid;
        m_poll_vwtp.txid = m_poll_vwtp.baseid;
        m_poll_vwtp.rxid = m_poll_vwtp.baseid + m_poll_vwtp.moduleid;
        
        // txid & rxid will be changed by the setup response frame, we offer a standard rxid
        // matching observed client behaviour with baseid=0x200 → rxid=0x300:
        uint16_t offer_rxid = m_poll_vwtp.baseid + 0x100;

        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: channel setup request bus=%d txid=%03X rxid=%03X",
          m_poll.bus_no,
          m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);

        txframe.MsgID = m_poll_vwtp.txid;
        txframe.FIR.B.DLC = 7;
        txframe.data.u8[0] = m_poll_vwtp.moduleid;
        txframe.data.u8[1] = 0xC0;  // setup request
        txframe.data.u8[2] = 0x00;
        txframe.data.u8[3] = 0x10;
        txframe.data.u8[4] = offer_rxid & 0xff;
        txframe.data.u8[5] = offer_rxid >> 8;
        txframe.data.u8[6] = 0x01;
        
        m_poll_txmsgid = m_poll_vwtp.txid;
        m_poll.moduleid_sent = m_poll_vwtp.txid;
        m_poll.moduleid_low = m_poll_vwtp.rxid;
        m_poll.moduleid_high = m_poll_vwtp.rxid;

        m_poll_wait = 2;
        m_poll_vwtp.state = VWTP_ChannelSetup;
        m_poll_vwtp.lastused = esp_log_timestamp();
        m_poll_vwtp.bus->Write(&txframe);
        }
      break;
      }

    case VWTP_ChannelParams:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: channel params request bus=%d txid=%03X rxid=%03X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);
      
      txframe.MsgID = m_poll_vwtp.txid;
      txframe.FIR.B.DLC = 6;
      txframe.data.u8[0] = 0xA0;  // params request
      txframe.data.u8[1] = 0x0F;  // block size: 15 frames
      txframe.data.u8[2] = 0x8A;  // time to wait for ACK: 10ms x 10 = 100 ms
      txframe.data.u8[3] = 0xFF;  // always ff
      txframe.data.u8[4] = 0x0A;  // interval between two packets:  0.1ms x 10 = 1 ms
      txframe.data.u8[5] = 0xFF;  // always ff
      
      m_poll_txmsgid = m_poll_vwtp.txid;
      m_poll.moduleid_sent = m_poll_vwtp.txid;
      m_poll.moduleid_low = m_poll_vwtp.rxid;
      m_poll.moduleid_high = m_poll_vwtp.rxid;
      m_poll_wait = 2;

      m_poll_vwtp.state = VWTP_ChannelParams;
      m_poll_vwtp.bus->Write(&txframe);
      break;
      }

    case VWTP_ChannelClose:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: close channel bus=%d txid=%03X rxid=%03X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);
      
      txframe.MsgID = m_poll_vwtp.txid;
      txframe.FIR.B.DLC = 1;
      txframe.data.u8[0] = 0xA8;  // close request
      
      m_poll_txmsgid = m_poll_vwtp.txid;
      m_poll.moduleid_sent = m_poll_vwtp.txid;
      m_poll.moduleid_low = m_poll_vwtp.rxid;
      m_poll.moduleid_high = m_poll_vwtp.rxid;
      m_poll_wait = 2;

      m_poll_vwtp.state = VWTP_ChannelClose;
      m_poll_vwtp.bus->Write(&txframe);
      break;
      }

    case VWTP_Closed:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: channel closed bus=%d txid=%03X rxid=%03X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);
      m_poll_vwtp = {};
      m_poll_vwtp.state = VWTP_Closed;
      m_poll_wait = 0;
      break;
      }

    case VWTP_Idle:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: idle bus=%d txid=%03X rxid=%03X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);
      m_poll_vwtp.state = VWTP_Idle;
      m_poll_vwtp.lastused = esp_log_timestamp();
      m_poll_wait = 0;
      break;
      }

    case VWTP_StartPoll:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: start poll type=%02X pid=%X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.type, m_poll.entry.pid);

      if (m_poll.entry.xargs.tag == POLL_TXDATA)
        {
        m_poll_tx_data = m_poll.entry.xargs.data;
        m_poll_tx_remain = m_poll.entry.xargs.datalen;
        }
      else
        {
        m_poll_tx_data = m_poll.entry.args.data;
        m_poll_tx_remain = m_poll.entry.args.datalen;
        }
      m_poll_tx_frame = 0;
      m_poll_tx_offset = 0;

      m_poll.mlframe = 0;
      m_poll.mloffset = 0;
      m_poll.mlremain = 0;

      // fall through to VWTP_Transmit
      }
      FALLTHROUGH;
    case VWTP_Transmit:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: transmit frame=%u remain=%u",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll_tx_frame, m_poll_tx_remain);

      // Transmit next block of frames:
      txframe.MsgID = m_poll_vwtp.txid;
      int i;
      uint8_t opcode;
      for (int block = 1; block <= m_poll_vwtp.blocksize; block++)
        {
        i = 1;
        if (m_poll_tx_frame == 0)
          {
          // First frame:
          uint16_t txlen = m_poll_tx_remain + 1;
          txframe.data.u8[3] = m_poll.entry.type;
          if (POLL_TYPE_HAS_16BIT_PID(m_poll.entry.type))
            {
            txframe.data.u8[4] = m_poll.entry.pid >> 8;
            txframe.data.u8[5] = m_poll.entry.pid & 0xff;
            txlen += 2;
            i = 6;
            }
          else if (POLL_TYPE_HAS_8BIT_PID(m_poll.entry.type))
            {
            txframe.data.u8[4] = m_poll.entry.pid & 0xff;
            txlen += 1;
            i = 5;
            }
          else
            {
            i = 4;
            }
          txframe.data.u8[1] = txlen >> 8;
          txframe.data.u8[2] = txlen & 0xff;
          }

        while (i < 8 && m_poll_tx_remain > 0)
          {
          txframe.data.u8[i++] = m_poll_tx_data[m_poll_tx_offset++];
          m_poll_tx_remain--;
          }
        txframe.FIR.B.DLC = i;

        if (m_poll_tx_remain == 0)
          opcode = 0x10;    // last packet, waiting for ACK
        else if (block < m_poll_vwtp.blocksize)
          opcode = 0x20;    // more packets following in this block
        else
          opcode = 0x00;    // more packets following, waiting for ACK
        txframe.data.u8[0] = opcode | (m_poll_vwtp.txseqnr & 0x0f);

        if (m_poll_vwtp.txseqnr > 0)
          usleep(m_poll_vwtp.septime);

        m_poll_vwtp.txseqnr++;
        m_poll_tx_frame++;
        m_poll_vwtp.bus->Write(&txframe);

        if (m_poll_tx_remain == 0)
          break;
        }
      
      m_poll_txmsgid = m_poll_vwtp.txid;
      m_poll.moduleid_sent = m_poll_vwtp.txid;
      m_poll.moduleid_low = m_poll_vwtp.rxid;
      m_poll.moduleid_high = m_poll_vwtp.rxid;
      m_poll_wait = 2;
      m_poll_vwtp.state = VWTP_Transmit;
      m_poll_vwtp.lastused = esp_log_timestamp();
      break;
      }

    case VWTP_Receive:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: receive frame=%u remain=%u",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.mlframe, m_poll.mlremain);
      m_poll_vwtp.state = VWTP_Receive;
      m_poll_vwtp.lastused = esp_log_timestamp();
      m_poll_wait = 2;
      break;
      }

    case VWTP_AbortXfer:
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: abort xfer",
        m_poll.bus_no, m_poll_vwtp.moduleid);

      txframe.MsgID = m_poll_vwtp.txid;
      txframe.FIR.B.DLC = 1;
      if (m_poll_vwtp.state == VWTP_Receive)
        {
        txframe.data.u8[0] = 0x90 | (m_poll_vwtp.rxseqnr & 0x0f);  // ACK, stop sending
        }
      else
        {
        txframe.data.u8[0] = 0x30 | (m_poll_vwtp.txseqnr & 0x0f);  // last frame, abort
        m_poll_vwtp.txseqnr++;
        }
      
      m_poll_txmsgid = m_poll_vwtp.txid;
      m_poll.moduleid_sent = m_poll_vwtp.txid;
      m_poll.moduleid_low = m_poll_vwtp.rxid;
      m_poll.moduleid_high = m_poll_vwtp.rxid;

      m_poll_tx_remain = 0;
      m_poll.mlremain = 0;
      m_poll_vwtp.state = VWTP_Idle;
      m_poll_vwtp.lastused = esp_log_timestamp();
      m_poll_wait = 0;
      m_poll_vwtp.bus->Write(&txframe);
      break;
      }

    default:
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPEnter[%02X]: idle bus=%d txid=%03X rxid=%03X",
        m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.entry.pollbus, m_poll_vwtp.txid, m_poll_vwtp.rxid);
      m_poll_vwtp.state = VWTP_Idle;
      m_poll_wait = 0;
      break;
    }
  }


/**
 * PollerVWTPTxCallback: CAN transmission result (internal)
 */
void OvmsPoller::PollerVWTPTxCallback(const CAN_frame_t* frame, bool success)
  {
  if (!success)
    {
    // Todo: try to recover by repeating the transmission (unless CAN is offline)
    //  -- this needs a higher frequency ticker, e.g. 100 ms
    // For now assume channel is terminated:
    PollerVWTPEnter(VWTP_Closed);
    }
  }

#if __cplusplus >= 202002L
#define CAP_THIS ,this
#else
#define CAP_THIS
#endif

/**
 * PollerVWTPReceive: process VW-TP frame received (internal)
 */
bool OvmsPoller::PollerVWTPReceive(CAN_frame_t* frame, uint32_t msgid)
  {
  // OvmsRecMutexLock lock(&m_poll_mutex);

  // Log utility:
  auto logFrameDump = [= CAP_THIS](const char* msg)
    {
    char *hexdump = NULL;
    FormatHexDump(&hexdump, (const char*)frame->data.u8, frame->FIR.B.DLC, frame->FIR.B.DLC);
    ESP_LOGW(TAG, "PollerVWTPReceive[%02X]: %s: %s", m_poll_vwtp.moduleid, msg, hexdump ? hexdump : "-");
    if (hexdump) free(hexdump);
    };

  // Send channel keepalive response (0xA3 response):
  auto sendPong = [= CAP_THIS]()
    {
    CAN_frame_t txframe = {};
    txframe.callback = &m_poll_txcallback;
    txframe.FIR.B.FF = CAN_frame_std;
    txframe.MsgID = m_poll_vwtp.txid;
    txframe.FIR.B.DLC = 6;
    txframe.data.u8[0] = 0xA1;  // params response
    txframe.data.u8[1] = 0x0F;  // block size: 15 frames
    txframe.data.u8[2] = 0x8A;  // time to wait for ACK: 10ms x 10 = 100 ms
    txframe.data.u8[3] = 0xFF;  // always ff
    txframe.data.u8[4] = 0x0A;  // interval between two packets:  0.1ms x 10 = 1 ms
    txframe.data.u8[5] = 0xFF;  // always ff
    m_poll_vwtp.bus->Write(&txframe);
    };

  // Send ACK:
  auto sendAck = [= CAP_THIS]()
    {
    CAN_frame_t txframe = {};
    txframe.callback = &m_poll_txcallback;
    txframe.FIR.B.FF = CAN_frame_std;
    txframe.MsgID = m_poll_vwtp.txid;
    txframe.FIR.B.DLC = 1;
    txframe.data.u8[0] = 0xB0 | (m_poll_vwtp.rxseqnr & 0x0f); // ACK, continue
    m_poll_vwtp.bus->Write(&txframe);
    };


  // After locking the mutex, check again for poll expectance match:
  if (m_poll_vwtp.state == VWTP_Closed || !m_poll_vwtp.bus || msgid != m_poll_vwtp.rxid)
    {
    logFrameDump("dropping unexpected frame");
    return false;
    }

  switch (m_poll_vwtp.state)
    {
    case VWTP_ChannelSetup:
      {
      // Expecting setup response, check if the frame fits:
      uint8_t opcode = frame->data.u8[1];
      if (frame->FIR.B.DLC != 7 || opcode < 0xD0 || opcode > 0xD8)
        {
        logFrameDump("channel setup: invalid/unexpected frame");
        return false;
        }
      else if (opcode != 0xD0)
        {
        // Setup failed:
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPReceive[%02X]: channel setup failed opcode=%02X",
          m_poll.bus_no, m_poll_vwtp.moduleid, opcode);
        m_poll_vwtp.bus = NULL;
        m_poll_vwtp.state = VWTP_Closed;
        m_poll_wait = 0;
        }
      else
        {
        // Setup success, configure txid & rxid:
        m_poll_vwtp.rxid = (uint16_t)frame->data.u8[3] << 8 | frame->data.u8[2];
        m_poll_vwtp.txid = (uint16_t)frame->data.u8[5] << 8 | frame->data.u8[4];
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPReceive[%02X]: channel setup OK, assigned txid=%03X rxid=%03X",
          m_poll.bus_no, m_poll_vwtp.moduleid, m_poll_vwtp.txid, m_poll_vwtp.rxid);
        // …and send channel parameters:
        PollerVWTPEnter(VWTP_ChannelParams);
        }
      break;
      }

    case VWTP_ChannelParams:
      {
      // Expecting params response or channel abort:
      uint8_t opcode = (frame->FIR.B.DLC > 0) ? frame->data.u8[0] : 0;
      if (opcode == 0xA1)
        {
        // Params received, configure block size & timing:
        const int timeunit[4] = { 100, 1000, 10000, 100000 };
        m_poll_vwtp.blocksize = frame->data.u8[1];
        m_poll_vwtp.acktime = (frame->data.u8[2] & 0b00111111) * timeunit[(frame->data.u8[2] & 0b11000000) >> 6];
        m_poll_vwtp.septime = (frame->data.u8[4] & 0b00111111) * timeunit[(frame->data.u8[4] & 0b11000000) >> 6];
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPReceive[%02X]: channel params OK: bs=%d acktime=%" PRIu32 "us septime=%" PRIu32 "us",
          m_poll.bus_no, m_poll_vwtp.moduleid, m_poll_vwtp.blocksize, m_poll_vwtp.acktime, m_poll_vwtp.septime);
        // …and proceed to data transmission:
        PollerVWTPEnter(VWTP_StartPoll);
        }
      else if (opcode == 0xA8)
        {
        // Channel abort received, send ACK & set closed state:
        PollerVWTPEnter(VWTP_ChannelClose);
        PollerVWTPEnter(VWTP_Closed);
        // TODO: application error callback
        }
      else
        {
        logFrameDump("channel params: invalid/unexpected frame");
        return false;
        }
      break;
      }

    case VWTP_ChannelClose:
      {
      // Expecting channel close ACK:
      uint8_t opcode = (frame->FIR.B.DLC == 1) ? frame->data.u8[0] : 0;
      if (opcode == 0xA8)
        {
        // Close ACK received:
        PollerVWTPEnter(VWTP_Closed);
        // Check if we shall open another channel:
        if (m_poll.protocol == VWTP_20 && m_poll.entry.rxmoduleid != 0)
          PollerVWTPEnter(VWTP_ChannelSetup);
        else
          {
          OvmsRecMutexLock lock(&m_poll_mutex);
          m_polls.IncomingError(m_poll, POLLSINGLE_OK);
          }
        }
      else
        {
        logFrameDump("channel params: invalid/unexpected frame");
        return false;
        }
      break;
      }

    case VWTP_Idle:
      {
      uint8_t opcode = frame->data.u8[0];
      if (opcode == 0xA3)
        {
        // Channel ping received:
        sendPong();
        }
      else if (opcode == 0xA8)
        {
        // Channel abort received, send ACK & set closed state:
        PollerVWTPEnter(VWTP_ChannelClose);
        PollerVWTPEnter(VWTP_Closed);
        }
      else if ((opcode & 0xf0) <= 0x30)
        {
        // Out of band data frame received in idle state, send ACK/abort:
        m_poll_vwtp.rxseqnr++;
        CAN_frame_t txframe = {};
        txframe.callback = &m_poll_txcallback;
        txframe.FIR.B.FF = CAN_frame_std;
        txframe.MsgID = m_poll_vwtp.txid;
        txframe.FIR.B.DLC = 1;
        txframe.data.u8[0] = 0x90 | (m_poll_vwtp.rxseqnr & 0x0f); // ACK, abort
        m_poll_vwtp.bus->Write(&txframe);
        }
      break;
      }

    case VWTP_Transmit:
      {
      uint8_t opcode = (frame->FIR.B.DLC == 1) ? frame->data.u8[0] : 0;
      if ((opcode & 0xf0) == 0xB0)
        {
        // ACK, continue:
        if (m_poll_tx_remain > 0)
          PollerVWTPEnter(VWTP_Transmit);
        else
          PollerVWTPEnter(VWTP_Receive);
        }
      else if ((opcode & 0xf0) == 0x90)
        {
        // ACK, abort:
        ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPReceive[%02X]: got abort request during transmission at frame=%u remain=%u",
          m_poll.bus_no, m_poll_vwtp.moduleid, m_poll_tx_frame, m_poll_tx_remain);
        m_poll_tx_remain = 0;
        PollerVWTPEnter(VWTP_Idle);
        // TODO: application error callback
        }
      else if (opcode == 0xA3)
        {
        // Channel ping received:
        sendPong();
        }
      else if (opcode == 0xA8)
        {
        // Channel abort received, send ACK & set closed state:
        PollerVWTPEnter(VWTP_ChannelClose);
        PollerVWTPEnter(VWTP_Closed);
        // TODO: application error callback
        }
      else
        {
        // Possibly remaining frames from previously aborted xfer:
        logFrameDump("unhandled frame during transmission");
        }
      break;
      }

    case VWTP_Receive:
      {
      uint8_t opcode = frame->data.u8[0];
      if (opcode == 0xA3)
        {
        // Channel ping received:
        sendPong();
        }
      else if (opcode == 0xA8)
        {
        // Channel abort received, send ACK & set closed state:
        PollerVWTPEnter(VWTP_ChannelClose);
        PollerVWTPEnter(VWTP_Closed);
        // TODO: application error callback
        }
      else if (opcode < 0x40)
        {
        // 
        // Data frame received: check sequence number
        // 

        m_poll_vwtp.lastused = esp_log_timestamp();
        uint32_t rxseqnr = m_poll_vwtp.rxseqnr++;
        if ((opcode & 0x0f) != (rxseqnr & 0x0f))
          {
          logFrameDump("received out of sequence frame, abort");
          PollerVWTPEnter(VWTP_AbortXfer);
          return true;
          }
        
        // 
        // Get & validate OBD/UDS meta data
        // 
        
        uint8_t* tp_data;                   // TP frame data section address
        uint8_t  tp_datalen;                // TP frame data section length (0…7)
        uint8_t  response_type = 0;         // OBD/UDS response type tag (expected: 0x40 + request type)
        uint16_t response_pid = 0;          // OBD/UDS response PID (expected: request PID)
        uint8_t* response_data = NULL;      // OBD/UDS frame payload address
        uint16_t response_datalen = 0;      // OBD/UDS frame payload length (0…7)
        uint8_t  error_type = 0;            // OBD/UDS error response service type (expected: request type)
        uint8_t  error_code = 0;            // OBD/UDS error response code (see ISO 14229 Annex A.1)

        if (m_poll.mlframe == 0)
          {
          // First frame: extract & validate meta data
          // Note: upper nibble of byte 1 masked out, length assumed to by 12 bit
          //  and we've seen 0x80 on byte 1 for response type UDS_RESP_NRC_RCRRP
          m_poll.mlremain = (frame->data.u8[1] & 0x0f) << 8 | frame->data.u8[2];
          tp_data = &frame->data.u8[3];
          tp_datalen = frame->FIR.B.DLC - 3;
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
            response_pid = m_poll.pid;
            response_data = &tp_data[1];
            response_datalen = tp_datalen - 1;
            }
          }
        else
          {
          // Consecutive frame:
          tp_data = &frame->data.u8[1];
          tp_datalen = frame->FIR.B.DLC - 1;
          response_type = 0x40+m_poll.type;
          response_pid = m_poll.pid;
          response_data = tp_data;
          response_datalen = tp_datalen;
          }
        
        // 
        // Process OBD/UDS payload
        // 

        if (response_type == UDS_RESP_TYPE_NRC && error_type == m_poll.type)
          {
          // Send ACK?
          if ((opcode & 0xf0) <= 0x10)
            sendAck();

          // Negative Response Code:
          if (error_code == UDS_RESP_NRC_RCRRP)
            {
            // Info: requestCorrectlyReceived-ResponsePending (server busy processing the request)
            ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPReceive[%02X]: got OBD/UDS info %02X(%X) code=%02X (pending)",
                      m_poll.bus_no, m_poll_vwtp.moduleid, m_poll.type, m_poll.pid, error_code);
            // reset wait time:
            m_poll_wait = 2;
            return true;
            }
          else
            {
            // Error: forward to application:
            ESP_LOGD(TAG, "PollerVWTPReceive[%02X]: process OBD/UDS error %02X(%X) code=%02X",
                      m_poll_vwtp.moduleid, m_poll.type, m_poll.pid, error_code);

              {
              OvmsRecMutexLock lock(&m_poll_mutex);
              m_poll.moduleid_rec = m_poll.moduleid_sent;
              m_polls.IncomingError(m_poll, error_code);
              }
            // abort receive:
            m_poll.mlremain = 0;
            PollerVWTPEnter(VWTP_Idle);
            }
          }
        else if (response_type == 0x40+m_poll.type && response_pid == m_poll.pid)
          {
          // Send ACK?
          if ((opcode & 0xf0) <= 0x10)
            sendAck();

          // Normal matching poll response, forward to application:
          m_poll.mlremain -= tp_datalen;
          ESP_LOGD(TAG, "PollerVWTPReceive[%02X]: process OBD/UDS response %02X(%X) frm=%u len=%u off=%u rem=%u",
                    m_poll_vwtp.moduleid, m_poll.type, m_poll.pid,
                    m_poll.mlframe, response_datalen, m_poll.mloffset, m_poll.mlremain);
            {
            OvmsRecMutexLock lock(&m_poll_mutex);
            m_poll.moduleid_rec = msgid;
            m_polls.IncomingPacket(m_poll, response_data, response_datalen);
            }
          }
        else
          {
          // Response type / PID mismatch: log & abort
          char *hexdump = NULL;
          FormatHexDump(&hexdump, (const char*)frame->data.u8, frame->FIR.B.DLC, frame->FIR.B.DLC);
          ESP_LOGW(TAG, "PollerVWTPReceive[%02X]: OBD/UDS response type/PID mismatch, got %02X(%X) vs %02X(%X) => ignoring: %s",
                    m_poll_vwtp.moduleid, response_type, response_pid, 0x40+m_poll.type, m_poll.pid, hexdump ? hexdump : "-");
          if (hexdump) free(hexdump);
          PollerVWTPEnter(VWTP_AbortXfer);
          return false;
          }
        
        // Do we expect more data?
        if (m_poll.mlremain)
          {
          m_poll.mlframe++;
          m_poll.mloffset += response_datalen;
          m_poll_wait = 2;
          }
        else
          {
          // Request response complete:
          PollerVWTPEnter(VWTP_Idle);
          }
        }
      else
        {
        logFrameDump("unhandled frame during reception");
        }
      break;
      }

    default:
      // Ignore all frames received in other states
      break;
    }

  if (m_poll_wait == 0)
    {
    // Succeeded - No more expected so check to send the next poll
    PollerSucceededPollNext();
    }

  return true;
  }


/**
 * PollerVWTPTicker: per second channel maintenance (internal)
 */
void OvmsPoller::PollerVWTPTicker()
  {
  uint32_t currenttime_ms = esp_log_timestamp();
  uint32_t timediff_ms = currenttime_ms - m_poll_vwtp.lastused;

  // As a VWTP operation may start at any time between two Ticker runs,
  // we need to adjust the wait time if the ticker offset is too short:
  if (m_poll_wait == 1 && timediff_ms < 200)
    {
    m_poll_wait = 2;
    return;
    }

  if (m_poll_wait > 0)
    {
    // State timeout?
    if (m_poll_vwtp.state == VWTP_ChannelSetup || m_poll_vwtp.state == VWTP_ChannelParams)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPTicker[%02X]: setup/params timeout",
        m_poll.bus_no, m_poll_vwtp.moduleid);
      PollerVWTPEnter(VWTP_Closed);
      }
    else if (m_poll_vwtp.state == VWTP_ChannelClose)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPTicker[%02X]: close timeout",
         m_poll.bus_no,  m_poll_vwtp.moduleid);
      PollerVWTPEnter(VWTP_Closed);
      if (m_poll.protocol == VWTP_20)
        PollerVWTPStart(true);
      }
    }
  else
    {
    // Poll timeout:
    if (m_poll_vwtp.state != VWTP_Closed && m_poll_vwtp.state != VWTP_Idle)
      {
      ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPTicker[%02X]: poll timeout",
        m_poll.bus_no, m_poll_vwtp.moduleid);
      PollerVWTPEnter(VWTP_ChannelClose);
      }
    }

  // Keepalive timeout?
  if (m_poll_vwtp.state == VWTP_Idle && m_poll_ch_keepalive > 0 &&
      timediff_ms > m_poll_ch_keepalive * 1000)
    {
    ESP_LOGD(TAG, "[%" PRIu8 "]PollerVWTPTicker[%02X]: channel inactivity timeout",
      m_poll.bus_no, m_poll_vwtp.moduleid);
    PollerVWTPEnter(VWTP_ChannelClose);
    }
  }
