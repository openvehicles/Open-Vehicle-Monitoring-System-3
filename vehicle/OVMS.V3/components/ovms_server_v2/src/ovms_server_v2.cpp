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
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "crypt_base64.h"
#include "crypt_hmac.h"
#include "crypt_md5.h"
#include "crypt_crc.h"
#include "ovms_server_v2.h"
#include "esp_system.h"

OvmsServerV2 *MyOvmsServerV2 = NULL;
size_t MyOvmsServerV2Modifier = 0;

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

    StandardMetrics.ms_s_v2_connected->SetValue(true);
    while(1)
      {
      if (!m_conn.IsOpen())
        {
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        continue;
        }
      while ((m_buffer->HasLine() >= 0)&&(m_conn.IsOpen()))
        {
        ProcessServerMsg();
        }
      if (m_buffer->PollSocket(m_conn.Socket(),20000) < 0)
        {
        m_conn.Disconnect();
        }
      }
    }
  }

void OvmsServerV2::ProcessServerMsg()
  {
  std::string line = m_buffer->ReadLine();

  uint8_t b[line.length()+1];
  int len = base64decode(line.c_str(),b);

  RC4_crypt(&m_crypto_rx1, &m_crypto_rx2, b, len);
  b[len]=0;
  line = std::string((char*)b);
  ESP_LOGI(TAG, "Incoming Msg: %s",line.c_str());

  if (line.compare(0, 5, "MP-0 ") != 0)
    {
    ESP_LOGI(TAG, "Invalid server message. Disconnecting.");
    Disconnect();
    return;
    }

  char code = line[5];
  std::string payload = std::string(line,6,line.length()-6);

  switch(code)
    {
    case 'A': // PING
      {
      Transmit("MP-0 a");
      break;
      }
    case 'Z': // Peer connections
      {
      int nc = atoi(payload.c_str());
      StandardMetrics.ms_s_v2_peers->SetValue(nc);
      break;
      }
    case 'h': // Historical data acknowledgement
      {
      break;
      }
    case 'C': // Command
      {
      break;
      }
    default:
      break;
    }
  }

void OvmsServerV2::Transmit(std::string message)
  {
  int len = message.length();
  char s[len];
  memcpy(s,message.c_str(),len);
  char buf[(len*2)+4];

  RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, (uint8_t*)s, len);
  base64encode((uint8_t*)s, len, (uint8_t*)buf);
  ESP_LOGI(TAG, "Send %s",buf);
  strcat(buf,"\r\n");
  m_conn.Write(buf,strlen(buf));
  }

void OvmsServerV2::Transmit(const char* message)
  {
  int len = strlen(message);
  char s[len];
  memcpy(s,message,len);
  char buf[(len*2)+4];

  RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, (uint8_t*)s, len);
  base64encode((uint8_t*)s, len, (uint8_t*)buf);
  ESP_LOGI(TAG, "Send %s",buf);
  strcat(buf,"\r\n");
  m_conn.Write(buf,strlen(buf));
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

  m_conn.Connect(m_server.c_str(), m_port.c_str());
  if (!m_conn.IsOpen())
    {
    ESP_LOGE(TAG, "Socket connect failed errno=%d", errno);
    return false;
    }

  ESP_LOGI(TAG, "Connected to OVMS Server V2 at %s",m_server.c_str());
  return true;
  }

void OvmsServerV2::Disconnect()
  {
  if (m_conn.IsOpen())
    {
    m_conn.Disconnect();
    ESP_LOGI(TAG, "Disconnected from OVMS Server V2");
    }
  StandardMetrics.ms_s_v2_connected->SetValue(false);
  }

bool OvmsServerV2::Login()
  {
  char token[OVMS_PROTOCOL_V2_TOKENSIZE+1];

  for (int k=0;k<OVMS_PROTOCOL_V2_TOKENSIZE;k++)
    {
    token[k] = (char)cb64[esp_random()%64];
    }
  token[OVMS_PROTOCOL_V2_TOKENSIZE] = 0;
  m_token = std::string(token);

  uint8_t digest[MD5_SIZE];
  hmac_md5((uint8_t*) token, OVMS_PROTOCOL_V2_TOKENSIZE, (uint8_t*)m_password.c_str(), m_password.length(), digest);

  char hello[256] = "";
  strcat(hello,"MP-C 0 ");
  strcat(hello,token);
  strcat(hello," ");
  base64encode(digest, MD5_SIZE, (uint8_t*)(hello+strlen(hello)));
  strcat(hello," ");
  strcat(hello,m_vehicleid.c_str());
  ESP_LOGI(TAG, "Sending server login: %s",hello);
  strcat(hello,"\r\n");

  m_conn.Write(hello, strlen(hello));

  // Wait 20 seconds for a server response
  while (m_buffer->HasLine() < 0)
    {
    int result = m_buffer->PollSocket(m_conn.Socket(),20000);
    if (result <= 0)
      {
      ESP_LOGI(TAG, "Server response is incomplete (%d bytes)",m_buffer->UsedSpace());
      m_conn.Disconnect();
      return false;
      }
    }

  std::string line = m_buffer->ReadLine();
  ESP_LOGI(TAG, "Received welcome response %s",line.c_str());
  if (line.compare(0,7,"MP-S 0 ") == 0)
    {
    ESP_LOGI(TAG, "Got server response: %s",line.c_str());
    size_t sep = line.find(' ',7);
    if (sep == std::string::npos)
      {
      ESP_LOGI(TAG, "Server response invalid (no token/digest separator)");
      return false;
      }
    std::string token = std::string(line,7,sep-7);
    std::string digest = std::string(line,sep+1,line.length()-sep);
    ESP_LOGI(TAG, "Server token is %s and digest is %s",token.c_str(),digest.c_str());
    if (m_token == token)
      {
      ESP_LOGI(TAG, "Detected token replay attack/collision");
      return false;
      }
    uint8_t sdigest[MD5_SIZE];
    hmac_md5((uint8_t*) token.c_str(), token.length(), (uint8_t*)m_password.c_str(), m_password.length(), sdigest);
    uint8_t sdb[MD5_SIZE*2];
    base64encode(sdigest, MD5_SIZE, sdb);
    if (digest.compare((char*)sdb) != 0)
      {
      ESP_LOGI(TAG, "Server digest does not authenticate");
      return false;
      }
    ESP_LOGI(TAG, "Server authentication is successful. Prime the crypto...");
    std::string key(token);
    key.append(m_token);
    ESP_LOGI(TAG, "Shared secret key is %s (%d bytes)",key.c_str(),key.length());
    hmac_md5((uint8_t*)key.c_str(), key.length(), (uint8_t*)m_password.c_str(), m_password.length(), sdigest);
    RC4_setup(&m_crypto_rx1, &m_crypto_rx2, sdigest, MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t v = 0;
      RC4_crypt(&m_crypto_rx1, &m_crypto_rx2, &v, 1);
      }
    RC4_setup(&m_crypto_tx1, &m_crypto_tx2, sdigest, MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t v = 0;
      RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, &v, 1);
      }
    ESP_LOGI(TAG, "OVMS V2 login successful, and crypto channel established");
    return true;
    }
  else
    {
    ESP_LOGI(TAG, "Server response was not a welcome MP-S");
    return false;
    }
  }

void OvmsServerV2::TransmitMsgStat(bool always)
  {
  }

void OvmsServerV2::TransmitMsgGPS(bool always)
  {
  }

void OvmsServerV2::TransmitMsgTPMS(bool always)
  {
  bool modified = 
    StandardMetrics.ms_v_tpms_fl_t->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_fr_t->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_rl_t->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_rr_t->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_fl_p->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_fr_p->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_rl_p->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_tpms_rr_p->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  char* buffer = new char[512];
  buffer[0] = 0;
  strcat(buffer, "MP-0 W");
  strcat(buffer, StandardMetrics.ms_v_tpms_fr_p->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_fr_t->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_rr_p->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_rr_t->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_fl_p->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_fl_t->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_rl_p->AsString().c_str());
  strcat(buffer, ",");
  strcat(buffer, StandardMetrics.ms_v_tpms_rl_t->AsString().c_str());

  bool stale =
    StandardMetrics.ms_v_tpms_fl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_p->IsStale();

  strcat(buffer, ",");
  if (stale)
    strcat(buffer,"0");
  else
    strcat(buffer,"1");

  Transmit(buffer);
  delete [] buffer;
  }

void OvmsServerV2::TransmitMsgFirmware(bool always)
  {
  }

void OvmsServerV2::TransmitMsgEnvironment(bool always)
  {
  }

void OvmsServerV2::TransmitMsgCapabilities(bool always)
  {
  }

void OvmsServerV2::TransmitMsgGroup(bool always)
  {
  }

std::string OvmsServerV2::ReadLine()
  {
  return std::string("");
  }

OvmsServerV2::OvmsServerV2(std::string name)
  : OvmsServer(name)
  {
  if (MyOvmsServerV2Modifier == 0)
    {
    MyOvmsServerV2Modifier = MyMetrics.RegisterModifier();
    ESP_LOGI(TAG, "OVMS Server V2 registered metric modifier is #%d",MyOvmsServerV2Modifier);
    }
  m_buffer = new OvmsBuffer(1024);
  }

OvmsServerV2::~OvmsServerV2()
  {
  Disconnect();
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
  OvmsCommand* cmd_v2 = cmd_server->RegisterCommand("v2","OVMS Server V2 Protocol", NULL, "", 0, 0);
  cmd_v2->RegisterCommand("start","Start an OVMS V2 Server Connection",ovmsv2_start, "", 0, 0);
  cmd_v2->RegisterCommand("stop","Stop an OVMS V2 Server Connection",ovmsv2_stop, "", 0, 0);
  cmd_v2->RegisterCommand("status","Show OVMS V2 Server connection status",ovmsv2_status, "", 0, 0);

  MyConfig.RegisterParam("server.v2", "V2 Server Configuration", true, false);
  // Our instances:
  //   'server': The server name/ip
  //   'password': The server password
  //   'port': The port to connect to (default: 6867)
  // Also note:
  //  Parameter "vehicle", instance "id", is the vehicle ID
  }
