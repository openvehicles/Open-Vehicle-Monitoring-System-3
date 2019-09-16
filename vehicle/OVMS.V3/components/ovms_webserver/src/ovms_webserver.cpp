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

#include "ovms_log.h"
static const char *TAG = "webserver";

#include <string.h>
#include <stdio.h>
#include <fstream>
#include "ovms_webserver.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "buffered_shell.h"
#include "vehicle.h"


OvmsWebServer MyWebServer __attribute__ ((init_priority (8200)));

OvmsWebServer::OvmsWebServer()
{
  ESP_LOGI(TAG, "Initialising WEBSERVER (8200)");

  m_running = false;
  m_configured = false;
  m_restart_countdown = 0;
  memset(m_sessions, 0, sizeof(m_sessions));

#if MG_ENABLE_FILESYSTEM
  m_file_enable = true;
  memset(&m_file_opts, 0, sizeof(m_file_opts));
#endif //MG_ENABLE_FILESYSTEM

  m_client_cnt = 0;
  m_client_mutex = xSemaphoreCreateMutex();
  m_client_backlog = xQueueCreate(50, sizeof(WebSocketTxTodo));
  m_update_ticker = xTimerCreate("Web client update ticker", 250 / portTICK_PERIOD_MS, pdTRUE, NULL, UpdateTicker);

  MyConfig.RegisterParam("http.server", "Webserver configuration", true, true);
  MyConfig.RegisterParam("http.plugin", "Webserver plugins", true, true);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "network.mgr.init", std::bind(&OvmsWebServer::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "network.mgr.stop", std::bind(&OvmsWebServer::NetManStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.changed", std::bind(&OvmsWebServer::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "config.mounted", std::bind(&OvmsWebServer::ConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "*", std::bind(&OvmsWebServer::EventListener, this, _1, _2));

  // register standard framework URIs:
  RegisterPage("/", "OVMS", HandleRoot);
  RegisterPage("/assets/style.css", "style.css", HandleAsset);
  RegisterPage("/assets/script.js", "script.js", HandleAsset);
  RegisterPage("/assets/charts.js", "charts.js", HandleAsset);
  RegisterPage("/assets/tables.js", "tables.js", HandleAsset);
  RegisterPage("/assets/zones.json", "zones.json", HandleAsset);
  RegisterPage("/assets/bootstrap.min.css.map", "-", HandleAsset);
  RegisterPage("/favicon.ico", "favicon.ico", HandleAsset);
  RegisterPage("/apple-touch-icon.png", "apple-touch-icon.png", HandleAsset);
  RegisterPage("/menu", "Menu", HandleMenu);
  RegisterPage("/home", "Home", HandleHome);
  RegisterPage("/login", "Login", HandleLogin);
  RegisterPage("/logout", "Logout", HandleLogout);

  // register standard API calls:
  RegisterPage("/api/execute", "Execute command", HandleCommand, PageMenu_None, PageAuth_Cookie);

  // register standard public pages:
  RegisterPage("/dashboard", "Dashboard", HandleDashboard, PageMenu_Main, PageAuth_None);

  // register standard administration pages:
  RegisterPage("/status", "Status", HandleStatus, PageMenu_Main, PageAuth_Cookie);
  RegisterPage("/shell", "Shell", HandleShell, PageMenu_Tools, PageAuth_Cookie);
  RegisterPage("/edit", "Editor", HandleEditor, PageMenu_Tools, PageAuth_Cookie);
  RegisterPage("/cfg/init", "Setup wizard", HandleCfgInit, PageMenu_None, PageAuth_Cookie);
  RegisterPage("/cfg/password", "Password", HandleCfgPassword, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/vehicle", "Vehicle", HandleCfgVehicle, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/wifi", "Wifi", HandleCfgWifi, PageMenu_Config, PageAuth_Cookie);
#ifdef CONFIG_OVMS_COMP_MODEM_SIMCOM
  RegisterPage("/cfg/modem", "Modem", HandleCfgModem, PageMenu_Config, PageAuth_Cookie);
#endif
#ifdef CONFIG_OVMS_COMP_SERVER
#ifdef CONFIG_OVMS_COMP_SERVER_V2
  RegisterPage("/cfg/server/v2", "Server V2 (MP)", HandleCfgServerV2, PageMenu_Config, PageAuth_Cookie);
#endif
#ifdef CONFIG_OVMS_COMP_SERVER_V3
  RegisterPage("/cfg/server/v3", "Server V3 (MQTT)", HandleCfgServerV3, PageMenu_Config, PageAuth_Cookie);
#endif
#endif
  RegisterPage("/cfg/webserver", "Webserver", HandleCfgWebServer, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/plugins", "Web Plugins", HandleCfgPlugins, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/autostart", "Autostart", HandleCfgAutoInit, PageMenu_Config, PageAuth_Cookie);
#ifdef CONFIG_OVMS_COMP_OTA
  RegisterPage("/cfg/firmware", "Firmware", HandleCfgFirmware, PageMenu_Config, PageAuth_Cookie);
#endif
  RegisterPage("/cfg/logging", "Logging", HandleCfgLogging, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/locations", "Locations", HandleCfgLocations, PageMenu_Config, PageAuth_Cookie);
  RegisterPage("/cfg/backup", "Backup &amp; Restore", HandleCfgBackup, PageMenu_Config, PageAuth_Cookie);
}

OvmsWebServer::~OvmsWebServer()
{
  // never destroyed
}


void OvmsWebServer::NetManInit(std::string event, void* data)
{
  m_running = true;
  ESP_LOGI(TAG,"Launching Web Server");

  if (!m_configured) {
    // safety measure, should not happen normally
    ConfigChanged("config.mounted", NULL);
  }

  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();

  char *error_string;
  struct mg_bind_opts bind_opts = {};
  bind_opts.error_string = (const char**) &error_string;
  struct mg_connection *nc = mg_bind_opt(mgr, ":80", EventHandler, bind_opts);
  if (!nc)
    ESP_LOGE(TAG, "Cannot bind to port 80: %s", error_string);
  else {
    mg_set_protocol_http_websocket(nc);
    mg_set_timer(nc, mg_time() + SESSION_CHECK_INTERVAL);
  }
}

void OvmsWebServer::NetManStop(std::string event, void* data)
{
  if (m_running) {
    ESP_LOGI(TAG,"Stopping Web Server");
    m_running = false;
  }
}


/**
 * ConfigChanged: read & apply configuration updates
 */
void OvmsWebServer::ConfigChanged(std::string event, void* data)
{
  m_configured = true;
  OvmsConfigParam* param = (OvmsConfigParam*) data;
  ESP_LOGD(TAG, "ConfigChanged: %s %s", event.c_str(), param ? param->GetName().c_str() : "");

  if (event == "config.mounted") {
    CfgInitStartup();
  }

#if MG_ENABLE_FILESYSTEM
  if (!param || param->GetName() == "http.server") {
    // Instances:
    //    Name                Default                 Function
    //    enable.files        yes                     Enable file serving from docroot
    //    enable.dirlist      yes                     Enable directory listings
    //    docroot             /sd                     File server document root
    //    auth.domain         ovms                    Default auth domain (digest realm)
    //    auth.file           .htpasswd               Per directory auth file (Note: no inheritance from parent dir!)
    //    auth.global         yes                     Use global auth for files (user "admin", module password)

    if (m_file_opts.document_root)
      free((void*)m_file_opts.document_root);
    if (m_file_opts.auth_domain)
      free((void*)m_file_opts.auth_domain);
    if (m_file_opts.per_directory_auth_file)
      free((void*)m_file_opts.per_directory_auth_file);

    m_file_enable =
      MyConfig.GetParamValueBool("http.server", "enable.files", true);
    m_file_opts.enable_directory_listing =
      MyConfig.GetParamValueBool("http.server", "enable.dirlist", true) ? "yes" : "no";
    m_file_opts.document_root =
      strdup(MyConfig.GetParamValue("http.server", "docroot", "/sd").c_str());

    m_file_opts.auth_domain =
      strdup(MyConfig.GetParamValue("http.server", "auth.domain", "ovms").c_str());
    m_file_opts.per_directory_auth_file =
      strdup(MyConfig.GetParamValue("http.server", "auth.file", ".htpasswd").c_str());
    m_file_opts.global_auth_file =
      MyConfig.GetParamValueBool("http.server", "auth.global", true) ? OVMS_GLOBAL_AUTH_FILE : NULL;
  }

  if (!param || param->GetName() == "password") {
    UpdateGlobalAuthFile();
  }
#endif //MG_ENABLE_FILESYSTEM

  if (!param || param->GetName() == "http.plugin") {
    DeregisterPlugins();
    RegisterPlugins();
  }
}


/**
 * UpdateGlobalAuthFile: create digest auth for main user "admin"
 *    if password is set and global auth is activated
 */
void OvmsWebServer::UpdateGlobalAuthFile()
{
#if MG_ENABLE_FILESYSTEM
  if (!m_file_opts.global_auth_file || !m_file_opts.auth_domain)
    return;

  std::string p = MyConfig.GetParamValue("password", "module");

  if (p.empty())
  {
    unlink(m_file_opts.global_auth_file);
    ESP_LOGW(TAG, "UpdateGlobalAuthFile: no password set => no auth for web console");
  }
  else
  {
    FILE *fp = fopen(m_file_opts.global_auth_file, "w");
    if (!fp) {
      ESP_LOGE(TAG, "UpdateGlobalAuthFile: can't write to '%s'", m_file_opts.global_auth_file);
      return;
    }
    std::string auth = MakeDigestAuth(m_file_opts.auth_domain, "admin", p.c_str());
    fprintf(fp, "%s\n", auth.c_str());
    ESP_LOGD(TAG, "UpdateGlobalAuthFile: 'admin' => %s", auth.c_str());
    auth = MakeDigestAuth(m_file_opts.auth_domain, "", p.c_str());
    fprintf(fp, "%s\n", auth.c_str());
    ESP_LOGD(TAG, "UpdateGlobalAuthFile: '' => %s", auth.c_str());
    if (fclose(fp) != 0)
      ESP_LOGE(TAG, "UpdateGlobalAuthFile: can't write to '%s': %s", m_file_opts.global_auth_file, strerror(errno));
  }
#endif //MG_ENABLE_FILESYSTEM
}

const std::string OvmsWebServer::MakeDigestAuth(const char* realm, const char* username, const char* password)
{
  std::string line;
  line = username;
  line += ":";
  line += realm;
  line += ":";
  line += password;

  unsigned char digest[16];
  cs_md5_ctx md5_ctx;
  cs_md5_init(&md5_ctx);
  cs_md5_update(&md5_ctx, (const unsigned char*) line.data(), line.length());
  cs_md5_final(digest, &md5_ctx);

  char hex[33];
  cs_to_hex(hex, digest, sizeof(digest));

  line = username;
  line += ":";
  line += realm;
  line += ":";
  line += hex;
  return line;
}


/**
 * ExecuteCommand: BufferedShell wrapper
 */
const std::string OvmsWebServer::ExecuteCommand(const std::string command, int verbosity /*=COMMAND_RESULT_NORMAL*/)
{
  std::string output;
  BufferedShell* bs = new BufferedShell(false, verbosity);
  if (!bs)
    return output;
  bs->SetSecure(true); // Note: assuming user is admin
  output = "";
  bs->ProcessChars(command.data(), command.size());
  bs->ProcessChar('\n');
  bs->Dump(output);
  delete bs;
  return output;
}


/**
 * RegisterPage: add a page to the URI handler map
 * Note: use PageMenu_Vehicle for vehicle specific pages.
 */
void OvmsWebServer::RegisterPage(std::string uri, std::string label, PageHandler_t handler,
  PageMenu_t menu /*=PageMenu_None*/, PageAuth_t auth /*=PageAuth_None*/, int priority /*=0*/)
{
  auto prev = m_pagemap.before_begin();
  for (auto it = m_pagemap.begin(); it != m_pagemap.end(); prev = it++) {
    if ((*it).uri == uri) {
      if (priority < 0) {
        ESP_LOGE(TAG, "RegisterPage: second registration for uri '%s' (ignored)", uri.c_str());
        return;
      } else {
        ESP_LOGI(TAG, "RegisterPage: second registration for uri '%s' prioritized", uri.c_str());
        m_pagemap.erase_after(prev);
        break;
      }
    }
  }
  m_pagemap.insert_after(prev, PageEntry(uri, label, handler, menu, auth));
}

void OvmsWebServer::DeregisterPage(std::string uri)
{
  m_pagemap.remove_if([uri](PageEntry &e){ return (e.uri == uri); });
}

PageEntry* OvmsWebServer::FindPage(std::string uri)
{
  for (PageEntry& e : m_pagemap) {
    if (e.uri == uri)
      return &e;
  }
  return NULL;
}


/**
 * Plugin Registry
 */

void PagePluginContent::LoadContent()
{
  std::string path = "/store/plugin/" + m_path;
  std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
  if (!file.is_open()) {
    ESP_LOGE(TAG, "Plugin file missing: '%s'", path.c_str());
    m_content = "<!-- ERROR: Plugin file missing: '";
    m_content += PageContext::encode_html(path).c_str();
    m_content += "' -->";
  } else {
    auto size = file.tellg();
    if (size < 0) {
      ESP_LOGE(TAG, "Plugin file '%s': seek failed", path.c_str());
      m_content = "<!-- ERROR: Plugin file missing: '";
      m_content += PageContext::encode_html(path).c_str();
      m_content += "' -->";
    } else {
      m_content.resize(size, '\0');
      file.seekg(0);
      file.read(&m_content[0], size);
      ESP_LOGD(TAG, "Plugin file loaded: '%s', %u bytes", path.c_str(), (size_t)size);
    }
  }
}

void OvmsWebServer::RegisterPlugins()
{
  OvmsConfigParam* cp = MyConfig.CachedParam("http.plugin");
  if (!cp)
    return;
  const ConfigParamMap& pmap = cp->GetMap();
  std::string key, label, page, hook;
  PageAuth_t auth;
  PageMenu_t menu;

  // Instances:
  //    <key>.enable        yes/no
  //    <key>.label         e.g. "My Plugin"
  //    <key>.page          e.g. "/myplugin" (page/hook URI)
  //    <key>.auth          [page] <PageAuth_t> code name
  //    <key>.menu          [page] <PageMenu_t> code name
  //    <key>.hook          [hook] Callback hook code
  // 
  // Files:
  //    /store/plugin/<key>

  for (auto& kv: pmap) {
    if (!endsWith(kv.first, ".enable") || !strtobool(kv.second))
      continue;
    
    key = kv.first.substr(0, kv.first.length() - 7);
    page = cp->GetValue(key+".page");
    hook = cp->GetValue(key+".hook");
    bool is_hook = cp->IsDefined(key+".hook");
    if (page == "" || (is_hook && hook == ""))
      continue;
    
    if (is_hook) {
      m_plugin_parts.insert({ page + ":" + hook, PagePluginContent(key) });
      RegisterCallback("http.plugin", page, PluginCallback, -1);
      ESP_LOGD(TAG, "Plugin hook registered: '%s:%s' => '%s'", page.c_str(), hook.c_str(), key.c_str());
    } else {
      m_plugin_pages.insert({ page, PagePluginContent(key) });
      label = cp->GetValue(key+".label");
      menu = Code2PageMenu(cp->GetValue(key+".menu"));
      auth = Code2PageAuth(cp->GetValue(key+".auth"));
      RegisterPage(page, label, PluginHandler, menu, auth, -1);
      ESP_LOGD(TAG, "Plugin page registered: '%s' => '%s'", page.c_str(), key.c_str());
    }
  }
}

void OvmsWebServer::DeregisterPlugins()
{
  m_pagemap.remove_if([](PageEntry& e){ return e.handler == PluginHandler; });
  m_plugin_pages.clear();
  DeregisterCallbacks("http.plugin");
  m_plugin_parts.clear();
}

void OvmsWebServer::ReloadPlugin(std::string path)
{
  if (startsWith(path, "/store/plugin/"))
    path = path.substr(14);
  for (auto i = m_plugin_pages.begin(); i != m_plugin_pages.end(); i++) {
    if (i->second.m_path == path)
      i->second.LoadContent();
  }
  for (auto i = m_plugin_parts.begin(); i != m_plugin_parts.end(); i++) {
    if (i->second.m_path == path)
      i->second.LoadContent();
  }
}

void OvmsWebServer::PluginHandler(PageEntry_t& p, PageContext_t& c)
{
  auto i = MyWebServer.m_plugin_pages.find(p.uri);
  if (i == MyWebServer.m_plugin_pages.end())
    return;

  extram::string& content = i->second.GetContent();
  c.head(200);
  c.print(content);
  c.done();
}

PageResult_t OvmsWebServer::PluginCallback(PageEntry_t& p, PageContext_t& c, const std::string& hook)
{
  std::string key = p.uri + ':' + hook;
  auto range = MyWebServer.m_plugin_parts.equal_range(key);
  if (range.first == MyWebServer.m_plugin_parts.end())
    return PageResult_OK;

  for (auto i = range.first; i != range.second; i++) {
    extram::string& content = i->second.GetContent();
    c.print(content);
  }

  return PageResult_OK;
}


/**
 * EventHandler: this is the mongoose main event handler.
 */
void OvmsWebServer::EventHandler(mg_connection *nc, int ev, void *p)
{
  PageContext_t c;
  MgHandler* handler = (MgHandler*) nc->user_data;

  //if (ev != MG_EV_POLL && ev != MG_EV_SEND)
  //if (nc->user_data)
  //  ESP_LOGV(TAG, "EventHandler: conn=%p handler=%p ev=%d p=%p rxbufsz=%d, txbufsz=%d", nc, nc->user_data, ev, p, nc->recv_mbuf.size, nc->send_mbuf.size);

  // call attached handler:
  if (handler)
    ev = handler->HandleEvent(ev, p);

  // framework handling:
  switch (ev)
  {
    case MG_EV_WEBSOCKET_HANDSHAKE_DONE:    // new websocket connection
      {
        MyWebServer.CreateWebSocketHandler(nc);
      }
      break;

    case MG_EV_HTTP_REQUEST:                // standard HTTP request
      {
        c.nc = nc;
        c.hm = (struct http_message *) p;
        c.session = MyWebServer.GetSession(c.hm);
        c.method.assign(c.hm->method.p, c.hm->method.len);
        c.uri.assign(c.hm->uri.p, c.hm->uri.len);
        ESP_LOGI(TAG, "HTTP %s %s", c.method.c_str(), c.uri.c_str());

        PageEntry* page = MyWebServer.FindPage(c.uri.c_str());
        if (page) {
          // serve by page handler:
          page->Serve(c);
        }
#if MG_ENABLE_FILESYSTEM
        else if (MyWebServer.m_file_enable) {
          // serve from file space:
          if (MyConfig.ProtectedPath(c.uri)) {
            mg_http_send_error(c.nc, 401, "Unauthorized");
            nc->flags |= MG_F_SEND_AND_CLOSE;
          }
          else {
            mg_serve_http(nc, c.hm, MyWebServer.m_file_opts);
          }
        }
#endif //MG_ENABLE_FILESYSTEM
        else {
          mg_http_send_error(c.nc, 404, "Not found");
          nc->flags |= MG_F_SEND_AND_CLOSE;
        }
      }
      break;

    case MG_EV_CLOSE:                       // connection has been closed
      {
        if (handler) {
          if (nc->flags & MG_F_IS_WEBSOCKET)
            MyWebServer.DestroyWebSocketHandler((WebSocketHandler*)handler);
          else
            delete handler;
        }
      }
      break;

    case MG_EV_TIMER:
      {
        // Perform session maintenance:
        MyWebServer.CheckSessions();
        mg_set_timer(nc, mg_time() + SESSION_CHECK_INTERVAL);
      }
      break;

    default:
      break;
  }
}


/**
 * PageEntry.Serve: check auth, call page handler
 */
void PageEntry::Serve(PageContext_t& c)
{
  // check auth:
  if
#if MG_ENABLE_FILESYSTEM
  (auth == PageAuth_Cookie && !MyConfig.GetParamValue("password", "module").empty())
#else
  (auth != PageAuth_None && !MyConfig.GetParamValue("password", "module").empty())
#endif
  {
    // use session cookie / access based auth:
    if (c.session == NULL)
    {
      // Check per access auth:
      // Note: apikey is currently just the admin password, but can later be replaced by an auth token
      std::string apikey = c.getvar("apikey");
      if (!MyWebServer.CheckLogin("admin", apikey)) {
        if (apikey != "") {
          // output API response:
          c.head(403);
          c.print("ERROR: Unauthorized\n");
          c.done();
        } else {
          // forward user to login page:
          MyWebServer.HandleLogin(*this, c);
        }
        return;
      }
    }
  }
#if MG_ENABLE_FILESYSTEM
  else if (auth == PageAuth_File)
  {
    // use mongoose htaccess file based digest auth:
    mg_serve_http_opts& opts = MyWebServer.m_file_opts;
    if (!mg_http_is_authorized(c.hm, c.hm->uri,
        opts.auth_domain, opts.global_auth_file,
        MG_AUTH_FLAG_IS_GLOBAL_PASS_FILE|MG_AUTH_FLAG_ALLOW_MISSING_FILE))
    {
      mg_http_send_digest_auth_request(c.nc, opts.auth_domain);
      return;
    }
  }
#endif //MG_ENABLE_FILESYSTEM

  // call page handler:
  handler(*this, c);
}


/**
 * Page Callback Registry:
 */

bool OvmsWebServer::RegisterCallback(std::string caller, std::string uri, PageCallback_t handler, int priority /*=0*/)
{
  PageEntry* e = FindPage(uri);
  if (!e)
    return false;
  e->RegisterCallback(caller, handler, priority);
  return true;
}

void OvmsWebServer::DeregisterCallbacks(std::string caller)
{
  for (PageEntry& e : m_pagemap)
    e.DeregisterCallback(caller);
}

void PageEntry::RegisterCallback(std::string caller, PageCallback_t handler, int priority /*=0*/)
{
  auto prev = callbacklist.before_begin();
  for (auto it = callbacklist.begin(); it != callbacklist.end(); prev = it++) {
    if ((*it).caller == caller && (*it).handler == handler)
      return; // already registered
  }
  if (priority < 0)
    callbacklist.insert_after(prev, PageCallbackEntry(caller, handler));
  else
    callbacklist.push_front(PageCallbackEntry(caller, handler));
}

void PageEntry::DeregisterCallback(std::string caller)
{
  callbacklist.remove_if([caller](PageCallbackEntry& e){ return e.caller == caller; });
}

PageResult_t PageEntry::callback(PageContext_t& c, const std::string& hook)
{
  PageResult_t res = PageResult_OK;
  for (PageCallbackEntry& e : callbacklist) {
    res = e.handler(*this, c, hook);
    if (res == PageResult_STOP)
      break;
  }
  return res;
}

PageResult_t PageContext::callback(PageEntry_t& p, const std::string& hook)
{
  return p.callback(*this, hook);
}


/**
 * MgHandler.RequestPoll: init transmission from other context.
 *
 * mg_broadcast() signals the mg_mgr_poll() task to send an MG_EV_POLL to all connections.
 */
void MgHandler::RequestPoll()
{
#if MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
  if (!m_nc)
    return;

  if (xTaskGetCurrentTaskHandle() == MyNetManager.GetMongooseTaskHandle()) {
    // we're in the NetManTask, can send directly:
    HandleEvent(MG_EV_POLL, NULL);
  } else {
    MgHandler* origin = this;
    mg_broadcast(MyNetManager.GetMongooseMgr(), HandlePoll, &origin, sizeof(origin));
  }
#endif // MG_ENABLE_BROADCAST && WEBSRV_USE_MG_BROADCAST
}

void MgHandler::HandlePoll(mg_connection* nc, int ev, void* p)
{
  MgHandler* origin = *((MgHandler**)p);
  if (nc->user_data == origin)
    origin->HandleEvent(MG_EV_POLL, NULL);
}


/**
 * HttpDataSender: chunked transfer of a memory region (needs to be const during xfer)
 */

HttpDataSender::HttpDataSender(mg_connection* nc, MemRegionList xferlist, bool keepalive /*=true*/)
: MgHandler(nc)
{
  m_xferlist = xferlist;
  m_xfer = -1;
  m_data = NULL;
  m_size = 0;
  m_sent = 0;
  m_keepalive = keepalive;
  //ESP_LOGV(TAG, "HttpDataSender[%p]: init %d regions", nc, m_xferlist.size());
}

HttpDataSender::HttpDataSender(mg_connection* nc, const uint8_t* data, size_t size, bool keepalive /*=true*/)
: MgHandler(nc)
{
  m_xferlist.push_back({ data, size });
  m_xfer = -1;
  m_data = NULL;
  m_size = 0;
  m_sent = 0;
  m_keepalive = keepalive;
  //ESP_LOGV(TAG, "HttpDataSender[%p]: init data=%p, %d bytes", nc, data, size);
}

HttpDataSender::~HttpDataSender()
{
  if (m_sent < m_size || m_xfer < m_xferlist.size()) {
    ESP_LOGV(TAG, "HttpDataSender[%p]: abort region=%d/%d, data=%p, %d/%d bytes sent",
             m_nc, m_xfer+1, m_xferlist.size(), m_data, m_sent, m_size);
  }
}

int HttpDataSender::HandleEvent(int ev, void* p)
{
  switch (ev)
  {
    case MG_EV_SEND:          // last transmission has finished
    {
      if (m_sent == m_size && ++m_xfer < m_xferlist.size()) {
        // send next region:
        m_data = m_xferlist[m_xfer].first;
        m_size = m_xferlist[m_xfer].second;
        m_sent = 0;
        //ESP_LOGV(TAG, "HttpDataSender[%p]: next region=%d data=%p, %d bytes", m_nc, m_xfer+1, m_data, m_size);
      }
      if (m_sent < m_size) {
        // send next chunk:
        size_t len = MIN(m_size - m_sent, XFER_CHUNK_SIZE);
        mg_send_http_chunk(m_nc, (const char*) m_data + m_sent, len);
        m_sent += len;
        //ESP_LOGV(TAG, "HttpDataSender[%p] data=%p sent %d/%d", m_nc, m_data, m_sent, m_size);
      }
      else {
        // done:
        if (!m_keepalive)
          m_nc->flags |= MG_F_SEND_AND_CLOSE;
        mg_send_http_chunk(m_nc, "", 0);
        //ESP_LOGV(TAG, "HttpDataSender[%p]: done", m_nc);
        delete this;
      }
    }
    break;

    default:
      break;
  }

  return ev;
}


/**
 * HttpStringSender: chunked transfer of a memory region (needs to be const during xfer)
 */
HttpStringSender::HttpStringSender(mg_connection* nc, std::string* msg, bool keepalive /*=true*/)
  : MgHandler(nc)
{
  m_msg = msg;
  m_sent = 0;
  m_keepalive = keepalive;
  //ESP_LOGV(TAG, "HttpStringSender[%p]: init msg=%p, %d bytes", nc, m_msg, m_msg->size());
}

HttpStringSender::~HttpStringSender()
{
  if (m_sent < m_msg->size()) {
    ESP_LOGV(TAG, "HttpStringSender[%p]: abort msg=%p, %d bytes sent", m_nc, m_msg, m_sent);
  }
  delete m_msg;
}

int HttpStringSender::HandleEvent(int ev, void* p)
{
  switch (ev)
  {
    case MG_EV_SEND:          // last transmission has finished
    {
      if (m_sent < m_msg->size()) {
        // send next chunk:
        size_t len = MIN(m_msg->size() - m_sent, XFER_CHUNK_SIZE);
        mg_send_http_chunk(m_nc, (const char*) m_msg->data() + m_sent, len);
        m_sent += len;
        //ESP_LOGV(TAG, "HttpStringSender[%p] msg=%p sent %d/%d", m_nc, m_msg, m_sent, m_msg->size());
      }
      else {
        // done:
        if (!m_keepalive)
          m_nc->flags |= MG_F_SEND_AND_CLOSE;
        mg_send_http_chunk(m_nc, "", 0);
        //ESP_LOGV(TAG, "HttpStringSender[%p]: done msg=%p, %d bytes sent", m_nc, m_msg, m_sent);
        delete this;
      }
    }
    break;

    default:
      break;
  }

  return ev;
}


/**
 * CheckLogin: check username & password
 *
 * We use "admin" as a fixed username for now to be able to extend users later on
 * and be compatible with the file based digest authentication.
 */
bool OvmsWebServer::CheckLogin(std::string username, std::string password)
{
  std::string adminpass = MyConfig.GetParamValue("password", "module");
  return (adminpass.empty() || (username == "admin" && password == adminpass));
}


/**
 * GetSession: parses the session cookie and returns a pointer to the session struct
 * or NULL if not found.
 */
user_session* OvmsWebServer::GetSession(http_message *hm)
{
  user_session* session = NULL;
  struct mg_str *cookie_header = mg_get_http_header(hm, "cookie");
  if (cookie_header == NULL) return NULL;
  char ssid_buf[21], *ssid = ssid_buf;
  mg_http_parse_header2(cookie_header, SESSION_COOKIE_NAME, &ssid, sizeof(ssid_buf));
  uint64_t sid = strtoull(ssid, NULL, 16);
  if (sid != 0) {
    for (int i = 0; i < NUM_SESSIONS; i++) {
      if (m_sessions[i].id == sid) {
        m_sessions[i].last_used = mg_time();
        session = &m_sessions[i];
        break;
      }
    }
  }
  if (ssid != ssid_buf)
    free(ssid);
  return session;
}


/**
 * CreateSession: create a new session
 */
user_session* OvmsWebServer::CreateSession(const http_message *hm)
{
  /* Find first available slot or use the oldest one. */
  user_session *s = NULL;
  user_session *oldest_s = m_sessions;
  for (int i = 0; i < NUM_SESSIONS; i++) {
    if (m_sessions[i].id == 0) {
      s = &m_sessions[i];
      break;
    }
    if (m_sessions[i].last_used < oldest_s->last_used) {
      oldest_s = &m_sessions[i];
    }
  }
  if (s == NULL) {
    DestroySession(oldest_s);
    ESP_LOGD(TAG, "CreateSession: evicted %" INT64_X_FMT, oldest_s->id);
    s = oldest_s;
  }

  /* Initialize new session. */
  s->last_used = mg_time();

  /* Create an ID by putting various volatiles into a pot and stirring. */
  cs_sha1_ctx ctx;
  cs_sha1_init(&ctx);
  cs_sha1_update(&ctx, (const unsigned char *) hm->message.p, hm->message.len);
  cs_sha1_update(&ctx, (const unsigned char *) m_sessions, sizeof(m_sessions));
  std::string sys;
  sys = StdMetrics.ms_m_serial->AsString();
  cs_sha1_update(&ctx, (const unsigned char *) sys.data(), sys.size());
  sys = StdMetrics.ms_m_monotonic->AsString();
  cs_sha1_update(&ctx, (const unsigned char *) sys.data(), sys.size());
  sys = StdMetrics.ms_m_freeram->AsString();
  cs_sha1_update(&ctx, (const unsigned char *) sys.data(), sys.size());
  sys = StdMetrics.ms_v_bat_12v_voltage->AsString();
  cs_sha1_update(&ctx, (const unsigned char *) sys.data(), sys.size());

  union {
    uint64_t u64;
    unsigned char digest[20];
  } buf;
  cs_sha1_final(buf.digest, &ctx);
  s->id = buf.u64;
  return s;
}


/**
 * DestroySession: delete (invalidate) a session
 */
void OvmsWebServer::DestroySession(user_session *s)
{
  memset(s, 0, sizeof(*s));
}


/**
 * CheckSessions: clean up sessions that have been idle for too long
 */
void OvmsWebServer::CheckSessions(void)
{
  time_t threshold = mg_time() - SESSION_TTL;
  for (int i = 0; i < NUM_SESSIONS; i++) {
    user_session *s = &m_sessions[i];
    if (s->id != 0 && s->last_used < threshold) {
      ESP_LOGD(TAG, "CheckSessions: session %" INT64_X_FMT " closed due to idleness.", s->id);
      DestroySession(s);
    }
  }
}


/**
 * HandleLogin: show/process login form
 *
 * This handler serves all URIs protected by PageAuth_Cookie,
 *  redirects to original URI or /home as indicated.
 */
void OvmsWebServer::HandleLogin(PageEntry_t& p, PageContext_t& c)
{
  std::string error;
  std::string uri = c.getvar("uri");

  if (c.method == "POST") {
    // validate login:
    std::string username = c.getvar("username");
    if (username == "")
      username = "admin";
    std::string password = c.getvar("password");
    user_session *s = NULL;

    if (!CheckLogin(username, password)) {
      error += "<li>Login validation failed, please check username &amp; password</li>";
      ESP_LOGW(TAG, "HandleLogin: auth failure for username '%s'", username.c_str());
    }
    else if ((s = MyWebServer.CreateSession(c.hm)) == NULL) {
      error += "<li>Session creation failed, please try again later</li>";
    }

    if (error == "") {
      // ok: set cookie, reload menu, redirect to /cfg/init, /cfg/password, original uri or /home:
      std::string init_step = MyConfig.GetParamValue("module", "init");
      if (uri != "")
        c.uri = uri;
      else if (!init_step.empty() && init_step != "done")
        c.uri = "/cfg/init";
      else if (MyConfig.GetParamValue("password", "module").empty())
        c.uri = "/cfg/password";
      else if (c.uri == "/login" || c.uri == "/logout" || c.uri == "/")
        c.uri = "/home";

      char shead[200];
      snprintf(shead, sizeof(shead),
        "Content-Type: text/html; charset=utf-8\r\n"
        "Cache-Control: no-cache\r\n"
        "Set-Cookie: %s=%" INT64_X_FMT "; path=/"
        , SESSION_COOKIE_NAME, s->id);
      c.head(200, shead);
      c.printf(
        "<script>loggedin = true; $(\"#menu\").load(\"/menu\"); loaduri(\"#main\", \"get\", \"%s\", {})</script>"
        , c.uri.c_str());
      c.done();

      ESP_LOGI(TAG, "HandleLogin: '%s' logged in, sid %" INT64_X_FMT, username.c_str(), s->id);
      return;
    }

    // output error, return to form:
    error = "<p class=\"lead\">Error!</p><ul class=\"errorlist\">" + error + "</ul>";
    c.head(403);
    c.alert("danger", error.c_str());
  }
  else {
    if (c.uri != "/login") {
      c.head(403);
      c.alert("danger", "<p class=\"lead\">Login required</p>");
    } else {
      c.head(200);
    }
  }

  // generate form:
  c.panel_start("primary", "Login");
  c.form_start(c.uri.c_str());
  if (uri != "")
    c.printf("<input type=\"hidden\" name=\"uri\" value=\"%s\">", c.encode_html(uri).c_str());
  c.input_text("Username", "username", "", "empty = 'admin'", NULL, "autocomplete=\"section-login username\"");
  c.input_password("Password", "password", "", NULL, NULL, "autocomplete=\"section-login current-password\"");
  c.input_button("default", "Login");
  c.form_end();
  c.panel_end();
  c.done();
}


/**
 * HandleLogout: remove cookie and associated session state
 */
void OvmsWebServer::HandleLogout(PageEntry_t& p, PageContext_t& c)
{
  user_session *s = MyWebServer.GetSession(c.hm);
  if (s != NULL) {
    ESP_LOGI(TAG, "HandleLogout: session %" INT64_X_FMT " destroyed", s->id);
    MyWebServer.DestroySession(s);
  }

  // erase cookie, reload menu & redirect to /home:
  char shead[200];
  snprintf(shead, sizeof(shead),
    "Content-Type: text/html; charset=utf-8\r\n"
    "Cache-Control: no-cache\r\n"
    "Set-Cookie: %s="
    , SESSION_COOKIE_NAME);
  c.head(200, shead);
  c.print(
    "<script>loggedin = false; $(\"#menu\").load(\"/menu\"); loaduri(\"#main\", \"get\", \"/home\", {})</script>");
  c.done();
}
