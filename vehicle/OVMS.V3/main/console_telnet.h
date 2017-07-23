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
#ifndef __CONSOLE_TELNET_H__
#define __CONSOLE_TELNET_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "libtelnet.h"
#include "console.h"

class TelnetServer
  {
  public:
    TelnetServer();
    ~TelnetServer();

  private:
    static void Task(void*);

  protected:
    TaskHandle_t m_taskid;
  };

class ConsoleTelnet : public OvmsConsole
  {
  public:
    ConsoleTelnet(int socket);
    virtual ~ConsoleTelnet();

  public:
    int puts(const char* s);
    int printf(const char* fmt, ...);
    ssize_t write(const void *buf, size_t nbyte);
    void finalise();

  private:
    static void TelnetReceiverTask(void *pvParameters);
    void DoTelnet();
    static void TelnetCallback(telnet_t *telnet, telnet_event_t *event, void *userData);
    void TelnetHandler(telnet_event_t *event);
    void HandleDeviceEvent(void* pEvent);
    static void Exit(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv);

  protected:
    TaskHandle_t m_receiver_taskid;
    int m_socket;
    telnet_t *m_telnet;
    int m_split_eol;
    SemaphoreHandle_t m_semaphore;
    uint8_t m_buffer[1024];
  };

#endif //#ifndef __CONSOLE_TELNET_H__
