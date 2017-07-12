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
#include "console_async.h"
#include "freertos/queue.h"
#include "esp_log.h"

#define EX_UART_NUM UART_NUM_0
#define BUF_SIZE (1024)
static QueueHandle_t uart0_queue;
static const char *TAG = "uart_events";
static char CRbuf[1] = { '\r' };
static char NLbuf[1] = { '\n' };
static char ctrlRbuf[1] = { 'R'-0100 };

typedef enum
  {
  AT_PROMPT,
  AWAITING_NL,
  NO_NL
  } DisplayState;

typedef enum
  {
  ASYNC_BUFFER = UART_EVENT_MAX+1
  } async_event_type_t;

// In order to share a queue with the UART driver, the following struct typedef
// shows how we are implicitly making a union with the typedef in driver/uart.h
typedef union
  {
  uart_event_t uart;            // Actual queue event struct, consisting of:
  //uart_event_type_t type;     /* UART event type enum */
  //size_t size;                /* UART data size for UART_DATA event */
  struct
    {
    async_event_type_t type;    // Our extended event type enum
    char* buffer;               // Pointer to dynamically allocated buffer
    } async;
  } AsyncEvent;

void ConsoleAsyncTask(void *pvParameters)
  {
  DisplayState state = AT_PROMPT;
  portTickType ticks = portMAX_DELAY;
  AsyncEvent event;
  size_t buffered_size;
  uint8_t* data = (uint8_t*) malloc(BUF_SIZE);
  ConsoleAsync* me = (ConsoleAsync*)pvParameters;

  vTaskDelay(50 / portTICK_PERIOD_MS);

  for (;;)
    {
    // Waiting for UART RX event or async log message event
    if (xQueueReceive(uart0_queue, (void * )&event, ticks))
      {
      if (event.async.type == ASYNC_BUFFER)
        {
        // We remove the newline from the end of a log message so that we can later
        // output a newline as part of restoring the command prompt and its line
        // without leaving a blank line above it.  So before we display a new log
        // message we need to output a newline if the last action was displaying a
        // log message, or output a carriage return to back over the prompt.
        if (state == AWAITING_NL)
          fwrite(NLbuf, 1, 1, stdout);
        else if (state == AT_PROMPT)
          fwrite(CRbuf, 1, 1, stdout);
        size_t len = strlen(event.async.buffer);
        if (event.async.buffer[len-1] == '\n')
          {
          event.async.buffer[--len] = '\0';
          if (event.async.buffer[len-1] == '\r')  // Remove CR, too, in case of \r\n
            event.async.buffer[--len] = '\0';
          state = AWAITING_NL;
          fwrite(event.async.buffer, len, 1, stdout);
          fflush(stdout);
          }
        else
          {
          state = NO_NL;
          fwrite(event.async.buffer, len, 1, stdout);
          }
        free(event.async.buffer);
        ticks = 200 / portTICK_PERIOD_MS;
        }
      else switch (event.uart.type)
        {
        case UART_DATA:
          if (state != AT_PROMPT)
            me->ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
          state = AT_PROMPT;
          uart_get_buffered_data_len(EX_UART_NUM, &buffered_size);
          if (buffered_size > 0)
            {
            int len = uart_read_bytes(EX_UART_NUM, data, BUF_SIZE, 100 / portTICK_RATE_MS);
            me->ProcessChars((char*)data, len);
            }
          break;
        case UART_FIFO_OVF:
          ESP_LOGI(TAG, "hw fifo overflow\n");
          // If fifo overflow happened, you should consider adding flow control for your application.
          // We can read data out out the buffer, or directly flush the rx buffer.
          uart_flush(EX_UART_NUM);
          break;
        case UART_BUFFER_FULL:
          ESP_LOGI(TAG, "ring buffer full\n");
          // If buffer full happened, you should consider encreasing your buffer size
          // We can read data out out the buffer, or directly flush the rx buffer.
          uart_flush(EX_UART_NUM);
          break;
        default:
          ESP_LOGI(TAG, "uart event type: %d\n", event.uart.type);
          break;
        }
      }
    else
      {
      // Timeout indicates the queue is empty
      if (state != AT_PROMPT)
        me->ProcessChars(ctrlRbuf, 1);    // Restore the prompt plus any type-in on a new line
      state = AT_PROMPT;
      ticks = portMAX_DELAY;
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

  xTaskCreatePinnedToCore(ConsoleAsyncTask, "ConsoleAsyncTask", 4096, (void*)this, 5, &m_taskid, 1);

  Initialize("async console");
  MyCommandApp.RegisterConsole(this);
  }

ConsoleAsync::~ConsoleAsync()
  {
  MyCommandApp.DeregisterConsole(this);
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

void ConsoleAsync::Log(char* message)
  {
  AsyncEvent event;
  event.async.type = ASYNC_BUFFER;
  event.async.buffer = message;
  BaseType_t ret = xQueueSendToBack(uart0_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret != pdPASS)
    {
    free(event.async.buffer);
    ESP_LOGI(TAG, "Timeout queueing message in ConsoleAsync::Log\n");
    }
  }
