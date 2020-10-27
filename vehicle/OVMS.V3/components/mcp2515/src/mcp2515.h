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
#include "ovms_mutex.h"

// MCP2515 SPI commands:
#define CMD_RESET         0b11000000
#define CMD_READ          0b00000011
#define CMD_WRITE         0b00000010
#define CMD_RTS           0b10000000
#define CMD_BITMODIFY     0b00000101
#define CMD_READ_RXBUF    0b10010000
#define CMD_LOAD_TXBUF    0b01000000
#define CMD_READ_STATUS   0b10100000

// CANSTAT register
#define CANSTAT_MODE_CONFIG     0x10000000
#define CANSTAT_MODE_LISTEN     0b01100000
#define CANSTAT_MODE_LOOPBACK   0b01000000
#define CANSTAT_MODE_SLEEP      0b00100000
#define CANSTAT_MODE_NORMAL     0b00000000
#define CANSTAT_ABAT            0b00010000  // Abort All Pending Transmissions bit
#define CANSTAT_OSM             0b00001000  // One shot mode bit
#define CANSTAT_CLKEN           0b00000100  // CLKOUT pin enable bit


// Configuration Registers
#define REG_CANSTAT         0x0E
#define REG_CANCTRL         0x0F
#define REG_BFPCTRL         0x0C
#define REG_TEC             0x1C
#define REG_REC             0x1D
#define REG_CNF3            0x28
#define REG_CNF2            0x29
#define REG_CNF1            0x2A
#define REG_CANINTE         0x2B
#define REG_CANINTF         0x2C
#define REG_EFLG            0x2D
#define REG_TXRTSCTRL       0x0D

#define REG_RXB0CTRL        0x60

#define MCP2515_TIMEOUT     100 // milliseconds


class mcp2515 : public canbus
  {
  public:
    mcp2515(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin, bool hw_cs=true);
    ~mcp2515();

  public:
    esp_err_t Start(CAN_mode_t mode, CAN_speed_t speed);
    esp_err_t Stop();
    esp_err_t WriteReg( uint8_t reg, uint8_t value );
    esp_err_t WriteRegAndVerify( uint8_t reg, uint8_t value, uint8_t read_back_mask = 0xff);
    esp_err_t ChangeMode( uint8_t mode );
    esp_err_t ViewRegisters();

  public:
    esp_err_t Write(const CAN_frame_t* p_frame, TickType_t maxqueuewait=0);
    bool AsynchronousInterruptHandler(CAN_frame_t* frame, bool * frameReceived);
    void TxCallback(CAN_frame_t* p_frame, bool success);

  protected:
    esp_err_t WriteFrame(const CAN_frame_t* p_frame);

  public:
    void SetPowerMode(PowerMode powermode);

  public:
    spi* m_spibus;
    spi_nodma_device_handle_t m_spi;

  protected:
    spi_nodma_device_interface_config_t m_devcfg;
    spi_nodma_host_device_t m_host;
    int m_clockspeed;
    int m_cspin;
    int m_intpin;
    uint8_t m_last_errflag = 0;
    OvmsMutex m_write_mutex;
  };

#endif //#ifndef __MCP2515_H__
