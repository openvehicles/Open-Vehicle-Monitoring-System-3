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

#include "esp_log.h"
static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include "ovms_webserver.h"

OvmsWebServer MyWebServer __attribute__ ((init_priority (8200)));

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

static void OvmsWebServerMongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  // ESP_LOGI(TAG, "Event %d",ev);
  switch (ev)
    {
    case MG_EV_HTTP_REQUEST:
      {
      struct http_message *h = (struct http_message *)p;
      std::string method(h->method.p, h->method.len);
      std::string uri(h->uri.p, h->uri.len);
      ESP_LOGI(TAG, "HTTP %s %s",method.c_str(),uri.c_str());
      mg_printf(nc, "%s", "HTTP/1.1 200 OK\r\n\r\nNothing to see here\r\n");
      nc->flags |= MG_F_SEND_AND_CLOSE;
      }
      break;
    default:
      break;
    }
  }

void OvmsWebServer::NetManInit(std::string event, void* data)
  {
  ESP_LOGI(TAG,"Launching Web Server");

  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();

  struct mg_connection *nc = mg_bind(mgr, ":80", OvmsWebServerMongooseHandler);
  mg_set_protocol_http_websocket(nc);
  }

void OvmsWebServer::NetManStop(std::string event, void* data)
  {
  ESP_LOGI(TAG,"Stopping Web Server");
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

OvmsWebServer::OvmsWebServer()
  {
#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  ESP_LOGI(TAG, "Initialising WEBSERVER (8200)");
  ESP_LOGI(TAG, "Using MONGOOSE engine");

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"network.mgr.init", std::bind(&OvmsWebServer::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.stop", std::bind(&OvmsWebServer::NetManStop, this, _1, _2));
#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE
  }

OvmsWebServer::~OvmsWebServer()
  {
  }
