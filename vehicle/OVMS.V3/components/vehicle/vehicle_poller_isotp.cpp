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
static const char *TAG = "vehicle-isotp";

#include <stdio.h>
#include <algorithm>
#include "vehicle.h"


/**
 * PollerISOTPStart: start ISO-TP request
 */
void OvmsVehicle::PollerISOTPStart(bool fromTicker)
  {
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

  ESP_LOGD(TAG, "PollerISOTPStart(%d): send [bus=%d, type=%02X, pid=%X], expecting %03x/%03x-%03x",
           fromTicker, m_poll_plcur->pollbus, m_poll_type, m_poll_pid, m_poll_moduleid_sent,
           m_poll_moduleid_low, m_poll_moduleid_high);

  //
  // Assemble ISO-TP single/first frame
  //

  uint8_t* fr_data;               // Frame data address
  uint8_t  fr_maxlen;             // Frame data max length
  uint16_t tp_len;                // TP payload length including this frame (0…4095)
  uint8_t* tp_data;               // TP frame data section address
  uint8_t  tp_datalen;            // TP frame data section length (0…7)

  const uint8_t* tx_data;         // Payload data
  uint16_t tx_datalen;            // Payload data length
  uint16_t tx_datasent;           // Payload data length sent with this frame

  if (m_poll_plcur->xargs.tag == POLL_TXDATA)
    {
    tx_data = m_poll_plcur->xargs.data;
    tx_datalen = m_poll_plcur->xargs.datalen;
    }
  else
    {
    tx_data = m_poll_plcur->args.data;
    tx_datalen = m_poll_plcur->args.datalen;
    }

  CAN_frame_t txframe = {};
  txframe.origin = m_poll_bus;
  txframe.callback = &m_poll_txcallback;
  txframe.FIR.B.DLC = 8;
  std::fill_n(txframe.data.u8, sizeof_array(txframe.data.u8), 0x55);

  if (m_poll_protocol == ISOTP_EXTFRAME)
    txframe.FIR.B.FF = CAN_frame_ext;
  else
    txframe.FIR.B.FF = CAN_frame_std;

  if (m_poll_protocol == ISOTP_EXTADR)
    {
    txframe.MsgID = m_poll_moduleid_sent >> 8;
    txframe.data.u8[0] = m_poll_moduleid_sent & 0xff;
    fr_data = &txframe.data.u8[1];
    fr_maxlen = 7;
    }
  else
    {
    txframe.MsgID = m_poll_moduleid_sent;
    fr_data = &txframe.data.u8[0];
    fr_maxlen = 8;
    }

  // Do we need to split this request into multiple frames?
  if (POLL_TYPE_HAS_16BIT_PID(m_poll_plcur->type))
    tp_len = 3 + tx_datalen;
  else if (POLL_TYPE_HAS_8BIT_PID(m_poll_plcur->type))
    tp_len = 2 + tx_datalen;
  else
    tp_len = 1 + tx_datalen;

  if (tp_len <= fr_maxlen - 1)
    {
    fr_data[0] = (ISOTP_FT_SINGLE << 4) + tp_len;
    tp_data = &fr_data[1];
    tp_datalen = fr_maxlen - 1;
    }
  else
    {
    fr_data[0] = (ISOTP_FT_FIRST << 4) + ((tp_len & 0x0f00) >> 8);
    fr_data[1] = tp_len & 0xff;
    tp_data = &fr_data[2];
    tp_datalen = fr_maxlen - 2;
    }

  // Add TP data:
  if (POLL_TYPE_HAS_16BIT_PID(m_poll_plcur->type))
    {
    tp_data[0] = m_poll_type;
    tp_data[1] = m_poll_pid >> 8;
    tp_data[2] = m_poll_pid & 0xff;
    tx_datasent = LIMIT_MAX(tx_datalen, tp_datalen - 3);
    memcpy(&tp_data[3], tx_data, tx_datasent);
    }
  else if (POLL_TYPE_HAS_8BIT_PID(m_poll_plcur->type))
    {
    tp_data[0] = m_poll_type;
    tp_data[1] = m_poll_pid;
    tx_datasent = LIMIT_MAX(tx_datalen, tp_datalen - 2);
    memcpy(&tp_data[2], tx_data, tx_datasent);
    }
  else
    {
    tp_data[0] = m_poll_type;
    tx_datasent = LIMIT_MAX(tx_datalen, tp_datalen - 1);
    memcpy(&tp_data[1], tx_data, tx_datasent);
    }

  m_poll_txmsgid = txframe.MsgID;
  m_poll_tx_frame = 0;
  m_poll_tx_data = tx_data;
  m_poll_tx_offset = tx_datasent;
  m_poll_tx_remain = tx_datalen - tx_datasent;
  m_poll_ml_frame = 0;
  m_poll_ml_offset = 0;
  m_poll_ml_remain = 0;
  m_poll_wait = 2;

  m_poll_bus->Write(&txframe);
  }


/**
 * PollerISOTPReceive: process ISO-TP poll response frame
 */
bool OvmsVehicle::PollerISOTPReceive(CAN_frame_t* frame, uint32_t msgid)
  {
  OvmsRecMutexLock lock(&m_poll_mutex);
  char *hexdump = NULL;

  // After locking the mutex, check again for poll expectance match:
  if (!m_poll_wait || !m_poll_plist || frame->origin != m_poll_bus ||
      msgid < m_poll_moduleid_low || msgid > m_poll_moduleid_high)
    {
    ESP_LOGD(TAG, "PollerISOTPReceive[%03X]: dropping expired poll response", msgid);
    return false;
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

  uint8_t  tp_fc_command;         // Flow control command (0 = continue, 1 = wait, 2 = abort)
  uint8_t  tp_fc_framecnt;        // Flow control max frame count (0 = unlimited)
  uint8_t  tp_fc_septime;         // Flow control frame separation time

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
    case ISOTP_FT_FLOWCTRL:
      tp_fc_command  = fr_data[0] & 0x0f;
      tp_fc_framecnt = fr_data[1];
      tp_fc_septime  = fr_data[2];
      break;
    default:
      {
      // This is most likely an indication there is a non ISO-TP device sending
      // in our expected RX ID range, so we log the frame and abort:
      FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
      ESP_LOGW(TAG, "PollerISOTPReceive[%03X]: ignoring unknown/invalid ISO TP frame: %s",
               msgid, hexdump ? hexdump : "-");
      if (hexdump) free(hexdump);
      return false;
      }
    }

  // Handle TX flow control:
  if (tp_frametype == ISOTP_FT_FLOWCTRL)
    {
    if (tp_fc_command > 2 || m_poll_tx_remain == 0)
      {
      FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
      ESP_LOGW(TAG, "PollerISOTPReceive[%03X]: ignoring unexpected/invalid ISO TP flow control frame: %s",
              msgid, hexdump ? hexdump : "-");
      if (hexdump) free(hexdump);
      return false;
      }

    if (tp_fc_command == 1)
      {
      // add some wait time:
      m_poll_wait++;
      }
    else if (tp_fc_command == 2)
      {
      // abort TX:
      m_poll_tx_remain = 0;
      // (but still wait for response)
      }
    else
      {
      // continue TX:
      CAN_frame_t tx_frame = {};
      uint8_t* tx_data;
      uint8_t tx_datalen;
      uint8_t tx_datasent;
      uint32_t txid;
      tx_frame.origin = frame->origin;
      tx_frame.FIR.B.DLC = 8;

      if (m_poll_protocol == ISOTP_EXTFRAME)
        tx_frame.FIR.B.FF = CAN_frame_ext;
      else
        tx_frame.FIR.B.FF = CAN_frame_std;

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
        tx_frame.MsgID = txid >> 8;
        tx_frame.data.u8[0] = txid & 0xff;
        tx_data = &tx_frame.data.u8[1];
        tx_datalen = 6;
        }
      else
        {
        tx_frame.MsgID = txid;
        tx_data = &tx_frame.data.u8[0];
        tx_datalen = 7;
        }

      // Send next chunk of frames:
      while (m_poll_tx_remain > 0)
        {
        ++m_poll_tx_frame;
        tx_data[0] = (ISOTP_FT_CONSECUTIVE << 4) + (m_poll_tx_frame & 0x0f);
        tx_datasent = LIMIT_MAX(m_poll_tx_remain, tx_datalen);
        memcpy(&tx_data[1], m_poll_tx_data+m_poll_tx_offset, tx_datasent);
        if (tx_datasent < tx_datalen)
          memset(&tx_data[1+tx_datasent], 0x55, tx_datalen-tx_datasent);
        tx_frame.Write();
        m_poll_tx_offset += tx_datasent;
        m_poll_tx_remain -= tx_datasent;

        if (m_poll_tx_remain == 0)
          break;
        if (tp_fc_framecnt > 0 && --tp_fc_framecnt == 0)
          break;

        if (tp_fc_septime > 0)
          {
          if (tp_fc_septime <= 127)
            usleep(tp_fc_septime * 1000);
          else if (tp_fc_septime > 240)
            usleep((tp_fc_septime - 240) * 100);
          }
        }

      if (m_poll_tx_remain > 0)
        m_poll_wait = 2;
      }

    return true;
    } // if (tp_frametype == ISOTP_FT_FLOWCTRL)


  // Check frame index:
  if (tp_frametype == ISOTP_FT_CONSECUTIVE)
    {
    // Note: we tolerate an index less than the expected one, as some devices
    //  begin counting at the first consecutive frame
    if (m_poll_ml_remain == 0 || tp_frameindex > (m_poll_ml_frame & 0x0f))
      {
      FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
      ESP_LOGW(TAG, "PollerISOTPReceive[%03X]: unexpected/out of sequence ISO TP frame (%d vs %d), aborting poll %02X(%X): %s",
              msgid, tp_frameindex, m_poll_ml_frame & 0x0f, m_poll_type, m_poll_pid,
              hexdump ? hexdump : "-");
      if (hexdump) free(hexdump);
      m_poll_moduleid_low = m_poll_moduleid_high = 0; // ignore further frames
      m_poll_wait = 2; // give the bus time to let remaining frames pass
      return true;
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
      ESP_LOGD(TAG, "PollerISOTPReceive[%03X]: got OBD/UDS info %02X(%X) code=%02X (pending)",
               msgid, m_poll_type, m_poll_pid, error_code);
      // add some wait time:
      m_poll_wait++;
      return true;
      }
    else
      {
      // Error: forward to application:
      ESP_LOGD(TAG, "PollerISOTPReceive[%03X]: process OBD/UDS error %02X(%X) code=%02X",
               msgid, m_poll_type, m_poll_pid, error_code);
      // Running single poll?
      if (m_poll_single_rxbuf)
        {
        m_poll_single_rxerr = error_code;
        m_poll_single_rxbuf = NULL;
        m_poll_single_rxdone.Give();
        }
      else
        {
        IncomingPollError(frame->origin, m_poll_type, m_poll_pid, error_code);
        }
      // abort:
      m_poll_ml_remain = 0;
      }
    }
  else if (response_type == 0x40+m_poll_type && response_pid == m_poll_pid)
    {
    // Normal matching poll response, forward to application:
    m_poll_ml_remain = tp_len - tp_datalen;
    ESP_LOGD(TAG, "PollerISOTPReceive[%03X]: process OBD/UDS response %02X(%X) frm=%u len=%u off=%u rem=%u",
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
    else
      {
      IncomingPollReply(frame->origin, m_poll_type, m_poll_pid, response_data, response_datalen, m_poll_ml_remain);
      }
    }
  else
    {
    // This is most likely a late response to a previous poll, log & skip:
    FormatHexDump(&hexdump, (const char*)frame->data.u8, 8, 8);
    ESP_LOGW(TAG, "PollerISOTPReceive[%03X]: OBD/UDS response type/PID mismatch, got %02X(%X) vs %02X(%X) => ignoring: %s",
             msgid, response_type, response_pid, 0x40+m_poll_type, m_poll_pid, hexdump ? hexdump : "-");
    if (hexdump) free(hexdump);
    return false;
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
      txframe.FIR.B.DLC = 8;

      if (m_poll_protocol == ISOTP_EXTFRAME)
        txframe.FIR.B.FF = CAN_frame_ext;
      else
        txframe.FIR.B.FF = CAN_frame_std;

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

  return true;
  }
