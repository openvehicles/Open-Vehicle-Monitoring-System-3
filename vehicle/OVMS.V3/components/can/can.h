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

#ifndef __CAN_H__
#define __CAN_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"
#include <stdint.h>
#include <list>
#include "pcp.h"
#include <esp_err.h>

class canbus; // Forward definition

// CAN mode
typedef enum
  {
  CAN_MODE_OFF=0,
  CAN_MODE_LISTEN=1,
  CAN_MODE_ACTIVE=2
  } CAN_mode_t;

// CAN link speed (100kbps -> 1MHz)
typedef enum
  {
  CAN_SPEED_100KBPS=100,     // CAN Node runs at 100kBit/s
  CAN_SPEED_125KBPS=125,     // CAN Node runs at 125kBit/s
  CAN_SPEED_250KBPS=250,     // CAN Node runs at 250kBit/s
  CAN_SPEED_500KBPS=500,     // CAN Node runs at 500kBit/s
  CAN_SPEED_1000KBPS=1000    // CAN Node runs at 1000kBit/s
  } CAN_speed_t;

// CAN frame type (standard/extended)
typedef enum
  {
  CAN_frame_std=0,           // Standard frame, using 11 bit identifer
  CAN_frame_ext=1            // Extended frame, using 29 bit identifer
  } CAN_frame_format_t;

// CAN RTR
typedef enum
  {
  CAN_no_RTR=0,              // No RTR frame
  CAN_RTR=1                  // RTR frame
  } CAN_RTR_t;

// CAN Frame Information Record
typedef union
  {
  uint32_t U;                           // Unsigned access
  struct
    {
    uint8_t             DLC:4;          // [3:0] DLC, Data length container
    unsigned int        unknown_2:2;    // internal unknown
    CAN_RTR_t           RTR:1;          // [6:6] RTR, Remote Transmission Request
    CAN_frame_format_t  FF:1;           // [7:7] Frame Format, see# CAN_frame_format_t
    unsigned int        reserved_24:24; // internal Reserved
    } B;
  } CAN_FIR_t;

// CAN Frame
typedef struct
  {
  canbus*     origin;                   // Origin of the frame
  CAN_FIR_t   FIR;                      // Frame information record
  uint32_t    MsgID;                    // Message ID
  union
    {
    uint8_t   u8[8];                    // Payload byte access
    uint32_t  u32[2];                   // Payload u32 access
    } data;
  } CAN_frame_t;

// CAN message type
typedef enum
  {
  CAN_frame = 0,
  CAN_rxcallback,
  CAN_txcallback
  } CAN_MSGID_t;

// CAN message
typedef struct
  {
  CAN_MSGID_t type;
  union
    {
    CAN_frame_t frame;  // CAN_frame
    canbus* bus;        // CAN_rxcallback, CAN_txcallback
    } body;
  } CAN_msg_t;

class canbus : public pcp
  {
  public:
    canbus(const char* name);
    virtual ~canbus();

  public:
    virtual esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    virtual esp_err_t Stop();

  public:
    virtual esp_err_t Write(const CAN_frame_t* p_frame);
    virtual bool RxCallback(CAN_frame_t* frame);
    virtual void TxCallback();

  public:
    CAN_speed_t m_speed;
    CAN_mode_t m_mode;
    bool m_trace;
  };

class can
  {
  public:
     can();
     ~can();

  public:
    void IncomingFrame(CAN_frame_t* p_frame);

  public:
    QueueHandle_t m_rxqueue;

  public:
    void RegisterListener(QueueHandle_t queue);
    void DeregisterListener(QueueHandle_t queue);

  private:
    std::list<QueueHandle_t> m_listeners;
    TaskHandle_t m_rxtask;            // Task to handle reception
  };

extern can MyCan;

#endif //#ifndef __CAN_H__
