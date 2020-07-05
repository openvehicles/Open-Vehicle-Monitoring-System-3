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
static const char *TAG = "gsm-mux";

#include <string.h>
#include "gsmmux.h"
#include "simcom.h"

#define GSM_CR         0x02
#define GSM_EA         0x01
#define GSM_PF         0x10

#define GSM_RR         0x01
#define GSM_UI         0x03
#define GSM_RNR        0x05
#define GSM_REJ        0x09
#define GSM_DM         0x0F
#define GSM_SABM       0x2F
#define GSM_DISC       0x43
#define GSM_UA         0x63
#define GSM_UIH        0xEF

#define GSM_CMD_NSC    0x09
#define GSM_CMD_TEST   0x11
#define GSM_CMD_PSC    0x21
#define GSM_CMD_RLS    0x29
#define GSM_CMD_FCOFF  0x31
#define GSM_CMD_PN     0x41
#define GSM_CMD_RPN    0x49
#define GSM_CMD_FCON   0x51
#define GSM_CMD_CLD    0x61
#define GSM_CMD_SNC    0x69
#define GSM_CMD_MSC    0x71

#define GSM_MDM_FC     0x01
#define GSM_MDM_RTC    0x02
#define GSM_MDM_RTR    0x04
#define GSM_MDM_IC     0x20
#define GSM_MDM_DV     0x40

#define GSM0_SOF       0xF9
#define XON            0x11
#define XOFF           0x13

static const uint8_t gsm_fcs8[256] =
  {
  0x00, 0x91, 0xE3, 0x72, 0x07, 0x96, 0xE4, 0x75,
  0x0E, 0x9F, 0xED, 0x7C, 0x09, 0x98, 0xEA, 0x7B,
  0x1C, 0x8D, 0xFF, 0x6E, 0x1B, 0x8A, 0xF8, 0x69,
  0x12, 0x83, 0xF1, 0x60, 0x15, 0x84, 0xF6, 0x67,
  0x38, 0xA9, 0xDB, 0x4A, 0x3F, 0xAE, 0xDC, 0x4D,
  0x36, 0xA7, 0xD5, 0x44, 0x31, 0xA0, 0xD2, 0x43,
  0x24, 0xB5, 0xC7, 0x56, 0x23, 0xB2, 0xC0, 0x51,
  0x2A, 0xBB, 0xC9, 0x58, 0x2D, 0xBC, 0xCE, 0x5F,
  0x70, 0xE1, 0x93, 0x02, 0x77, 0xE6, 0x94, 0x05,
  0x7E, 0xEF, 0x9D, 0x0C, 0x79, 0xE8, 0x9A, 0x0B,
  0x6C, 0xFD, 0x8F, 0x1E, 0x6B, 0xFA, 0x88, 0x19,
  0x62, 0xF3, 0x81, 0x10, 0x65, 0xF4, 0x86, 0x17,
  0x48, 0xD9, 0xAB, 0x3A, 0x4F, 0xDE, 0xAC, 0x3D,
  0x46, 0xD7, 0xA5, 0x34, 0x41, 0xD0, 0xA2, 0x33,
  0x54, 0xC5, 0xB7, 0x26, 0x53, 0xC2, 0xB0, 0x21,
  0x5A, 0xCB, 0xB9, 0x28, 0x5D, 0xCC, 0xBE, 0x2F,
  0xE0, 0x71, 0x03, 0x92, 0xE7, 0x76, 0x04, 0x95,
  0xEE, 0x7F, 0x0D, 0x9C, 0xE9, 0x78, 0x0A, 0x9B,
  0xFC, 0x6D, 0x1F, 0x8E, 0xFB, 0x6A, 0x18, 0x89,
  0xF2, 0x63, 0x11, 0x80, 0xF5, 0x64, 0x16, 0x87,
  0xD8, 0x49, 0x3B, 0xAA, 0xDF, 0x4E, 0x3C, 0xAD,
  0xD6, 0x47, 0x35, 0xA4, 0xD1, 0x40, 0x32, 0xA3,
  0xC4, 0x55, 0x27, 0xB6, 0xC3, 0x52, 0x20, 0xB1,
  0xCA, 0x5B, 0x29, 0xB8, 0xCD, 0x5C, 0x2E, 0xBF,
  0x90, 0x01, 0x73, 0xE2, 0x97, 0x06, 0x74, 0xE5,
  0x9E, 0x0F, 0x7D, 0xEC, 0x99, 0x08, 0x7A, 0xEB,
  0x8C, 0x1D, 0x6F, 0xFE, 0x8B, 0x1A, 0x68, 0xF9,
  0x82, 0x13, 0x61, 0xF0, 0x85, 0x14, 0x66, 0xF7,
  0xA8, 0x39, 0x4B, 0xDA, 0xAF, 0x3E, 0x4C, 0xDD,
  0xA6, 0x37, 0x45, 0xD4, 0xA1, 0x30, 0x42, 0xD3,
  0xB4, 0x25, 0x57, 0xC6, 0xB3, 0x22, 0x50, 0xC1,
  0xBA, 0x2B, 0x59, 0xC8, 0xBD, 0x2C, 0x5E, 0xCF
  };

#define FCS_INIT 0xFF
#define FCS_GOOD 0xCF

static inline uint8_t gsm_fcs_add(uint8_t fcs, uint8_t c)
  {
  return gsm_fcs8[fcs ^ c];
  }

static inline uint8_t gsm_fcs_add_block(uint8_t fcs, uint8_t *c, size_t len)
  {
  while (len--) fcs = gsm_fcs8[fcs ^ *c++];
  return fcs;
  }

GsmMuxChannel::GsmMuxChannel(GsmMux* mux, int channel, size_t buffersize)
  : m_buffer(buffersize)
  {
  m_state = ChanClosed;
  m_mux = mux;
  m_channel = channel;
  }

GsmMuxChannel::~GsmMuxChannel()
  {
  }

void GsmMuxChannel::ProcessFrame(uint8_t* frame, size_t length, size_t iframepos)
  {
  // Note the <length> provided excludes the start byte, stop byte, and checksum
  // The <frame> pointer itself points to the byte after the start byte
  //ESP_LOGV(TAG, "ChanProcessFrame(CHAN=%d, ADDR=%02x, CTRL=%02x, LEN=%d, IFP=%d)", m_channel, frame[0], frame[1], length, iframepos);
  switch (m_state)
    {
    case ChanClosed:
      break;
    case ChanOpening:
      if (frame[1] == (GSM_UA + GSM_PF))
        {
        ESP_LOGI(TAG, "Channel #%d is open",m_channel);
        m_state = ChanOpen; // SABM established
        if (m_channel != 0) m_mux->m_openchannels++;
        if (m_channel==0) m_mux->m_state = GsmMux::DlciOpen;
        if (m_channel<GSM_MUX_CHANNELS) m_mux->StartChannel(m_channel+1);
        }
#if __GNUC__ >= 7
	[[fallthrough]];
#endif
    case ChanOpen:
      if (frame[1] == (GSM_UIH + GSM_PF))
        {
        for (size_t k=iframepos;k<length;k++)
          m_buffer.Push(frame[k]);
        m_mux->m_modem->IncomingMuxData(this);
        }
      break;
    case ChanClosing:
      break;
    default:
      break;
    }
  }

GsmMux::GsmMux(simcom* modem, size_t maxframesize)
  {
  m_state = DlciClosed;
  m_modem = modem;
  m_frame = new uint8_t[maxframesize];
  m_framesize = maxframesize;
  m_framepos = 0;
  m_frameipos = 0;
  m_framelen = 0;
  m_framemorelen = false;
  m_openchannels = 0;
  m_framingerrors = 0;
  m_lastgoodrxframe = 0;
  m_rxframecount = 0;
  m_txframecount = 0;
  }

GsmMux::~GsmMux()
  {
  delete [] m_frame;
  }

void GsmMux::Start()
  {
  ESP_LOGI(TAG, "Start MUX");
  m_framingerrors = 0;
  m_lastgoodrxframe = 0;
  m_rxframecount = 0;
  m_txframecount = 0;
  m_channels.insert(m_channels.end(),new GsmMuxChannel(this,0,8));
  for (int k=1; k<=GSM_MUX_CHANNELS; k++)
    {
    m_channels.insert(m_channels.end(),
      new GsmMuxChannel(this,k,(k==GSM_MUX_CHAN_DATA)?2048:512));
    }
  StartChannel(0);
  m_state = DlciOpening;
  }

void GsmMux::Stop()
  {
  ESP_LOGI(TAG, "Stop MUX");
  for (int k=0; k<m_channels.size(); k++)
    {
    GsmMuxChannel* chan = m_channels[k];
    if (chan) delete chan;
    }
  m_channels.clear();
  m_state = DlciClosed;
  m_framepos = 0;
  m_frameipos = 0;
  m_framelen = 0;
  m_framemorelen = false;
  m_openchannels = 0;
  m_framingerrors = 0;
  m_lastgoodrxframe = 0;
  m_rxframecount = 0;
  m_txframecount = 0;
  }

void GsmMux::StartChannel(int channel)
  {
  int cn = (channel<<2)+GSM_EA+GSM_CR;
  uint8_t sabm[] =
    {
    GSM0_SOF,
    (uint8_t)cn,      // Address: EA=1, CR=1, DLCI=channel
    GSM_SABM+GSM_PF,  // Control: SABM (0x2f) + Poll (0x10)
    GSM_EA+0,         // Length: EA=1, Length=0
    0x00,             // FCS
    GSM0_SOF
    };
  txfcs(sabm,6);
  m_channels[channel]->m_state = GsmMuxChannel::ChanOpening;
  }

void GsmMux::StopChannel(int channel)
  {
  ESP_LOGI(TAG, "StopChannel(%d)",channel);
  }

bool GsmMux::IsChannelOpen(int channel)
  {
  GsmMuxChannel* chan = m_channels[channel];
  return (chan && chan->m_state == GsmMuxChannel::ChanOpen);
  }

bool GsmMux::IsMuxUp()
  {
  return (m_openchannels == GSM_MUX_CHANNELS);
  }

void GsmMux::Process(OvmsBuffer* buf)
  {
  while (buf->UsedSpace()>0)
    {
    if (m_framepos == m_framesize)
      {
      // Overflow frame
      ESP_LOGW(TAG, "Frame overflow (%d bytes)",m_framesize);
      MyCommandApp.HexDump(TAG, "Frame head", (const char*)m_frame, 8*16);
      m_framepos = 0;
      m_framelen = 0;
      m_framemorelen = false;
      m_framingerrors++;
      continue;
      }
    uint8_t b = buf->Pop();
    if ((m_framepos == 0)&&(b != GSM0_SOF))
      {
      continue; // Skip to start of frame
      }
    if ((m_framepos == 1)&&(b == GSM0_SOF)) continue; // We found end of previous frame, so just skip it
    // ESP_LOGI(TAG, "Got %02x at %d (length sofar = %d)",b,m_framepos,m_framelen);
    m_frame[m_framepos++] = b;
    if (m_framepos == 4)
      {
      // First byte of length field
      m_framemorelen = !(b & GSM_EA);
      m_framelen = (b>>1);
      if (!m_framemorelen)
        {
        m_framelen += (m_framepos+2);
        m_frameipos = m_framepos;
        }
      else
        {
        m_framelen += (m_framepos+3);
        m_frameipos = m_framepos+1;
        }
      // ESP_LOGI(TAG, "Frame length (first byte) = %d",m_framelen);
      }
    if ((m_framepos == 5)&&(m_framemorelen))
      {
      // Second byte of length field
      m_framelen += (b<<7);
      m_framemorelen = false;
      // ESP_LOGI(TAG, "Frame length (second byte) = %d",m_framelen);
      }
    if (m_framepos == m_framelen)
      {
      if (b == GSM0_SOF)
        {
        // We have a complete frame...
        ProcessFrame();
        }
      else
        {
        // Frame error:
        int channel = m_frame[1] >> 2;
        ESP_LOGW(TAG, "Frame error: EOF mismatch (CHAN=%d, ADDR=%02x, CTRL=%02x, FCS=%02x, LEN=%d)",
          channel, m_frame[1], m_frame[2], m_frame[m_framelen-2], m_framelen);
        MyCommandApp.HexDump(TAG, "Frame dump", (const char*)m_frame, m_framelen);
        // find next frame:
        m_framepos = 0;
        m_framelen = 0;
        m_framemorelen = false;
        m_framingerrors++;
        }
      }
    }
  }

void GsmMux::ProcessFrame()
  {
  int channel = m_frame[1] >>2;

  ESP_LOGV(TAG, "ProcessFrame(CHAN=%d, ADDR=%02x, CTRL=%02x, FCS=%02x, LEN=%d)",
    channel, m_frame[1], m_frame[2], m_frame[m_framelen-2], m_framelen);

  uint8_t fcs = 0xFF - gsm_fcs_add_block(FCS_INIT, m_frame+1, m_frameipos-1);
  if (fcs != m_frame[m_framelen-2])
    {
    ESP_LOGW(TAG, "FCS mismatch (%02x != %02x)",fcs,m_frame[m_framelen-2]);
    m_framepos = 0;
    m_frameipos = 0;
    m_framelen = 0;
    m_framingerrors++;
    m_framemorelen = false;
    return;
    }

  GsmMuxChannel* chan = m_channels[channel];
  if (chan)
    {
    m_lastgoodrxframe = monotonictime;
    m_rxframecount++;
    chan->ProcessFrame(m_frame+1,m_framelen-3,m_frameipos-1);
    }
  else
    {
    ESP_LOGW(TAG, "Incoming message for unrecognised channel #%d",channel);
    }

  m_framepos = 0;
  m_frameipos = 0;
  m_framelen = 0;
  m_framemorelen = false;
  }

void GsmMux::txfcs(uint8_t* data, size_t size, size_t ipos)
  {
  data[size-2] = 0xFF - gsm_fcs_add_block(FCS_INIT, data+1, ipos-1);
  m_modem->tx(data,size);
  m_txframecount++;
  }

size_t GsmMux::tx(int channel, uint8_t* data, ssize_t size)
  {
  uint8_t* buf = new uint8_t[size+7];
  size_t ipos;
  int len;

  int cn = (channel<<2)+GSM_EA;
  buf[0] = GSM0_SOF;
  buf[1] = (uint8_t)cn;    // Address: EA=1, DLCI=channel
  buf[2] = GSM_UIH+GSM_PF; // Control: UIH + Poll
  if (size < 128)
    {
    len = (size<<1) + GSM_EA;
    buf[3] = (uint8_t)len; // Length: EA=1, Length=size
    ipos = 4;
    }
  else
    {
    len = ((size%128)<<1);
    buf[3] = (uint8_t)len; // Length: lower 7 bit, shifted once
    len = (size/128);
    buf[4] = (uint8_t)len; // Length: upper 7 bits
    ipos = 5;
    }
  for (size_t k=0; k<size; k++)
    {
    buf[ipos+k] = data[k];
    }
  buf[ipos+size] = 0; // For FCS
  buf[ipos+size+1] = GSM0_SOF;
  txfcs(buf,ipos+size+2,ipos);
  delete [] buf;

  return size;
  }

size_t GsmMux::tx(int channel, const char* data, ssize_t size)
  {
  if (size >= 0)
    return tx(channel, (uint8_t*)data,size);
  else
    return tx(channel, (uint8_t*)data,strlen(data));
  }
