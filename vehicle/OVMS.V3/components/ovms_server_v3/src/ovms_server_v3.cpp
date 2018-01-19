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
static const char *TAG = "ovms-server-v3";

#include <string.h>
#include <stdint.h>
#include "ovms_server_v3.h"
#include "ovms_command.h"
#include "ovms_config.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

OvmsServerV3 *MyOvmsServerV3 = NULL;
size_t MyOvmsServerV3Modifier = 0;
size_t MyOvmsServerV3Reader = 0;

bool OvmsServerV3ReaderCallback(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  if (MyOvmsServerV3)
    return MyOvmsServerV3->IncomingNotification(type, entry);
  else
    return true; // No server v3 running, so just discard
  }

static void OvmsServerV3MongooseCallback(struct mg_connection *nc, int ev, void *p)
  {
  struct mg_mqtt_message *msg = (struct mg_mqtt_message *) p;
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int *success = (int*)p;
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_CONNECT=%d)",*success);
      if (*success == 0)
        {
        // Successful connection
        ESP_LOGI(TAG, "Connection successful");
        struct mg_send_mqtt_handshake_opts opts;
        memset(&opts, 0, sizeof(opts));
        opts.user_name = MyOvmsServerV3->m_user.c_str();
        opts.password = MyOvmsServerV3->m_password.c_str();
        mg_set_protocol_mqtt(nc);
        mg_send_mqtt_handshake_opt(nc, MyOvmsServerV3->m_vehicleid.c_str(), opts);
        }
      else
        {
        // Connection failed
        ESP_LOGW(TAG, "Connection failed");
        if (MyOvmsServerV3) MyOvmsServerV3->m_connretry = 20;
        }
      }
      break;
    case MG_EV_MQTT_CONNACK:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_CONNACK)");
      if (msg->connack_ret_code != MG_EV_MQTT_CONNACK_ACCEPTED)
        {
        ESP_LOGE(TAG, "Got mqtt connection error: %d\n", msg->connack_ret_code);
        if (MyOvmsServerV3)
          {
          MyOvmsServerV3->Disconnect();
          MyOvmsServerV3->m_connretry = 20;
          }
        }
      else
        {
        if (MyOvmsServerV3)
          {
          StandardMetrics.ms_s_v3_connected->SetValue(true);
          MyOvmsServerV3->SetStatus("OVMS V3 MQTT login successful");
          MyOvmsServerV3->m_sendall = true;
          }
        }
      break;
    case MG_EV_MQTT_PUBACK:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBACK)");
      ESP_LOGI(TAG, "Message publishing acknowledged (msg_id: %d)", msg->message_id);
      break;
    case MG_EV_MQTT_SUBACK:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_SUBACK)");
      ESP_LOGI(TAG, "Subscription acknowledged");
      break;
    case MG_EV_MQTT_PUBLISH:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBLISH)");
      ESP_LOGI(TAG, "Incoming message %.*s: %.*s", (int) msg->topic.len,
             msg->topic.p, (int) msg->payload.len, msg->payload.p);
      break;
    case MG_EV_CLOSE:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_CLOSE)");
      if (MyOvmsServerV3)
        {
        MyOvmsServerV3->Disconnect();
        MyOvmsServerV3->m_connretry = 20;
        }
      break;
    default:
      break;
    }
  }

OvmsServerV3::OvmsServerV3(const char* name)
  : OvmsServer(name)
  {
  if (MyOvmsServerV3Modifier == 0)
    {
    MyOvmsServerV3Modifier = MyMetrics.RegisterModifier();
    ESP_LOGI(TAG, "OVMS Server V3 registered metric modifier is #%d",MyOvmsServerV3Modifier);
    }

  SetStatus("Starting");
  m_connretry = 0;
  m_mgconn = NULL;
  m_sendall = false;

  ESP_LOGI(TAG, "OVMS Server v3 running");

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsServerV3::MetricModified, this, _1));

  if (MyOvmsServerV3Reader == 0)
    {
    MyOvmsServerV3Reader = MyNotify.RegisterReader(TAG, COMMAND_RESULT_NORMAL, std::bind(OvmsServerV3ReaderCallback, _1, _2));
    }

  // init event listener:
  MyEvents.RegisterEvent(TAG,"network.up", std::bind(&OvmsServerV3::NetUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.down", std::bind(&OvmsServerV3::NetDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.reconfigured", std::bind(&OvmsServerV3::NetReconfigured, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.init", std::bind(&OvmsServerV3::NetmanInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.stop", std::bind(&OvmsServerV3::NetmanStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&OvmsServerV3::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.received.ussd", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsServerV3::EventListener, this, _1, _2));

  if (MyNetManager.m_connected_any)
    {
    Connect(); // Kick off the connection
    }
  }

OvmsServerV3::~OvmsServerV3()
  {
  MyMetrics.DeregisterListener(TAG);
  MyEvents.DeregisterEvent(TAG);
  MyNotify.ClearReader(TAG);
  Disconnect();
  }

void OvmsServerV3::TransmitAllMetrics()
  {
  OvmsMetric* metric = MyMetrics.m_first;
  while (metric != NULL)
    {
    std::string topic("ovms/");
    topic.append(m_vehicleid);
    topic.append("/m/");
    topic.append(metric->m_name);
    std::string val = metric->AsString();
    ESP_LOGI(TAG,"Tx(all) metric %s=%s",topic.c_str(),val.c_str());
    mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++, MG_MQTT_QOS(0), val.c_str(), val.length());
    metric = metric->m_next;
    }
  }

void OvmsServerV3::Connect()
  {
  m_msgid = 1;
  m_vehicleid = MyConfig.GetParamValue("vehicle", "id");
  m_server = MyConfig.GetParamValue("server.v3", "server");
  m_user = MyConfig.GetParamValue("server.v3", "user");
  m_password = MyConfig.GetParamValue("server.v3", "password");
  m_port = MyConfig.GetParamValue("server.v3", "port");
  if (m_port.empty()) m_port = "1883";
  std::string address(m_server);
  address.append(":");
  address.append(m_port);

  ESP_LOGI(TAG, "Connection is %s:%s %s/%s/%s",
    m_server.c_str(), m_port.c_str(),
    m_vehicleid.c_str(), m_user.c_str(), m_password.c_str());

  if (m_vehicleid.empty())
    {
    SetStatus("Error: Parameter vehicle/id must be defined",true);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }
  if (m_server.empty())
    {
    SetStatus("Error: Parameter server.v2/server must be defined",true);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }

  SetStatus("Connecting...");
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts;
  const char* err;
  memset(&opts, 0, sizeof(opts));
  opts.error_string = &err;
  if ((m_mgconn = mg_connect_opt(mgr, address.c_str(), OvmsServerV3MongooseCallback, opts)) == NULL)
    {
    ESP_LOGE(TAG, "mg_connect(%s) failed: %s", address.c_str(), err);
    m_connretry = 20; // Try again in 20 seconds...
    return;
    }
  return;
  }

void OvmsServerV3::Disconnect()
  {
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    SetStatus("Error: Disconnected from OVMS Server V3",true);
    }
  m_connretry = 0;
  StandardMetrics.ms_s_v3_connected->SetValue(false);
  }

void OvmsServerV3::SetStatus(const char* status, bool fault)
  {
  if (fault)
    ESP_LOGE(TAG, "Status: %s", status);
  else
    ESP_LOGI(TAG, "Status: %s", status);
  m_status = status;
  }

void OvmsServerV3::MetricModified(OvmsMetric* metric)
  {
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;

  std::string topic("ovms/");
  topic.append(m_vehicleid);
  topic.append("/m/");
  topic.append(metric->m_name);
  std::string val = metric->AsString();
  ESP_LOGI(TAG,"Tx(mod) metric %s=%s",topic.c_str(),val.c_str());
  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++, MG_MQTT_QOS(0), val.c_str(), val.length());
  }

bool OvmsServerV3::IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  // TODO
  return true; // Mark it read, we have no interest in it
  }

void OvmsServerV3::EventListener(std::string event, void* data)
  {
  }

void OvmsServerV3::NetUp(std::string event, void* data)
  {
  }

void OvmsServerV3::NetDown(std::string event, void* data)
  {
  }

void OvmsServerV3::NetReconfigured(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Network is reconfigured, so disconnect network connection");
  Disconnect();
  m_connretry = 10;
  }

void OvmsServerV3::NetmanInit(std::string event, void* data)
  {
  if (m_mgconn == NULL)
    {
    ESP_LOGI(TAG, "Network is up, so attempt network connection");
    Connect(); // Kick off the connection
    }
  }

void OvmsServerV3::NetmanStop(std::string event, void* data)
  {
  if (m_mgconn)
    {
    ESP_LOGI(TAG, "Network is down, so disconnect network connection");
    Disconnect();
    }
  }

void OvmsServerV3::Ticker1(std::string event, void* data)
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

  if (m_sendall)
    {
    ESP_LOGI(TAG, "Transmit all metrics");
    TransmitAllMetrics();
    m_sendall = false;
    }
  }

void OvmsServerV3::SetPowerMode(PowerMode powermode)
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

void ovmsv3_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV3 == NULL)
    {
    MyOvmsServerV3 = new OvmsServerV3("oscv3");
    }
  }

void ovmsv3_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV3 != NULL)
    {
    delete MyOvmsServerV3;
    MyOvmsServerV3 = NULL;
    }
  }

void ovmsv3_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV3 == NULL)
    {
    writer->puts("OVMS v3 server has not been started");
    }
  else
    {
    writer->puts(MyOvmsServerV3->m_status.c_str());
    }
  }

class OvmsServerV3Init
  {
  public: OvmsServerV3Init();
  } OvmsServerV3Init  __attribute__ ((init_priority (6200)));

OvmsServerV3Init::OvmsServerV3Init()
  {
  ESP_LOGI(TAG, "Initialising OVMS V3 Server (6200)");

  OvmsCommand* cmd_server = MyCommandApp.FindCommand("server");
  OvmsCommand* cmd_v3 = cmd_server->RegisterCommand("v3","OVMS Server V3 Protocol", NULL, "", 0, 0);
  cmd_v3->RegisterCommand("start","Start an OVMS V3 Server Connection",ovmsv3_start, "", 0, 0);
  cmd_v3->RegisterCommand("stop","Stop an OVMS V3 Server Connection",ovmsv3_stop, "", 0, 0);
  cmd_v3->RegisterCommand("status","Show OVMS V3 Server connection status",ovmsv3_status, "", 0, 0);

  MyConfig.RegisterParam("server.v3", "V3 Server Configuration", true, false);
  // Our instances:
  //   'server': The server name/ip
  //   'user': The server username
  //   'password': The server password
  //   'port': The port to connect to (default: 1883)
  // Also note:
  //  Parameter "vehicle", instance "id", is the vehicle ID
  }
