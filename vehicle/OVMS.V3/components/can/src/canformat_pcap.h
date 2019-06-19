/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump CRTD format
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

#ifndef __CANFORMAT_PCAP_H__
#define __CANFORMAT_PCAP_H__

#include "canformat.h"

#define CANFORMAT_PCAP_MAXLEN 32

typedef struct __attribute__ ((__packed__))
  {
  uint32_t magic_number;  /* magic number */
  uint16_t version_major; /* major version number */
  uint16_t version_minor; /* minor version number */
  uint32_t thiszone;      /* GMT to local correction */
  uint32_t sigfigs;       /* accuracy of timestamps */
  uint32_t snaplen;       /* max length of captured packets, in octets */
  uint32_t network;       /* data link type */
  } pcap_hdr_t;

typedef struct __attribute__ ((__packed__))
  {
  uint32_t ts_sec;        /* timestamp seconds */
  uint32_t ts_usec;       /* timestamp microseconds */
  uint32_t incl_len;      /* number of octets of packet saved in file */
  uint32_t orig_len;      /* actual length of packet */
  } pcaprec_hdr_t;

typedef struct __attribute__ ((__packed__))
  {
  uint32_t idflags;       /* CAN ID and flags */
  uint8_t len;            /* frame payload length */
  uint8_t padding1;       /* padding */
  uint8_t padding2;       /* padding */
  uint8_t padding3;       /* padding */
  } pcaprec_canphdr_t;

typedef struct __attribute__ ((__packed__))
  {
  pcaprec_hdr_t hdr;
  pcaprec_canphdr_t phdr;
  uint8_t data[8];
  } pcaprec_can_t;

#define CANFORMAT_PCAP_FL_MSG  0x20000000
#define CANFORMAT_PCAP_FL_RTR  0x40000000
#define CANFORMAT_PCAP_FL_EXT  0x80000000
#define CANFORMAT_PCAP_FL_MASK 0x1fffffff

class canformat_pcap : public canformat
  {
  public:
    canformat_pcap(const char* type);
    virtual ~canformat_pcap();

  protected:
    uint8_t m_buf[CANFORMAT_PCAP_MAXLEN];
    size_t m_bufpos;
    bool m_discarding;

  public:
    virtual std::string get(CAN_log_message_t* message);
    virtual std::string getheader(struct timeval *time);
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len);
  };

#endif // __CANFORMAT_PCAP_H__
