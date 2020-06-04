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

#include "canformat_pcap.h"
#include <errno.h>
#include <endian.h>
#include "pcp.h"

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
  }

canformat_pcap::~canformat_pcap()
  {
  }

std::string canformat_pcap::get(CAN_log_message_t* message)
  {
  pcaprec_can_t m;

  if (message->type != CAN_LogFrame_RX)
    {
    return std::string("");
    }

  memset(&m,0,sizeof(m));

  m.hdr.ts_sec = htobe32(message->timestamp.tv_sec);
  m.hdr.ts_usec = htobe32(message->timestamp.tv_usec);
  m.hdr.incl_len = htobe32(16);
  m.hdr.orig_len = htobe32(16);

  uint32_t idfl = message->frame.MsgID;
  if (message->frame.FIR.B.FF == CAN_frame_ext) idfl |= CANFORMAT_PCAP_FL_EXT;
  if (message->frame.FIR.B.RTR == CAN_RTR) idfl |= CANFORMAT_PCAP_FL_RTR;
  m.phdr.idflags = htobe32(idfl);
  m.phdr.len = message->frame.FIR.B.DLC;

  memcpy(m.data, message->frame.data.u8, message->frame.FIR.B.DLC);

  return std::string((const char*)&m, sizeof(m));
  }

std::string canformat_pcap::getheader(struct timeval *time)
  {
  pcap_hdr_t h;
  struct timeval t;

  if (time == NULL)
    {
    gettimeofday(&t,NULL);
    time = &t;
    }

  memset(&h,0,sizeof(h));
  h.magic_number = htobe32(0xa1b2c3d4);
  h.version_major = htobe16(2);
  h.version_minor = htobe16(4);
  h.snaplen = htobe32(8);
  h.network = htobe32(0xe3);

  return std::string((const char*)&h, sizeof(h));
  }

size_t canformat_pcap::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata)
  {
  union
    {
    pcap_hdr_t header;
    pcaprec_can_t record;
    } m;

  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible

  if (m_buf.UsedSpace() < 24) return consumed; // Insufficient data so far

  // At this point, we have our 24 bytes...
  m_buf.Peek(24,(uint8_t*)&m);
  uint32_t magic = be32toh(m.header.magic_number);
  if (magic == 0xa1b2c3d4)
    {
    // This is a PCAP header - just ignore it
    if (be32toh(m.header.network) != 0xe3)
      {
      ESP_LOGE(TAG,"PCAP header network != 0xe3: Discarding");
      SetServeDiscarding(true);
      return consumed;
      }
    m_buf.Pop(24,(uint8_t*)&m);
    }
  else if ((magic == 0xd4c3b2a1)||
           (magic == 0xa1b23c4d)||
           (magic == 0x4d3cb2a1))
    {
    ESP_LOGE(TAG,"pcap format %08x not supported: Discarding",magic);
    SetServeDiscarding(true);
    return consumed;
    }

  if (m_buf.UsedSpace() < sizeof(pcaprec_can_t)) return consumed; // Insufficient data so far

  m_buf.Pop(sizeof(pcaprec_can_t), (uint8_t*)&m);

  uint32_t idf = be32toh(m.record.phdr.idflags);
  if (idf & CANFORMAT_PCAP_FL_MSG)
    {
    // Just ignore it
    return consumed;
    }
  message->type = CAN_LogFrame_RX;
  message->frame.FIR.B.RTR = (idf & CANFORMAT_PCAP_FL_RTR)?CAN_RTR:CAN_no_RTR;
  message->frame.FIR.B.FF = (idf & CANFORMAT_PCAP_FL_EXT)?CAN_frame_ext:CAN_frame_std;
  message->frame.MsgID = idf & CANFORMAT_PCAP_FL_MASK;
  message->origin = MyCan.GetBus(0);
  message->frame.FIR.B.DLC = m.record.phdr.len;
  memcpy(message->frame.data.u8, m.record.data, m.record.phdr.len);

  return consumed;
  }
