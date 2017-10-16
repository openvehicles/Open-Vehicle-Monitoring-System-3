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

#include <string.h>
#include "mcp2515.h"
#include "soc/gpio_struct.h"
#include "driver/gpio.h"
#include "esp_intr.h"
#include "soc/dport_reg.h"

static void MCP2515_rxtask(void *pvParameters)
  {
  mcp2515 *me = (mcp2515*)pvParameters;
  uint8_t buf[32];
  spi* spibus = me->m_spibus;
  spi_nodma_device_handle_t spi = me->m_spi;

  while(1)
    {
    if (xSemaphoreTake(me->m_rxsem, portMAX_DELAY) == pdTRUE)
      {
      uint8_t *p = spibus->spi_cmd(spi, buf, 2, 2, 0b00000011, 0x2c);
      uint8_t intstat = p[0];
      //uint8_t errflag = p[1];

      // MCP2515 BITMODIFY CANINTE (Interrupt Enable) Clear flags
      spibus->spi_cmd(spi, buf, 0, 4, 0b00000101, 0x2c, *p, 0x00);

      int rxbuf = -1;
      if (intstat & 0x01)
        {
        // RX buffer 0 is full, handle it
        rxbuf = 0;
        }
      else if (intstat & 0x02)
        {
        // RX buffer 1 is full, handle it
        rxbuf = 4;
        }

      if (rxbuf >= 0)
        {
        // The indicated RX buffer has a message to be read
        CAN_frame_t msg;
        memset(&msg,0,sizeof(msg));
        msg.origin = me;

        uint8_t *p = spibus->spi_cmd(spi, buf, 13, 1, 0x90+rxbuf);
        if(p[1] & 0x20) //check for extended mode=0, or std mode=1, though this seems backwards from standard
          msg.MsgID = (p[0] << 3) | (p[1] >> 5);  // Standard mode
        else
        { msg.FIR.B.FF = CAN_frame_ext;           // Extended mode
          msg.MsgID = (p[0]<<21) | ((p[1]&0x80)<<13) | ((p[1]&0x0f)<<16) | (p[2]<<8) | (p[3]);
        }
        msg.FIR.B.DLC = p[4] & 0x0f;
      
        memcpy(&msg.data,p+5,8);

        //send frame to main CAN processor task
        xQueueSend(MyCan.m_rxqueue,&msg,0);
        // MyCan.IncomingFrame(&msg);
        }
      }
    }
  }

static void MCP2515_isr(void *pvParameters)
  {
  mcp2515 *me = (mcp2515*)pvParameters;

  xSemaphoreGive(me->m_rxsem);
  }

mcp2515::mcp2515(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin)
  : canbus(name)
  {
  m_spibus = spibus;
  m_host = host;
  m_clockspeed = clockspeed;
  m_cspin = cspin;
  m_intpin = intpin;

  memset(&m_devcfg, 0, sizeof(spi_nodma_device_interface_config_t));
  m_devcfg.clock_speed_hz=m_clockspeed;     // Clock speed (in hz)
  m_devcfg.mode=0;                          // SPI mode 0
  m_devcfg.command_bits=0;
  m_devcfg.address_bits=0;
  m_devcfg.dummy_bits=0;
  m_devcfg.spics_io_num=m_cspin;
  m_devcfg.queue_size=7;                    // We want to be able to queue 7 transactions at a time

  esp_err_t ret = spi_nodma_bus_add_device(m_host, &m_spibus->m_buscfg, &m_devcfg, &m_spi);
  assert(ret==ESP_OK);

  m_rxsem = xSemaphoreCreateBinary();
  xTaskCreatePinnedToCore(MCP2515_rxtask, "MCP2515RxTask", 4096, (void*)this, 5, &m_rxtask, 1);

  gpio_set_intr_type((gpio_num_t)m_intpin, GPIO_INTR_NEGEDGE);
  gpio_isr_handler_add((gpio_num_t)m_intpin, MCP2515_isr, (void*)this);

  // Initialise in powered down mode
  m_powermode = Off; // Stop an event being raised
  SetPowerMode(Off);
  }

mcp2515::~mcp2515()
  {
  vTaskDelete(m_rxtask);
  vSemaphoreDelete(m_rxsem);
  gpio_isr_handler_remove((gpio_num_t)m_intpin);
  }

esp_err_t mcp2515::Start(CAN_mode_t mode, CAN_speed_t speed)
  {
  uint8_t buf[16];

  m_mode = mode;
  m_speed = speed;

  // RESET commmand
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0b11000000);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // Set CONFIG mode (abort transmisions, one-shot mode, clkout disabled)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0b10011000);
  vTaskDelay(50 / portTICK_PERIOD_MS);

  // Rx Buffer 0 control (receive all and enable buffer 1 rollover)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x60, 0b01100100);

  // CANINTE (interrupt enable), all interrupts
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x2b, 0b11111111);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0c,  0b00001100);

  // Bus speed
  uint8_t cnf1 = 0;
  uint8_t cnf2 = 0;
  uint8_t cnf3 = 0;
  switch (m_speed)
    {
    case CAN_SPEED_100KBPS:
      cnf1=0x03; cnf2=0xfa; cnf3=0x87;
      break;
    case CAN_SPEED_125KBPS:
      cnf1=0x03; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_250KBPS:
      cnf1=0x41; cnf2=0xf1; cnf3=0x85;
      break;
    case CAN_SPEED_500KBPS:
      cnf1=0x00; cnf2=0xf0; cnf3=0x86;
      break;
    case CAN_SPEED_1000KBPS:
      cnf1=0x00; cnf2=0xd0; cnf3=0x82;
      break;
    }
  m_spibus->spi_cmd(m_spi, buf, 0, 5, 0x02, 0x28, cnf3, cnf2, cnf1);

  // Set NORMAL mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0x00);

  // And record that we are powered on
  pcp::SetPowerMode(On);

  return ESP_OK;
  }

esp_err_t mcp2515::Stop()
  {
  uint8_t buf[16];

  // RESET command
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0b11000000);
  vTaskDelay(5 / portTICK_PERIOD_MS);

  // BFPCTRL RXnBF PIN CONTROL AND STATUS
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0c, 0b00111100);

  // Set SLEEP mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0x30);

  // And record that we are powered down
  pcp::SetPowerMode(Off);

  return ESP_OK;
  }

esp_err_t mcp2515::Write(const CAN_frame_t* p_frame)
  {
  canbus::Write(p_frame);
  uint8_t buf[16];
  uint8_t id[4];

//  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 1, 2, 0b00000011, 0x30);
//  printf("MCP2515 TXB0CTRL(0x30) is %02x\n",p[0]);

  if (p_frame->FIR.B.FF == CAN_frame_std)
    {
    // Transmit a standard frame
    id[0] = p_frame->MsgID >> 3;     // HIGH 8 bits of standard ID
    id[1] = p_frame->MsgID << 5;     // LOW 3 bits of standard ID
    id[2] = 0;
    id[3] = 0;
    }
  else
    {
    // Transmit an extended frame
    id[0] = (p_frame->MsgID >> 21) & 0xff;    // HIGH bits
    id[1] = ((p_frame->MsgID >> 13) & 0xf6)+0x08;  // Next middle bits of extended ID; set the EXT bit too.
    id[2] = (p_frame->MsgID >> 8)  & 0xff;    // MID 8 bits of extended ID
    id[3] = (p_frame->MsgID & 0xff);          // LOW 8 bits of extended ID
    }

  // MCP2515 Transmit Buffer
  m_spibus->spi_cmd(m_spi, buf, 0, 14,
    0x40, id[0], id[1], id[2], id[3], p_frame->FIR.B.DLC,
    p_frame->data.u8[0],
    p_frame->data.u8[1],
    p_frame->data.u8[2],
    p_frame->data.u8[3],
    p_frame->data.u8[4],
    p_frame->data.u8[5],
    p_frame->data.u8[6],
    p_frame->data.u8[7]);

  // MCP2515 RTS
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0x81);

  return ESP_OK;
  }

void mcp2515::SetPowerMode(PowerMode powermode)
  {
  pcp::SetPowerMode(powermode);
  switch (powermode)
    {
    case  On:
      if (m_mode != CAN_MODE_OFF)
        {
        Start(m_mode, m_speed);
        }
      break;
    case Sleep:
    case DeepSleep:
    case Off:
      Stop();
      break;
    default:
      break;
    }
  }
