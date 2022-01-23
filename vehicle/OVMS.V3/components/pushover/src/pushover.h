/*
;    Project:       Open Vehicle Monitor System
;    Date:          23rd July 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
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

#ifndef __PUSHOVER_H__
#define __PUSHOVER_H__

#include "ovms_config.h"
#include "ovms_notify.h"
#include "ovms_buffer.h"
#include <string>


class Pushover : public InternalRamAllocated
  {
  public:
    Pushover();
    ~Pushover();

  public:
    bool SendMessage( const std::string message, int priority, const std::string sound, bool replyNotification=false );
    bool SendMessageOpt( const std::string user_key, const std::string token,
      const std::string message, int priority, const std::string sound, int retry, int expire, bool replyNotification );
    bool SendMessageBlocking( const std::string message, std::string * reply, int priority, const std::string sound ); // Use only from different task..
    size_t IncomingData(uint8_t* data, size_t len);
    bool NotificationFilter(OvmsNotifyType* type, const char* subtype);
    bool IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry);
    struct mg_connection *m_mgconn;
    OvmsBuffer* m_buffer;
    std::string m_reply;
    OvmsMutex m_mgconn_mutex;
    bool sendReplyNotification;

  protected:
    void EventListener(std::string event, void* data);

  private:
    size_t reader;

  };

extern Pushover MyPushoverClient;


#endif //#ifndef __PUSHOVER_H__
