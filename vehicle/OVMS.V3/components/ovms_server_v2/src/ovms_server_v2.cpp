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

#include "ovms.h"
#include "buffered_shell.h"
#include "ovms_peripherals.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "metrics_standard.h"
#include "crypt_base64.h"
#include "crypt_hmac.h"
#include "crypt_crc.h"
#include "ovms_server_v2.h"
#include "ovms_netmanager.h"
#include "vehicle.h"
#include "esp_system.h"
#include "ovms_utils.h"
#include "ovms_boot.h"
#include "ovms_tls.h"

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

#define PMAX_MAX 24
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
  { "password", "server.v2" },             //  9 PARAM_SERVERPASS
  { "",          "" },                     // 10 PARAM_PARANOID
  { "",          "" },                     // 11 PARAM_S_GROUP1
  { "",          "" },                     // 12 PARAM_S_GROUP2
  { "",          "" },                     // 13 PARAM_GSMLOCK
  { "auto",      "vehicle.type" },         // 14 PARAM_VEHICLETYPE
  { "",          "" },                     // 15 PARAM_COOLDOWN
  { "",          "" },                     // 16 PARAM_ACC_1
  { "",          "" },                     // 17 PARAM_ACC_2
  { "",          "" },                     // 18 PARAM_ACC_3
  { "",          "" },                     // 19 PARAM_ACC_4
  { "",          "" },                     // 20
  { "",          "" },                     // 21
  { "network",   "dns" },                  // 22 PARAM_GPRSDNS
  { "vehicle",   "timezone" }              // 23 PARAM_TIMEZONE
  };

OvmsServerV2 *MyOvmsServerV2 = NULL;
size_t MyOvmsServerV2Modifier = 0;
size_t MyOvmsServerV2Reader = 0;

bool OvmsServerV2ReaderCallback(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  if (MyOvmsServerV2)
    return MyOvmsServerV2->IncomingNotification(type, entry);
  else
    return true; // No server v2 running, so just discard
  }

bool OvmsServerV2ReaderFilterCallback(OvmsNotifyType* type, const char* subtype)
  {
  if (MyOvmsServerV2)
    return MyOvmsServerV2->NotificationFilter(type, subtype);
  else
    return false;
  }

static void OvmsServerV2MongooseCallback(struct mg_connection *nc, int ev, void *p)
  {
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int *success = (int*)p;
      ESP_LOGV(TAG, "OvmsServerV2MongooseCallback(MG_EV_CONNECT=%d)",*success);
      if (*success == 0)
        {
        // Successful connection
        ESP_LOGI(TAG, "Connection successful");
        if (MyOvmsServerV2) MyOvmsServerV2->SendLogin(nc);
        }
      else
        {
        // Connection failed
        ESP_LOGW(TAG, "Connection failed");
        if (MyOvmsServerV2)
          {
          MyOvmsServerV2->SetStatus("Error: Connection failed", true, OvmsServerV2::WaitReconnect);
          MyOvmsServerV2->m_connretry = 60;
          }
        }
      }
      break;
    case MG_EV_CLOSE:
      ESP_LOGV(TAG, "OvmsServerV2MongooseCallback(MG_EV_CLOSE)");
      if (MyOvmsServerV2)
        {
        if (MyOvmsServerV2->m_state == OvmsServerV2::Authenticating)
          {
          MyOvmsServerV2->SetStatus("Authentication error (wrong ID/password)", true, OvmsServerV2::WaitReconnect);
          MyOvmsServerV2->Reconnect(120);
          }
        else
          {
          MyOvmsServerV2->SetStatus("Disconnected", false, OvmsServerV2::WaitReconnect);
          MyOvmsServerV2->Reconnect(60);
          }
        }
      break;
    case MG_EV_RECV:
      ESP_LOGV(TAG, "OvmsServerV2MongooseCallback(MG_EV_RECV)");
      if (MyOvmsServerV2)
        mbuf_remove(&nc->recv_mbuf, MyOvmsServerV2->IncomingData((uint8_t*)nc->recv_mbuf.buf, nc->recv_mbuf.len));
      break;
    default:
      break;
    }
  }

void OvmsServerV2::ProcessServerMsg()
  {
  m_lastrx_time = esp_log_timestamp();
  std::string line = m_buffer->ReadLine();

  if (line.compare(0,7,"MP-S 0 ") == 0)
    {
    ESP_LOGI(TAG, "Got server response: %s",line.c_str());
    size_t sep = line.find(' ',7);
    if (sep == std::string::npos)
      {
      SetStatus("Error: Server response invalid (no token/digest separator)", true, WaitReconnect);
      Reconnect(60);
      return;
      }
    std::string token = std::string(line,7,sep-7);
    std::string digest = std::string(line,sep+1,line.length()-sep);
    ESP_LOGI(TAG, "Server token is %s and digest is %s",token.c_str(),digest.c_str());
    if (m_token == token)
      {
      SetStatus("Error: Detected token replay attack/collision", true, WaitReconnect);
      Reconnect(60);
      m_connretry = 60; // Try again in 60 seconds...
      return;
      }
    uint8_t sdigest[OVMS_MD5_SIZE];
    hmac_md5((uint8_t*) token.c_str(), token.length(), (uint8_t*)m_password.c_str(), m_password.length(), sdigest);
    uint8_t sdb[OVMS_MD5_SIZE*2];
    base64encode(sdigest, OVMS_MD5_SIZE, sdb);
    if (digest.compare((char*)sdb) != 0)
      {
      SetStatus("Error: Server digest does not authenticate", true, WaitReconnect);
      Reconnect(60);
      return;
      }
    SetStatus("Server authentication ok. Now priming crypto.");
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

    if (m_paranoid)
      {
      // Paranoid mode welcome
      char token[OVMS_PROTOCOL_V2_TOKENSIZE+1];

      // Generate a random paranoid mode token
      for (int k=0;k<OVMS_PROTOCOL_V2_TOKENSIZE;k++)
        {
        token[k] = (char)cb64[esp_random()%64];
        }
      token[OVMS_PROTOCOL_V2_TOKENSIZE] = 0;
      m_ptoken = std::string(token);

      // Transmit the token (unencrypted) to the server
      m_ptoken_ready = false;
      std::string msg("MP-0 ET");
      msg.append(m_ptoken);
      Transmit(msg);
      m_ptoken_ready = true;

      // Generate, and store, the digest for future use
      std::string modpass = MyConfig.GetParamValue("password","module");
      hmac_md5((uint8_t*) token, OVMS_PROTOCOL_V2_TOKENSIZE, (uint8_t*)modpass.c_str(), modpass.length(), m_pdigest);
      }

    m_pending_notify_info = true;
    m_pending_notify_error = true;
    m_pending_notify_alert = true;
    m_pending_notify_data = true;
    m_pending_notify_data_last = 0;
    m_pending_notify_data_retransmit = 0;
    m_connretry = 0;

    StandardMetrics.ms_s_v2_connected->SetValue(true);
    if (m_paranoid)
      {
      SetStatus("OVMS V2 login successful, and crypto channel established (in paranoid mode)", false, Connected);
      }
    else
      {
      SetStatus("OVMS V2 login successful, and crypto channel established", false, Connected);
      }
    return;
    }

  uint8_t* b = new uint8_t[line.length()+1];
  int len = base64decode(line.c_str(),b);

  RC4_crypt(&m_crypto_rx1, &m_crypto_rx2, b, len);
  b[len]=0;
  line = std::string((char*)b);
  delete [] b;
  ESP_LOGI(TAG, "Incoming Msg: %s",line.c_str());

  if (line.compare(0, 5, "MP-0 ") != 0)
    {
    SetStatus("Error: Invalid server message. Disconnecting.", true, WaitReconnect);
    Reconnect(60);
    return;
    }

  if ((line.at(5) == 'E')&&(line.at(6) == 'M'))
    {
    char code[2]; code[0] = line.at(7); code[1] = 0;
    uint8_t *d = new uint8_t[line.length()-6];
    len = base64decode(line.c_str()+7,d+1);

    RC4_CTX1* pm_crypto1 = new RC4_CTX1;
    RC4_CTX2* pm_crypto2 = new RC4_CTX2;
    RC4_setup(pm_crypto1, pm_crypto2, m_pdigest, OVMS_MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t zero = 0;
      RC4_crypt(pm_crypto1, pm_crypto2, &zero, 1);
      }
    RC4_crypt(pm_crypto1, pm_crypto2, d, len);

    line.erase(5);
    line = std::string("MP-0 ");
    line.append(code);
    line.append((char*)d);
    len = line.length();

    delete pm_crypto1;
    delete pm_crypto2;
    delete d;
    ESP_LOGI(TAG, "Decoded Paranoid Msg: %s",line.c_str());
    }

  char code = line[5];
  const char* payload = line.c_str()+6;

  switch(code)
    {
    case 'A': // PING
      {
      Transmit(std::string("MP-0 a"));
      break;
      }
    case 'Z': // Peer connections
      {
      int nc = atoi(payload);
      StandardMetrics.ms_s_v2_peers->SetValue(nc);
      if (nc > m_peers)
        {
        ESP_LOGI(TAG, "One or more peers have connected");
        m_lasttx = 0; // A peer has connected, so force a transmission of status messages
        }
      if ((nc == 0)&&(m_peers != 0))
        MyEvents.SignalEvent("app.disconnected",NULL);
      else if (nc > 0)
        MyEvents.SignalEvent("app.connected",NULL);
      m_peers = nc;
      break;
      }
    case 'h': // Historical data acknowledgement
      {
      HandleNotifyDataAck(atoi(payload));
      break;
      }
    case 'C': // Command
      {
      ProcessCommand(payload);
      break;
      }
    default:
      break;
    }
  }

void OvmsServerV2::ProcessCommand(const char* payload)
  {
  int command = atoi(payload);
  char *sep = index(payload,',');
  int k = 0;

  OvmsVehicle* vehicle = MyVehicleFactory.ActiveVehicle();

  extram::ostringstream* buffer = new extram::ostringstream();

  if (vehicle)
    {
    // Ask vehicle to process command first:
    std::string rt;
    OvmsVehicle::vehicle_command_t vc = vehicle->ProcessMsgCommand(rt, command, sep ? sep+1 : NULL);
    if (vc != OvmsVehicle::NotImplemented)
      {
      *buffer << "MP-0 c" << command << "," << ((vc == OvmsVehicle::Success) ? 0 : 1) << "," << rt;
      k = 1;
      }
    }

  if (!k) switch (command)
    {
    case 1: // Request feature list
      // Notes:
      // - V2 only supported integer values, V3 values may be text
      // - V2 only supported 16 features, V3 supports 32
      {
      for (k=0;k<32;k++)
        {
        *buffer << "MP-0 c1,0," << k << ",32," << (vehicle ? vehicle->GetFeature(k) : "0");
        Transmit(buffer->str().c_str());
        buffer->str("");
        buffer->clear();
        }
      delete buffer;
      return;
      break;
      }
    case 2: // Set feature
      {
      int rc = 1;
      const char* rt = "";
      if (!vehicle)
        rt = "No active vehicle";
      else if (!sep)
        rt = "Missing feature key";
      else
        {
        k = atoi(sep+1);
        sep = index(sep+1,',');
        if (vehicle->SetFeature(k, sep ? sep+1 : ""))
          rc = 0;
        else
          rt = "Feature not supported by vehicle";
        }
      *buffer << "MP-0 c2," << rc << "," << rt;
      break;
      }
    case 3: // Request parameter list
      {
      for (k=0;k<32;k++)
        {
        *buffer << "MP-0 c3,0," << k << ",32,";
        if ((k<PMAX_MAX)&&(pmap[k].param[0] != 0))
          {
          *buffer << MyConfig.GetParamValue(pmap[k].param, pmap[k].instance);
          }
        Transmit(buffer->str().c_str());
        buffer->str("");
        buffer->clear();
        }
      delete buffer;
      return;
      break;
      }
    case 4: // Set parameter
      {
      int rc = 1;
      const char* rt = "";
      if (!sep)
        rt = "Missing parameter key";
      else
        {
        k = atoi(sep+1);
        if ((k<PMAX_MAX)&&(pmap[k].param[0] != 0))
          {
          sep = index(sep+1,',');
          MyConfig.SetParamValue(pmap[k].param, pmap[k].instance, sep ? sep+1 : "");
          rc = 0;
          }
        else
          rt = "Parameter key not supported";
        }
      *buffer << "MP-0 c4," << rc << "," << rt;
      break;
      }
    case 5: // Reboot
      {
      MyBoot.Restart();
      break;
      }
    case 6: // Charge alert
      MyNotify.NotifyCommand("info","charge.stopped","stat");
      *buffer << "MP-0 c6,0";
      break;
    case 7: // Execute command
      {
      BufferedShell* bs = new BufferedShell(false, COMMAND_RESULT_NORMAL);
      bs->SetSecure(true); // this is an authorized channel
      bs->ProcessChars(sep+1, strlen(sep)-1);
      bs->ProcessChar('\n');
      extram::string val; bs->Dump(val);
      delete bs;
      *buffer << "MP-0 c7,0,";
      *buffer << mp_encode(val);
      break;
      }
    case 10: // Set Charge Mode
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandSetChargeMode((OvmsVehicle::vehicle_mode_t)atoi(sep+1)) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c10," << k;
      break;
      }
    case 11: // Start Charge
      {
      k = 1;
      if (vehicle)
        {
        if (vehicle->CommandStartCharge() == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c11," << k;
      break;
      }
    case 12: // Stop Charge
      {
      k = 1;
      if (vehicle)
        {
        if (vehicle->CommandStopCharge() == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c12," << k;
      break;
      }
    case 15: // Set Charge Current
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandSetChargeCurrent(atoi(sep+1)) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c15," << k;
      break;
      }
    case 16: // Set Charge Mode and Current
      {
      k = 1;
      if (vehicle && sep)
        {
        OvmsVehicle::vehicle_mode_t mode = (OvmsVehicle::vehicle_mode_t)atoi(sep+1);
        sep = index(sep+1,',');
        if (sep)
          {
          if (vehicle->CommandSetChargeMode(mode) == OvmsVehicle::Success) k = 0;
          if (k == 0)
            {
            vTaskDelay(50 / portTICK_PERIOD_MS);
            if (vehicle->CommandSetChargeCurrent(atoi(sep+1)) != OvmsVehicle::Success)
              k = 1;
            }
          }
        }
      *buffer << "MP-0 c16," << k;
      break;
      }
    case 17: // Set Charge Timer Mode and Start Time
      {
      k = 1;
      if (vehicle && sep)
        {
        bool timermode = atoi(sep+1);
        sep = index(sep+1,',');
        if (sep)
          {
          if (vehicle->CommandSetChargeTimer(timermode,atoi(sep+1)) == OvmsVehicle::Success) k = 0;
          }
        }
      *buffer << "MP-0 c17," << k;
      break;
      }
    case 18: // Wakeup Car
    case 19: // Wakeup Temperature Subsystem
      {
      k = 1;
      if (vehicle)
        {
        if (vehicle->CommandWakeup() == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c" << command << "," << k;
      break;
      }
    case 20: // Lock Car
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandLock(sep+1) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c20," << k;
      break;
      }
    case 21: // Activate Valet Mode
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandActivateValet(sep+1) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c21," << k;
      break;
      }
    case 22: // Unlock Car
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandUnlock(sep+1) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c22," << k;
      break;
      }
    case 23: // Deactivate Valet Mode
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandDeactivateValet(sep+1) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c23," << k;
      break;
      }
    case 24: // Homelink
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandHomelink(atoi(sep+1)) == OvmsVehicle::Success) k = 0;
        }
      else if (vehicle)
        {
        if (vehicle->CommandHomelink(-1) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c24," << k;
      break;
      }
    case 25: // Cooldown
      {
      k = 1;
      if (vehicle)
        {
        if (vehicle->CommandCooldown(true) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c25," << k;
      break;
      }
    case 26: // Remote Climate Control
      {
      k = 1;
      if (vehicle && sep)
        {
        if (vehicle->CommandClimateControl(atoi(sep+1)) == OvmsVehicle::Success) k = 0;
        }
      *buffer << "MP-0 c26," << k;
      break;
      }
    case 40: // Send SMS
      *buffer << "MP-0 c" << command << ",2";
      break;
    case 41: // Send MMI/USSD codes
      if (!sep)
        *buffer << "MP-0 c" << command << ",1,No command";
      else
        {
#ifdef CONFIG_OVMS_COMP_MODEM
        *buffer << "AT+CUSD=1,\"" << sep+1 << "\",15\r\n";
        extram::string msg = buffer->str();
        buffer->str("");
        if (MyPeripherals->m_modem->txcmd(msg.c_str(), msg.length()))
          *buffer << "MP-0 c" << command << ",0";
        else
          *buffer << "MP-0 c" << command << ",1,Cannot send command";
#else // #ifdef CONFIG_OVMS_COMP_MODEM
        *buffer << "MP-0 c" << command << ",1,No modem";
#endif // #ifdef CONFIG_OVMS_COMP_MODEM
        }
      break;
    case 49: // Send raw AT command
      *buffer << "MP-0 c" << command << ",2";
      break;
    default:
      *buffer << "MP-0 c" << command << ",2";
      break;
    }

  Transmit(buffer->str().c_str());
  delete buffer;
  }

void OvmsServerV2::Transmit(const std::string& message)
  {
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (!m_mgconn)
    return;

  int len = message.length();
  char* s = new char[(len*2)+4];
  memcpy(s,message.c_str(),len);
  ESP_LOGI(TAG, "Send %s",message.c_str());

  if ((m_ptoken_ready)&&
      (s[5] != 'E')&&
      (s[5] != 'A')&&
      (s[5] != 'a')&&
      (s[5] != 'g')&&
      (s[5] != 'P'))
    {
    // We must convert the message to a paranoid one...
    // The message is of the form MP-0 X...
    // Where X is the code and ... is the (optional) data
    char code[2]; code[0] = s[5]; code[1] = 0;
    uint8_t* d = new uint8_t[len-6];
    memcpy(d,s+6,len-6);

    // Paranoid encrypt the message part of the transaction
    RC4_CTX1* pm_crypto1 = new RC4_CTX1;
    RC4_CTX2* pm_crypto2 = new RC4_CTX2;
    RC4_setup(pm_crypto1, pm_crypto2, m_pdigest, OVMS_MD5_SIZE);
    for (int k=0;k<1024;k++)
      {
      uint8_t zero = 0;
      RC4_crypt(pm_crypto1, pm_crypto2, &zero, 1);
      }
    RC4_crypt(pm_crypto1, pm_crypto2, d, len-6);

    strcpy(s,"MP-0 EM");
    strcat(s,code);
    base64encode(d, len-6, (uint8_t*)s+8);
    // The messdage is now in paranoid mode...
    delete d;
    delete pm_crypto1;
    delete pm_crypto2;
    }

  RC4_crypt(&m_crypto_tx1, &m_crypto_tx2, (uint8_t*)s, len);

  char* buf = new char[(len*2)+4];
  base64encode((uint8_t*)s, len, (uint8_t*)buf);
  strcat(buf,"\r\n");
  mg_send(m_mgconn, buf, strlen(buf));

  delete [] buf;
  delete [] s;
  }

void OvmsServerV2::SetStatus(const char* status, bool fault, State newstate)
  {
  if (fault)
    ESP_LOGE(TAG, "Status: %s", status);
  else
    ESP_LOGI(TAG, "Status: %s", status);
  m_status = status;
  if (newstate != Undefined)
    {
    m_state = newstate;
    switch (m_state)
      {
      case OvmsServerV2::WaitNetwork:
        MyEvents.SignalEvent("server.v2.waitnetwork", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::ConnectWait:
        MyEvents.SignalEvent("server.v2.connectwait", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::Connecting:
        MyEvents.SignalEvent("server.v2.connecting", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::Authenticating:
        MyEvents.SignalEvent("server.v2.authenticating", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::Connected:
        MyEvents.SignalEvent("server.v2.connected", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::Disconnected:
        MyEvents.SignalEvent("server.v2.disconnected", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV2::WaitReconnect:
        MyEvents.SignalEvent("server.v2.waitreconnect", (void*)m_status.c_str(), m_status.length()+1);
        break;
      default:
        break;
      }
    }
  }

void OvmsServerV2::Connect()
  {
  m_vehicleid = MyConfig.GetParamValue("vehicle", "id");
  m_server = MyConfig.GetParamValue("server.v2", "server");
  m_password = MyConfig.GetParamValue("password","server.v2");
  m_port = MyConfig.GetParamValue("server.v2", "port");
  m_tls = MyConfig.GetParamValueBool("server.v2", "tls",false);
  m_paranoid = MyConfig.GetParamValueBool("server.v2", "paranoid",false);

  if (m_port.empty()) m_port = (m_tls)?"6870":"6867";
  std::string address(m_server);
  address.append(":");
  address.append(m_port);

  ESP_LOGI(TAG, "Connection is %s:%s %s",
    m_server.c_str(), m_port.c_str(),
    m_vehicleid.c_str());

  if (m_vehicleid.empty())
    {
    SetStatus("Error: Parameter vehicle/id must be defined", true, WaitReconnect);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }
  if (m_server.empty())
    {
    SetStatus("Error: Parameter server.v2/server must be defined", true, WaitReconnect);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }
  if (m_password.empty())
    {
    SetStatus("Error: Parameter server.v2/password must be defined", true, WaitReconnect);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }

  SetStatus("Connecting...", false, Connecting);
  OvmsMutexLock mg(&m_mgconn_mutex);
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts;
  const char* err;
  memset(&opts, 0, sizeof(opts));
  opts.error_string = &err;
  if (m_tls)
    {
    opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList();
    opts.ssl_server_name = m_server.c_str();
    }
  if ((m_mgconn = mg_connect_opt(mgr, address.c_str(), OvmsServerV2MongooseCallback, opts)) == NULL)
    {
    ESP_LOGE(TAG, "mg_connect(%s) failed: %s", address.c_str(), err);
    SetStatus("Error: Connection failed", true, WaitReconnect);
    m_connretry = 60; // Try again in 60 seconds...
    return;
    }
  return;
  }

void OvmsServerV2::Disconnect()
  {
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    }
  m_buffer->EmptyAll();
  m_connretry = 0;
  StandardMetrics.ms_s_v2_connected->SetValue(false);
  StandardMetrics.ms_s_v2_peers->SetValue(0);
  }

void OvmsServerV2::Reconnect(int connretry)
  {
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    }
  m_buffer->EmptyAll();
  m_connretry = connretry;
  StandardMetrics.ms_s_v2_connected->SetValue(false);
  StandardMetrics.ms_s_v2_peers->SetValue(0);
  }

size_t OvmsServerV2::IncomingData(uint8_t* data, size_t len)
  {
  // MyCommandApp.HexDump(TAG, "IncomingData", (const char*)data, len);
  if (m_buffer->Push(data, len))
    {
    while ((m_buffer->HasLine()>=0)&&(m_mgconn))
      {
      // m_buffer->Diagnostics();
      ProcessServerMsg();
      }
    return len;
    }
  return 0;
  }

void OvmsServerV2::SendLogin(struct mg_connection *nc)
  {
  OvmsMutexLock mg(&m_mgconn_mutex);

  SetStatus("Logging in...", false, Authenticating);

  char token[OVMS_PROTOCOL_V2_TOKENSIZE+1];

  for (int k=0;k<OVMS_PROTOCOL_V2_TOKENSIZE;k++)
    {
    token[k] = (char)cb64[esp_random()%64];
    }
  token[OVMS_PROTOCOL_V2_TOKENSIZE] = 0;
  m_token = std::string(token);

  m_ptoken.clear();
  m_ptoken_ready = false;

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

  mg_send(nc, hello, strlen(hello));
  m_connretry = 60; // Give the server 60 seconds to respond
  }

void OvmsServerV2::TransmitMsgStat(bool always)
  {
  m_now_stat = false;

  bool modified =
    StandardMetrics.ms_v_bat_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_current->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_state->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_substate->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_mode->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_range_ideal->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_range_est->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_climit->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_timermode->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_timerstart->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_cac->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_duration_full->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_duration_range->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_duration_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_inprogress->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_limit_range->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_limit_soc->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_cooling->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_range_full->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_power->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_soh->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_power->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_efficiency->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  int mins_range = StandardMetrics.ms_v_charge_duration_range->AsInt();
  int mins_soc = StandardMetrics.ms_v_charge_duration_soc->AsInt();
  bool charging = StandardMetrics.ms_v_charge_inprogress->AsBool();

  extram::ostringstream buffer;
  buffer
    << std::fixed
    << std::setprecision(2)
    << "MP-0 S"
    << StandardMetrics.ms_v_bat_soc->AsString("0", Other, 1)
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
    << StandardMetrics.ms_v_charge_time->AsInt(0,Seconds)
    << ","
    << "0"  // car_charge_b4
    << ","
    << (int)(StandardMetrics.ms_v_charge_kwh->AsFloat() * 10)
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
    << ","
    << StandardMetrics.ms_v_charge_power->AsFloat()
    << ","
    << StandardMetrics.ms_v_charge_efficiency->AsFloat()
    ;

  Transmit(buffer.str().c_str());
  }

void OvmsServerV2::TransmitMsgGPS(bool always)
  {
  m_now_gps = false;

  bool modified =
    StandardMetrics.ms_v_pos_latitude->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_pos_longitude->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_pos_direction->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_pos_altitude->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_pos_gpslock->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_pos_speed->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_drivemode->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_power->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_energy_used->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_energy_recd->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_inv_power->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_inv_efficiency->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  bool stale =
    StandardMetrics.ms_v_pos_latitude->IsStale() ||
    StandardMetrics.ms_v_pos_longitude->IsStale() ||
    StandardMetrics.ms_v_pos_direction->IsStale() ||
    StandardMetrics.ms_v_pos_altitude->IsStale();

  char drivemode[10];
  sprintf(drivemode, "%x", StandardMetrics.ms_v_env_drivemode->AsInt());

  extram::ostringstream buffer;
  buffer
    << "MP-0 L"
    << StandardMetrics.ms_v_pos_latitude->AsString("0",Other,6)
    << ","
    << StandardMetrics.ms_v_pos_longitude->AsString("0",Other,6)
    << ","
    << StandardMetrics.ms_v_pos_direction->AsString("0")
    << ","
    << StandardMetrics.ms_v_pos_altitude->AsString("0")
    << ","
    << StandardMetrics.ms_v_pos_gpslock->AsBool(false)
    << ((stale)?",0,":",1,")
    << ((m_units_distance == Kilometers)? StandardMetrics.ms_v_pos_speed->AsString("0") : StandardMetrics.ms_v_pos_speed->AsString("0",Mph))
    << ","
    << int(StandardMetrics.ms_v_pos_trip->AsFloat(0, m_units_distance)*10)
    << ","
    << drivemode
    << ","
    << StandardMetrics.ms_v_bat_power->AsString("0",Other,3)
    << ","
    << StandardMetrics.ms_v_bat_energy_used->AsString("0",Other,3)
    << ","
    << StandardMetrics.ms_v_bat_energy_recd->AsString("0",Other,3)
    << ","
    << StandardMetrics.ms_v_inv_power->AsFloat()
    << ","
    << StandardMetrics.ms_v_inv_efficiency->AsFloat()
    ;

  Transmit(buffer.str().c_str());
  }

void OvmsServerV2::TransmitMsgTPMS(bool always)
  {
  m_now_tpms = false;

  bool modified =
    StandardMetrics.ms_v_tpms_fl_t->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_fr_t->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_rl_t->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_rr_t->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_fl_p->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_fr_p->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_rl_p->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_tpms_rr_p->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  bool stale =
    StandardMetrics.ms_v_tpms_fl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_t->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_t->IsStale() ||
    StandardMetrics.ms_v_tpms_fl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_fr_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rl_p->IsStale() ||
    StandardMetrics.ms_v_tpms_rr_p->IsStale();

  bool defined =
    StandardMetrics.ms_v_tpms_fl_p->IsDefined() ||
    StandardMetrics.ms_v_tpms_fr_p->IsDefined() ||
    StandardMetrics.ms_v_tpms_rl_p->IsDefined() ||
    StandardMetrics.ms_v_tpms_rr_p->IsDefined();

  int defstale;
  if (!defined)
    { defstale = -1; }
  else if (stale)
    { defstale = 0; }
  else
    { defstale = 1; }

  extram::ostringstream buffer;
  buffer
    << "MP-0 W"
    << StandardMetrics.ms_v_tpms_fr_p->AsString("0",PSI)
    << ","
    << StandardMetrics.ms_v_tpms_fr_t->AsString("0")
    << ","
    << StandardMetrics.ms_v_tpms_rr_p->AsString("0",PSI)
    << ","
    << StandardMetrics.ms_v_tpms_rr_t->AsString("0")
    << ","
    << StandardMetrics.ms_v_tpms_fl_p->AsString("0",PSI)
    << ","
    << StandardMetrics.ms_v_tpms_fl_t->AsString("0")
    << ","
    << StandardMetrics.ms_v_tpms_rl_p->AsString("0",PSI)
    << ","
    << StandardMetrics.ms_v_tpms_rl_t->AsString("0")
    << ","
    << defstale
    ;

  Transmit(buffer.str().c_str());
  }

void OvmsServerV2::TransmitMsgFirmware(bool always)
  {
  m_now_firmware = false;

  bool modified =
    StandardMetrics.ms_m_version->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_vin->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_m_net_sq->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_type->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_m_net_provider->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  extram::ostringstream buffer;
  buffer
    << "MP-0 F"
    << StandardMetrics.ms_m_version->AsString("")
    << ","
    << StandardMetrics.ms_v_vin->AsString("")
    << ","
    << StandardMetrics.ms_m_net_sq->AsString("0",sq)
    << ",1,"
    << StandardMetrics.ms_v_type->AsString("")
    << ","
    << StandardMetrics.ms_m_net_provider->AsString("")
    ;

  Transmit(buffer.str().c_str());
  }

uint8_t Doors1()
  {
  car_doors1_t car_doors1;
  car_doors1.bits.FrontLeftDoor = StandardMetrics.ms_v_door_fl->AsBool();
  car_doors1.bits.FrontRightDoor = StandardMetrics.ms_v_door_fr->AsBool();
  car_doors1.bits.ChargePort = StandardMetrics.ms_v_door_chargeport->AsBool();
  car_doors1.bits.PilotSignal = StandardMetrics.ms_v_charge_pilot->AsBool();
  car_doors1.bits.Charging = StandardMetrics.ms_v_charge_inprogress->AsBool();
  car_doors1.bits.HandBrake = StandardMetrics.ms_v_env_handbrake->AsBool();
  car_doors1.bits.CarON = StandardMetrics.ms_v_env_on->AsBool();

  return car_doors1.flags;
  }

uint8_t Doors2()
  {
  car_doors2_t car_doors2;
  car_doors2.bits.CarLocked = StandardMetrics.ms_v_env_locked->AsBool();
  car_doors2.bits.ValetMode = StandardMetrics.ms_v_env_valet->AsBool();
  car_doors2.bits.Headlights = StandardMetrics.ms_v_env_headlights->AsBool();
  car_doors2.bits.Bonnet = StandardMetrics.ms_v_door_hood->AsBool();
  car_doors2.bits.Trunk = StandardMetrics.ms_v_door_trunk->AsBool();

  return car_doors2.flags;
  }

uint8_t Doors3()
  {
  car_doors3_t car_doors3;
  car_doors3.bits.CarAwake = StandardMetrics.ms_v_env_awake->AsBool();
  car_doors3.bits.CoolingPump = StandardMetrics.ms_v_env_cooling->AsBool();
  car_doors3.bits.CtrlLoggedIn = StandardMetrics.ms_v_env_ctrl_login->AsBool();
  car_doors3.bits.CtrlCfgMode = StandardMetrics.ms_v_env_ctrl_config->AsBool();

  return car_doors3.flags;
  }

uint8_t Doors4()
  {
  car_doors4_t car_doors4;
  car_doors4.bits.AlarmSounds = StandardMetrics.ms_v_env_alarm->AsBool();

  return car_doors4.flags;
  }

uint8_t Doors5()
  {
  car_doors5_t car_doors5;
  car_doors5.bits.RearLeftDoor = StandardMetrics.ms_v_door_rl->AsBool();
  car_doors5.bits.RearRightDoor = StandardMetrics.ms_v_door_rr->AsBool();
  car_doors5.bits.Frunk = false; // should this be hood or something else?
  car_doors5.bits.Charging12V = StandardMetrics.ms_v_env_charging12v->AsBool();
  car_doors5.bits.HVAC = StandardMetrics.ms_v_env_hvac->AsBool();

  return car_doors5.flags;
  }

void OvmsServerV2::TransmitMsgEnvironment(bool always)
  {
  m_now_environment = false;

  bool modified =
    // doors 1
    StandardMetrics.ms_v_door_fl->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_door_fr->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_door_chargeport->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_pilot->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_charge_inprogress->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_handbrake->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_on->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    // doors 2
    StandardMetrics.ms_v_env_locked->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_valet->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_headlights->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_door_hood->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_door_trunk->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    // doors 3
    StandardMetrics.ms_v_env_awake->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_cooling->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_ctrl_login->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_ctrl_config->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    // doors 4
    StandardMetrics.ms_v_env_alarm->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    StandardMetrics.ms_v_inv_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_mot_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_bat_12v_voltage->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    // doors 5
    StandardMetrics.ms_v_door_rl->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_door_rr->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_charging12v->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_hvac->IsModifiedAndClear(MyOvmsServerV2Modifier) |

    StandardMetrics.ms_v_charge_temp->IsModifiedAndClear(MyOvmsServerV2Modifier) |
    StandardMetrics.ms_v_env_cabintemp->IsModifiedAndClear(MyOvmsServerV2Modifier);

  // Quick exit if nothing modified
  if ((!always)&&(!modified)) return;

  // v2 has one "stale" flag for all temperatures, we say they're stale only if
  // all are stale, IE one valid temperature makes them all valid
  bool stale_temps =
    StandardMetrics.ms_v_inv_temp->IsStale() &&
    StandardMetrics.ms_v_mot_temp->IsStale() &&
    StandardMetrics.ms_v_bat_temp->IsStale() &&
    StandardMetrics.ms_v_charge_temp->IsStale() &&
    StandardMetrics.ms_v_env_temp->IsStale() &&
    StandardMetrics.ms_v_env_cabintemp->IsStale();

  extram::ostringstream buffer;
  buffer
    << "MP-0 D"
    << (int)Doors1()
    << ","
    << (int)Doors2()
    << ","
    << (StandardMetrics.ms_v_env_locked->AsBool()?"4":"5")
    << ","
    << StandardMetrics.ms_v_inv_temp->AsString("0")
    << ","
    << StandardMetrics.ms_v_mot_temp->AsString("0")
    << ","
    << StandardMetrics.ms_v_bat_temp->AsString("0")
    << ","
    << int(StandardMetrics.ms_v_pos_trip->AsFloat(0, m_units_distance)*10)
    << ","
    << int(StandardMetrics.ms_v_pos_odometer->AsFloat(0, m_units_distance)*10)
    << ","
    << StandardMetrics.ms_v_pos_speed->AsString("0")
    << ","
    << StandardMetrics.ms_v_env_parktime->AsString("0")
    << ","
    << StandardMetrics.ms_v_env_temp->AsString("0")
    << ","
    << (int)Doors3()
    << ","
    << (stale_temps ? "0" : "1")
    << ","
    << (StandardMetrics.ms_v_env_temp->IsStale() ? "0" : "1")
    << ","
    << StandardMetrics.ms_v_bat_12v_voltage->AsString("0")
    << ","
    << (int)Doors4()
    << ","
    << StandardMetrics.ms_v_bat_12v_voltage_ref->AsString("0")
    << ","
    << (int)Doors5()
    << ","
    << StandardMetrics.ms_v_charge_temp->AsString("0")
    << ","
    << StandardMetrics.ms_v_bat_12v_current->AsString("0")
    << ","
    << StandardMetrics.ms_v_env_cabintemp->AsString("0")
    ;

  Transmit(buffer.str().c_str());
  }

void OvmsServerV2::TransmitMsgCapabilities(bool always)
  {
  m_now_capabilities = false;
  }

void OvmsServerV2::TransmitMsgGroup(bool always)
  {
  m_now_group = false;
  }

void OvmsServerV2::TransmitNotifyInfo()
  {
  m_pending_notify_info = false;

  // Find the type object
  OvmsNotifyType* info = MyNotify.GetType("info");
  if (info == NULL) return;

  while(1)
    {
    // Find the first entry
    OvmsNotifyEntry* e = info->FirstUnreadEntry(MyOvmsServerV2Reader, 0);
    if (e == NULL) return;

    extram::ostringstream buffer;
    buffer
      << "MP-0 PI"
      << mp_encode(e->GetValue());
    Transmit(buffer.str().c_str());

    info->MarkRead(MyOvmsServerV2Reader, e);
    }
  }

void OvmsServerV2::TransmitNotifyError()
  {
  m_pending_notify_error = false;

  // Find the type object
  OvmsNotifyType* alert = MyNotify.GetType("error");
  if (alert == NULL) return;

  while(1)
    {
    // Find the first entry
    OvmsNotifyEntry* e = alert->FirstUnreadEntry(MyOvmsServerV2Reader, 0);
    if (e == NULL) return;

    extram::ostringstream buffer;
    buffer
      << "MP-0 PE"
      << e->GetValue(); // no mp_encode; payload structure "<vehicletype>,<errorcode>,<errordata>"
    Transmit(buffer.str().c_str());

    alert->MarkRead(MyOvmsServerV2Reader, e);
    }
  }

void OvmsServerV2::TransmitNotifyAlert()
  {
  m_pending_notify_alert = false;

  // Find the type object
  OvmsNotifyType* alert = MyNotify.GetType("alert");
  if (alert == NULL) return;

  while(1)
    {
    // Find the first entry
    OvmsNotifyEntry* e = alert->FirstUnreadEntry(MyOvmsServerV2Reader, 0);
    if (e == NULL) return;

    extram::ostringstream buffer;
    buffer
      << "MP-0 PA"
      << mp_encode(e->GetValue());
    Transmit(buffer.str().c_str());

    alert->MarkRead(MyOvmsServerV2Reader, e);
    }
  }

void OvmsServerV2::TransmitNotifyData()
  {
  // Find the type object
  OvmsNotifyType* data = MyNotify.GetType("data");
  if (data == NULL) return;

  uint32_t starttime = esp_log_timestamp();
  int cnt = 0;
  size_t size = 0;

  while(1)
    {
    // Find the first entry
    OvmsNotifyEntry* e = data->FirstUnreadEntry(MyOvmsServerV2Reader, m_pending_notify_data_last);
    if (e == NULL)
      {
      m_pending_notify_data = false;
      // if we have sent something, check for retransmissions in 10 seconds:
      if (m_pending_notify_data_last)
        m_pending_notify_data_retransmit = 10;
      return;
      }

    extram::string msg = e->GetValue();
    ESP_LOGD(TAG, "TransmitNotifyData: msg=%s", msg.c_str());

    // terminate payload at first LF:
    size_t eol = msg.find('\n');
    if (eol != std::string::npos)
      msg.resize(eol);

    uint32_t now = esp_log_timestamp();
    extram::ostringstream buffer;
    buffer
      << "MP-0 h"
      << e->m_id
      << ","
      << -((int)(now - e->m_created) / 1000)
      << ","
      << msg;
    Transmit(buffer.str().c_str());
    m_pending_notify_data_last = e->m_id;

    // be nice to other tasks, the network & the server:
    // limits per second: 300 ms / 5 transmissions / 4000 bytes payload
    cnt++;
    size += buffer.str().size();
    if (now - starttime >= 300 || cnt == 5 || size >= 4000)
      {
      ESP_LOGD(TAG, "TransmitNotifyData: used %d ms for %d records, %u bytes", now - starttime, cnt, size);
      return;
      }
    }
  }

void OvmsServerV2::HandleNotifyDataAck(uint32_t ack)
  {
  OvmsNotifyType* data = MyNotify.GetType("data");
  if (data == NULL) return;

  OvmsNotifyEntry* e = data->FindEntry(ack);
  if (e)
    {
    data->MarkRead(MyOvmsServerV2Reader, e);
    }
  }

void OvmsServerV2::MetricModified(OvmsMetric* metric)
  {
  // A metric has been changed: if peers are connected,
  //  check for important changes that should be transmitted ASAP:

  if (StandardMetrics.ms_s_v2_peers->AsInt() == 0)
    return;

  if ((metric == StandardMetrics.ms_v_charge_climit)||
      (metric == StandardMetrics.ms_v_charge_limit_range)||
      (metric == StandardMetrics.ms_v_charge_limit_soc)||
      (metric == StandardMetrics.ms_v_charge_state)||
      (metric == StandardMetrics.ms_v_charge_substate)||
      (metric == StandardMetrics.ms_v_charge_mode)||
      (metric == StandardMetrics.ms_v_charge_inprogress)||
      (metric == StandardMetrics.ms_v_env_cooling)||
      (metric == StandardMetrics.ms_v_bat_cac)||
      (metric == StandardMetrics.ms_v_bat_soh))
    {
    m_now_stat = true;
    }

  if ((metric == StandardMetrics.ms_v_door_fl)||
      (metric == StandardMetrics.ms_v_door_fr)||
      (metric == StandardMetrics.ms_v_door_chargeport)||
      (metric == StandardMetrics.ms_v_charge_pilot)||
      (metric == StandardMetrics.ms_v_charge_inprogress)||
      (metric == StandardMetrics.ms_v_env_handbrake)||
      (metric == StandardMetrics.ms_v_env_on)||
      (metric == StandardMetrics.ms_v_env_locked)||
      (metric == StandardMetrics.ms_v_env_valet)||
      (metric == StandardMetrics.ms_v_door_hood)||
      (metric == StandardMetrics.ms_v_door_trunk)||
      (metric == StandardMetrics.ms_v_env_awake)||
      (metric == StandardMetrics.ms_v_env_cooling)||
      (metric == StandardMetrics.ms_v_env_alarm)||
      (metric == StandardMetrics.ms_v_door_rl)||
      (metric == StandardMetrics.ms_v_door_rr)||
      (metric == StandardMetrics.ms_v_env_charging12v)||
      (metric == StandardMetrics.ms_v_env_hvac))
    {
    m_now_environment = true;
    }

  if ((metric == StandardMetrics.ms_v_env_drivemode)||
      (metric == StandardMetrics.ms_v_pos_gpslock))
    {
    m_now_gps = true;
    }
  }

bool OvmsServerV2::NotificationFilter(OvmsNotifyType* type, const char* subtype)
  {
  if (strcmp(type->m_name, "info") == 0 ||
      strcmp(type->m_name, "error") == 0 ||
      strcmp(type->m_name, "alert") == 0 ||
      strcmp(type->m_name, "data") == 0)
    return true;
  else
    return false;
  }

bool OvmsServerV2::IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  if (strcmp(type->m_name,"info")==0)
    {
    // Info notifications
    if (!StandardMetrics.ms_s_v2_connected->AsBool())
      {
      m_pending_notify_info = true;
      return false; // No connection, so leave it queued for when we do
      }
    extram::ostringstream buffer;
    buffer
      << "MP-0 PI"
      << mp_encode(entry->GetValue());
    Transmit(buffer.str().c_str());
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"error")==0)
    {
    // Error notification
    if (!StandardMetrics.ms_s_v2_connected->AsBool())
      {
      m_pending_notify_error = true;
      return false; // No connection, so leave it queued for when we do
      }
    extram::ostringstream buffer;
    buffer
      << "MP-0 PE"
      << entry->GetValue(); // no mp_encode; payload structure "<vehicletype>,<errorcode>,<errordata>"
    Transmit(buffer.str().c_str());
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"alert")==0)
    {
    // Alert notifications
    if (!StandardMetrics.ms_s_v2_connected->AsBool())
      {
      m_pending_notify_alert = true;
      return false; // No connection, so leave it queued for when we do
      }
    extram::ostringstream buffer;
    buffer
      << "MP-0 PA"
      << mp_encode(entry->GetValue());
    Transmit(buffer.str().c_str());
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"data")==0)
    {
    // Data notifications
    m_pending_notify_data = true;
    return false; // We just flag it for later transmission
    }
  else
    return true; // Mark it read, as no interest to us
  }

/**
 * EventListener:
 */
void OvmsServerV2::EventListener(std::string event, void* data)
  {
  if (event == "system.modem.received.ussd")
    {
    // forward USSD response to server:
    std::string buf = "MP-0 c41,0,";
    buf.append(mp_encode(std::string((char*) data)));
    Transmit(buf);
    }
  else if (event == "config.changed" || event == "config.mounted")
    {
    ConfigChanged((OvmsConfigParam*) data);
    }
  }

/**
 * ConfigChanged: read new configuration
 *  - param: NULL = read all configurations
 */
void OvmsServerV2::ConfigChanged(OvmsConfigParam* param)
  {
  m_streaming = MyConfig.GetParamValueInt("vehicle", "stream", 0);
  m_updatetime_connected = MyConfig.GetParamValueInt("server.v2", "updatetime.connected", 60);
  m_updatetime_idle = MyConfig.GetParamValueInt("server.v2", "updatetime.idle", 600);
  }

void OvmsServerV2::NetUp(std::string event, void* data)
  {
  // workaround for wifi AP mode startup (manager up before interface)
  if ( (m_mgconn == NULL) && MyNetManager.MongooseRunning() )
    {
    SetStatus("Network is up, so attempt network connection", false, ConnectWait);
    m_connretry = 2;
    }
  }

void OvmsServerV2::NetDown(std::string event, void* data)
  {
  }

void OvmsServerV2::NetReconfigured(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Network was reconfigured: disconnect, and reconnect in 10 seconds");
  SetStatus("Network was reconfigured: disconnect, and reconnect in 10 seconds", false, ConnectWait);
  Reconnect(10);
  }

void OvmsServerV2::NetmanInit(std::string event, void* data)
  {
  if ((m_mgconn == NULL)&&(MyNetManager.m_connected_any))
    {
    SetStatus("Network is up, so attempt network connection", false, ConnectWait);
    m_connretry = 2;
    }
  }

void OvmsServerV2::NetmanStop(std::string event, void* data)
  {
  if (m_mgconn)
    {
    SetStatus("Network is down, so disconnect network connection", false, WaitNetwork);
    Disconnect();
    }
  }

void OvmsServerV2::Ticker1(std::string event, void* data)
  {
  if (m_connretry > 0)
    {
    if (MyNetManager.m_connected_any)
      {
      m_connretry--;
      if (m_connretry == 0)
        {
        if (m_mgconn) Disconnect(); // Disconnect first (timeout)
        Connect(); // Kick off the connection
        }
      }
    }

  if ((!MyNetManager.m_connected_any) && (m_state != WaitNetwork))
    {
    m_connretry = 0;
    SetStatus("Server is ready to start", false, WaitNetwork);
    }

  if (StandardMetrics.ms_s_v2_connected->AsBool())
    {
    // check for issue #241 condition:
    if (esp_log_timestamp() - m_lastrx_time > MyConfig.GetParamValueInt("server.v2", "timeout.rx", 960) * 1000)
      {
      ESP_LOGW(TAG, "Detected stale connection (issue #241), restarting network");
      MyNetManager.RestartNetwork();
      return;
      }

    // Periodic transmission of metrics
    bool caron = StandardMetrics.ms_v_env_on->AsBool();
    int now = StandardMetrics.ms_m_monotonic->AsInt();
    int next = (m_peers==0) ? m_updatetime_idle : m_updatetime_connected;
    if ((m_lasttx==0)||(now>(m_lasttx+next)))
      {
      TransmitMsgStat(true);          // Send always, periodically
      TransmitMsgEnvironment(true);   // Send always, periodically
      TransmitMsgGPS(m_lasttx==0);
      TransmitMsgGroup(m_lasttx==0);
      TransmitMsgTPMS(m_lasttx==0);
      TransmitMsgFirmware(m_lasttx==0);
      TransmitMsgCapabilities(m_lasttx==0);
      m_lasttx = m_lasttx_stream = now;
      }
    else if (m_streaming && caron && m_peers && now > m_lasttx_stream+m_streaming)
      {
      TransmitMsgGPS();
      m_lasttx_stream = now;
      }

    if (m_now_stat) TransmitMsgStat();
    if (m_now_environment) TransmitMsgEnvironment();
    if (m_now_gps) TransmitMsgGPS();
    if (m_now_group) TransmitMsgGroup();
    if (m_now_tpms) TransmitMsgTPMS();
    if (m_now_firmware) TransmitMsgFirmware();
    if (m_now_capabilities) TransmitMsgCapabilities();

    if (m_pending_notify_alert) TransmitNotifyAlert();
    if (m_pending_notify_error) TransmitNotifyError();
    if (m_pending_notify_info) TransmitNotifyInfo();

    if (m_pending_notify_data)
      {
      TransmitNotifyData();
      }
    else if (m_pending_notify_data_retransmit > 0 && --m_pending_notify_data_retransmit == 0)
      {
      // check for retransmissions:
      ESP_LOGD(TAG, "TransmitNotifyData: checking for retransmissions");
      m_pending_notify_data_last = 0;
      TransmitNotifyData();
      }
    }
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
  SetStatus("Server has been started", false, WaitNetwork);
  m_now_stat = false;
  m_now_gps = false;
  m_now_tpms = false;
  m_now_firmware = false;
  m_now_environment = false;
  m_now_capabilities = false;
  m_now_group = false;
  m_streaming = 0;
  m_updatetime_idle = 600;
  m_updatetime_connected = 60;
  m_lasttx = 0;
  m_lasttx_stream = 0;
  m_peers = 0;
  m_connretry = 0;
  m_mgconn = NULL;

  m_pending_notify_info = false;
  m_pending_notify_error = false;
  m_pending_notify_alert = false;
  m_pending_notify_data = false;
  m_pending_notify_data_last = 0;
  m_pending_notify_data_retransmit = 0;

  if (MyConfig.GetParamValue("vehicle", "units.distance").compare("M") == 0)
    m_units_distance = Miles;
  else
    m_units_distance = Kilometers;

  ESP_LOGI(TAG, "OVMS Server v2 running");

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsServerV2::MetricModified, this, _1));

  if (MyOvmsServerV2Reader == 0)
    {
    MyOvmsServerV2Reader = MyNotify.RegisterReader("ovmsv2", COMMAND_RESULT_NORMAL, std::bind(OvmsServerV2ReaderCallback, _1, _2),
                                                   true, std::bind(OvmsServerV2ReaderFilterCallback, _1, _2));
    }
  else
    {
    MyNotify.RegisterReader(MyOvmsServerV2Reader, "ovmsv2", COMMAND_RESULT_NORMAL, std::bind(OvmsServerV2ReaderCallback, _1, _2),
                            true, std::bind(OvmsServerV2ReaderFilterCallback, _1, _2));
    }

  // init event listener:
  MyEvents.RegisterEvent(TAG,"network.up", std::bind(&OvmsServerV2::NetUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.down", std::bind(&OvmsServerV2::NetDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.reconfigured", std::bind(&OvmsServerV2::NetReconfigured, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.init", std::bind(&OvmsServerV2::NetmanInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.stop", std::bind(&OvmsServerV2::NetmanStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&OvmsServerV2::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.received.ussd", std::bind(&OvmsServerV2::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsServerV2::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsServerV2::EventListener, this, _1, _2));

  // read config:
  ConfigChanged(NULL);

  if (MyNetManager.m_connected_any)
    {
    Connect(); // Kick off the connection
    }
  }

OvmsServerV2::~OvmsServerV2()
  {
  MyMetrics.DeregisterListener(TAG);
  MyEvents.DeregisterEvent(TAG);
  MyNotify.ClearReader(MyOvmsServerV2Reader);
  Disconnect();
  if (m_buffer)
    {
    delete m_buffer;
    m_buffer = NULL;
    }
  MyEvents.SignalEvent("server.v2.stopped", NULL);
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
    switch (MyOvmsServerV2->m_state)
      {
      case OvmsServerV2::WaitNetwork:
        writer->puts("State: Waiting for network connectivity");
        break;
      case OvmsServerV2::ConnectWait:
        writer->printf("State: Waiting to connect (%d second(s) remaining)\n",MyOvmsServerV2->m_connretry);
        break;
      case OvmsServerV2::Connecting:
        writer->puts("State: Connecting...");
        break;
      case OvmsServerV2::Authenticating:
        writer->puts("State: Authenticating...");
        break;
      case OvmsServerV2::Connected:
        writer->puts("State: Connected");
        break;
      case OvmsServerV2::Disconnected:
        writer->puts("State: Disconnected");
        break;
      case OvmsServerV2::WaitReconnect:
        writer->printf("State: Waiting to reconnect (%d second(s) remaining)\n",MyOvmsServerV2->m_connretry);
        break;
      default:
        writer->puts("State: undetermined");
        break;
      }
    writer->printf("       %s\n",MyOvmsServerV2->m_status.c_str());
    }
  }

OvmsServerV2Init MyOvmsServerV2Init  __attribute__ ((init_priority (6100)));

OvmsServerV2Init::OvmsServerV2Init()
  {
  ESP_LOGI(TAG, "Initialising OVMS V2 Server (6100)");

  OvmsCommand* cmd_server = MyCommandApp.FindCommand("server");
  OvmsCommand* cmd_v2 = cmd_server->RegisterCommand("v2","OVMS Server V2 Protocol", ovmsv2_status, "", 0, 0, false);
  cmd_v2->RegisterCommand("start","Start an OVMS V2 Server Connection",ovmsv2_start);
  cmd_v2->RegisterCommand("stop","Stop an OVMS V2 Server Connection",ovmsv2_stop);
  cmd_v2->RegisterCommand("status","Show OVMS V2 Server connection status",ovmsv2_status);

  MyConfig.RegisterParam("server.v2", "V2 Server Configuration", true, true);
  // Our instances:
  //   'server': The server name/ip
  //   'port': The port to connect to (default: 6867)
  //   'updatetime.connected': Time between updates when one or more apps connected (default: 60)
  //   'updatetime.idle': Time between updates when no apps connected (default: 600)
  // Also note:
  //  Parameter "vehicle", instance "id", is the vehicle ID
  //  Server Password has been movied to password/server.v2
  }

void OvmsServerV2Init::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "server.v2", false))
    MyOvmsServerV2 = new OvmsServerV2("oscv2");
  }
