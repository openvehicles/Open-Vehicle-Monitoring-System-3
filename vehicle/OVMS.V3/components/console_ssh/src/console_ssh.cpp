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
#include <sys/stat.h>
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#include <lwip/def.h>
#include <lwip/sockets.h>
#undef bind
#include "freertos/queue.h"
#include "esp_heap_caps.h"
#include "ovms_log.h"
#include "ovms_events.h"
#include "ovms_netmanager.h"
#include "ovms_config.h"
#include <wolfssl/wolfcrypt/memory.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssh/ssh.h>
#include <wolfssh/log.h>
#include <wolfssh/internal.h>
#include "console_ssh.h"

static void wolf_logger(enum wolfSSH_LogLevel level, const char* const msg);
static void* wolfssh_malloc(size_t size);
static void wolfssh_free(void* ptr);
static void* wolfssh_realloc(void* ptr, size_t size);
static void* wolfssl_malloc(size_t size);
static void wolfssl_free(void* ptr);
static void* wolfssl_realloc(void* ptr, size_t size);
static const char* const tag = "ssh";
static const char* const wolfssh_tag = "wolfssh";
static const char* const wolfssl_tag = "wolfssl";
static uint8_t CRLF[2] = { '\r', '\n' };
static const char newline = '\n';

//-----------------------------------------------------------------------------
//    Class OvmsSSH
//-----------------------------------------------------------------------------

OvmsSSH MySSH __attribute__ ((init_priority (8300)));

static void MongooseHandler(struct mg_connection *nc, int ev, void *p)
  {
  MySSH.EventHandler(nc, ev, p);
  }

void OvmsSSH::EventHandler(struct mg_connection *nc, int ev, void *p)
  {
  switch (ev)
    {
    case MG_EV_ACCEPT:
      {
      ESP_LOGV(tag, "Event MG_EV_ACCEPT conn %p, data %p", nc, p);
      ConsoleSSH* child = new ConsoleSSH(this, nc);
      nc->user_data = child;
      break;
      }

    case MG_EV_POLL:
      {
      //ESP_LOGV(tag, "Event MG_EV_ACCEPT conn %p, data %p", nc, p);
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      if (child)
        {
        child->Send();
        child->Poll(0);
        }
      if (!m_keyed)
        {
        std::string skey = MyConfig.GetParamValueBinary("ssh.server", "key", std::string());
        if (!skey.empty())
          {
          m_keyed = true;
          int ret = wolfSSH_CTX_UsePrivateKey_buffer(m_ctx, (const uint8_t*)skey.data(),
            skey.size(),  WOLFSSH_FORMAT_ASN1);
          if (ret < 0)
            ESP_LOGE(tag, "Couldn't use configured server key, error %d: %s", ret,
              GetErrorString(ret));
          else
            {
            std::string fp = MyConfig.GetParamValue("ssh.info", "fingerprint", "[not available]");
            MyCommandApp.Log("SSH server key installed with fingerprint %s", fp.c_str());
            }
          }
        }
      }
      break;

    case MG_EV_RECV:
      {
      ESP_LOGV(tag, "Event MG_EV_RECV conn %p, data received %d", nc, *(int*)p);
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      child->Receive();
      }
      break;

    case MG_EV_SEND:
      {
      ESP_LOGV(tag, "Event MG_EV_SEND conn %p, data %p", nc, p);
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      child->Sent();
      break;
      }

    case MG_EV_CLOSE:
      {
      ESP_LOGV(tag, "Event MG_EV_CLOSE conn %p, data %p", nc, p);
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      if (child)
        delete child;
      }
      break;

    default:
      break;
    }
  }

OvmsSSH::OvmsSSH()
  {
  ESP_LOGI(tag, "Initialising SSH (8300)");
  m_keyed = false;
  m_ctx = NULL;

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(tag,"network.mgr.init", std::bind(&OvmsSSH::NetManInit, this, _1, _2));
  MyEvents.RegisterEvent(tag,"network.mgr.stop", std::bind(&OvmsSSH::NetManStop, this, _1, _2));
  MyConfig.RegisterParam("ssh.server", "SSH server private key store", true, false);
  MyConfig.RegisterParam("ssh.info", "SSH server information", false, true);
  MyConfig.RegisterParam("ssh.keys", "SSH public key store", true, true);
  }

void OvmsSSH::NetManInit(std::string event, void* data)
  {
  // Only initialise server for WIFI connections
  // TODO: Disabled as this introduces a network interface ordering issue. It
  //       seems that the correct way to do this is to always start the mongoose
  //       listener, but to filter incoming connections to check that the
  //       destination address is a Wifi interface address.
  // if (!(MyNetManager.m_connected_wifi || MyNetManager.m_wifi_ap))
  //   return;

  ESP_LOGI(tag, "Launching SSH Server");
  int ret = wolfSSH_Init();
  if (ret != WS_SUCCESS)
    {
    ESP_LOGE(tag, "Couldn't initialize wolfSSH, error %d: %s", ret,
      GetErrorString(ret));
    return;
    }

  m_ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
  if (m_ctx == NULL)
    {
    ::printf("\nInsufficient memory to allocate SSH context\n");
    return;
    }
  wolfSSH_SetLoggingCb(&wolf_logger);
  wolfSSH_CTX_SetBanner(m_ctx, NULL);
  wolfSSH_SetUserAuth(m_ctx, Authenticate);
  std::string skey = MyConfig.GetParamValueBinary("ssh.server", "key", std::string());
  if (skey.empty())
    {
    MyCommandApp.Log("Generating SSH Server key, wait before attempting access.");
    new RSAKeyGenerator();
    }
  else
    {
    m_keyed = true;
    ret = wolfSSH_CTX_UsePrivateKey_buffer(m_ctx, (const uint8_t*)skey.data(),
      skey.size(),  WOLFSSH_FORMAT_ASN1);
    if (ret < 0)
      ESP_LOGE(tag, "Couldn't use configured server key, error %d: %s", ret,
        GetErrorString(ret));
    }

  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  mg_connection* nc = mg_bind(mgr, ":22", MongooseHandler);
  if (nc)
    nc->user_data = NULL;
  else
    ESP_LOGE(tag, "Launching SSH Server failed");
  }

void OvmsSSH::NetManStop(std::string event, void* data)
  {
  if (m_ctx)
    {
    ESP_LOGI(tag, "Stopping SSH Server");
    wolfSSH_CTX_free(m_ctx);
    if (wolfSSH_Cleanup() != WS_SUCCESS)
      ESP_LOGE(tag, "Couldn't clean up wolfSSH.");
    m_ctx = NULL;
    }
  }

int OvmsSSH::Authenticate(uint8_t type, const WS_UserAuthData* data, void* ctx)
  {
  ConsoleSSH* cons = (ConsoleSSH*)ctx;
  if (type == WOLFSSH_USERAUTH_PASSWORD)
    {
    std::string user((const char*)data->username, data->usernameSz);
    std::string pw = MyConfig.GetParamValue("password", user, std::string());
    if (pw.empty())
      pw = MyConfig.GetParamValue("password", "module", std::string());
    if (pw.empty())
      {
      if (MyConfig.IsDefined("ssh.keys", ""))
        return WOLFSSH_USERAUTH_INVALID_PASSWORD; // Can't use null pw if have key
      else
        return WOLFSSH_USERAUTH_SUCCESS;  // No pw or key set means no authentication
      }
    if (pw.size() != data->sf.password.passwordSz ||
      memcmp(data->sf.password.password, pw.data(), pw.size()) != 0)
      return WOLFSSH_USERAUTH_INVALID_PASSWORD;
    }
  else if (type == WOLFSSH_USERAUTH_PUBLICKEY)
    {
    std::string user((const char*)data->username, data->usernameSz);
    std::string key = MyConfig.GetParamValue("ssh.keys", user, std::string());
    if (key.empty())
      return WOLFSSH_USERAUTH_INVALID_USER;
    byte der[560];
    uint32_t len;
    if (Base64_Decode((const byte*)key.data(), key.size(), der, &len) != 0 ||
      len != data->sf.publicKey.publicKeySz ||
      memcmp(data->sf.publicKey.publicKey, der, len) != 0)
      return WOLFSSH_USERAUTH_INVALID_PUBLICKEY;
    }
  else
    return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
  cons->SetSecure(true); // Successful SSH authentication grants privileged access
  return WOLFSSH_USERAUTH_SUCCESS;
  }

//-----------------------------------------------------------------------------
//    Class ConsoleSSH
//-----------------------------------------------------------------------------

int RecvCallback(WOLFSSH* ssh, void* data, uint32_t size, void* ctx);
int SendCallback(WOLFSSH* ssh, void* data, uint32_t size, void* ctx);

ConsoleSSH::ConsoleSSH(OvmsSSH* server, struct mg_connection* nc)
  {
  m_server = server;
  m_connection = nc;
  m_queue = xQueueCreate(100, sizeof(Event));
  m_ssh = NULL;
  m_state = ACCEPT;
  m_drain = 0;
  m_sent = true;
  m_rekey = false;
  m_needDir = false;
  m_isDir = false;
  m_verbose = false;
  m_recursive = false;
  m_file = NULL;
  m_ssh = wolfSSH_new(m_server->ctx());
  if (m_ssh == NULL)
    {
    ::printf("Couldn't allocate SSH session data.\n");
    return;
    }
  wolfSSH_SetAllocators(wolfssh_malloc, wolfssh_free, wolfssh_realloc);
  wolfSSL_SetAllocators(wolfssl_malloc, wolfssl_free, wolfssl_realloc);
  wolfSSH_SetIORecv(m_server->ctx(), ::RecvCallback);
  wolfSSH_SetIOSend(m_server->ctx(), ::SendCallback);
  wolfSSH_SetIOReadCtx(m_ssh, this);
  wolfSSH_SetIOWriteCtx(m_ssh, m_connection);
  wolfSSH_SetUserAuthCtx(m_ssh, this);
  /* Use the session object for its own highwater callback ctx */
  wolfSSH_SetHighwaterCtx(m_ssh, (void*)m_ssh);
  wolfSSH_SetHighwater(m_ssh, DEFAULT_HIGHWATER_MARK);
  wolfSSH_Debugging_ON();
  }

ConsoleSSH::~ConsoleSSH()
  {
  WOLFSSH* ssh = m_ssh;
  m_ssh = NULL;
  wolfSSH_free(ssh);
  vQueueDelete(m_queue);
  m_dirs.clear();
  }

// Handle MG_EV_RECV event.
void ConsoleSSH::Receive()
  {
  OvmsConsole::Event event;
  event.type = OvmsConsole::event_type_t::RECV;
  BaseType_t ret = xQueueSendToBack(m_queue, (void * )&event, (portTickType)(1000 / portTICK_PERIOD_MS));
  if (ret == pdPASS)
    {
    // Process the event we just queued in sequence with any queued logging.
    Poll(0);
    }
  else
    {
    ESP_LOGE(tag, "Timeout queueing message in ConsoleSSH::Receive\n");
    mbuf *io = &m_connection->recv_mbuf;
    mbuf_remove(io, io->len);
    }
  }

// Handle MG_EV_POLL event.
void ConsoleSSH::Send()
  {
  if (m_state != SOURCE_SEND || !m_sent)
    {
    if (m_drain > 0 && m_connection->send_mbuf.len == 0)
      write("", 0);       // Wake up output after draining
    return;
    }
  int ret = 0;
  while (true)
    {
    if (m_size == 0)
      m_size = fread(m_buffer, sizeof(char), BUFFER_SIZE, m_file);
    if (m_size <= 0)
      break;
    m_sent = false;
    ret = wolfSSH_stream_send(m_ssh, (uint8_t*)m_buffer + m_index, m_size);
    if (ret < 0)
      break;
    m_size -= ret;
    if (m_size == 0)
      m_index = 0;
    else
      {
      m_index += ret;
      return;
      }
    if (!m_sent)
      return;
    }
  if (ret < 0)
    {
    // Would need to check ret != WS_WANT_WRITE here if mg_send is changed to
    // return EWOUDBLOCK
    ESP_LOGE(tag, "Error %d in wolfSSH_stream_send: %s", ret, GetErrorString(ret));

    m_connection->flags |= MG_F_SEND_AND_CLOSE;
    m_state = CLOSING;
    }
  else if (m_size < 0)
    {
    ESP_LOGE(tag, "Error %d reading file in source scp: %s", m_size, strerror(m_size));
    m_connection->flags |= MG_F_SEND_AND_CLOSE;
    m_state = CLOSING;
    }
  else  // EOF on fread()
    {
    ESP_LOGD(tag, "Sending %s completed", m_path.c_str());
    m_state = SOURCE_RESPONSE;
    wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
    }
  fclose(m_file);
  m_file = NULL;
  }

// Handle MG_EV_SEND event.
void ConsoleSSH::Sent()
  {
  m_sent = true;
  }

int ConsoleSSH::GetResponse()
  {
  // Read the single binary 0 byte ACK or the firat byte of an error message or
  // protocol line.
  int rc = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, 1);
  if (rc < 1)
    return rc;
  if (m_state != SINK_LOOP || *m_buffer < ' ')
    ESP_LOGD(tag, "response() received protocol ack byte %d", *m_buffer);
  if (*m_buffer != 0)
    {
    // Read the rest of the protocol line or error message, stopping on the
    // newline and leaving anything else queued.  We assume that it will all be
    // sent in one IP packet so we don't have to worry about WS_WANT_READ.
    char* p = m_buffer + 1;
    do
      rc = wolfSSH_stream_read(m_ssh, (uint8_t*)p, 1);
    while (rc == 1 && *p++ != '\n' && p < &m_buffer[BUFFER_SIZE-1]);
    *p-- = '\0';
    if (*p == '\n')
      *p = '\0';
    }
  return 1;
  }

// Handle RECV event from queue.
void ConsoleSSH::HandleDeviceEvent(void* pEvent)
  {
  struct stat sbuf;
  std::string msg;
  size_t filename = 0;
  Level level;
  int rc = 0;
  Event event = *(Event*)pEvent;
  if (event.type != RECV)
    {
    ESP_LOGE(tag, "Unknown event type %d in ConsoleSSH", event.type);
    return;
    }
  do
    {
    switch (m_state)
      {
      case ACCEPT:
        {
        rc = wolfSSH_accept(m_ssh);
        if (rc != WS_SUCCESS && (rc = wolfSSH_get_error(m_ssh)) != WS_SUCCESS)
          break;
        if (wolfSSH_GetSessionType(m_ssh) == WOLFSSH_SESSION_SHELL)
          {
          Initialize("SSH");
          m_state = SHELL;
          }
        else if (wolfSSH_GetSessionType(m_ssh) == WOLFSSH_SESSION_EXEC)
          {
          const char* cmdline = wolfSSH_GetSessionCommand(m_ssh);
          if (!cmdline)
            {
            rc = WS_BAD_USAGE;        // no command provided
            break;
            }
          ESP_LOGD(tag, "SSH command request: %s", cmdline);
          if (strncmp(cmdline, "scp -", 5) == 0)
            {
            bool from = false, to = false;
            int i;
            for (i = 5; ; ++i)
              {
              if (cmdline[i] == 'f') from = true;
              if (cmdline[i] == 't') to = true;
              if (cmdline[i] == 'd') m_needDir = true;
              if (cmdline[i] == 'v') m_verbose = true;
              if (cmdline[i] == 'r') m_recursive = true;
              if (cmdline[i] == ' ' && cmdline[++i] != '-') break;
              if (cmdline[i] == '\0') break;
              }
            m_path = &cmdline[i];
            if (from == to)
              rc = WS_BAD_USAGE;
            else if (from)
              m_state = SOURCE;
            else  // -t (to) case
              m_state = SINK;
            }
          else
            m_state = EXEC;
          }
        else  // Session did not request a 'shell' or 'exec' program
          rc = WS_UNIMPLEMENTED_E;
        break;
        }

      // Normal interactive shell session
      case SHELL:
        {
        rc = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, BUFFER_SIZE);
        if (rc > 0)
          {
          // Translate CR (Enter) from ssh client into \n for microrl
          for (int i = 0; i < rc; ++i)
            {
            if (m_buffer[i] == '\r')
              m_buffer[i] = '\n';
            }
          ProcessChars(m_buffer, rc);
          }
        break;
        }

      // 'exec' session with command to be executed and results returned
      case EXEC:
        {
        Initialize(NULL);
        const char* cmdline = wolfSSH_GetSessionCommand(m_ssh);
        ProcessChars(cmdline, strlen(cmdline));
        ProcessChar('\n');
        wolfSSH_stream_exit(m_ssh, 0); // XXX Need commands to return status
        m_state = CLOSING;
        break;
        }

      // SCP session pulling data from OVMS
      case SOURCE:
        {
        // Only expecting to read a single binary 0 byte
        rc = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, 1);
        if (rc < 1)
          break;
        ESP_LOGD(tag, "SOURCE received protocol ack byte %d", *m_buffer);
        if (*m_buffer == 0)
          m_state = SOURCE_LOOP;
        else
          rc = WS_BAD_USAGE;  // client always sends 0 so this shouldn't happen
        break;
        }

      case SOURCE_LOOP:
        {
        ESP_LOGD(tag, "SCP 'from' file %s", m_path.c_str());
        msg.assign("\2scp: ").append(m_path).append(": ");
        if (MyConfig.ProtectedPath(m_path.c_str()))
          msg.append("protected path\n");
        else
          {
          while (m_path.size() > 0 && m_path.at(m_path.size()-1) == '/')
            m_path.resize(m_path.size()-1);          // Trim off a trailing '/'
          filename = m_path.find_last_of('/');
          if (filename == std::string::npos)
            filename = 0;
          else
            ++filename;
          int ret = 0;
          // Need to fake these because stat says "Invalid argument"
          if (m_path.compare("/store") == 0 || m_path.compare("/sd") == 0)
            sbuf.st_mode = S_IFDIR;  // Don't care about permissions
          else
            ret = stat(m_path.c_str(), &sbuf);
          if (ret < 0)
            msg.append(strerror(errno)).append("\n");
          else if (S_ISDIR(sbuf.st_mode))
            {
            if (!m_recursive)
              msg.append("received directory without -r\n");
            else if ((level.dir = opendir(m_path.c_str())) == NULL)
              msg.append(strerror(errno)).append("\n");
            else
              {
              level.size = m_path.size();
              m_dirs.push_front(level);
              msg.assign("D0755 0 ").append(m_path.substr(filename));
              ESP_LOGD(tag, "Source: %s", msg.c_str());
              msg.append("\n");
              wolfSSH_stream_send(m_ssh, (uint8_t*)msg.c_str(), msg.size());
              m_state = SOURCE_RESPONSE;
              break;
              }
            }
          else if (S_ISREG(sbuf.st_mode))
            {
            if ((m_file = fopen(m_path.c_str(), "r")) == NULL)
              msg.append(strerror(errno)).append("\n");
            else
              {
              char num[12];
              snprintf(num, sizeof(num), "%ld ", sbuf.st_size);
              msg.assign("C0644 ").append(num).append(m_path.substr(filename));
              ESP_LOGD(tag, "Source: %s", msg.c_str());
              msg.append("\n");
              wolfSSH_stream_send(m_ssh, (uint8_t*)msg.c_str(), msg.size());
              m_state = SOURCE_RESPONSE;
              break;
              }
            }
          else
            msg.append("not a regular file\n");
          }
        // Send the composed error message
        wolfSSH_stream_send(m_ssh, (uint8_t*)msg.c_str(), msg.size());
        m_state = SOURCE_RESPONSE;
        ESP_LOGI(tag, "%.*s", msg.size()-2, msg.substr(1,msg.size()-1).c_str());
        break;
        }

      case SOURCE_DIR:
        {
        level = m_dirs.front();
        m_path.resize(level.size);
        struct dirent* dp = readdir(level.dir);
        if (dp == NULL)
          {
          closedir(level.dir);
          m_dirs.pop_front();
          ESP_LOGD(tag, "Source: E");
          wolfSSH_stream_send(m_ssh, (uint8_t*)"E\n", 2);
          m_state = SOURCE_RESPONSE;
          break;
          }
        m_path.append("/").append(dp->d_name);
        m_state = SOURCE_LOOP;
        break;
        }

      case SOURCE_SEND:
      case SOURCE_RESPONSE:
        {
        rc = GetResponse();
        if (rc < 1)
          break;
	if (m_state == SOURCE_SEND)
	    ESP_LOGW(tag, "RECV not expected in SOURCE_SEND state");
        if (m_buffer[0] == '\1')
          {
          ESP_LOGW(tag, "Source: %s", &m_buffer[1]);
          if (m_file) // Abort the file we were going to send, if any
            {
            fclose(m_file);
            m_file = NULL;
            }
          }
        else if (m_buffer[0] == '\2')
          {
          ESP_LOGE(tag, "Source: %s", &m_buffer[1]);
          wolfSSH_stream_exit(m_ssh, 1);
          m_state = CLOSING;
          break;
          }
        if (m_file)
          {
          m_state = SOURCE_SEND;
          m_index = m_size = 0;
          Send();       // Send the first buffer, more when sent
          return;
          }
        if (!m_dirs.empty())
          {
          m_state = SOURCE_DIR;   // working on a directory
          break;
          }
        wolfSSH_stream_exit(m_ssh, 0);
        m_state = CLOSING;
        break;
        }

      // SCP session pushing data to OVMS
      case SINK:
        {
        msg.assign("\2scp: ").append(m_path).append(": ");
        while (m_path.size() > 1 && m_path.at(m_path.size()-1) == '/')
          {
          m_path.resize(m_path.size()-1);          // Trim off a trailing '/'
          m_needDir = true;
          }
        level.size = m_path.size();
        m_dirs.push_front(level);
        filename = std::string::npos;
        int ret = 0;
        // Need to fake these because stat says "Invalid argument"
        if (m_path.compare("/store") == 0 || m_path.compare("/sd") == 0)
          sbuf.st_mode = S_IFDIR;  // Don't care about permissions
        else
          ret = stat(m_path.c_str(), &sbuf);
        if (ret < 0 && errno == ENOENT && !m_needDir &&
          (filename = m_path.find_last_of('/')) != std::string::npos)
          {
          // Check path dirname for case where we're creating a new file
          int retd = stat(m_path.substr(0, filename).c_str(), &sbuf);
          if (retd == 0)
            ret = retd;
          }
        if (ret < 0)
          msg.append(strerror(errno)).append("\n");
        else if (S_ISDIR(sbuf.st_mode) || S_ISREG(sbuf.st_mode))
          {
          m_isDir = S_ISDIR(sbuf.st_mode) && filename == std::string::npos;
          if (m_isDir || !m_needDir)
            {
            m_state = SINK_LOOP;
            wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
            break;
            }
          msg.append(strerror(ENOTDIR)).append("\n");
          }
        else
          msg.append("not a regular file\n");  // Probably can't hit this
        wolfSSH_stream_send(m_ssh, (uint8_t*)msg.c_str(), msg.size());
        ESP_LOGI(tag, "%.*s", msg.size()-2, msg.substr(1,msg.size()-1).c_str());
        wolfSSH_stream_exit(m_ssh, 0);
        m_state = CLOSING;
        break;
        }

      case SINK_LOOP:
        {
        // Read the protocol line or error message.
        rc = GetResponse();
        if (rc < 1)
          break;
        if (*m_buffer == '\1')
          {
          ESP_LOGW(tag, "Sink: %s", &m_buffer[1]);
          break;  // Continue with more protocol lines
          }
        else if (*m_buffer == '\2')
          {
          ESP_LOGE(tag, "Sink: %s", &m_buffer[1]);
          wolfSSH_stream_exit(m_ssh, 1);
          m_state = CLOSING;
          break;
          }
        ESP_LOGD(tag, "Sink: %s", m_buffer);
        msg.assign("\2scp: ");
        if (*m_buffer == 'T')                         // Times are ignored
          {
          wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
          break;
          }
        else if (*m_buffer == 'E')
          {
          level = m_dirs.front();
          m_path.resize(level.size);
          m_dirs.pop_front();
          if (!m_dirs.empty())
            {
            wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
            break;
            }
          msg.append("protocol error: unbalanced D-E pair\n");
          }
        else if (*m_buffer == 'D' || *m_buffer == 'C')
          {
          char* end = m_buffer;
          if (m_buffer[1] != '0' || m_buffer[2] < '0' || m_buffer[2] > '7' ||
            m_buffer[3] < '0' || m_buffer[3] > '7' || m_buffer[4] < '0' || m_buffer[4] > '7' ||
            m_buffer[5] != ' ')
            msg.append("bad mode ").append(&m_buffer[1], 4).append("\n");
          else if ((m_size = strtoul(&m_buffer[6], &end, 10)) < 0 || m_size > 10485760 || *end++ != ' ')
            msg.append("size unacceptable: ").append(&m_buffer[6], end-&m_buffer[6]).append("\n");
          else if ((strchr(end, '/') != NULL) || (strcmp(end, "..") == 0))
            msg.append("unexpected filename: ").append(end).append("\n");
          else
            {
            level = m_dirs.front();
            m_path.resize(level.size);
            if (m_isDir)
              {
              if (m_path.compare("/") != 0)  // Probably impossible to target "/" on OVMS
                m_path.append("/");
              m_path.append(end);
              }
            bool exists = (stat(m_path.c_str(), &sbuf) == 0);
            msg.append(m_path).append(": ");
            if (MyConfig.ProtectedPath(m_path.c_str()))
              msg.append("protected path\n");
            else if (*m_buffer == 'D')
              {
              if (!m_recursive)
                msg.append("received directory without -r\n");
              else
                {
                level.size = m_path.size();
                m_dirs.push_front(level);
                if (exists)
                  {
                  if (!S_ISDIR(sbuf.st_mode))
                    msg.append(strerror(ENOTDIR)).append("\n");
                  else
                    {
                    wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
                    break;
                    }
                  }
                else
                  {
                  ESP_LOGD(tag, "mkdir(\"%s\", 0777)", m_path.c_str());
                  if (mkdir(m_path.c_str(), 0777) < 0)
                    msg.append("mkdir: ").append(strerror(errno)).append("\n");
                  else
                    {
                    wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
                    break;
                    }
                  }
                }
              }
            else // 'C'
              {
              if (exists && !S_ISREG(sbuf.st_mode))
                msg.append("not a regular file\n");
              else
                {
                ESP_LOGD(tag, "fopen(\"%s\", \"w\")", m_path.c_str());
		if ((m_file = fopen(m_path.c_str(), "w")) == NULL)
                  msg.append("fopen: ").append(strerror(errno)).append("\n");
                else
                  {
                  m_state = SINK_RECEIVE;
                  wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
                  break;
                  }
                }
              }
            }
          }
        else
          msg.append("protocol error: ").append(m_buffer).append("\n");
        wolfSSH_stream_send(m_ssh, (uint8_t*)msg.c_str(), msg.size());
        ESP_LOGI(tag, "%.*s", msg.size()-2, msg.substr(1,msg.size()-1).c_str());
        wolfSSH_stream_exit(m_ssh, 1);
        break;
        }

      case SINK_RECEIVE:
        {
        int size = BUFFER_SIZE;
        if (m_size < size)
          size = m_size;
        rc = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, size);
        if (rc < 0)
          break;       // Probably WS_WANT_READ, loop end checks it
        size = fwrite(m_buffer, sizeof(uint8_t), rc, m_file);
        if (size < rc)
          ESP_LOGE(tag, "Write error on %s: %s", m_path.c_str(), strerror(errno));
        m_size -= rc;
        if (m_size == 0)
          {
          fclose(m_file);
          m_file = NULL;
          m_state = SINK_RESPONSE;
          wolfSSH_stream_send(m_ssh, (uint8_t*)"", 1);
          }
        break;
        }

      case SINK_RESPONSE:
        {
        rc = GetResponse();
        if (rc < 1)
          break;
        if (*m_buffer == '\1')       // Warning, so we continue
          ESP_LOGW(tag, "Sink: %s", &m_buffer[1]);
        else if (*m_buffer == '\2')  // Error, so we exit
          {
          ESP_LOGE(tag, "Sink: %s", &m_buffer[1]);
          wolfSSH_stream_exit(m_ssh, 1);
          m_state = CLOSING;
          break;
          }
        m_state = SINK_LOOP;
        break;
        }

      case CLOSING:
        ESP_LOGD(tag, "Reached CLOSING");
        // Try to read to let SSH process packets, but ignore anything delivered
        rc = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, BUFFER_SIZE);
        break;
      }
    } while (rc >= 0);

  if (rc == WS_WANT_READ || rc == WS_BAD_ARGUMENT)  // Latter => channel closed
    return;
  if (rc == WS_EOF)
    {
    wolfSSH_stream_exit(m_ssh, 0);
    m_state = CLOSING;
    return;
    }
  ESP_LOGE(tag, "Error %d in reception: %s", rc, GetErrorString(rc));
  m_connection->flags |= MG_F_SEND_AND_CLOSE;
  }

// This is called to shut down the SSH connection when the "exit" command is input.
void ConsoleSSH::Exit()
  {
  printf("logout\n");
  wolfSSH_stream_exit(m_ssh, 0);
  }

int ConsoleSSH::puts(const char* s)
  {
  if (!m_ssh)
    return -1;
  write(s, strlen(s));
  write(&newline, 1);
  return 0;
  }

int ConsoleSSH::printf(const char* fmt, ...)
  {
  if (!m_ssh)
    return 0;
  char *buffer = NULL;
  va_list args;
  va_start(args,fmt);
  int ret = vasprintf(&buffer, fmt, args);
  va_end(args);
  if (ret < 0)
    {
    if (buffer)
      free(buffer);
    return ret;
    }
  ret = write(buffer, ret);
  free(buffer);
  return ret;
  }

ssize_t ConsoleSSH::write(const void *buf, size_t nbyte)
  {
  if (!m_ssh || (m_connection->flags & MG_F_SEND_AND_CLOSE))
    return 0;

  int ret = 0;
  if (m_drain > 0)
    {
    if (m_connection->send_mbuf.len > 0)
      {
      m_drain += nbyte;
      return 0;
      }
    if (--m_drain)
      {
      ESP_LOGE(tag, "%zu output bytes lost due to low free memory", m_drain);
      char *buffer = NULL;
      ret = asprintf(&buffer,
        "\r\n\033[33m[%zu bytes lost due to low free memory]\033[0m\r\n", m_drain);
      if (ret < 0)
        {
        if (buffer)
          free(buffer);
        return ret;
        }
      ret = wolfSSH_stream_send(m_ssh, (uint8_t*)buffer, ret);
      free(buffer);
      if (ret < 0)
        {
        ++m_drain;
        return 0;
        }
      m_drain = 0;
      ProcessChar('R'-0100);
      }
    }
  if (!nbyte)
    return 0;

  uint8_t* start = (uint8_t*)buf;
  uint8_t* eol = start;
  uint8_t* end = start + nbyte;
  while (eol < end)
    {
    if (*eol == '\n')
      {
      if (eol > start)
        {
        ret = wolfSSH_stream_send(m_ssh, start, eol - start);
        if (ret <= 0) break;
        }
      ret = wolfSSH_stream_send(m_ssh, CRLF, 2);
      if (ret <= 0) break;
      ++eol;
      start = eol;
      }
    else if (++eol == end)
      {
      ret = wolfSSH_stream_send(m_ssh, start, eol - start);
      break;
      }
  }

  if (ret <= 0)
    {
    if (ret == WS_REKEYING)
      {
      if (!m_rekey)
        ESP_LOGW(tag, "wolfSSH rekeying, some output will be lost");
      m_rekey = true;
      }
    else if (ret == WS_WANT_WRITE)
      {
      m_drain = 1;
      ret = 0;
      }
    else
      {
      ESP_LOGE(tag, "wolfSSH_stream_send returned %d: %s", ret, GetErrorString(ret));
      m_connection->flags |= MG_F_SEND_AND_CLOSE;
      }
    }
  else
    m_rekey = false;
  return ret;
  }

// Routines to be called from within WolfSSH to receive and send data from and
// to the network socket.

int RecvCallback(WOLFSSH* ssh, void* data, uint32_t size, void* ctx)
  {
  ConsoleSSH* me = (ConsoleSSH*)ctx;
  return me->RecvCallback((char*)data, size);
  }

int ConsoleSSH::RecvCallback(char* buf, uint32_t size)
  {
  mbuf *io = &m_connection->recv_mbuf;
  size_t len = io->len;
  if (size < len)
    len = size;
  else if (len == 0)
    return WS_CBIO_ERR_WANT_READ;       // No more data available
  memcpy(buf, io->buf, len);
  mbuf_remove(io, len);
  return len;
  }

int SendCallback(WOLFSSH* ssh, void* data, uint32_t size, void* ctx)
  {
  mg_connection* nc = (mg_connection*)ctx;
  nc->flags |= MG_F_SEND_IMMEDIATELY;
  size_t ret = mg_send(nc, (char*)data, size);
  if (ret == 0)
    {
    if (!((ConsoleSSH*)nc->user_data)->IsDraining())
      {
      size_t free8 = heap_caps_get_free_size(MALLOC_CAP_8BIT|MALLOC_CAP_INTERNAL);
      ESP_LOGW(tag, "send blocked on %zu-byte packet: low free memory %zu", size, free8);
      }
    size = WS_CBIO_ERR_WANT_WRITE;
    }
  return size;
  }

//-----------------------------------------------------------------------------
//    Class RSAKeyGenerator
//-----------------------------------------------------------------------------

RSAKeyGenerator::RSAKeyGenerator() : TaskBase("RSAKeyGen", 4096)
  {
  Instantiate();
  }

void RSAKeyGenerator::Service()
  {
  // Generate a random 2048-bit SSH host key for the SSH server in DER format.
  // The source of entropy in the WolfSSL code is the esp_random() function.
  RsaKey key;
  RNG rng;
  int ret;
  ret = wc_InitRng(&rng);
  if (ret != 0)
    {
    ESP_LOGE(tag, "Failed to init RNG to generate SSH server key, error = %d", ret);
    return;
    }
  ret = wc_InitRsaKey(&key, 0);
  if (ret != 0)
    {
    ESP_LOGE(tag, "Failed to init RsaKey to generate SSH server key, error = %d", ret);
    return;
    }
  ret = wc_MakeRsaKey(&key, 2048, 65537, &rng);
  if (ret != 0)
    {
    ESP_LOGE(tag, "Failed to generate SSH server key, error = %d", ret);
    return;
    }
  int size = wc_RsaKeyToDer(&key, m_der, sizeof(m_der));
  if (size < 0)
    {
    ESP_LOGE(tag, "Failed to convert SSH server key to DER format, error = %d", size);
    return;
    }

  // Calculate the SHA256 fingerprint of the public half of the key, which
  // is the hash of the 32-bit length of the item and the item itself for
  // the type string, the exponent, and the modulus.
  byte digest[SHA256_DIGEST_SIZE];
  Sha256 sha;
  const char* type = "ssh-rsa";
  uint32_t length[2];
  byte  exp[8];
  byte  mod[260];
  uint32_t explen = sizeof(exp);
  uint32_t modlen = sizeof(mod);
  ret = wc_RsaFlattenPublicKey(&key, exp, &explen, mod, &modlen);
  wc_InitSha256(&sha);
  length[0] = htonl(strlen(type));
  wc_Sha256Update(&sha, (byte*)length, sizeof(uint32_t));
  wc_Sha256Update(&sha, (const byte*)type, 7);
  length[0] = htonl(explen);
  wc_Sha256Update(&sha, (byte*)length, sizeof(uint32_t));
  wc_Sha256Update(&sha, exp, explen);
  int extra = 0;
  if (mod[0] & 0x80)  // DER encoding inserts 0x00 byte if first data bit is 1
    {
    ++extra;
    length[1] = 0;
    }
  length[0] = htonl(modlen+extra);
  wc_Sha256Update(&sha, (byte*)length, sizeof(uint32_t)+extra);
  wc_Sha256Update(&sha, mod, modlen);
  wc_Sha256Final(&sha, digest);
  unsigned char fp[48];
  size_t fplen = sizeof(fp);
  int rc = Base64_Encode(digest, sizeof(digest), fp, &fplen);
  if (rc != 0)
    {
    ESP_LOGE(tag, "Failed to encode SSH server key fingerprint in base64");
    return;
    }
  fp[43] = '\0';
  MyConfig.SetParamValue("ssh.info", "fingerprint", std::string((char*)fp, fplen));
  MyConfig.SetParamValueBinary("ssh.server", "key", std::string((char*)m_der, size));

  if (wc_FreeRsaKey(&key) != 0)
    ESP_LOGE(tag, "RSA key free failed");
  if (wc_FreeRng(&rng) != 0)
    ESP_LOGE(tag, "Couldn't free RNG");
  }

static void wolf_logger(enum wolfSSH_LogLevel level, const char* const msg)
  {
  switch (level)
    {
    case WS_LOG_USER:
    case WS_LOG_ERROR:
      ESP_LOGE(wolfssh_tag, "%s", msg);
      break;

    case WS_LOG_WARN:
      ESP_LOGW(wolfssh_tag, "%s", msg);
      break;

    case WS_LOG_INFO:
      ESP_LOGI(wolfssh_tag, "%s", msg);
      break;

    case WS_LOG_DEBUG:
      ESP_LOGD(wolfssh_tag, "%s", msg);
      break;
    }
  }

static void* wolfssh_malloc(size_t size)
  {
  void* ptr = ExternalRamMalloc(size);
  if (!ptr)
    ESP_LOGE(wolfssh_tag, "memory allocation failed for size %zu", size);
  return ptr;
  }

static void wolfssh_free(void* ptr)
  {
  free(ptr);
  }

static void* wolfssh_realloc(void* ptr, size_t size)
  {
  void* nptr = ExternalRamRealloc(ptr, size);
  if (!nptr)
    ESP_LOGE(wolfssh_tag, "memory reallocation failed for size %zu", size);
  return nptr;
  }

static void* wolfssl_malloc(size_t size)
  {
  void* ptr = ExternalRamMalloc(size);
  if (!ptr)
    ESP_LOGE(wolfssl_tag, "memory allocation failed for size %zu", size);
  return ptr;
  }

static void wolfssl_free(void* ptr)
  {
  free(ptr);
  }

static void* wolfssl_realloc(void* ptr, size_t size)
  {
  void* nptr = ExternalRamRealloc(ptr, size);
  if (!nptr)
    ESP_LOGE(wolfssl_tag, "memory reallocation failed for size %zu", size);
  return nptr;
  }
