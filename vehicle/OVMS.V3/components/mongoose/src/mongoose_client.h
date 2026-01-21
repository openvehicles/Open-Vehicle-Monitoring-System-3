/*
;    Project:       Open Vehicle Monitor System
;    Date:          18th January 2026
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2026  Mark Webb-Johnson
;    (C) 2011        Sonny Chen @ EPRO/DX
;    (C) 2026       Michael Balzer
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

#ifndef __MONGOOSE_CLIENT_H__
#define __MONGOOSE_CLIENT_H__

#include "ovms_mutex.h"

#define MG_LOCALS 1
#include "mongoose.h"

/**
 * MongooseClient: base class for all Mongoose API users to manage shared API access
 * 
 * Mongoose is single threaded, the API MUST NOT be used concurrently from different threads.
 * 
 * We need to open connections from different contexts, and we need to be able to send data
 * from the event task or component specific tasks. All API users thus need to synchronize
 * their access to at least all Mongoose manager or connection related API calls, and to
 * all API methods involving send/recv mbuf manipulation or direct access to the underlying
 * sockets. Simple utilitiy methods mostly do not need to be synchronized, but when in doubt,
 * use the lock.
 * 
 * Class usage: add as a base class to your component class (may be public/protected/private
 *    as needed, e.g. if you need to get a lock from outside the class, use public)
 * 
 * To gain exclusive access to the Mongoose API, create a scoped lock instance like this:
 *    {
 *    auto mglock = MongooseLock();
 *    … call mg_connect() / mg_send() / mbuf_remove() / …
 *    }
 * 
 * See auto-locking wrappers below for methods that do not need to explicitly get the lock.
 * In most situations it will be better to get the lock for a sequence of operations, so
 * only add new methods to these, if a single call is the standard.
 * 
 * The lock is recursive, so nesting is allowed (within the same task).
 * 
 * Always release the lock as soon as possible, to keep network operations fluid, either
 * by exiting the scope, or by explicitly doing…
 *    mglock.Unlock();
 * 
 * To try locking, pass a timeout (in ticks) to MongooseLock(), and check for success:
 *    if (mglock) / if (mglock.IsLocked()) …
 * 
 * A common scheme is to copy a Mongoose connection pointer into the handler object. This
 * copy needs to be always checked before use, and every access to the copy needs to be
 * synchronized by the Mongoose lock.
 * 
 * The dedicated Mongoose task run by the OVMS NetManager locks the API for currently 100 ms
 * per poll, so any other API user may need to wait up to 100 ms for the lock. If this turns
 * out to be an issue, the poll timeout may be reconsidered.
 */
class MongooseClient
  {
  // Lock management:
  public:
    OvmsRecMutexLock MongooseLock(TickType_t timeout=portMAX_DELAY)
      {
      return OvmsRecMutexLock(&m_mongoose_mutex, timeout);
      }
    OvmsRecMutexLock* CreateMongooseLock(TickType_t timeout=portMAX_DELAY)
      {
      return new OvmsRecMutexLock(&m_mongoose_mutex, timeout);
      }

  // Auto-locking wrappers:
  public:
    void mg_mgr_init(struct mg_mgr *mgr, void *user_data);
    void mg_mgr_free(struct mg_mgr *mgr);
    time_t mg_mgr_poll(struct mg_mgr *mgr, int milli);

  protected:
    static OvmsRecMutex m_mongoose_mutex;     // global mutex for Mongoose access
  };

#endif // __MONGOOSE_CLIENT_H__
