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
#include <unistd.h>
#include <string.h>
#include <cstdarg>
#include "rom/uart.h"
#include "command.h"
#include "console_async.h"
#include "microrl.h"
#include "driver/uart.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
static QueueHandle_t uart0_queue;
static const char *TAG = "uart_events";

void ConsoleAsyncPrint(microrl_t* rl, const char * str)
  {
  fwrite(str,strlen(str),1,stdout);
  fflush(stdout);
  }

char ** ConsoleAsyncComplete (microrl_t* rl, int argc, const char * const * argv )
  {
  return MyCommandApp.Complete((ConsoleAsync*)rl->userdata, argc, argv);
  }

int ConsoleAsyncExecute (microrl_t* rl, int argc, const char * const * argv )
  {
  MyCommandApp.Execute(COMMAND_RESULT_VERBOSE, (OvmsWriter*)rl->userdata, argc, argv);
  return 0;
  }

void ConsoleAsyncTask(void *pvParameters)
  {
  uart_event_t event;
  size_t buffered_size;
  uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  ConsoleAsync* me = (ConsoleAsync*)pvParameters;

  vTaskDelay(50 / portTICK_PERIOD_MS);

  for (;;)
    {
    // Waiting for UART RX event.
    if (xQueueReceive(uart0_queue, (void * )&event, (portTickType)portMAX_DELAY))
      {
      switch (event.type)
        {
        // Event of UART receving data
        case UART_DATA:
          uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
          if (buffered_size > 0)
            {
            int len = uart_read_bytes(EX_UART_NUM, data, BUF_SIZE, 100 / portTICK_RATE_MS);
            for (int i = 0; i < len; ++i)
              {
              // put received char from UART to microrl lib
              microrl_insert_char (me->GetRL(), data[i]);
              }
            }
          break;
          // Event of HW FIFO overflow detected
        case UART_FIFO_OVF:
          ESP_LOGI(TAG, "hw fifo overflow\n");
          // If fifo overflow happened, you should consider adding flow control for your application.
          // We can read data out out the buffer, or directly flush the rx buffer.
          uart_flush(EX_UART_NUM);
          break;
          // Event of UART ring buffer full
        case UART_BUFFER_FULL:
          ESP_LOGI(TAG, "ring buffer full\n");
          // If buffer full happened, you should consider encreasing your buffer size
          // We can read data out out the buffer, or directly flush the rx buffer.
          uart_flush(EX_UART_NUM);
          break;
          // Others
        default:
          ESP_LOGI(TAG, "uart event type: %d\n", event.type);
          break;
        }
      }
    }
  }

ConsoleAsync::ConsoleAsync()
  {
  uart_config_t uart_config =
    {
    .baud_rate = 115200,
    .data_bits = UART_DATA_8_BITS,
    .parity = UART_PARITY_DISABLE,
    .stop_bits = UART_STOP_BITS_1,
    .flow_ctrl = UART_HW_FLOWCTRL_DISABLE,
    .rx_flow_ctrl_thresh = 122,
    };
  // Set UART parameters
  uart_param_config(EX_UART_NUM, &uart_config);

  // Install UART driver, and get the queue.
  uart_driver_install(EX_UART_NUM, BUF_SIZE * 2, BUF_SIZE * 2, 10, &uart0_queue, 0);

  puts("\n\033[32mWelcome to the Open Vehicle Monitoring System (OVMS) - async console\033[0m");

  microrl_init (&m_rl, ConsoleAsyncPrint);
  m_rl.userdata = (void*)this;
  microrl_set_complete_callback (&m_rl, ConsoleAsyncComplete);
  microrl_set_execute_callback (&m_rl, ConsoleAsyncExecute);

  xTaskCreatePinnedToCore(ConsoleAsyncTask, "ConsoleAsyncTask", 4096, (void*)this, 5, &m_taskid, 1);
  }

ConsoleAsync::~ConsoleAsync()
  {
  }

microrl_t* ConsoleAsync::GetRL()
  {
  return &m_rl;
  }

int ConsoleAsync::puts(const char* s)
  {
  return ::puts(s);
  }

int ConsoleAsync::printf(const char* fmt, ...)
  {
  va_list args;
  va_start(args,fmt);
  vprintf(fmt,args);
  va_end(args);
  fflush(stdout);
  return 0;
  }

ssize_t ConsoleAsync::write(const void *buf, size_t nbyte)
  {
  ssize_t n = fwrite(buf, nbyte, 1, stdout);
  fflush(stdout);
  return n;
  }

void ConsoleAsync::finalise()
  {
  }

char ** ConsoleAsync::GetCompletion(OvmsCommandMap& children, const char* token)
  {
  unsigned int index = 0;
  if (token)
    {
    for (OvmsCommandMap::iterator it = children.begin(); it != children.end(); ++it)
      {
      const char *key = it->first.c_str();
      if (strncmp(key, token, strlen(token)) == 0)
        {
        if (index < COMPLETION_MAX_TOKENS+1)
          {
          if (index == COMPLETION_MAX_TOKENS)
            {
            key = "...";
            }
          strncpy(m_space[index], key, TOKEN_MAX_LENGTH-1);
          m_space[index][TOKEN_MAX_LENGTH-1] = '\0';
          m_completions[index] = m_space[index];
          ++index;
          }
        }
      }
    }
  m_completions[index] = NULL;
  return m_completions;
  }
