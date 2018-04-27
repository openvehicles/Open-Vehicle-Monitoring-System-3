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
//static const char *TAG = "candump-pcap";

#include <errno.h>
#include <endian.h>
#include "pcp.h"
#include "candump_pcap.h"

candump_pcap::candump_pcap()
  {
  m_bufpos = 0;
  m_discarding = false;
  }

candump_pcap::~candump_pcap()
  {
  }

const char* candump_pcap::formatname()
  {
  return "pcap";
  }

std::string candump_pcap::get(struct timeval *time, CAN_frame_t *frame)
  {
  m_bufpos = 0;

  pcaprec_can_t* m = (pcaprec_can_t*)&m_buf;
  memset(m_buf,0,sizeof(pcaprec_can_t));

  m->hdr.ts_sec = htobe32(time->tv_sec);
  m->hdr.ts_usec = htobe32(time->tv_usec);
  m->hdr.incl_len = htobe32(16);
  m->hdr.orig_len = htobe32(16);

  uint32_t idfl = frame->MsgID;
  if (frame->FIR.B.FF == CAN_frame_ext) idfl |= CANDUMP_PCAP_FL_EXT;
  if (frame->FIR.B.RTR == CAN_RTR) idfl |= CANDUMP_PCAP_FL_RTR;
  m->phdr.idflags = htobe32(idfl);
  m->phdr.len = frame->FIR.B.DLC;

  memcpy(m->data, frame->data.u8, frame->FIR.B.DLC);

  return std::string((const char*)m, sizeof(pcaprec_can_t));
  }

std::string candump_pcap::getheader(struct timeval *time)
  {
  m_bufpos = 0;

  pcap_hdr_t* h = (pcap_hdr_t*)&m_buf;
  memset(m_buf,0,sizeof(pcap_hdr_t));
  h->magic_number = htobe32(0xa1b2c3d4);
  h->version_major = htobe16(2);
  h->version_minor = htobe16(4);
  h->snaplen = htobe32(8);
  h->network = htobe32(0xe3);

  return std::string((const char*)h, sizeof(pcap_hdr_t));
  }

size_t candump_pcap::put(CAN_frame_t *frame, uint8_t *buffer, size_t len)
  {
  // Check if we've failed somewhere and are simply discarding
  if (m_discarding)
    {
    frame->origin = NULL;
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
      frame->origin = NULL;
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
  if (be32toh(h->magic_number) == 0xa1b2c3d4)
    {
    // This is a PCAP header - just ignore it
    if (be32toh(h->network) != 0xe3) m_discarding = true;
    m_bufpos = 0;
    frame->origin = NULL;
    return consumed;
    }

  // We must have a message, so need 32 bytes...
  remain = 32 - m_bufpos;
  if (len < remain)
    {
    // Just bring in what we can..
    memcpy(m_buf+m_bufpos, buffer, len);
    m_bufpos += len;
    frame->origin = NULL;
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
  memset(frame,0,sizeof(CAN_frame_t));

  uint32_t idf = be32toh(m->phdr.idflags);
  if (idf & CANDUMP_PCAP_FL_MSG)
    {
    // Just ignore it
    frame->origin = NULL;
    m_bufpos = 0;
    return consumed;
    }
  frame->FIR.B.RTR = (idf & CANDUMP_PCAP_FL_RTR)?CAN_RTR:CAN_no_RTR;
  frame->FIR.B.FF = (idf & CANDUMP_PCAP_FL_EXT)?CAN_frame_ext:CAN_frame_std;
  frame->MsgID = idf & CANDUMP_PCAP_FL_MASK;
  frame->origin = (canbus*)MyPcpApp.FindDeviceByName("can1");
  frame->FIR.B.DLC = m->phdr.len;
  memcpy(frame->data.u8, m->data, m->phdr.len);

  m_bufpos = 0;
  return consumed;
  }
