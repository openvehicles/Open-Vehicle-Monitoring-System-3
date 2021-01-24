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
#ifndef __CONSOLE_SSH_H__
#define __CONSOLE_SSH_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ovms_console.h"
#include "task_base.h"

#define BUFFER_SIZE 512

class ConsoleSSH;
struct mg_connection;

class OvmsSSH
  {
  public:
    OvmsSSH();

  public:
    void EventHandler(struct mg_connection *nc, int ev, void *p);
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
    static int Authenticate(uint8_t type, WS_UserAuthData* data, void* ctx);
    WOLFSSH_CTX* ctx() { return m_ctx; }

  protected:
    WOLFSSH_CTX* m_ctx;
    bool m_keyed;
  };

class ConsoleSSH : public OvmsConsole
  {
  public:
    ConsoleSSH(OvmsSSH* server, struct mg_connection* nc);
    virtual ~ConsoleSSH();

  private:
    void Service();
    void HandleDeviceEvent(void* pEvent);
    int GetResponse();

  public:
    void Receive();
    void Send();
    void Sent();
    void Exit();
    int puts(const char* s);
    int printf(const char* fmt, ...);
    ssize_t write(const void *buf, size_t nbyte);
    int RecvCallback(char* buf, uint32_t size);
    bool IsDraining() { return m_drain > 0; }

  private:
    typedef struct {DIR* dir; size_t size;} Level;
    enum States
      {
      ACCEPT,
      SHELL,
      EXEC,
      SOURCE,
      SOURCE_LOOP,
      SOURCE_DIR,
      SOURCE_SEND,
      SOURCE_RESPONSE,
      SINK,
      SINK_LOOP,
      SINK_RECEIVE,
      SINK_RESPONSE,
      CLOSING
      } m_state;
    OvmsSSH* m_server;
    mg_connection* m_connection;
    WOLFSSH* m_ssh;
    char m_buffer[BUFFER_SIZE];
    int m_index;                // Index into m_buffer of data remaining to send
    int m_size;                 // Size of data remaining to send
    int m_drain;                // Bytes discarded waiting for socket to drain
    bool m_sent;
    bool m_rekey;
    bool m_needDir;
    bool m_isDir;
    bool m_verbose;
    bool m_recursive;
    std::string m_path;
    std::string m_filename;
    FILE* m_file;
    std::forward_list<Level> m_dirs;
  };

class RSAKeyGenerator : public TaskBase
  {
  public:
    RSAKeyGenerator();
    virtual ~RSAKeyGenerator() {}

  private:
    void Service();

  private:
  byte m_der[1200];   // Big enough for 2048-bit private key
  };
#endif //#ifndef __CONSOLE_SSH_H__
