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
  mcp2515* device = (mcp2515*)pvParameters;

  for(;;)
    {
    if (xSemaphoreTake(device->m_rxsem, pdMS_TO_TICKS(500)))
      {
      device->PollRX();
      }
    }
  }

static void MCP2515_isr(void *arg_p)
  {
  mcp2515* device = (mcp2515*)arg_p;

  xSemaphoreGiveFromISR(device->m_rxsem, NULL);
  }

mcp2515::mcp2515(std::string name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin, int intpin)
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
  xTaskCreatePinnedToCore(MCP2515_rxtask, "MCP2515CanRxTask", 4096, (void*)this, 5, &m_rxtask, 1);
  gpio_set_intr_type((gpio_num_t)m_intpin, GPIO_INTR_NEGEDGE);
  gpio_isr_handler_add((gpio_num_t)m_intpin, MCP2515_isr, (void*)this);
  }

mcp2515::~mcp2515()
  {
  gpio_isr_handler_remove((gpio_num_t)m_intpin);
  vTaskDelete(m_rxtask);
  vSemaphoreDelete(m_rxsem);
  }

esp_err_t mcp2515::Init(CAN_speed_t speed)
  {
  uint8_t buf[16];

  m_speed = speed;

  // RESET commmand
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0b11000000);
  vTaskDelay(5 / portTICK_PERIOD_MS);

  // Set CONFIG mode (abort transmisions, one-shot mode, clkout disabled)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0b10011000);

  // Rx Buffer 0 control (rececive all and enable buffer 1 rollover)
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x60, 0b01100100);

  // CANINTE (interrupt enable), all interrupts
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x2b, 0b11111111);

  // Bus speed
  // Figures from https://www.kvaser.com/support/calculators/bit-timing-calculator/
  // Using SJW=1, SP%=75
  uint8_t cnf0 = 0;
  uint8_t cnf1 = 0;
  uint8_t cnf2 = 0;
  switch (m_speed)
    {
    case CAN_SPEED_100KBPS:
      cnf0=0x04; cnf1=0xb6; cnf2=0x03;
      break;
    case CAN_SPEED_125KBPS:
      cnf0=0x03; cnf1=0xac; cnf2=0x03;
      break;
    case CAN_SPEED_250KBPS:
      cnf0=0x03; cnf1=0xac; cnf2=0x01;
      break;
    case CAN_SPEED_500KBPS:
      cnf0=0x03; cnf1=0xac; cnf2=0x00;
      break;
    case CAN_SPEED_800KBPS:
      cnf0=0x02; cnf1=0x92; cnf2=0x00;
      break;
    case CAN_SPEED_1000KBPS:
      cnf0=0x01; cnf1=0x91; cnf2=0x00;
      break;
    }
  m_spibus->spi_cmd(m_spi, buf, 0, 5, 0x02, 0x28, cnf0, cnf1, cnf2);

  // Set NORMAL mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0x00);

  return ESP_OK;
  }

esp_err_t mcp2515::Stop()
  {
  uint8_t buf[16];

  // RESET command
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0b11000000);
  vTaskDelay(5 / portTICK_PERIOD_MS);

  // Set SLEEP mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0x30);

  // Get STATUS
  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 2, 2, 0b00000011, 0x0e);
  printf("Got back status %02x error flag %02x\n",p[0],p[1]);

  return ESP_OK;
  }

esp_err_t mcp2515::Write(const CAN_frame_t* p_frame)
  {
  uint8_t buf[16];
  uint8_t id[4];

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
    id[0] = 0;
    id[1] = (p_frame->MsgID >> 16) + 0x08;  // HIGH 2 bits of extended ID
    id[2] = (p_frame->MsgID >> 8) & 0xff;   // MID 8 bits of extended ID
    id[3] = (p_frame->MsgID & 0xff);        // LOW 8 bits of extended ID
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

void mcp2515::PollRX()
  {
  uint8_t buf[16];

  uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 2, 2, 0b00000011, 0x2c);
  uint8_t intstat = p[0];
  uint8_t errflag = p[1];
  printf("MCP2515 %s CAN status is %02x error flag %02x\n",m_name.c_str(),intstat,errflag);

  // MCP2515 BITMODIFY CANINTE (Interrupt Enable) Clear flags
  m_spibus->spi_cmd(m_spi, buf, 0, 4, 0b00000101, 0x2c, *p, 0x00);

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

    uint8_t *p = m_spibus->spi_cmd(m_spi, buf, 13, 1, 0x90+rxbuf);
    msg.MsgID = (*p << 3) + (p[1] >> 5);
    msg.FIR.B.DLC = p[4] & 0x0f;
    memcpy(p+5,&msg.data,8);
    MyCan.IncomingFrame(&msg);
    }
  }

void mcp2515::SetPowerMode(PowerMode powermode)
  {
  switch (powermode)
    {
    case  On:
      Init(m_speed);
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
