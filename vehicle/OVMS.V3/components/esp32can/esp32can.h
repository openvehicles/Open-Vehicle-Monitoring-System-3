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
#include "driver/gpio.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"
#include <math.h>

/**
 * \brief Initialize the CAN Module
 *
 * \return 0 CAN Module had been initialized
 */
int CAN_init(void);

/**
 * \brief Send a can frame
 *
 * \param	p_frame	Pointer to the frame to be send, see #CAN_frame_t
 * \return  0 Frame has been written to the module
 */
int CAN_write_frame(const CAN_frame_t* p_frame);

/**
 * \brief Stops the CAN Module
 *
 * \return 0 CAN Module was stopped
 */
int CAN_stop(void);

class esp32can : public canbus
  {
  public:
    esp32can(int txpin, int rxpin);
    ~esp32can();

  public:
    esp_err_t Init(CAN_speed_t speed);
    esp_err_t Stop();

  public:
    esp_err_t Write(const CAN_frame_t* p_frame);

  public:
    gpio_num_t m_txpin;               // TX pin
    gpio_num_t m_rxpin;               // RX pin
    QueueHandle_t m_rxqueue;          // Handler to FreeROS TX queue
  };

#endif //#ifndef __ESP32CAN_H__
