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
#ifndef __CONSOLE_H__
#define __CONSOLE_H__

#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "ovms_shell.h"

#define TOKEN_MAX_LENGTH 32
#define COMPLETION_MAX_TOKENS 20

class OvmsCommandMap;
class Parent;
class LogBuffers;
struct mbuf;

class OvmsConsole : public OvmsShell
  {
  public:
    OvmsConsole();
    ~OvmsConsole();

  public:
    // In order to share a queue with the UART driver (and possibly other
    // drivers for console devices), we implicitly make a union with the
    // uart_event_t typedef in driver/uart.h and we start our event type enum
    // with a value large enough to leave plenty of space below.
    typedef enum
      {
      RECV = 0x10000,
      ALERT,
      ALERT_MULTI
      } event_type_t;

    typedef struct
      {
      event_type_t type;  // Our extended event type enum
      union
        {
        char* buffer;       // Pointer to ALERT buffer
        LogBuffers* multi;  // Pointer to ALERT_MULTI message
        ssize_t size;       // Buffer size for RECV
        struct mbuf* mbuf;  // Buffer pointer for RECV with Mongoose
        };
      } Event;

    typedef enum
      {
      AT_PROMPT,
      AWAITING_NL,
      NO_NL
      } DisplayState;

  public:
    void Initialize(const char* console);
    char** SetCompletion(int index, const char* token);
    char** GetCompletions() { return m_completions; }
    void Log(LogBuffers* message);
    void Poll(portTickType ticks, QueueHandle_t queue = NULL);

  protected:
    void Service();
    void finalise();

  protected:
    virtual void HandleDeviceEvent(void* event) = 0;

  protected:
    bool m_ready;
    char *m_completions[COMPLETION_MAX_TOKENS+2];
    char m_space[COMPLETION_MAX_TOKENS+2][TOKEN_MAX_LENGTH];
    QueueHandle_t m_queue;
    QueueHandle_t m_deferred;
    int m_discarded;
    DisplayState m_state;
    unsigned int m_lost;        // Log messages lost due to full queue
    unsigned int m_acked;       // Log messages acknowledged as lost
  };

#endif //#ifndef __CONSOLE_H__
