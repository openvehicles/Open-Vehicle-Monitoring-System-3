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
;    (C) 2018       Michael Balzer
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

#ifndef __WEBSERVER_H__
#define __WEBSERVER_H__

#include <forward_list>
#include <iterator>

#include "ovms_events.h"
#include "ovms_command.h"
#include "ovms_netmanager.h"

#define OVMS_GLOBAL_AUTH_FILE     "/store/.htpasswd"

#define SESSION_COOKIE_NAME       "ovms_session"
#define SESSION_TTL               3600
#define SESSION_CHECK_INTERVAL    60
#define NUM_SESSIONS              5

struct user_session {
  uint64_t id;
  time_t last_used;
  //char* username;
};


typedef enum {
  PageMenu_None,
  PageMenu_Main,              // → main menu
  PageMenu_Config,            // → config menu
  PageMenu_Vehicle,           // → vehicle menu
} PageMenu_t;

typedef enum {
  PageAuth_None,              // public
  PageAuth_Cookie,            // use auth cookie
  PageAuth_File,              // use htaccess file(s) (digest auth)
} PageAuth_t;

struct PageContext
{
  mg_connection *nc;
  http_message *hm;
  user_session *session;
  std::string method;
  std::string uri;
  
  // utils:
  std::string getvar(const char* name, size_t maxlen=200);
  std::string encode_html(const char* text);
  std::string encode_html(const std::string text);
  
  // output:
  void head(int code, const char* headers=NULL);
  void print(const std::string text);
  void done();
  void panel_start(const char* type, const char* title);
  void panel_end(const char* footer="");
  void form_start(const char* action);
  void form_end();
  void input(const char* type, const char* label, const char* name, const char* value);
  void input_text(const char* label, const char* name, const char* value);
  void input_password(const char* label, const char* name, const char* value);
  void input_select_start(const char* label, const char* name);
  void input_select_option(const char* label, const char* value, bool selected);
  void input_select_end();
  void input_button(const char* type, const char* label);
  void alert(const char* type, const char* text);
};

typedef struct PageContext PageContext_t;

struct PageEntry;
typedef struct PageEntry PageEntry_t;
typedef void (*PageHandler_t)(PageEntry_t& p, PageContext_t& c);

struct PageEntry
{
  const char* uri;
  const char* label;
  PageHandler_t handler;
  PageMenu_t menu;
  PageAuth_t auth;
  
  PageEntry(const char* _uri, const char* _label, PageHandler_t _handler, PageMenu_t _menu=PageMenu_None, PageAuth_t _auth=PageAuth_None)
  {
    uri = _uri;
    label = _label;
    handler = _handler;
    menu = _menu;
    auth = _auth;
  }
  
  void Serve(PageContext_t& c);
};

typedef std::forward_list<PageEntry*> PageMap_t;


struct chunked_xfer {
  const uint8_t* data;
  size_t size;
  size_t sent;
  bool keepalive;
  
  chunked_xfer(const uint8_t* _data, size_t _size, bool _keepalive=true) {
    data = _data;
    size = _size;
    sent = 0;
    keepalive = _keepalive;
  }
};

#define MG_F_USER_CHUNKED_XFER      MG_F_USER_1
#define XFER_CHUNK_SIZE             1024


class OvmsWebServer
{
  public:
    OvmsWebServer();
    ~OvmsWebServer();

  public:
    static void EventHandler(struct mg_connection *nc, int ev, void *p);
    void NetManInit(std::string event, void* data);
    void NetManStop(std::string event, void* data);
    void ConfigChanged(std::string event, void* data);
    void UpdateGlobalAuthFile();
    static const std::string MakeDigestAuth(const char* realm, const char* username, const char* password);
    static const std::string ExecuteCommand(const std::string command, int verbosity=COMMAND_RESULT_NORMAL);

  public:
    void RegisterPage(const char* uri, const char* label, PageHandler_t handler,
      PageMenu_t menu=PageMenu_None, PageAuth_t auth=PageAuth_None);
    void DeregisterPage(const char* uri);
    PageEntry* FindPage(const char* uri);
  
  public:
    user_session* CreateSession(const http_message *hm);
    void DestroySession(user_session *s);
    user_session* GetSession(http_message *hm);
    void CheckSessions(void);
    static bool CheckLogin(std::string username, std::string password);
  
  public:
    static std::string CreateMenu(PageContext_t& c);
    static void HandleRoot(PageEntry_t& p, PageContext_t& c);
    static void HandleAsset(PageEntry_t& p, PageContext_t& c);
    static void HandleMenu(PageEntry_t& p, PageContext_t& c);
    static void HandleHome(PageEntry_t& p, PageContext_t& c);
    static void HandleLogin(PageEntry_t& p, PageContext_t& c);
    static void HandleLogout(PageEntry_t& p, PageContext_t& c);
  
  public:
    static void HandleStatus(PageEntry_t& p, PageContext_t& c);
    static void HandleCommand(PageEntry_t& p, PageContext_t& c);
    static void HandleShell(PageEntry_t& p, PageContext_t& c);
    static void HandleCfgPassword(PageEntry_t& p, PageContext_t& c);
    static void HandleCfgVehicle(PageEntry_t& p, PageContext_t& c);
  
  public:
    bool m_running;
#if MG_ENABLE_FILESYSTEM
    bool m_file_enable;
    mg_serve_http_opts m_file_opts;
#endif //MG_ENABLE_FILESYSTEM
    PageMap_t m_pagemap;
    user_session m_sessions[NUM_SESSIONS];
};

extern OvmsWebServer MyWebServer;

#endif //#ifndef __WEBSERVER_H__
