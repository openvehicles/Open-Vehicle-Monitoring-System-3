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

#include "esp_log.h"
static const char *TAG = "gsm-mux";

#include "gsmmux.h"
#include "simcom.h"

GsmMuxChannel::GsmMuxChannel(size_t buffersize)
  : m_buffer(buffersize)
  {
  }

GsmMuxChannel::~GsmMuxChannel()
  {
  }

GsmMux::GsmMux(simcom* modem, size_t maxframesize)
  {
  m_modem = modem;
  m_frame = new uint8_t[maxframesize];
  m_framesize = maxframesize;
  m_framepos = 0;
  }

GsmMux::~GsmMux()
  {
  delete [] m_frame;
  }

void GsmMux::Start()
  {
  ESP_LOGI(TAG, "Start MUX");
  uint8_t sabm[] = {0xf9, 0x03, 0x3f, 0x01, 0x1c, 0xf9};
  m_modem->tx(sabm,6);
  }

void GsmMux::Stop()
  {
  ESP_LOGI(TAG, "Stop MUX");
  }

void GsmMux::StartChannel(int channel)
  {
  ESP_LOGI(TAG, "StartChannel(%d)",channel);
  }

void GsmMux::StopChannel(int channel)
  {
  ESP_LOGI(TAG, "StopChannel(%d)",channel);
  }

void GsmMux::Process(OvmsBuffer* buf)
  {
  }

void GsmMux::tx(int channel, uint8_t data, ssize_t size)
  {
  ESP_LOGI(TAG, "tx(%d bytes)",size);
  }
