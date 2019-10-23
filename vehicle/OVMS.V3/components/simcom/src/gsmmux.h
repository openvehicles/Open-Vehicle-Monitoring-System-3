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

#ifndef __GSM_MUX_H__
#define __GSM_MUX_H__

#include <vector>
#include <unistd.h>
#include "ovms.h"
#include "ovms_buffer.h"

class simcom; // Forward declared
class GsmMux; // Forward declared

#define GSM_MUX_CHANNELS  4
#define GSM_MUX_CHAN_CTRL 0
#define GSM_MUX_CHAN_NMEA 1
#define GSM_MUX_CHAN_DATA 2
#define GSM_MUX_CHAN_POLL 3
#define GSM_MUX_CHAN_CMD  4

class GsmMuxChannel : public InternalRamAllocated
  {
  public:
    GsmMuxChannel(GsmMux* mux, int channel, size_t buffersize);
    ~GsmMuxChannel();

  public:
    enum GsmMuxChannelState
      {
      ChanClosed,    // DLCI closed
      ChanOpening,   // SABM sent, awaiting UA
      ChanOpen,      // DLCI open (SABM sent, UA received)
      ChanClosing    // DISC sent, awaiting UA/DM
      };

  public:
    void ProcessFrame(uint8_t* frame, size_t length, size_t iframepos);

  public:
    GsmMuxChannelState m_state;
    GsmMux* m_mux;
    int m_channel;
    OvmsBuffer m_buffer;
  };

class GsmMux : public InternalRamAllocated
  {
  public:
    GsmMux(simcom* modem, size_t maxframesize = 2048);
    ~GsmMux();

  public:
    void Start();
    void Stop();
    void StartChannel(int channel);
    void StopChannel(int channel);
    void Process(OvmsBuffer* buf);
    void ProcessFrame();
    size_t tx(int channel, uint8_t* data, ssize_t size);
    size_t tx(int channel, const char* data, ssize_t size = -1);
    bool IsChannelOpen(int channel);
    bool IsMuxUp();

  protected:
    void txfcs(uint8_t* data, size_t size, size_t ipos = 4);

  public:
    enum GsmMuxState
      {
      DlciClosed,    // DLCI closed
      DlciOpening,   // SABM sent, awaiting UA
      DlciOpen,      // DLCI open (SABM sent, UA received)
      DlciClosing    // DISC sent, awaiting UA/DM
      };

  public:
    GsmMuxState m_state;
    int m_openchannels;
    uint32_t m_framingerrors;
    uint32_t m_lastgoodrxframe;
    uint32_t m_rxframecount;
    uint32_t m_txframecount;

  public:
    simcom* m_modem;
    uint8_t* m_frame;
    size_t m_framesize;
    size_t m_framepos;
    size_t m_frameipos;
    size_t m_framelen;
    bool m_framemorelen;
    std::vector<GsmMuxChannel*> m_channels;
  };

#endif //#ifndef __GSM_MUX__
