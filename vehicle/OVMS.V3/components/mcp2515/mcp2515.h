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

#ifndef __MCP2515_H__
#define __MCP2515_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "can.h"
#include "spi.h"

// MCP2515 SPI commands:
#define CMD_RESET         0b11000000
#define CMD_READ          0b00000011
#define CMD_WRITE         0b00000010
#define CMD_RTS           0b10000000
#define CMD_BITMODIFY     0b00000101
#define CMD_READ_RXBUF    0b10010000
#define CMD_LOAD_TXBUF    0b01000000


class mcp2515 : public canbus
  {
  public:
    mcp2515(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin);
    ~mcp2515();

  public:
    esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    esp_err_t Stop();

  public:
    esp_err_t Write(const CAN_frame_t* p_frame);
    virtual bool RxCallback(CAN_frame_t* frame);
    virtual void TxCallback();

  public:
    virtual void SetPowerMode(PowerMode powermode);

  public:
    spi* m_spibus;
    spi_nodma_device_handle_t m_spi;

  protected:
    spi_nodma_device_interface_config_t m_devcfg;
    spi_nodma_host_device_t m_host;
    int m_clockspeed;
    int m_cspin;
    int m_intpin;
  };

#endif //#ifndef __MCP2515_H__
