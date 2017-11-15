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

#define BUFFER_SIZE 512

class ConsoleSSH;

/* Map user names to passwords */
/* Use arrays for username and p. The password or public key can
 * be hashed and the hash stored here. Then I won't need the type. */
typedef struct PwMap {
    uint8_t type;
    uint8_t username[32];
    uint32_t usernameSz;
    uint8_t p[SHA256_DIGEST_SIZE];
    struct PwMap* next;
} PwMap;

typedef struct PwMapList {
    PwMap* head;
} PwMapList;

class OvmsSSH : public Parent
  {
  public:
    OvmsSSH();

  public:
    void WifiUp(std::string event, void* data);
    void WifiDown(std::string event, void* data);
  };

class SSHServer : public TaskBase, public Parent
  {
  public:
    SSHServer(Parent* parent);
    ~SSHServer();
    WOLFSSH_CTX* ctx() { return m_ctx; }

  private:
    bool Instantiate();
    void Service();
    void Cleanup();
    static int SSHUserAuthCallback(uint8_t authType, const WS_UserAuthData* authData, void* ctx);

  protected:
    WOLFSSH_CTX* m_ctx;
    int m_socket;

  public:
    PwMapList pwMapList;
  };

class SSHReceiver : public TaskBase
  {
  public:
    SSHReceiver(ConsoleSSH* parent, int socket, QueueHandle_t queue, SemaphoreHandle_t sem);
    ~SSHReceiver();

  private:
    bool Instantiate();
    void Service();

  protected:
    int m_socket;
    QueueHandle_t m_queue;
    SemaphoreHandle_t m_semaphore;
  };

class ConsoleSSH : public OvmsConsole, public Parent
  {
  public:
    ConsoleSSH(SSHServer* server, int socket);
    virtual ~ConsoleSSH();

  private:
    SSHServer* server() { return (SSHServer*)parent(); }
    bool Instantiate();
    void Service();
    void HandleDeviceEvent(void* pEvent);

  public:
    void Exit();
    int puts(const char* s);
    int printf(const char* fmt, ...);
    ssize_t write(const void *buf, size_t nbyte);

  protected:
    WOLFSSH* m_ssh;
    int m_socket;
    SemaphoreHandle_t m_semaphore;
    char m_buffer[BUFFER_SIZE];
  };

#endif //#ifndef __CONSOLE_SSH_H__
