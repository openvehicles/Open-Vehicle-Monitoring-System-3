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

#include <stdio.h>
#include <string.h>
#include "command.h"
#include "driver/gpio.h"
#include "peripherals.h"

Peripherals MyPeripherals __attribute__ ((init_priority (3000)));

Peripherals::Peripherals()
  {
  puts("Initialising OVMS Peripherals...");

  gpio_set_direction((gpio_num_t)VSPI_PIN_MISO, GPIO_MODE_INPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MOSI, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MAX7317_CS, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_1_CS, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)VSPI_PIN_MCP2515_2_CS, GPIO_MODE_OUTPUT);

  gpio_set_direction((gpio_num_t)SDCARD_PIN_CLK, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)SDCARD_PIN_CMD, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)SDCARD_PIN_CD, GPIO_MODE_INPUT);

  puts ("  SPI bus...");
  m_spibus = new spi("spi", VSPI_PIN_MISO, VSPI_PIN_MOSI, VSPI_PIN_CLK);

  puts("  MAX7317 I/O Expander...");
  m_max7317 = new max7317("egpio", m_spibus, VSPI_HOST, 1000000, VSPI_PIN_MAX7317_CS);
  puts("  ESP32 CAN...");
  m_esp32can = new esp32can("can1", ESP32CAN_PIN_TX, ESP32CAN_PIN_RX);
  m_esp32can->Init(CAN_SPEED_1000KBPS);
  puts("  ESP32 WIFI...");
  m_esp32wifi = new esp32wifi("wifi");
  puts("  ESP32 BLUETOOTH...");
  m_esp32bluetooth = new esp32bluetooth("bluetooth");
  puts("  ESP32 ADC...");
  m_esp32adc = new esp32adc("adc", ADC1_CHANNEL_0, ADC_WIDTH_12Bit, ADC_ATTEN_11db);
  puts("  MCP2515 CAN 1/2...");
  m_mcp2515_1 = new mcp2515("can2", m_spibus, VSPI_HOST, 1000000, VSPI_PIN_MCP2515_1_CS);
  puts("  MCP2515 CAN 2/2...");
  m_mcp2515_2 = new mcp2515("can3", m_spibus, VSPI_HOST, 1000000, VSPI_PIN_MCP2515_2_CS);
  puts("  SD CARD...");
  m_sdcard = new sdcard("sdcard", false,true,SDCARD_PIN_CD);
  }

Peripherals::~Peripherals()
  {
  }
