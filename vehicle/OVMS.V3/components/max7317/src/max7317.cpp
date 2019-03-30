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

#include "ovms_log.h"
static const char *TAG = "max7317";

#include <string.h>
#include "max7317.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"

max7317::max7317(const char* name, spi* spibus, spi_nodma_host_device_t host, int clockspeed, int cspin)
  : pcp(name)
  {
  m_spibus = spibus;
  m_host = host;
  m_clockspeed = clockspeed;
  m_cspin = cspin;

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
  }

max7317::~max7317()
  {
  }

void max7317::Output(uint8_t port, uint8_t level)
  {
  uint8_t buf[4];

  m_spibus->spi_cmd(m_spi, buf, 0, 2, port, level);
  }

uint8_t max7317::Input(uint8_t port)
  {
  uint8_t buf[4];

  uint8_t* p = m_spibus->spi_cmd(m_spi, buf, 1, 1, port + 0x80);
  return *p;
  }

void max7317_output(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int port = atoi(argv[0]);
  if ((port <0)||(port>9))
    {
    writer->puts("Error: Port should be in range 0..9");
    return;
    }
  int level = atoi(argv[1]);
  if ((level<0)||(level>255))
    {
    writer->puts("Error: Level should be in range 0..255");
    return;
    }

  MyPeripherals->m_max7317->Output((uint8_t)port,(uint8_t)level);
  writer->printf("EGPIO port %d set to output level %d\n", port, level);
  }

class Max7317Init
  {
  public: Max7317Init();
} MyMax7317Init  __attribute__ ((init_priority (4200)));

Max7317Init::Max7317Init()
  {
  ESP_LOGI(TAG, "Initialising MAX7317 EGPIO (4200)");

  OvmsCommand* cmd_egpio = MyCommandApp.RegisterCommand("egpio","EGPIO framework");
  cmd_egpio->RegisterCommand("output","Set EGPIO output level",max7317_output, "<port> <level>", 2, 2);
  }
