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
;
; Portions of this are based on the work of Thomas Barth, and licensed
; under MIT license.
; Copyright (c) 2017, Thomas Barth, barth-dev.de
; https://github.com/ThomasBarth/ESP32-CAN-Driver
*/

#ifndef __ESP32CAN_H__
#define __ESP32CAN_H__

#include <stdint.h>
#include "can.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/semphr.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "esp_intr_alloc.h"
#include "soc/dport_reg.h"
#include <math.h>


// ESP32CAN/SJA1000 filter bit layout:
typedef union
  {
  uint32_t u32;
  uint8_t u8[4];
  struct
    {
    unsigned db2:8;   // filter 1 data byte 2
    unsigned db1:8;   // filter 1 data byte 1
    unsigned :4;
    unsigned rtr:1;
    unsigned id:11;
    } bss;  // bitfield single/standard
  struct
    {
    unsigned :2;
    unsigned rtr:1;
    unsigned id:29;
    } bse;  // bitfield single/extended
  struct
    {
    unsigned f1db1ln:4;   // filter 1 data byte 1 low nibble
    unsigned f2rtr:1;
    unsigned f2id:11;
    unsigned f1db1hn:4;   // filter 1 data byte 1 high nibble
    unsigned f1rtr:1;
    unsigned f1id:11;
    } bds;  // bitfield dual/standard
  struct
    {
    unsigned f2id:16;
    unsigned f1id:16;
    } bde;  // bitfield dual/extended
  } esp32can_filter_t;

// ESP32CAN/SJA1000 filter configuration:
typedef struct
  {
  esp32can_filter_t code;       /**< 32-bit acceptance code */
  esp32can_filter_t mask;       /**< 32-bit acceptance mask */
  bool single_filter;             /**< Use Single Filter Mode (see documentation) */
  } esp32can_filter_config_t;


class esp32can : public canbus
  {
  public:
    esp32can(const char* name, int txpin, int rxpin);
    ~esp32can();

  public:
    esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    esp_err_t Stop();
    esp_err_t InitController();
    esp_err_t SetAcceptanceFilter(const esp32can_filter_config_t& cfg);

  public:
    esp_err_t Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait=0);
    void TxCallback(CAN_frame_t* p_frame, bool success);

  protected:
    esp_err_t WriteFrame(const CAN_frame_t* p_frame);
    void BusTicker10(std::string event, void* data);
    esp_err_t ChangeResetMode(unsigned int newmode, int timeout_us=50);

  public:
    void SetPowerMode(PowerMode powermode);
    bool GetErrorFlagsDesc(std::string &buffer, uint32_t error_flags) override;
    void SetTransceiverMode(CAN_mode_t mode);
 
  public:
    static void shell_setaccfilter(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  public:
    gpio_num_t m_txpin;               // TX pin
    gpio_num_t m_rxpin;               // RX pin
    OvmsMutex m_write_mutex;
    bool m_tx_abort;
  };

#endif //#ifndef __ESP32CAN_H__
