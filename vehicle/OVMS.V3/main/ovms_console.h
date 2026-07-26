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

#include <list>
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
      ALERT,                // String alert (single C-string to single console) [DEPRECATED,UNUSED]
      ALERT_MULTI,          // LogBuffers style alert (multiple C-strings to multiple consoles)
      } event_type_t;

    typedef struct
      {
      event_type_t type;    // Our extended event type enum
      union
        {
        char* buffer;       // Pointer to ALERT buffer [DEPRECATED,UNUSED]
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
    char** SetCompletion(int index, const char* token, bool isfinal) override;
    char** GetCompletions(int &common_len, bool &finished ) override;
    void Log(LogBuffers* message);
    void Poll(portTickType ticks, QueueHandle_t queue = NULL);
    // Set once the transport is closing: the mongoose connection may already be
    // freed, so an in-flight write() from a follow-mode task must short-circuit
    // before dereferencing it. Read cross-task (see ConsoleReaper).
    void SetClosing() { m_closing = true; }
    bool IsClosing() { return m_closing; }

  protected:
    void Service();
    void DeleteEventQueue(QueueHandle_t* queuep);
    void finalise();

  protected:
    virtual void HandleDeviceEvent(void* event) = 0;

  protected:
    bool m_ready;
    volatile bool m_closing;    // true once the transport is closing; read cross-task
    char *m_completions[COMPLETION_MAX_TOKENS+2];
    int m_common;
    char m_space[COMPLETION_MAX_TOKENS+2][TOKEN_MAX_LENGTH];
    bool m_final;
    QueueHandle_t m_queue;      // Queue for console specific device events & log messages
    QueueHandle_t m_deferred;   // Temporary queue for log messages received while InsertCallback active
    int m_discarded;            // Number of overflows on the deferred queue
    DisplayState m_state;
    unsigned int m_lost;        // Log messages lost due to full queue
    unsigned int m_acked;       // Log messages acknowledged as lost
  };

// Deferred-teardown support for consoles served over a Mongoose transport
// (SSH, Telnet). Such a console is deleted from its server's MG_EV_CLOSE
// handler, which runs holding the Mongoose lock. If an interactive follow-mode
// task (e.g. "vfs tail") is still bound to the console, its write path needs
// that same lock, so joining it synchronously from MG_EV_CLOSE deadlocks.
// ConsoleReaper breaks the cycle: mark the console closing (so its in-flight
// write bails without touching the about-to-be-freed connection), ask the task
// to stop, and delete the console later — from the server's MG_EV_POLL handler
// — once the task has actually exited. A server mixes this in alongside
// MongooseClient and calls CloseConsole() from MG_EV_CLOSE and ReapConsoles()
// from MG_EV_POLL.
class ConsoleReaper
  {
  public:
    // Call from MG_EV_CLOSE. Returns true if the console was queued for deferred
    // reaping (the caller must NOT delete it); false if no follow-mode task is
    // bound and the caller should delete it synchronously as before.
    bool CloseConsole(OvmsConsole* child);
    // Call from MG_EV_POLL: delete any deferred consoles whose follow-mode task
    // has now exited.
    void ReapConsoles();

  protected:
    std::list<OvmsConsole*> m_reaping;   // consoles awaiting follow-task exit before delete
  };

#endif //#ifndef __CONSOLE_H__
