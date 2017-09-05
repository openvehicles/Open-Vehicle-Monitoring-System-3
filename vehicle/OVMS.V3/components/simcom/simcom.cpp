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

#include "esp_log.h"
static const char *TAG = "simcom";

#include <string.h>
#include "simcom.h"
#include "peripherals.h"

static void SIMCOM_task(void *pvParameters)
  {
  simcom *me = (simcom*)pvParameters;

  for(;;)
    {
    // Waiting for UART event.
    me->EventHandler();
    }
  }

void simcom::EventHandler()
  {
  uart_event_t event;
  size_t buffered_size = 0;

  if (!m_task) return; // Quick exit if no task running (we are stopped)

  if (xQueueReceive(m_queue, (void *)&event, (portTickType)portMAX_DELAY))
    {
    switch(event.type)
      {
      case UART_DATA:
        {
        buffered_size = 1;
        }
        break;
      case UART_FIFO_OVF:
        uart_flush(m_uartnum);
        break;
      case UART_BUFFER_FULL:
        uart_flush(m_uartnum);
        break;
      case UART_BREAK:
      case UART_PARITY_ERR:
      case UART_FRAME_ERR:
      case UART_PATTERN_DET:
      default:
        break;
      }
    }

  while (buffered_size>0)
    {
    char data[16];
    uart_get_buffered_data_len(m_uartnum, &buffered_size);
    if (buffered_size>16) buffered_size = 16;
    if (buffered_size>0)
      {
      int len = uart_read_bytes(m_uartnum, (uint8_t*)data, buffered_size, 100 / portTICK_RATE_MS);
      MyCommandApp.HexDump("SIMCOM rx",data,len);
      }
    }
  }

simcom::simcom(std::string name, uart_port_t uartnum, int baud, int rxpin, int txpin, int pwregpio)
  : pcp(name)
  {
  m_task = 0;
  m_uartnum = uartnum;
  m_baud = baud;
  m_pwregpio = pwregpio;
  m_rxpin = rxpin;
  m_txpin = txpin;
  }

simcom::~simcom()
  {
  }

void simcom::Start()
  {
  if (!m_task)
    {
    uart_config_t uart_config =
      {
      .baud_rate = m_baud,
      .data_bits = UART_DATA_8_BITS,
      .parity = UART_PARITY_DISABLE,
      .stop_bits = UART_STOP_BITS_1,
      .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
      .rx_flow_ctrl_thresh = 122,
      };
    // Set UART parameters
    uart_param_config(m_uartnum, &uart_config);

    // Install UART driver, and get the queue.
    uart_driver_install(m_uartnum, SIMCOM_BUF_SIZE * 2, SIMCOM_BUF_SIZE * 2, 10, &m_queue, 0);

    // Set UART pins
    uart_set_pin(m_uartnum, m_txpin, m_rxpin, 0, 0);

    xTaskCreatePinnedToCore(SIMCOM_task, "SIMCOMTask", 4096, (void*)this, 5, &m_task, 1);
    }
  }

void simcom::Stop()
  {
  if (m_task)
    {
    uart_driver_delete(m_uartnum);
    vTaskDelete(m_task);
    m_task = 0;
    }
  }

void simcom::SetPowerMode(PowerMode powermode)
  {
  switch (powermode)
    {
    case On:
      Start();
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

void simcom::tx(const char* data, size_t size)
  {
  if (!m_task) return; // Quick exit if not task (we are stopped)
  uart_write_bytes(m_uartnum, data, size);
  }

void simcom_tx(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  for (int k=0; k<argc; k++)
    {
    if (k>0)
      {
      MyPeripherals->m_simcom->tx(" ",1);
      }
    MyPeripherals->m_simcom->tx(argv[k],strlen(argv[k]));
    }
  MyPeripherals->m_simcom->tx("\r\n",2);
  }

class SimcomInit
  {
  public: SimcomInit();
} SimcomInit  __attribute__ ((init_priority (4600)));

SimcomInit::SimcomInit()
  {
  ESP_LOGI(TAG, "Initialising SIMCOM (4600)");

  OvmsCommand* cmd_simcom = MyCommandApp.RegisterCommand("simcom","SIMCOM framework",NULL, "", 1);
  cmd_simcom->RegisterCommand("tx","Transmit data on SIMCOM",simcom_tx, "", 1);
  }
