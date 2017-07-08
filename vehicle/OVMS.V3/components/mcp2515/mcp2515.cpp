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
      //send frame to listeners
      //MyCan.IncomingFrame(&msg);

      //Let the hardware know the frame has been read.
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
  uint8_t buf[4];

  m_speed = speed;

  // RESET commmand
  m_spibus->spi_cmd(m_spi, buf, 0, 1, 0b11000000);
  vTaskDelay(5 / portTICK_PERIOD_MS);

  // Set NORMAL mode
  m_spibus->spi_cmd(m_spi, buf, 0, 3, 0x02, 0x0f, 0x00);

  // Get STATUS
  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 2, 2, 0b00000011, 0x0e);
  printf("Got back status %02x error flag %02x\n",p[0],p[1]);

  return ESP_OK;
  }

esp_err_t mcp2515::Stop()
  {
  uint8_t buf[4];

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
  return ESP_OK;
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
