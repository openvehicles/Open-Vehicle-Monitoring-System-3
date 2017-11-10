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

#include "ovms_log.h"
static const char *TAG = "ovms-server-v2";

#include <string.h>
#include <sstream>
#include <iostream>
#include <iomanip>
#include "ovms_command.h"
#include "ovms_config.h"
#include "metrics_standard.h"
#include "crypt_base64.h"
#include "crypt_hmac.h"
#include "crypt_md5.h"
#include "crypt_crc.h"
#include "ovms_server_v2.h"
#include "ovms_netmanager.h"
#include "vehicle.h"
#include "esp_system.h"

// should this go in the .h or in the .cpp?
typedef union {
  struct {
  unsigned FrontLeftDoor:1;     // 0x01
  unsigned FrontRightDoor:1;    // 0x02
  unsigned ChargePort:1;        // 0x04
  unsigned PilotSignal:1;       // 0x08
  unsigned Charging:1;          // 0x10
  unsigned :1;                  // 0x20
  unsigned HandBrake:1;         // 0x40
  unsigned CarON:1;             // 0x80
  } bits;
  uint8_t flags;
} car_doors1_t;

typedef union {
  struct {
  unsigned :1;                  // 0x01
  unsigned :1;                  // 0x02
  unsigned :1;                  // 0x04
  unsigned CarLocked:1;         // 0x08
  unsigned ValetMode:1;         // 0x10
  unsigned Headlights:1;        // 0x20
  unsigned Bonnet:1;            // 0x40
  unsigned Trunk:1;             // 0x80
  } bits;
  uint8_t flags;
} car_doors2_t;

typedef union {
  struct {
  unsigned CarAwake:1;          // 0x01
  unsigned CoolingPump:1;       // 0x02
  unsigned :1;                  // 0x04
  unsigned :1;                  // 0x08
  unsigned :1;                  // 0x10
  unsigned :1;                  // 0x20
  unsigned CtrlLoggedIn:1;      // 0x40 - logged into controller
  unsigned CtrlCfgMode:1;       // 0x80 - controller in configuration mode
  } bits;
  uint8_t flags;
} car_doors3_t;

typedef union {
  struct {
  unsigned :1;                  // 0x01
  unsigned AlarmSounds:1;       // 0x02
  unsigned :1;                  // 0x04
  unsigned :1;                  // 0x08
  unsigned :1;                  // 0x10
  unsigned :1;                  // 0x20
  unsigned :1;                  // 0x40
  unsigned :1;                  // 0x80
  } bits;
  uint8_t flags;
} car_doors4_t;

typedef union {
  struct {
  unsigned RearLeftDoor:1;      // 0x01
  unsigned RearRightDoor:1;     // 0x02
  unsigned Frunk:1;             // 0x04
  unsigned :1;                  // 0x08
  unsigned Charging12V:1;       // 0x10
  unsigned :1;                  // 0x20
  unsigned :1;                  // 0x40
  unsigned HVAC:1;              // 0x80
  } bits;
  uint8_t flags;
} car_doors5_t;

#define PMAX_MAX 15
static struct
  {
  const char* param;
  const char* instance;
  } pmap[]
  =
  {
  { "vehicle",   "registered.phone" },     //  0 PARAM_REGPHONE
  { "password",  "module" },               //  1 PARAM_MODULEPASS
  { "vehicle",   "units.distance" },       //  2 PARAM_MILESKM
  { "",          "" },                     //  3 PARAM_NOTIFIES
  { "server.v2", "server" },               //  4 PARAM_SERVERIP
  { "modem",     "apn" },                  //  5 PARAM_GPRSAPN
  { "modem",     "apn.user" },             //  6 PARAM_GPRSUSER
  { "modem",     "apn.password" },         //  7 PARAM_GPRSPASS
  { "vehicle",   "id" },                   //  8 PARAM_VEHICLEID
  { "server.v2", "password" },             //  9 PARAM_SERVERPASS
  { "",          "" },                     // 10 PARAM_PARANOID
  { "",          "" },                     // 11 PARAM_S_GROUP1
  { "",          "" },                     // 12 PARAM_S_GROUP2
  { "",          "" },                     // 13 PARAM_GSMLOCK
  { "",          "" },                     // 14 PARAM_VEHICLETYPE
  { "",          "" }                      // 15 PARAM_COOLDOWN
  };

OvmsServerV2 *MyOvmsServerV2 = NULL;
size_t MyOvmsServerV2Modifier = 0;

void OvmsServerV2::ServerTask()
  {
  ESP_LOGI(TAG, "OVMS Server v2 task running");

  if (MyConfig.GetParamValue("vehicle", "units.distance").compare("M") == 0)
    m_units_distance = Miles;
  else
    m_units_distance = Kilometers;

  int lasttx = 0;
  MAINLOOP: while(1)
    {
    while (!MyNetManager.m_connected_any)
      {
      m_status = "Waiting for network availability";
      vTaskDelay(5000 / portTICK_PERIOD_MS);
      continue;
      }

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

    m_status = "Connected and logged in";
    StandardMetrics.ms_s_v2_connected->SetValue(true);
    while(1)
      {
      if (!m_conn.IsOpen())
        {
        // Loop until connection is open
        vTaskDelay(10000 / portTICK_PERIOD_MS);
        goto MAINLOOP;
        }

      // Handle incoming requests
      while ((m_buffer->HasLine() >= 0)&&(m_conn.IsOpen()))
        {
        int peers = StandardMetrics.ms_s_v2_peers->AsInt();
        ProcessServerMsg();
        if ((StandardMetrics.ms_s_v2_peers->AsInt() != peers)&&(peers==0))
          {
          ESP_LOGI(TAG, "One or more peers have connected");
          lasttx = 0; // A peer has connected, so force a transmission of status messages
          }
        }

      // Periodic transmission of metrics
      int now = StandardMetrics.ms_m_monotonic->AsInt();
      int next = (StandardMetrics.ms_s_v2_peers->AsInt()==0)?600:60;
      if ((lasttx==0)||(now>(lasttx+next)))
        {
        TransmitMsgStat(true);          // Send always, periodically
        TransmitMsgEnvironment(true);   // Send always, periodically
        TransmitMsgGPS(lasttx==0);
        TransmitMsgGroup(lasttx==0);
        TransmitMsgTPMS(lasttx==0);
        TransmitMsgFirmware(lasttx==0);
        TransmitMsgCapabilities(lasttx==0);
        lasttx = now;
        }

      if (m_now_stat) TransmitMsgStat();
      if (m_now_environment) TransmitMsgEnvironment();
      if (m_now_gps) TransmitMsgGPS();
      if (m_now_group) TransmitMsgGroup();
      if (m_now_tpms) TransmitMsgTPMS();
      if (m_now_firmware) TransmitMsgFirmware();
      if (m_now_capabilities) TransmitMsgCapabilities();

      // Poll for new data
      if ((m_buffer->PollSocket(m_conn.Socket(),1000) < 0)||
          (!MyNetManager.m_connected_any))
        {
        Disconnect();
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
      if (nc > 0)
        MyEvents.SignalEvent("app.connected",NULL);
      else
        MyEvents.SignalEvent("app.disconnected",NULL);
      break;
      }
    case 'h': // Historical data acknowledgement
      {
      break;
      }
    case 'C': // Command
      {
      ProcessCommand(&payload);
      break;
      }
    default:
      break;
    }
  }

void OvmsServerV2::ProcessCommand(std::string* payload)
  {
  int command = atoi(payload->c_str());
  size_t sep = payload->find(',');
  // std::string token = std::string(line,7,sep-7);

  OvmsVehicle* vehicle = MyVehicleFactory.ActiveVehicle();

  std::ostringstream buffer;
  switch (command)
    {
    case 1: // Request feature list
      {
      for (int k=0;k<16;k++)
        {
        buffer = std::ostringstream();
        buffer << "MP-0 c1,0," << k << ",16,0";
        Transmit(buffer.str());
        }
      break;
      }
    case 2: // Set feature
      {
      Transmit("MP-0 c2,1");
      break;
      }
    case 3: // Request parameter list
      {
      for (int k=0;k<32;k++)
        {
        buffer = std::ostringstream();
        buffer << "MP-0 c3,0," << k << ",32,";
        if ((k<PMAX_MAX)&&(pmap[k].param[0] != 0))
          {
          buffer << MyConfig.GetParamValue(pmap[k].param, pmap[k].instance);
          }
        Transmit(buffer.str());
        }
      break;
      }
    case 4: // Set parameter
      {
      if (sep != std::string::npos)
        {
        int k = atoi(payload->substr(sep+1).c_str());
        if ((k<PMAX_MAX)&&(pmap[k].param[0] != 0))
          {
          sep = payload->find(',',sep+1);
          MyConfig.SetParamValue(pmap[k].param, pmap[k].instance, payload->substr(sep+1));
          }
        }
      Transmit("MP-0 c4,0");
      break;
      }
    case 5: // Reboot
      {
      esp_restart();
      break;
      }
    case 10: // Set Charge Mode
      {
      int result = 1;
      if ((vehicle)&&(sep != std::string::npos))
        {
        if (vehicle->CommandSetChargeMode((OvmsVehicle::vehicle_mode_t)atoi(payload->substr(sep+1).c_str())) == OvmsVehicle::Success) result = 0;
        }
      buffer << "MP-0 c10," << result;
      Transmit(buffer.str());
      break;
      }
    case 11: // Start Charge
      {
      int result = 1;
      if (vehicle)
        {
        if (vehicle->CommandStartCharge() == OvmsVehicle::Success) result = 0;
        }
      buffer << "MP-0 c11," << result;
      Transmit(buffer.str());
      break;
      }
    case 12: // Stop Charge
      {
      int result = 1;
      if (vehicle)
        {
        if (vehicle->CommandStopCharge() == OvmsVehicle::Success) result = 0;
        }
      buffer << "MP-0 c12," << result;
      Transmit(buffer.str());
      break;
      }
    case 15: // Set Charge Current
      {
      int result = 1;
      if ((vehicle)&&(sep != std::string::npos))
        {
        if (vehicle->CommandSetChargeCurrent(atoi(payload->substr(sep+1).c_str())) == OvmsVehicle::Success) result = 0;
        }
      buffer << "MP-0 c15," << result;
      Transmit(buffer.str());
      break;
      }
    case 16: // Set Charge Mode and Current
      {
      int result = 1;
      if ((vehicle)&&(sep != std::string::npos))
        {
        OvmsVehicle::vehicle_mode_t mode = (OvmsVehicle::vehicle_mode_t)atoi(payload->substr(sep+1).c_str());
        sep = payload->find(',',sep+1);
        if (sep != std::string::npos)
          {
          if (vehicle->CommandSetChargeMode(mode) == OvmsVehicle::Success) result = 0;
          if ((result == 0)&&(vehicle->CommandSetChargeCurrent(atoi(payload->substr(sep+1).c_str())) != OvmsVehicle::Success))
            result = 1;
          }
        }
      buffer << "MP-0 c16," << result;
      Transmit(buffer.str());
      break;
      }
    case 18: // Wakeup Car
    case 19: // Wakeup Temperature Subsystem
      {
      int result = 1;
      if (vehicle)
        {
        if (vehicle->CommandWakeup() == OvmsVehicle::Success) result = 0;
        }
      buffer << "MP-0 c" << command << "," << result;
      Transmit(buffer.str());
      break;
      }
    case 6: // Charge alert
    case 7: // Execute command
    case 17: // Set Charge Timer Mode and Start Time
    case 20: // Lock Car
    case 21: // Activate Valet Mode
    case 22: // Unlock Car
    case 23: // Deactivate Valet Mode
    case 24: // Homelink
    case 25: // Cooldown
    case 30: // request GPRS utilisation data
    case 31: // Request historical data summary
    case 32: // Request historical data records
    case 40: // Send SMS
    case 41: // Send MMI/USSD codes
    case 49: // Send raw AT command
    default:
      buffer << "MP-0 c" << command << ",2";
      break;
    }
  }

void OvmsServerV2::Transmit(std::string message)
  {
  int len = message.length();
  char s[len];
  memcpy(s,message.c_str(),len);
  char buf[(len*2)+4];

  ESP_LOGI(TAG, "Send %s",message.c_str());

  RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, (uint8_t*)s, len);
  base64encode((uint8_t*)s, len, (uint8_t*)buf);
  strcat(buf,"\r\n");
  m_conn.Write(buf,strlen(buf));
  }

void OvmsServerV2::Transmit(const char* message)
  {
  int len = strlen(message);
  char s[len];
  memcpy(s,message,len);
  char buf[(len*2)+4];

  ESP_LOGI(TAG, "Send %s",message);

  RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, (uint8_t*)s, len);
  base64encode((uint8_t*)s, len, (uint8_t*)buf);
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
    m_status = "Error: Parameter vehicle/id must be defined";
    ESP_LOGW(TAG, "Parameter vehicle/id must be defined");
    return false;
    }
  if (m_server.empty())
    {
    m_status = "Error: Parameter server.v2/server must be defined";
    ESP_LOGW(TAG, "Parameter server.v2/server must be defined");
    return false;
    }
  if (m_password.empty())
    {
    m_status = "Error: Parameter server.v2/password must be defined";
    ESP_LOGW(TAG, "Parameter server.v2/password must be defined");
    return false;
    }

  m_conn.Connect(m_server.c_str(), m_port.c_str());
  if (!m_conn.IsOpen())
    {
    m_status = "Error: Cannot connect to server";
    ESP_LOGE(TAG, "Socket connect failed errno=%d", errno);
    return false;
    }

  m_status = "Connected to server";
  ESP_LOGI(TAG, "Connected to OVMS Server V2 at %s",m_server.c_str());
  return true;
  }

void OvmsServerV2::Disconnect()
  {
  if (m_conn.IsOpen())
    {
    m_conn.Disconnect();
    m_status = "Disconnected";
    ESP_LOGI(TAG, "Disconnected from OVMS Server V2");
    }
  StandardMetrics.ms_s_v2_connected->SetValue(false);
  }

bool OvmsServerV2::Login()
  {
  m_status = "Logging in...";

  char token[OVMS_PROTOCOL_V2_TOKENSIZE+1];

  for (int k=0;k<OVMS_PROTOCOL_V2_TOKENSIZE;k++)
    {
    token[k] = (char)cb64[esp_random()%64];
    }
  token[OVMS_PROTOCOL_V2_TOKENSIZE] = 0;
  m_token = std::string(token);

  uint8_t digest[OVMS_MD5_SIZE];
  hmac_md5((uint8_t*) token, OVMS_PROTOCOL_V2_TOKENSIZE, (uint8_t*)m_password.c_str(), m_password.length(), digest);

  char hello[256] = "";
  strcat(hello,"MP-C 0 ");
  strcat(hello,token);
  strcat(hello," ");
  base64encode(digest, OVMS_MD5_SIZE, (uint8_t*)(hello+strlen(hello)));
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
      m_status = "Error: Server response is incomplete";
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
      m_status = "Error: Server response is invalid";
      ESP_LOGI(TAG, "Server response invalid (no token/digest separator)");
      return false;
      }
    std::string token = std::string(line,7,sep-7);
    std::string digest = std::string(line,sep+1,line.length()-sep);
    ESP_LOGI(TAG, "Server token is %s and digest is %s",token.c_str(),digest.c_str());
    if (m_token == token)
      {
      m_status = "Error: Detected token replay attack/collision";
      ESP_LOGI(TAG, "Detected token replay attack/collision");
      return false;
      }
    uint8_t sdigest[OVMS_MD5_SIZE];
    hmac_md5((uint8_t*) token.c_str(), token.length(), (uint8_t*)m_password.c_str(), m_password.length(), sdigest);
    uint8_t sdb[OVMS_MD5_SIZE*2];
    base64encode(sdigest, OVMS_MD5_SIZE, sdb);
    if (digest.compare((char*)sdb) != 0)
      {
      m_status = "Error: Server digest does not authenticate";
      ESP_LOGI(TAG, "Server digest does not authenticate");
      return false;
      }
    m_status = "Login priming crypto...";
    ESP_LOGI(TAG, "Server authentication is successful. Prime the crypto...");
    std::string key(token);
    key.append(m_token);
    ESP_LOGI(TAG, "Shared secret key is %s (%d bytes)",key.c_str(),key.length());
    hmac_md5((uint8_t*)key.c_str(), key.length(), (uint8_t*)m_password.c_str(), m_password.length(), sdigest);
    RC4_setup(&m_crypto_rx1, &m_crypto_rx2, sdigest, OVMS_MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t v = 0;
      RC4_crypt(&m_crypto_rx1, &m_crypto_rx2, &v, 1);
      }
    RC4_setup(&m_crypto_tx1, &m_crypto_tx2, sdigest, OVMS_MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t v = 0;
      RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, &v, 1);
      }
    m_status = "Login successful...";
    ESP_LOGI(TAG, "OVMS V2 login successful, and crypto channel established");
    return true;
    }
  else
    {
    m_status = "Error: Server response was not a welcome MP-S";
    ESP_LOGI(TAG, "Server response was not a welcome MP-S");
    return false;
    }
  }

void OvmsServerV2::TransmitMsgStat(bool always)
  {
  m_now_stat = false;

  bool modified =
    StandardMetrics.ms_v_bat_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_current->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_state->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_substate->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_mode->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_range_ideal->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_range_est->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_climit->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_minutes->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_kwh->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_timermode->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_timerstart->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_cac->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_duration_full->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_duration_range->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_duration_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_inprogress->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_limit_range->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_limit_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_cooling->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_range_full->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_power->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_soh->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;
  
  int mins_range = StandardMetrics.ms_v_charge_duration_range->AsInt();
  int mins_soc = StandardMetrics.ms_v_charge_duration_soc->AsInt();
  bool charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
  
  std::ostringstream buffer;
  buffer
    << std::fixed
    << std::setprecision(2)
    << "MP-0 S"
    << StandardMetrics.ms_v_bat_soc->AsInt()
    << ","
    << ((m_units_distance == Kilometers) ? "K" : "M")
    << ","
    << StandardMetrics.ms_v_charge_voltage->AsInt()
    << ","
    << StandardMetrics.ms_v_charge_current->AsInt()
    << ","
    << StandardMetrics.ms_v_charge_state->AsString("stopped")
    << ","
    << StandardMetrics.ms_v_charge_mode->AsString("standard")
    << ","
    << StandardMetrics.ms_v_bat_range_ideal->AsInt(0, m_units_distance)
    << ","
    << StandardMetrics.ms_v_bat_range_est->AsInt(0, m_units_distance)
    << ","
    << StandardMetrics.ms_v_charge_climit->AsInt()
    << ","
    << StandardMetrics.ms_v_charge_minutes->AsInt()
    << ","
    << "0"  // car_charge_b4
    << ","
    << StandardMetrics.ms_v_charge_kwh->AsInt()
    << ","
    << chargesubstate_key(StandardMetrics.ms_v_charge_substate->AsString(""))
    << ","
    << chargestate_key(StandardMetrics.ms_v_charge_state->AsString("stopped"))
    << ","
    << chargemode_key(StandardMetrics.ms_v_charge_mode->AsString("standard"))
    << ","
    << StandardMetrics.ms_v_charge_timermode->AsBool()
    << ","
    << StandardMetrics.ms_v_charge_timerstart->AsInt()
    << ","
    << "0"  // car_stale_timer
    << ","
    << StandardMetrics.ms_v_bat_cac->AsFloat()
    << ","
    << StandardMetrics.ms_v_charge_duration_full->AsInt()
    << ","
    << (((mins_range >= 0) && (mins_range < mins_soc)) ? mins_range : mins_soc)
    << ","
    << (int) StandardMetrics.ms_v_charge_limit_range->AsFloat(0, m_units_distance)
    << ","
    << StandardMetrics.ms_v_charge_limit_soc->AsInt()
    << ","
    << (StandardMetrics.ms_v_env_cooling->AsBool() ? 0 : -1)
    << ","
    << "0"  // car_cooldown_tbattery
    << ","
    << "0"  // car_cooldown_timelimit
    << ","
    << "0"  // car_chargeestimate
    << ","
    << mins_range
    << ","
    << mins_soc
    << ","
    << StandardMetrics.ms_v_bat_range_full->AsInt(0, m_units_distance)
    << ","
    << "0"  // car_chargetype
    << ","
    << (charging ? -StandardMetrics.ms_v_bat_power->AsFloat() : 0)
    << ","
    << StandardMetrics.ms_v_bat_voltage->AsFloat()
    << ","
    << StandardMetrics.ms_v_bat_soh->AsInt()
    ;

  Transmit(buffer.str().c_str());
  }

void OvmsServerV2::TransmitMsgGPS(bool always)
  {
  m_now_gps = false;

  bool modified =
    StandardMetrics.ms_v_pos_latitude->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_pos_longitude->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_pos_direction->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_pos_altitude->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_pos_gpslock->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_pos_speed->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_drivemode->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_power->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_energy_used->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_energy_recd->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  bool stale =
    StandardMetrics.ms_v_pos_latitude->IsStale() ||
    StandardMetrics.ms_v_pos_longitude->IsStale() ||
    StandardMetrics.ms_v_pos_direction->IsStale() ||
    StandardMetrics.ms_v_pos_altitude->IsStale();

  std::string buffer;
  buffer.reserve(512);
  buffer.append("MP-0 L");
  buffer.append(StandardMetrics.ms_v_pos_latitude->AsString("0",Other,6).c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_pos_longitude->AsString("0",Other,6).c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_pos_direction->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_pos_altitude->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_pos_gpslock->AsString("0").c_str());
  if (stale)
    buffer.append(",0,");
  else
    buffer.append(",1,");
  if (m_units_distance == Kilometers)
    buffer.append(StandardMetrics.ms_v_pos_speed->AsString("0").c_str());
  else
    buffer.append(StandardMetrics.ms_v_pos_speed->AsString("0",Mph).c_str());
  buffer.append(",");
  char hex[10];
  sprintf(hex, "%x", StandardMetrics.ms_v_env_drivemode->AsInt());
  buffer.append(hex);
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_power->AsString("0",Other,1).c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_energy_used->AsString("0",Other,0).c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_energy_recd->AsString("0",Other,0).c_str());

  Transmit(buffer.c_str());
  }

void OvmsServerV2::TransmitMsgTPMS(bool always)
  {
  m_now_tpms = false;

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

  std::string buffer;
  buffer.reserve(512);
  buffer.append("MP-0 W");
  buffer.append(StandardMetrics.ms_v_tpms_fr_p->AsString("0",PSI));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_fr_t->AsString("0"));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_rr_p->AsString("0",PSI));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_rr_t->AsString("0"));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_fl_p->AsString("0",PSI));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_fl_t->AsString("0"));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_rl_p->AsString("0",PSI));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_tpms_rl_t->AsString("0"));

  bool stale =
    StandardMetrics.ms_v_tpms_fl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_p->IsStale();

  if (stale)
    buffer.append(",0");
  else
    buffer.append(",1");

  Transmit(buffer.c_str());
  }

void OvmsServerV2::TransmitMsgFirmware(bool always)
  {
  m_now_firmware = false;

  bool modified =
    StandardMetrics.ms_m_version->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_vin->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_m_net_sq->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_type->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_m_net_provider->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  std::string buffer;
  buffer.reserve(512);
  buffer.append("MP-0 F");
  buffer.append(StandardMetrics.ms_m_version->AsString(""));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_vin->AsString(""));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_m_net_sq->AsString("0",sq));
  buffer.append(",1,");
  buffer.append(StandardMetrics.ms_v_type->AsString(""));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_m_net_provider->AsString(""));

  Transmit(buffer.c_str());
  }

void AppendDoors1(std::string *buffer)
  {
  car_doors1_t car_doors1;
  car_doors1.bits.FrontLeftDoor = StandardMetrics.ms_v_door_fl->AsBool();
  car_doors1.bits.FrontRightDoor = StandardMetrics.ms_v_door_fr->AsBool();
  car_doors1.bits.ChargePort = StandardMetrics.ms_v_door_chargeport->AsBool();
  car_doors1.bits.PilotSignal = StandardMetrics.ms_v_charge_pilot->AsBool();
  car_doors1.bits.Charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
  car_doors1.bits.HandBrake = StandardMetrics.ms_v_env_handbrake->AsBool();
  car_doors1.bits.CarON = StandardMetrics.ms_v_env_on->AsBool();

  char b[5];
  itoa(car_doors1.flags, b, 10);
  buffer->append(b);
  }

void AppendDoors2(std::string *buffer)
  {
  car_doors2_t car_doors2;
  car_doors2.bits.CarLocked = StandardMetrics.ms_v_env_locked->AsBool();
  car_doors2.bits.ValetMode = StandardMetrics.ms_v_env_valet->AsBool();
  car_doors2.bits.Headlights = StandardMetrics.ms_v_env_headlights->AsBool();
  car_doors2.bits.Bonnet = StandardMetrics.ms_v_door_hood->AsBool();
  car_doors2.bits.Trunk = StandardMetrics.ms_v_door_trunk->AsBool();

  char b[5];
  itoa(car_doors2.flags, b, 10);
  buffer->append(b);
  }

void AppendDoors3(std::string *buffer)
  {
  car_doors3_t car_doors3;
  car_doors3.bits.CarAwake = StandardMetrics.ms_v_env_awake->AsBool();
  car_doors3.bits.CoolingPump = StandardMetrics.ms_v_env_cooling->AsBool();
  car_doors3.bits.CtrlLoggedIn = StandardMetrics.ms_v_env_ctrl_login->AsBool();
  car_doors3.bits.CtrlCfgMode = StandardMetrics.ms_v_env_ctrl_config->AsBool();

  char b[5];
  itoa(car_doors3.flags, b, 10);
  buffer->append(b);
  }

void AppendDoors4(std::string *buffer)
  {
  car_doors4_t car_doors4;
  car_doors4.bits.AlarmSounds = StandardMetrics.ms_v_env_alarm->AsBool();

  char b[5];
  itoa(car_doors4.flags, b, 10);
  buffer->append(b);
  }

void AppendDoors5(std::string *buffer)
  {
  car_doors5_t car_doors5;
  car_doors5.bits.RearLeftDoor = StandardMetrics.ms_v_door_rl->AsBool();
  car_doors5.bits.RearRightDoor = StandardMetrics.ms_v_door_rr->AsBool();
  car_doors5.bits.Frunk = false; // should this be hood or something else?
  car_doors5.bits.Charging12V = StandardMetrics.ms_v_env_charging12v->AsBool();
  car_doors5.bits.HVAC = StandardMetrics.ms_v_env_hvac->AsBool();

  char b[5];
  itoa(car_doors5.flags, b, 10);
  buffer->append(b);
  }

void OvmsServerV2::TransmitMsgEnvironment(bool always)
  {
  m_now_environment = false;

  bool modified =
    // doors 1
    StandardMetrics.ms_v_door_fl->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_door_fr->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_door_chargeport->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_pilot->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_charge_inprogress->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_handbrake->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_on->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    // doors 2
    StandardMetrics.ms_v_env_locked->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_valet->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_headlights->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_door_hood->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_door_trunk->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    // doors 3
    StandardMetrics.ms_v_env_awake->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_cooling->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_ctrl_login->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_ctrl_config->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    // doors 4
    StandardMetrics.ms_v_env_alarm->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    StandardMetrics.ms_v_inv_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_mot_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_bat_12v_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    // doors 5
    StandardMetrics.ms_v_door_rl->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_door_rr->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_charging12v->IsModifiedAndClear(MyOvmsServerV2Modifier) ||
    StandardMetrics.ms_v_env_hvac->IsModifiedAndClear(MyOvmsServerV2Modifier) ||

    StandardMetrics.ms_v_charge_temp->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  std::string buffer;
  buffer.reserve(512);
  buffer.append("MP-0 D");
  AppendDoors1(&buffer);
  buffer.append(",");
  AppendDoors2(&buffer);
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_env_locked->AsBool()?"4":"5");
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_inv_temp->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_mot_temp->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_temp->AsString("0").c_str());
  buffer.append(",");

  float v = StandardMetrics.ms_v_pos_trip->AsFloat(0, m_units_distance);
  std::ostringstream vs;
  vs << int(v*10);
  buffer.append(vs.str());
  buffer.append(",");

  v = StandardMetrics.ms_v_pos_odometer->AsFloat(0, m_units_distance);
  vs = std::ostringstream();
  vs << int(v*10);
  buffer.append(vs.str());
  buffer.append(",");

  buffer.append(StandardMetrics.ms_v_pos_speed->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_env_parktime->AsString("0"));
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_env_temp->AsString("0").c_str());
  buffer.append(",");
  AppendDoors3(&buffer);
  buffer.append(",");

  // v2 has one "stale" flag for 4 temperatures, we say they're stale only if
  // all are stale, IE one valid temperature makes them all valid
  bool stale_temps =
    StandardMetrics.ms_v_inv_temp->IsStale() &&
    StandardMetrics.ms_v_mot_temp->IsStale() &&
    StandardMetrics.ms_v_bat_temp->IsStale() &&
    StandardMetrics.ms_v_charge_temp->IsStale();
  buffer.append(stale_temps ? "0" : "1");
  buffer.append(",");

  buffer.append(StandardMetrics.ms_v_env_temp->IsStale() ? "0" : "1");
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_12v_voltage->AsString("0").c_str());
  buffer.append(",");
  AppendDoors4(&buffer);
  buffer.append(",");
  buffer.append("0");  // car_12vline_ref
  buffer.append(",");
  AppendDoors5(&buffer);
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_charge_temp->AsString("0").c_str());
  buffer.append(",");
  buffer.append(StandardMetrics.ms_v_bat_12v_current->AsString("0"));

  Transmit(buffer.c_str());
  }

void OvmsServerV2::MetricModified(OvmsMetric* metric)
  {
  // A metric has been changed...

  if ((metric == StandardMetrics.ms_v_charge_climit)||
      (metric == StandardMetrics.ms_v_charge_state)||
      (metric == StandardMetrics.ms_v_charge_substate)||
      (metric == StandardMetrics.ms_v_charge_mode)||
      (metric == StandardMetrics.ms_v_bat_cac))
    {
    m_now_environment = true;
    m_now_stat = true;
    }

  if (StandardMetrics.ms_s_v2_peers->AsInt() > 0)
    m_now_environment = true; // Transmit environment message if necessary
  }

void OvmsServerV2::TransmitMsgCapabilities(bool always)
  {
  m_now_capabilities = false;
  }

void OvmsServerV2::TransmitMsgGroup(bool always)
  {
  m_now_group = false;
  }

std::string OvmsServerV2::ReadLine()
  {
  return std::string("");
  }

OvmsServerV2::OvmsServerV2(const char* name)
  : OvmsServer(name)
  {
  if (MyOvmsServerV2Modifier == 0)
    {
    MyOvmsServerV2Modifier = MyMetrics.RegisterModifier();
    ESP_LOGI(TAG, "OVMS Server V2 registered metric modifier is #%d",MyOvmsServerV2Modifier);
    }
  m_buffer = new OvmsBuffer(1024);
  m_status = "Starting";
  m_now_stat = false;
  m_now_gps = false;
  m_now_tpms = false;
  m_now_firmware = false;
  m_now_environment = false;
  m_now_capabilities = false;
  m_now_group = false;

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsServerV2::MetricModified, this, _1));
  }

OvmsServerV2::~OvmsServerV2()
  {
  MyMetrics.DeregisterListener(TAG);
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
  if (MyOvmsServerV2 == NULL)
    {
    writer->puts("OVMS v2 server has not been started");
    }
  else
    {
    writer->puts(MyOvmsServerV2->m_status.c_str());
    }
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
