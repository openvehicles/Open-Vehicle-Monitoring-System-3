/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump framework
;    Date:          18th January 2018
;
;    (C) 2018       Mark Webb-Johnson
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
static const char *TAG = "canformat-pcap";

#include <errno.h>
#include <endian.h>
#include "pcp.h"
#include "canformat_pcap.h"

class OvmsCanFormatPCAPInit
  {
  public: OvmsCanFormatPCAPInit();
} MyOvmsCanFormatPCAPInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatPCAPInit::OvmsCanFormatPCAPInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: PCAP (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_pcap>("pcap");
  }

canformat_pcap::canformat_pcap(const char* type)
  : canformat(type)
  {
  m_bufpos = 0;
  m_discarding = false;
  }

canformat_pcap::~canformat_pcap()
  {
  }

std::string canformat_pcap::get(CAN_log_message_t* message)
  {
  m_bufpos = 0;

  if (message->type != CAN_LogFrame_RX)
    {
    return std::string("");
    }
    
  pcaprec_can_t* m = (pcaprec_can_t*)&m_buf;
  memset(m_buf,0,sizeof(pcaprec_can_t));

  m->hdr.ts_sec = htobe32(message->timestamp.tv_sec);
  m->hdr.ts_usec = htobe32(message->timestamp.tv_usec);
  m->hdr.incl_len = htobe32(16);
  m->hdr.orig_len = htobe32(16);

  uint32_t idfl = message->frame.MsgID;
  if (message->frame.FIR.B.FF == CAN_frame_ext) idfl |= CANFORMAT_PCAP_FL_EXT;
  if (message->frame.FIR.B.RTR == CAN_RTR) idfl |= CANFORMAT_PCAP_FL_RTR;
  m->phdr.idflags = htobe32(idfl);
  m->phdr.len = message->frame.FIR.B.DLC;

  memcpy(m->data, message->frame.data.u8, message->frame.FIR.B.DLC);

  return std::string((const char*)m, sizeof(pcaprec_can_t));
  }

std::string canformat_pcap::getheader(struct timeval *time)
  {
  m_bufpos = 0;
  struct timeval t;

  if (time == NULL)
    {
    gettimeofday(&t,NULL);
    time = &t;
    }

  pcap_hdr_t* h = (pcap_hdr_t*)&m_buf;
  memset(m_buf,0,sizeof(pcap_hdr_t));
  h->magic_number = htobe32(0xa1b2c3d4);
  h->version_major = htobe16(2);
  h->version_minor = htobe16(4);
  h->snaplen = htobe32(8);
  h->network = htobe32(0xe3);

  return std::string((const char*)h, sizeof(pcap_hdr_t));
  }

size_t canformat_pcap::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  // Check if we've failed somewhere and are simply discarding
  if (m_discarding)
    {
    message->origin = NULL;
    return len;
    }

  // I need 24 bytes, to make a decision as to what this is...
  size_t remain = 24 - m_bufpos;
  size_t consumed = 0;
  if (remain > 0)
    {
    if (len < remain)
      {
      // Just bring in what we can..
      memcpy(m_buf+m_bufpos, buffer, len);
      m_bufpos += len;
      message->origin = NULL;
      return len;
      }
    else
      {
      memcpy(m_buf+m_bufpos, buffer, remain);
      m_bufpos += remain;
      buffer += remain;
      len -= remain;
      consumed += remain;
      }
    }

  // At this point, we have our 24 bytes...
  pcap_hdr_t* h = (pcap_hdr_t*)&m_buf;
  uint32_t magic = be32toh(h->magic_number);
  if (magic == 0xa1b2c3d4)
    {
    // This is a PCAP header - just ignore it
    if (be32toh(h->network) != 0xe3) m_discarding = true;
    m_bufpos = 0;
    message->origin = NULL;
    return consumed;
    }
  else if ((magic == 0xd4c3b2a1)||
           (magic == 0xa1b23c4d)||
           (magic == 0x4d3cb2a1))
    {
    ESP_LOGW(TAG,"pcap format %08x not supported - ignoring data stream",magic);
    m_discarding = true;
    m_bufpos = 0;
    message->origin = NULL;
    return consumed;
    }

  // We must have a message, so need 32 bytes...
  remain = 32 - m_bufpos;
  if (len < remain)
    {
    // Just bring in what we can..
    memcpy(m_buf+m_bufpos, buffer, len);
    m_bufpos += len;
    message->origin = NULL;
    return consumed+len;
    }
  else
    {
    memcpy(m_buf+m_bufpos, buffer, remain);
    m_bufpos += remain;
    buffer += remain;
    len -= remain;
    consumed += remain;
    }

  // At this point, we have our 32 bytes...
  pcaprec_can_t* m = (pcaprec_can_t*)&m_buf;
  memset(message,0,sizeof(CAN_log_message_t));

  uint32_t idf = be32toh(m->phdr.idflags);
  if (idf & CANFORMAT_PCAP_FL_MSG)
    {
    // Just ignore it
    m_bufpos = 0;
    return consumed;
    }
  message->type = CAN_LogFrame_RX;
  message->frame.FIR.B.RTR = (idf & CANFORMAT_PCAP_FL_RTR)?CAN_RTR:CAN_no_RTR;
  message->frame.FIR.B.FF = (idf & CANFORMAT_PCAP_FL_EXT)?CAN_frame_ext:CAN_frame_std;
  message->frame.MsgID = idf & CANFORMAT_PCAP_FL_MASK;
  message->origin = (canbus*)MyPcpApp.FindDeviceByName("can1");
  message->frame.FIR.B.DLC = m->phdr.len;
  memcpy(message->frame.data.u8, m->data, m->phdr.len);

  m_bufpos = 0;
  return consumed;
  }
