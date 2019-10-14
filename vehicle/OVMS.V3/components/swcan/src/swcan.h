/*
;    Project:       Open Vehicle Monitor System
;    Date:          27th March 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
;
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

#ifndef __OVMS_SWCAN_H__
#define __OVMS_SWCAN_H__

#include "mcp2515.h"
#include "ovms_led.h"

enum TransceiverMode { tmode_normal, tmode_sleep, tmode_highvoltagewakeup, tmode_highspeed };

class swcan : public mcp2515
  {

  public:
    swcan(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin, bool hw_cs=true);
    ~swcan();

  public:
    esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    esp_err_t Stop();
    void SetPowerMode(PowerMode powermode);
    void SetTransceiverMode(TransceiverMode opmode);
    bool AsynchronousInterruptHandler(CAN_frame_t* frame, bool* frameReceived);
    void TxCallback(CAN_frame_t* frame, bool success);
    //void AutoInit();
    ovms_led * m_status_led, * m_tx_led, * m_rx_led;

  private:
    void SystemUp(std::string event, void* data);
    void ServerConnected(std::string event, void* data);
    void ServerDisconnected(std::string event, void* data);
    void ModemEvent(std::string event, void* data);

    void DoSetTransceiverMode(bool mode0, bool mode1);
  	TransceiverMode m_tmode;

  };

#endif //#ifndef __OVMS_SWCAN_H__
