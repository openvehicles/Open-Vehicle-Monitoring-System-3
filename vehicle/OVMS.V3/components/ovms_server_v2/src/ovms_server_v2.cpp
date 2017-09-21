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
static const char *TAG = "ovms-server-v2";

#include <string.h>
#include "ovms_server_v2.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "crypt_base64.h"
#include "crypt_hmac.h"
#include "crypt_md5.h"

#include <sys/socket.h>
#include "esp_system.h"
#include "lwip/err.h"
#include "lwip/sockets.h"
#include "lwip/sys.h"
#include "lwip/netdb.h"
#include "lwip/dns.h"

OvmsServerV2 *MyOvmsServerV2 = NULL;

void OvmsServerV2::ServerTask()
  {
  ESP_LOGI(TAG, "OVMS Server v2 task running");

  while(1)
    {
    if (!Connect())
      {
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
      }

    if (!Login())
      {
      Disconnect();
      vTaskDelay(10000 / portTICK_PERIOD_MS);
      continue;
      }

    while(1)
      {
      vTaskDelay(1000 / portTICK_PERIOD_MS);
      }
    }
  }

bool OvmsServerV2::Connect()
  {
  m_vehicleid = MyConfig.GetParamValue("vehicle", "id");
  m_server = MyConfig.GetParamValue("server.v2", "server");
  m_password = MyConfig.GetParamValue("server.v2", "password");
  m_port = MyConfig.GetParamValue("server.v2", "port");
  if (m_port.empty()) m_port = "6867";

  ESP_LOGI(TAG, "Connection is %s:%s %s/%s",
    m_server.c_str(), m_port.c_str(),
    m_vehicleid.c_str(), m_password.c_str());

  if (m_vehicleid.empty())
    {
    ESP_LOGW(TAG, "Parameter vehicle/id must be defined");
    return false;
    }
  if (m_server.empty())
    {
    ESP_LOGW(TAG, "Parameter server.v2/server must be defined");
    return false;
    }
  if (m_password.empty())
    {
    ESP_LOGW(TAG, "Parameter server.v2/password must be defined");
    return false;
    }

  struct addrinfo hints;
  hints.ai_family = AF_INET;
  hints.ai_socktype = SOCK_STREAM;
  struct addrinfo *res;
  struct in_addr *addr;
  int err = getaddrinfo(m_server.c_str(), m_port.c_str(), &hints, &res);
  if ((err != 0) || (res == NULL))
    {
    ESP_LOGW(TAG, "Could not resolve DNS for %s",m_server.c_str());
    return false;
    }

  addr = &((struct sockaddr_in *)res->ai_addr)->sin_addr;
  ESP_LOGI(TAG, "DNS lookup for %s is %s", m_server.c_str(), inet_ntoa(*addr));

  m_sock = socket(res->ai_family, res->ai_socktype, 0);
  if (m_sock < 0)
    {
    ESP_LOGE(TAG, "Failed to allocate socket");
    freeaddrinfo(res);
    return false;
    }

  if (connect(m_sock, res->ai_addr, res->ai_addrlen) != 0)
    {
    ESP_LOGE(TAG, "socket connect failed errno=%d", errno);
    close(m_sock); m_sock = -1;
    freeaddrinfo(res);
    return false;
    }

  ESP_LOGI(TAG, "Connected to OVMS Server V2 at %s",m_server.c_str());
  freeaddrinfo(res);

  return true;
  }

void OvmsServerV2::Disconnect()
  {
  if (m_sock >= 0)
    {
    ESP_LOGI(TAG, "Disconnected from OVMS Server V2");
    close(m_sock);
    m_sock = -1;
    }
  }

bool OvmsServerV2::Login()
  {
  char token[OVMS_PROTOCOL_V2_TOKENSIZE+1];

  for (int k=0;k<OVMS_PROTOCOL_V2_TOKENSIZE;k++)
    {
    token[k] = (char)cb64[esp_random()%64];
    }
  token[OVMS_PROTOCOL_V2_TOKENSIZE+1] = 0;
  m_token = std::string(token);

  uint8_t digest[MD5_SIZE];
  hmac_md5((uint8_t*) token, OVMS_PROTOCOL_V2_TOKENSIZE+1, (uint8_t*)m_password.c_str(), m_password.length(), digest);

  char hello[256] = "";
  strcat(hello,"MP-C 0 ");
  strcat(hello,token);
  strcat(hello," ");
  base64encode(digest, MD5_SIZE, (uint8_t*)(hello+strlen(hello)));
  strcat(hello," ");
  strcat(hello,m_vehicleid.c_str());
  ESP_LOGI(TAG, "Sending server login: %s",hello);
  strcat(hello,"\r\n");

  write(m_sock, hello, strlen(hello));

  // Wait 20 seconds for a server response
  PollForData(20000);

  if (m_buffer->HasLine())
    {
    std::string line = m_buffer->ReadLine();
    if (line.compare(0,7,"MP-S 0 ") == 0)
      {
      ESP_LOGI(TAG, "Got server response: %s",line.c_str());
      }
    }

  return false;
  }

void OvmsServerV2::PollForData(long timeoutms)
  {
  fd_set fds;

  FD_ZERO(&fds);
  FD_SET(m_sock,&fds);

  struct timeval timeout;
  timeout.tv_sec = timeoutms/1000;
  timeout.tv_usec = (timeoutms%1000)*1000;

  int result = select(m_sock, &fds, NULL, NULL, &timeout);
  if (result > 0)
    {
    // We have some data ready to read
    size_t avail = m_buffer->FreeSpace();
    uint8_t buf[avail];
    size_t n = read(m_sock, &buf, avail);
    if (n > 0) m_buffer->Push(buf,n);
    }
  }

std::string OvmsServerV2::ReadLine()
  {
  return std::string("");
  }

OvmsServerV2::OvmsServerV2(std::string name)
  : OvmsServer(name)
  {
  m_sock = -1;
  m_buffer = new OvmsBuffer(1024);
  }

OvmsServerV2::~OvmsServerV2()
  {
  if (m_sock>0)
    {
    close(m_sock);
    m_sock = -1;
    }
  if (m_buffer)
    {
    delete m_buffer;
    m_buffer = NULL;
    }
  }

void OvmsServerV2::SetPowerMode(PowerMode powermode)
  {
  m_powermode = powermode;
  switch (powermode)
    {
    case On:
      break;
    case Sleep:
      break;
    case DeepSleep:
      break;
    case Off:
      break;
    default:
      break;
    }
  }

void ovmsv2_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV2 == NULL)
    {
    writer->puts("Launching OVMS Server V2 connection (oscv2)");
    MyOvmsServerV2 = new OvmsServerV2("oscv2");
    }
  }

void ovmsv2_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV2 != NULL)
    {
    writer->puts("Stopping OVMS Server V2 connection (oscv2)");
    delete MyOvmsServerV2;
    MyOvmsServerV2 = NULL;
    }
  }

void ovmsv2_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  }

class OvmsServerV2Init
    {
    public: OvmsServerV2Init();
  } OvmsServerV2Init  __attribute__ ((init_priority (6100)));

OvmsServerV2Init::OvmsServerV2Init()
  {
  ESP_LOGI(TAG, "Initialising OVMS V2 Server (6100)");

  OvmsCommand* cmd_server = MyCommandApp.FindCommand("server");
  cmd_server->FindCommand("start")->RegisterCommand("v2","Start an OVMS V2 Server Connection",ovmsv2_start, "", 0, 0);
  cmd_server->FindCommand("stop")->RegisterCommand("v2","Stop an OVMS V2 Server Connection",ovmsv2_stop, "", 0, 0);
  cmd_server->FindCommand("status")->RegisterCommand("v2","Show OVMS V2 Server connection status",ovmsv2_status, "", 0, 0);

  MyConfig.RegisterParam("server.v2", "V2 Server Configuration", true, false);
  // Our instances:
  //   'server': The server name/ip
  //   'password': The server password
  //   'port': The port to connect to (default: 6867)
  // Also note:
  //  Parameter "vehicle", instance "id", is the vehicle ID
  }
