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
#define LWIP_POSIX_SOCKETS_IO_NAMES 0
#include <lwip/def.h>
#include <lwip/sockets.h>
#undef bind
#include "freertos/queue.h"
#include "ovms_log.h"
#include "ovms_events.h"
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/sha.h>
#include <wolfssh/ssh.h>
#include "console_ssh.h"
#include "ovms_netmanager.h"
#include "ovms_config.h"

static const char *tag = "ssh";
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
  //ESP_LOGI(tag, "Event %d conn %p, data %p", ev, nc, p);
  switch (ev)
    {
    case MG_EV_ACCEPT:
      {
      ConsoleSSH* child = new ConsoleSSH(this, nc);
      nc->user_data = child;
      break;
      }

    case MG_EV_POLL:
      {
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      if (child)
        child->Poll(0);
      if (!m_keyed)
        {
        std::string skey = MyConfig.GetParamValueBinary("ssh.server", "key", std::string());
        if (!skey.empty())
          {
          m_keyed = true;
          int ret = wolfSSH_CTX_UsePrivateKey_buffer(m_ctx, (const uint8_t*)skey.data(),
            skey.size(),  WOLFSSH_FORMAT_ASN1);
          if (ret < 0)
            {
            ESP_LOGE(tag, "Couldn't use configured server key, error = %d", ret);
            }
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
      ConsoleSSH* child = (ConsoleSSH*)nc->user_data;
      child->Receive();
      child->Poll(0);
      }
      break;

    case MG_EV_CLOSE:
      {
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
  ESP_LOGI(tag, "Launching SSH Server");
  int ret = wolfSSH_Init();
  if (ret != WS_SUCCESS)
    {
    ESP_LOGE(tag, "Couldn't initialize wolfSSH, error = %d", ret);
    return;
    }

  m_ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
  if (m_ctx == NULL)
    {
    ::printf("\nInsufficient memory to allocate SSH context\n");
    return;
    }
  wolfSSH_CTX_SetBanner(m_ctx, "OVMS SSH Server\n");
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
      {
      ESP_LOGE(tag, "Couldn't use configured server key, error = %d", ret);
      }
    }

  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  mg_connection* nc = mg_bind(mgr, ":22", MongooseHandler);
  if (nc)
    nc->user_data = NULL;
  else
    {
    ESP_LOGE(tag, "Launching SSH Server failed");
    }
  }

void OvmsSSH::NetManStop(std::string event, void* data)
  {
  ESP_LOGI(tag, "Stopping SSH Server");
  wolfSSH_CTX_free(m_ctx);
  if (wolfSSH_Cleanup() != WS_SUCCESS)
    {
    ESP_LOGE(tag, "Couldn't clean up wolfSSH.");
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
  m_accepted = false;
  m_pending = false;
  m_rekey = false;
  m_ssh = wolfSSH_new(m_server->ctx());
  if (m_ssh == NULL)
    {
    ::printf("Couldn't allocate SSH session data.\n");
    return;
    }
  wolfSSH_SetIORecv(m_server->ctx(), ::RecvCallback);
  wolfSSH_SetIOSend(m_server->ctx(), SendCallback);
  wolfSSH_SetIOReadCtx(m_ssh, this);
  wolfSSH_SetIOWriteCtx(m_ssh, m_connection);
  wolfSSH_SetUserAuthCtx(m_ssh, this);
  /* Use the session object for its own highwater callback ctx */
  wolfSSH_SetHighwaterCtx(m_ssh, (void*)m_ssh);
  wolfSSH_SetHighwater(m_ssh, DEFAULT_HIGHWATER_MARK);
  //wolfSSH_Debugging_ON();

  // Start the accept process, but it will likely not complete immediately
  int rc = wolfSSH_accept(m_ssh);
  if (rc == WS_SUCCESS)
    {
    m_accepted = true;
    Initialize("SSH");
    }
  else if ((rc = wolfSSH_get_error(m_ssh)) != WS_WANT_READ)
    {
    ESP_LOGE(tag, "SSH server failed to accept connection, error = %d", rc);
    m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
    return;
    }
  }

ConsoleSSH::~ConsoleSSH()
  {
  WOLFSSH* ssh = m_ssh;
  m_ssh = NULL;
  wolfSSH_free(ssh);
  vQueueDelete(m_queue);
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

// Handle RECV event from queue.
void ConsoleSSH::HandleDeviceEvent(void* pEvent)
  {
  Event event = *(Event*)pEvent;
  switch (event.type)
    {
    case RECV:
      m_pending = true;         // We have data waiting to be taken
      if (!m_accepted)
        {
        int rc = wolfSSH_accept(m_ssh);
        if (rc == WS_SUCCESS)
          {
          m_accepted = true;
          Initialize("SSH");
          }
        else if ((rc = wolfSSH_get_error(m_ssh)) != WS_WANT_READ)
          {
          ESP_LOGE(tag, "SSH server failed to accept connection, error = %d", rc);
          m_connection->flags |= MG_F_CLOSE_IMMEDIATELY;
          break;
          }
        }
      else while (true)
        {
        int size = wolfSSH_stream_read(m_ssh, (uint8_t*)m_buffer, BUFFER_SIZE);
        if (size > 0)
          {
          // Translate CR (Enter) from ssh client into \n for microrl
          for (int i = 0; i < size; ++i)
            {
            if (m_buffer[i] == '\r')
              m_buffer[i] = '\n';
            }
          ProcessChars(m_buffer, size);
          }
        else
          {
          if (size != WS_WANT_READ)
            // Some error occurred, presume it is connection closing.
            m_connection->flags |= MG_F_SEND_AND_CLOSE;
          break;
          }
        }
      break;

    default:
      ESP_LOGE(tag, "Unknown event type in ConsoleSSH");
      break;
    }
  }

// This is called to shut down the SSH connection when the "exit" command is input.
void ConsoleSSH::Exit()
  {
  printf("logout\n");
  m_connection->flags |= MG_F_SEND_AND_CLOSE;
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
  if (!m_ssh || !nbyte || (m_connection->flags & MG_F_SEND_AND_CLOSE))
    return 0;
  uint8_t* start = (uint8_t*)buf;
  uint8_t* eol = start;
  uint8_t* end = start + nbyte;
  int ret = 0;
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
    if (ret != WS_REKEYING)
      {
      ESP_LOGE(tag, "wolfSSH_stream_send returned %d", ret);
      m_connection->flags |= MG_F_SEND_AND_CLOSE;
      }
    else
      {
      if (!m_rekey)
        ESP_LOGW(tag, "wolfSSH rekeying, some output will be lost");
      m_rekey = true;
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
  if (!m_pending)
    return WS_CBIO_ERR_WANT_READ;       // No more data available
  mbuf *io = &m_connection->recv_mbuf;
  size_t len = io->len;
  if (size < len)
    len = size;
  else
    m_pending = false;
  memcpy(buf, io->buf, len);
  mbuf_remove(io, len);
  return len;
  }

int SendCallback(WOLFSSH* ssh, void* data, uint32_t size, void* ctx)
  {
  mg_send((mg_connection*)ctx, (char*)data, size);
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
  MyConfig.SetParamValueBinary("ssh.server", "key", std::string((char*)m_der, size));

  // Calculate the SHA256 fingerprint of the public half of the key, which
  // is the hash of the 32-bit length of the item and the item itself for
  // the type string, the exponent, and the modulus.
  byte digest[SHA256_DIGEST_SIZE];
  Sha256 sha;
  const char* type = "ssh-rsa";
  byte length[8];
  byte  exp[8];
  byte  mod[260];
  uint32_t explen = sizeof(exp);
  uint32_t modlen = sizeof(mod);
  ret = wc_RsaFlattenPublicKey(&key, exp, &explen, mod, &modlen);
  wc_InitSha256(&sha);
  *(uint32_t*)length = htonl(strlen(type));
  wc_Sha256Update(&sha, length, sizeof(uint32_t));
  wc_Sha256Update(&sha, (const byte*)type, 7);
  *(uint32_t*)length = htonl(explen);
  wc_Sha256Update(&sha, length, sizeof(uint32_t));
  wc_Sha256Update(&sha, exp, explen);
  int extra = 0;
  if (mod[0] & 0x80)  // DER encoding inserts 0x00 byte if first data bit is 1
    {
    ++extra;
    length[sizeof(uint32_t)] = 0x00;
    }
  *(uint32_t*)length = htonl(modlen+extra);
  wc_Sha256Update(&sha, length, sizeof(uint32_t)+extra);
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

  if (wc_FreeRsaKey(&key) != 0)
    {
    ESP_LOGE(tag, "RSA key free failed");
    }
  if (wc_FreeRng(&rng) != 0)
    {
    ESP_LOGE(tag, "Couldn't free RNG");
    }
  }
