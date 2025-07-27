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

#include <sdkconfig.h>
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

#include "ovms_log.h"
static const char *TAG = "ovms-duk-http";

#include <string>
#include <fstream>
#include <iostream>
#include <string.h>
#include <stdio.h>
#include <dirent.h>
#include <esp_task_wdt.h>
#include "ovms_malloc.h"
#include "ovms_module.h"
#include "ovms_duktape.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "console_async.h"
#include "buffered_shell.h"
#include "ovms_netmanager.h"
#if CONFIG_MG_ENABLE_SSL
#include "ovms_tls.h"
#endif

////////////////////////////////////////////////////////////////////////////////
// DuktapeHTTPRequest

////////////////////////////////////////////////////////////////////////////////
// DuktapeHTTPRequest: perform asynchronous HTTP request
//  - uses GET/POST (if post data is given)
//  - follows 301/302 redirects automatically (max 5 hops)
//  - automatically prevents garbage collection while active
//  - Note: any valid server response is considered a success (= triggers done callback)
//  - TODO: implement request.abort() method
//  - TODO: implement digest authentication
//
// Javascript API:
//  create:
//     var request = HTTP.Request({
//       url: "…",
//       [headers: [{ "key": "value", … }, …]]    // Note: array members may contain multiple headers
//       [post: "foo=bar&…",]                     // assumed x-www-form-urlencoded w/o Content-Type
//       [timeout: ms,]                           // default: 120 seconds
//       [binary: bool,]                          // default: false
//       [done: function(response){},]
//       [fail: function(error){},]
//     });
//
//  done(): (this = request, response = request.response)
//    request.response = {
//      statusCode: e.g. 200,
//      statusText: e.g. "OK",
//      [body: <string>,]                         // if binary=false
//      [data: <Uint8Array>,]                     // if binary=true
//      headers: [{ key: value }, …],
//    }
//
//  fail(): (this = request, error = request.error)
//    request.error = "error description"
//
//  also:
//    request.url = last URL used if redirected
//    request.redirectCount = number of redirects

class DuktapeHTTPRequest : public DuktapeObject
  {
  public:
    DuktapeHTTPRequest(duk_context *ctx, int obj_idx);
    ~DuktapeHTTPRequest();
    static duk_ret_t Create(duk_context *ctx);

  protected:
    bool StartRequest(duk_context *ctx=NULL);

  public:
    static void MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data);
    void MongooseCallback(struct mg_connection *nc, int ev, void *ev_data);

  public:
    duk_ret_t CallMethod(duk_context *ctx, const char* method, DuktapeCallbackParameter* params=nullptr) override;

  protected:
    extram::string m_url;
    int m_redirectcnt = 0;
    bool m_ispost = false;
    extram::string m_post;
    int m_timeout = 120*1000;
    bool m_binary = false;
    extram::string m_headers;
    extram::string m_error;
    struct mg_connection *m_mgconn = NULL;
    int m_response_status = 0;
    extram::string m_response_statusmsg;
    extram::string m_response_body;
    std::list<std::pair<extram::string, extram::string>> m_response_headers;
  };

DuktapeHTTPRequest::DuktapeHTTPRequest(duk_context *ctx, int obj_idx)
  : DuktapeObject(ctx, obj_idx)
  {
  // get args:
  duk_require_stack(ctx, 5);
  if (duk_get_prop_string(ctx, 0, "url"))
    m_url = duk_to_string(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "post"))
    {
    m_ispost = true;
    m_post = duk_to_string(ctx, -1);
    }
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "timeout"))
    m_timeout = duk_to_int(ctx, -1);
  duk_pop(ctx);
  if (duk_get_prop_string(ctx, 0, "binary"))
    m_binary = duk_to_boolean(ctx, -1);
  duk_pop(ctx);

  if (m_url.empty())
    {
    m_error = "missing argument: url";
    CallMethod(ctx, "fail");
    return;
    }

  // …request headers:
  extram::string key, val;
  bool have_useragent = false, have_contenttype = false;

  duk_get_prop_string(ctx, 0, "headers");
  if (duk_is_array(ctx, -1))
    {
    for (int i=0; true; i++)
      {
      if (!duk_get_prop_index(ctx, -1, i))
        {
        // array end
        duk_pop(ctx);
        break;
        }
      if (duk_is_object(ctx, -1))
        {
        duk_enum(ctx, -1, 0);
        while (duk_next(ctx, -1, true))
          {
          key = duk_to_string(ctx, -2);
          val = duk_to_string(ctx, -1);
          duk_pop_2(ctx);
          m_headers.append(key);
          m_headers.append(": ");
          m_headers.append(val);
          m_headers.append("\r\n");
          if (strcasecmp(key.c_str(), "User-Agent") == 0)
            have_useragent = true;
          else if (strcasecmp(key.c_str(), "Content-Type") == 0)
            have_contenttype = true;
          }
        duk_pop(ctx); // enum
        }
      duk_pop(ctx); // array element
      }
    }
  duk_pop(ctx); // [array]

  // add defaults:
  if (!have_useragent)
    {
    m_headers.append("User-Agent: ");
    m_headers.append(get_user_agent().c_str());
    m_headers.append("\r\n");
    }
  if (m_ispost && !have_contenttype)
    {
    m_headers.append("Content-Type: application/x-www-form-urlencoded\r\n");
    }

  // check network:
  if (!MyNetManager.MongooseRunning() || !MyNetManager.m_connected_any)
    {
    m_error = "network unavailable";
    CallMethod(ctx, "fail");
    return;
    }

  // start initial request:
  if (StartRequest(ctx))
    {
    // running, prevent deletion & GC:
    Ref();
    Register(ctx);
    ESP_LOGD(TAG, "DuktapeHTTPRequest: started '%s'", m_url.c_str());
    }
  }

bool DuktapeHTTPRequest::StartRequest(duk_context *ctx /*=NULL*/)
  {
  // create connection:
  m_mgconn = NULL;
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts = {};
  opts.user_data = this;
  const char* err;
  opts.error_string = &err;

  if (startsWith(m_url, "https://"))
    {
    #if MG_ENABLE_SSL
      opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList();
    #else
      m_error = "SSL support disabled";
      ESP_LOGD(TAG, "DuktapeHTTPRequest: connect to '%s' failed: %s", m_url.c_str(), m_error.c_str());
      CallMethod(ctx, "fail");
      return false;
    #endif
    }

  m_mgconn = mg_connect_http_opt(mgr, MongooseCallbackEntry, opts,
    m_url.c_str(), m_headers.c_str(), m_ispost ? m_post.c_str() : NULL);

  if (!m_mgconn)
    {
    ESP_LOGD(TAG, "DuktapeHTTPRequest: connect to '%s' failed: %s", m_url.c_str(), err);
    m_error = (err && *err) ? err : "unknown";
    CallMethod(ctx, "fail");
    return false;
    }

  // connection created:
  if (m_timeout > 0)
    mg_set_timer(m_mgconn, mg_time() + (double)m_timeout / 1000);
  return true;
  }

DuktapeHTTPRequest::~DuktapeHTTPRequest()
  {
  // ESP_LOGD(TAG, "~DuktapeHTTPRequest");
  if (m_mgconn)
    {
    m_mgconn->user_data = NULL;
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    }
  }

duk_ret_t DuktapeHTTPRequest::Create(duk_context *ctx)
  {
  // var request = HTTP.Request({ args })
  DuktapeHTTPRequest* request = new DuktapeHTTPRequest(ctx, 0);
  request->Push(ctx);
  return 1;
  }

void DuktapeHTTPRequest::MongooseCallbackEntry(struct mg_connection *nc, int ev, void *ev_data)
  {
  DuktapeHTTPRequest* me = (DuktapeHTTPRequest*)nc->user_data;
  if (me) me->MongooseCallback(nc, ev, ev_data);
  }

void DuktapeHTTPRequest::MongooseCallback(struct mg_connection *nc, int ev, void *ev_data)
  {
  OvmsRecMutexLock lock(&m_mutex);
  if (nc != m_mgconn) return; // ignore events of previous connections
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int err = *(int*) ev_data;
      const char* errdesc = strerror(err);
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CONNECT err=%d/%s", err, errdesc);
      if (err)
        {
        #if MG_ENABLE_SSL
        if (err == MG_SSL_ERROR)
          m_error = "SSL error";
        else
        #endif
          m_error = (errdesc && *errdesc) ? errdesc : "unknown";
        RequestCallback("fail");
        nc->flags |= MG_F_CLOSE_IMMEDIATELY;
        }
      }
      break;

    case MG_EV_HTTP_REPLY:
      {
      // response is complete, store:
      http_message *hm = (http_message*) ev_data;
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_HTTP_REPLY status=%d bodylen=%d", hm->resp_code, hm->body.len);
      m_response_status = hm->resp_code;
      m_response_statusmsg.assign(hm->resp_status_msg.p, hm->resp_status_msg.len);
      m_response_body.assign(hm->body.p, hm->body.len);
      extram::string key, val, location;
      for (int i = 0; i < MG_MAX_HTTP_HEADERS && hm->header_names[i].len > 0; i++)
        {
        key.assign(hm->header_names[i].p, hm->header_names[i].len);
        val.assign(hm->header_values[i].p, hm->header_values[i].len);
        m_response_headers.push_back(std::make_pair(key, val));
        if (strcasecmp(key.c_str(), "Location") == 0) location = val;
        }

      // follow redirect?
      if (m_response_status == 301 || m_response_status == 302)
        {
        if (location.empty())
          {
          m_error = "redirect without location";
          RequestCallback("fail");
          }
        else if (++m_redirectcnt > 5)
          {
          m_error = "too many redirects";
          RequestCallback("fail");
          }
        else
          {
          ESP_LOGD(TAG, "DuktapeHTTPRequest: redirect code=%d to '%s'", m_response_status, location.c_str());
          m_url = location;
          m_response_status = 0;
          m_response_statusmsg.clear();
          m_response_body.clear();
          m_response_headers.clear();
          if (!StartRequest(NULL))
            RequestCallback("fail");
          }
        }
      else
        {
        RequestCallback("done");
        }
      // in any case, this connection is done:
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;

    case MG_EV_TIMER:
      {
      ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_TIMER");
      m_error = "timeout";
      RequestCallback("fail");
      nc->flags |= MG_F_CLOSE_IMMEDIATELY;
      }
      break;

    case MG_EV_CLOSE:
      {
      if (m_response_status == 0 && m_error.empty())
        {
        ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CLOSE: abort");
        m_error = "abort";
        RequestCallback("fail");
        }
      else
        {
        ESP_LOGD(TAG, "DuktapeHTTPRequest: MG_EV_CLOSE status=%d", m_response_status);
        }
      // Mongoose part done:
      nc->user_data = NULL;
      m_mgconn = NULL;
      Unref();
      }
      break;

    default:
      break;
    }
  }

duk_ret_t DuktapeHTTPRequest::CallMethod(duk_context *ctx, const char* method, DuktapeCallbackParameter* params/*=nullptr*/)
  {
  if (!ctx)
    {
    RequestCallback(method, params);
    return 0;
    }
  OvmsRecMutexLock lock(&m_mutex);
  if (!IsCoupled()) return 0;

  duk_require_stack(ctx, 7);
  int entry_top = duk_get_top(ctx);

  bool deregister = false;

  while (method)
    {
    const char* followup_method = NULL;

    // check method:
    int obj_idx = Push(ctx);
    duk_get_prop_string(ctx, obj_idx, method);
    bool callable = duk_is_callable(ctx, -1);
    duk_pop(ctx);
    if (callable) duk_push_string(ctx, method);
    int nargs = 0;

    // update request.url, set request.redirectCount:
    duk_push_string(ctx, m_url.c_str());
    duk_put_prop_string(ctx, obj_idx, "url");
    duk_push_int(ctx, m_redirectcnt);
    duk_put_prop_string(ctx, obj_idx, "redirectCount");

    // create results & method arguments:
    if (strcmp(method, "done") == 0)
      {
      // done(response):
      followup_method = "always";
      ESP_LOGD(TAG, "DuktapeHTTPRequest: done status=%d bodylen=%d url='%s'",
        m_response_status, m_response_body.size(), m_url.c_str());

      // clear request.error:
      duk_push_string(ctx, "");
      duk_put_prop_string(ctx, obj_idx, "error");

      // create response object:
      duk_push_object(ctx);
      duk_push_int(ctx, m_response_status);
      duk_put_prop_string(ctx, -2, "statusCode");
      duk_push_string(ctx, m_response_statusmsg.c_str());
      duk_put_prop_string(ctx, -2, "statusText");

      // …body:
      if (m_binary)
        {
        void* p = duk_push_fixed_buffer(ctx, m_response_body.size());
        memcpy(p, m_response_body.data(), m_response_body.size());
        duk_put_prop_string(ctx, -2, "data");
        }
      else
        {
        duk_push_lstring(ctx, m_response_body.data(), m_response_body.size());
        duk_put_prop_string(ctx, -2, "body");
        }
      m_response_body.clear();
      m_response_body.shrink_to_fit();

      // …response headers:
      duk_push_array(ctx);
      int i = 0;
      for (auto it = m_response_headers.begin(); it != m_response_headers.end(); it++, i++)
        {
        duk_push_object(ctx);
        duk_push_string(ctx, it->second.c_str());
        duk_put_prop_string(ctx, -2, it->first.c_str());
        duk_put_prop_index(ctx, -2, i);
        }
      duk_put_prop_string(ctx, -2, "headers");
      m_response_headers.clear();

      duk_dup(ctx, -1);
      duk_put_prop_string(ctx, obj_idx, "response");
      nargs++;
      }
    else if (strcmp(method, "fail") == 0)
      {
      // fail(error):
      followup_method = "always";
      ESP_LOGD(TAG, "DuktapeHTTPRequest: failed error='%s' url='%s'",
        m_error.c_str(), m_url.c_str());

      // set request.error:
      duk_push_string(ctx, m_error.c_str());
      duk_dup(ctx, -1);
      duk_put_prop_string(ctx, obj_idx, "error");
      nargs++;
      }
    else if (strcmp(method, "always") == 0)
      {
      // always():
      deregister = true;
      }

    // call method:
    if (callable)
      {
      ESP_LOGD(TAG, "DuktapeHTTPRequest: calling method '%s' nargs=%d", method, nargs);
      if (duk_pcall_prop(ctx, obj_idx, nargs) != 0)
        {
        DukOvmsErrorHandler(ctx, -1);
        }
      }

    // clear stack:
    duk_pop_n(ctx, duk_get_top(ctx) - entry_top);

    // followup call:
    method = followup_method;
    } // while (method)

  // allow GC:
  if (deregister) Deregister(ctx);
  return 0;
  }

////////////////////////////////////////////////////////////////////////////////
// DuktapeHTTPInit registration

class DuktapeHTTPInit
  {
  public: DuktapeHTTPInit();
} MyDuktapeHTTPInit  __attribute__ ((init_priority (1700)));

DuktapeHTTPInit::DuktapeHTTPInit()
  {
  ESP_LOGI(TAG, "Installing DUKTAPE HTTP (1710)");

  DuktapeObjectRegistration* dt_http = new DuktapeObjectRegistration("HTTP");
  dt_http->RegisterDuktapeFunction(DuktapeHTTPRequest::Create, 1, "request"); // legacy
  dt_http->RegisterDuktapeFunction(DuktapeHTTPRequest::Create, 1, "Request");
  MyDuktape.RegisterDuktapeObject(dt_http);
  }

#endif // CONFIG_OVMS_SC_GPL_MONGOOSE
