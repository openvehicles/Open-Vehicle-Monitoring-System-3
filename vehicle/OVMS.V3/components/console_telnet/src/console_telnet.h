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
#include "ovms_console.h"

#define BUFFER_SIZE 1024

class ConsoleTelnet;

class OvmsTelnet : public Parent
  {
  public:
    OvmsTelnet();

  public:
    void WifiUp(std::string event, void* data);
    void WifiDown(std::string event, void* data);
  };

class TelnetServer : public TaskBase, public Parent
  {
  public:
    TelnetServer(Parent* parent);
    ~TelnetServer();

  private:
    bool Instantiate();
    void Service();
    void Cleanup();

  protected:
    int m_socket;
  };

class TelnetReceiver : public TaskBase
  {
  public:
    TelnetReceiver(ConsoleTelnet* parent, int socket, char* buffer, QueueHandle_t queue, SemaphoreHandle_t sem);
    ~TelnetReceiver();

  private:
    bool Instantiate();
    void Service();

  protected:
    int m_socket;
    char* m_buffer;
    QueueHandle_t m_queue;
    SemaphoreHandle_t m_semaphore;
  };

class ConsoleTelnet : public OvmsConsole, public Parent
  {
  public:
    ConsoleTelnet(TelnetServer* server, int socket);
    virtual ~ConsoleTelnet();

  public:
    int puts(const char* s);
    int printf(const char* fmt, ...);
    ssize_t write(const void *buf, size_t nbyte);

  private:
    bool Instantiate();
    void HandleDeviceEvent(void* pEvent);
    static void TelnetCallback(telnet_t *telnet, telnet_event_t *event, void *userData);
    void TelnetHandler(telnet_event_t *event);

  public:
    void Exit();

  protected:
    telnet_t *m_telnet;
    int m_socket;
    SemaphoreHandle_t m_semaphore;
    char m_buffer[BUFFER_SIZE];
  };

#endif //#ifndef __CONSOLE_TELNET_H__
