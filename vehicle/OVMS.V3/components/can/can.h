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
struct CAN_frame_t
  {
  canbus*     origin;                   // Origin of the frame
  CAN_FIR_t   FIR;                      // Frame information record
  uint32_t    MsgID;                    // Message ID
  union
    {
    uint8_t   u8[8];                    // Payload byte access
    uint32_t  u32[2];                   // Payload u32 access (Att: little endian!)
    } data;
  
  esp_err_t Write(canbus* bus = NULL);  // default = use origin
  };


/**
 * canbitset<StoreType>: CAN data packed bit extraction utility
 *  Usage examples:
 *    canbitset<uint64_t> canbits(p_frame->data.u8, 8) -- load whole CAN frame
 *    canbitset<uint32_t> canbits(data+1, 3) -- load 3 bytes beginning at second
 *    val = canbits.get(12,23) -- extract bits 12-23 of bytes loaded
 *  Note: no sanity checks.
 *  Hint: for best performance on ESP32, avoid StoreType uint64_t
 */
template <typename StoreType>
class canbitset
  {
  private:
    StoreType m_store;

  public:
    canbitset(uint8_t* src, int len)
      {
      load(src, len);
      }
    
    // load message bytes:
    void load(uint8_t* src, int len)
      {
      m_store = 0;
      for (int i=0; i<len; i++)
        {
        m_store <<= 8;
        m_store |= src[i];
        }
      m_store <<= ((sizeof(StoreType)-len) << 3);
      }
    
    // extract message part:
    //  - start,end: bit positions 0…63, 0=MSB of first msg byte
    StoreType get(int start, int end)
      {
      return (StoreType) ((m_store << start) >> (start + (sizeof(StoreType)<<3) - end - 1));
      }
  };


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
    virtual esp_err_t WriteExtended(uint32_t id, uint8_t length, uint8_t *data);
    virtual esp_err_t WriteStandard(uint16_t id, uint8_t length, uint8_t *data);
    virtual bool RxCallback(CAN_frame_t* frame);
    virtual void TxCallback();

  public:
    CAN_speed_t m_speed;
    CAN_mode_t m_mode;
    bool m_trace;
    uint32_t m_trace_id_from;
    uint32_t m_trace_id_to;

  public:
    uint32_t m_packets_rx;
    uint32_t m_errors_rx;
    uint32_t m_packets_tx;
    uint32_t m_errors_tx;
    uint32_t m_errors_rxbuf_overflow;
    uint32_t m_errors_txbuf_overflow;
    uint32_t m_error_flags;
  
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
