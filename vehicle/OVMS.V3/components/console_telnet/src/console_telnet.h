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

#include "libtelnet.h"
#include "ovms_console.h"

struct mg_connection;

class OvmsTelnet
  {
  public:
    OvmsTelnet();

  public:
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
    void EventHandler(struct mg_connection *nc, int ev, void *p);

  public:
    bool m_running;
  };

class ConsoleTelnet : public OvmsConsole
  {
  public:
    ConsoleTelnet(struct mg_connection* nc);
    virtual ~ConsoleTelnet();

  private:
    void HandleDeviceEvent(void* pEvent);
    static void TelnetCallback(telnet_t *telnet, telnet_event_t *event, void *userData);
    void TelnetHandler(telnet_event_t *event);

  public:
    void Receive();
    void Exit();
    int puts(const char* s);
    int printf(const char* fmt, ...) __attribute__ ((format (printf, 2, 3)));
    ssize_t write(const void *buf, size_t nbyte);

  protected:
    mg_connection* m_connection;
    telnet_t *m_telnet;
  };

#endif //#ifndef __CONSOLE_TELNET_H__
