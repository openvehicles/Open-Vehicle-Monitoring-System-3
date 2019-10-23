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
#include "buffered_shell.h"
#include "ovms_command.h"
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

bool OvmsServerV3ReaderFilterCallback(OvmsNotifyType* type, const char* subtype)
  {
  if (MyOvmsServerV3)
    return MyOvmsServerV3->NotificationFilter(type, subtype);
  else
    return false;
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
        opts.will_topic = MyOvmsServerV3->m_will_topic.c_str();
        opts.will_message = "no";
        opts.flags |= MG_MQTT_WILL_RETAIN;
        mg_set_protocol_mqtt(nc);
        mg_send_mqtt_handshake_opt(nc, MyOvmsServerV3->m_vehicleid.c_str(), opts);
        }
      else
        {
        // Connection failed
        ESP_LOGW(TAG, "Connection failed");
        if (MyOvmsServerV3)
          {
          StandardMetrics.ms_s_v3_connected->SetValue(false);
          MyOvmsServerV3->SetStatus("Error: Connection failed", true, OvmsServerV3::WaitReconnect);
          MyOvmsServerV3->m_connretry = 20;
          }
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
          MyOvmsServerV3->SetStatus("OVMS V3 MQTT login successful", false, OvmsServerV3::Connected);
          MyOvmsServerV3->m_sendall = true;
          MyOvmsServerV3->m_notify_info_pending = true;
          MyOvmsServerV3->m_notify_error_pending = true;
          MyOvmsServerV3->m_notify_alert_pending = true;
          MyOvmsServerV3->m_notify_data_pending = true;
          MyOvmsServerV3->m_notify_data_waitcomp = 0;
          MyOvmsServerV3->m_notify_data_waittype = NULL;
          MyOvmsServerV3->m_notify_data_waitentry = NULL;
          }
        }
      break;
    case MG_EV_MQTT_SUBACK:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_SUBACK)");
      ESP_LOGI(TAG, "Subscription acknowledged");
      break;
    case MG_EV_MQTT_PUBLISH:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBLISH)");
      ESP_LOGI(TAG, "Incoming message %.*s: %.*s", (int) msg->topic.len,
             msg->topic.p, (int) msg->payload.len, msg->payload.p);
      if (MyOvmsServerV3)
        {
        MyOvmsServerV3->IncomingMsg(std::string(msg->topic.p,msg->topic.len),
                                    std::string(msg->payload.p,msg->payload.len));
        }
      if (msg->qos == 1)
        {
        mg_mqtt_puback(nc, msg->message_id);
        }
      else if (msg->qos == 2)
        {
        mg_mqtt_pubrec(nc, msg->message_id);
        }
      break;
    case MG_EV_MQTT_PUBACK:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBACK)");
      ESP_LOGI(TAG, "Message publishing acknowledged (msg_id: %d)", msg->message_id);
      break;
    case MG_EV_MQTT_PUBREL:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBREL)");
      mg_mqtt_pubcomp(nc, msg->message_id);
      break;
    case MG_EV_MQTT_PUBREC:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBCOMP)");
      if (MyOvmsServerV3)
        {
        MyOvmsServerV3->IncomingPubRec(msg->message_id);
        }
      break;
    case MG_EV_MQTT_PUBCOMP:
      ESP_LOGV(TAG, "OvmsServerV3MongooseCallback(MG_EV_MQTT_PUBCOMP)");
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

  SetStatus("Server has been started", false, WaitNetwork);
  m_connretry = 0;
  m_mgconn = NULL;
  m_sendall = false;
  m_lasttx = 0;
  m_lasttx_stream = 0;
  m_peers = 0;
  m_streaming = 0;
  m_updatetime_idle = 600;
  m_updatetime_connected = 60;
  m_notify_info_pending = false;
  m_notify_error_pending = false;
  m_notify_alert_pending = false;
  m_notify_data_pending = false;
  m_notify_data_waitcomp = 0;
  m_notify_data_waittype = NULL;
  m_notify_data_waitentry = NULL;

  ESP_LOGI(TAG, "OVMS Server v3 running");

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyMetrics.RegisterListener(TAG, "*", std::bind(&OvmsServerV3::MetricModified, this, _1));

  if (MyOvmsServerV3Reader == 0)
    {
    MyOvmsServerV3Reader = MyNotify.RegisterReader("ovmsv3", COMMAND_RESULT_NORMAL, std::bind(OvmsServerV3ReaderCallback, _1, _2),
                                                   true, std::bind(OvmsServerV3ReaderFilterCallback, _1, _2));
    }

  // init event listener:
  MyEvents.RegisterEvent(TAG,"network.up", std::bind(&OvmsServerV3::NetUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.down", std::bind(&OvmsServerV3::NetDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.reconfigured", std::bind(&OvmsServerV3::NetReconfigured, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.init", std::bind(&OvmsServerV3::NetmanInit, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.mgr.stop", std::bind(&OvmsServerV3::NetmanStop, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.1", std::bind(&OvmsServerV3::Ticker1, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.60", std::bind(&OvmsServerV3::Ticker60, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"system.modem.received.ussd", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.mounted", std::bind(&OvmsServerV3::EventListener, this, _1, _2));

  // read config:
  ConfigChanged(NULL);

  if (MyNetManager.m_connected_any)
    {
    Connect(); // Kick off the connection
    }
  }

OvmsServerV3::~OvmsServerV3()
  {
  MyMetrics.DeregisterListener(TAG);
  MyEvents.DeregisterEvent(TAG);
  MyNotify.ClearReader(MyOvmsServerV3Reader);
  Disconnect();
  MyEvents.SignalEvent("server.v3.stopped", NULL);
  }

void OvmsServerV3::TransmitAllMetrics()
  {
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (!m_mgconn)
    return;

  OvmsMetric* metric = MyMetrics.m_first;
  while (metric != NULL)
    {
    metric->ClearModified(MyOvmsServerV3Modifier);
    TransmitMetric(metric);
    metric = metric->m_next;
    }
  }

void OvmsServerV3::TransmitModifiedMetrics()
  {
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (!m_mgconn)
    return;

  OvmsMetric* metric = MyMetrics.m_first;
  while (metric != NULL)
    {
    if (metric->IsModifiedAndClear(MyOvmsServerV3Modifier))
      {
      TransmitMetric(metric);
      }
    metric = metric->m_next;
    }
  }

void OvmsServerV3::TransmitMetric(OvmsMetric* metric)
  {
  std::string topic(m_topic_prefix);
  topic.append("metric/");
  topic.append(metric->m_name);

  // Replace '.' inside the metric name by '/' for MQTT like namespacing.
  for(size_t i = m_topic_prefix.length(); i < topic.length(); i++)
    {
      if(topic[i] == '.')
        topic[i] = '/';
    }

  std::string val = metric->AsString();

  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
    MG_MQTT_QOS(0) | MG_MQTT_RETAIN, val.c_str(), val.length());
  ESP_LOGI(TAG,"Tx metric %s=%s",topic.c_str(),val.c_str());
  }

int OvmsServerV3::TransmitNotificationInfo(OvmsNotifyEntry* entry)
  {
  std::string topic(m_topic_prefix);
  topic.append("notify/info/");
  topic.append(entry->m_subtype);

  const extram::string result = mp_encode(entry->GetValue());

  int id = m_msgid++;
  mg_mqtt_publish(m_mgconn, topic.c_str(), id,
    MG_MQTT_QOS(1), result.c_str(), result.length());
  ESP_LOGI(TAG,"Tx notify %s=%s",topic.c_str(),result.c_str());
  return id;
  }

int OvmsServerV3::TransmitNotificationError(OvmsNotifyEntry* entry)
  {
  std::string topic(m_topic_prefix);
  topic.append("notify/error/");
  topic.append(entry->m_subtype);

  const extram::string result = mp_encode(entry->GetValue());

  int id = m_msgid++;
  mg_mqtt_publish(m_mgconn, topic.c_str(), id,
    MG_MQTT_QOS(1), result.c_str(), result.length());
  ESP_LOGI(TAG,"Tx notify %s=%s",topic.c_str(),result.c_str());
  return id;
  }

int OvmsServerV3::TransmitNotificationAlert(OvmsNotifyEntry* entry)
  {
  std::string topic(m_topic_prefix);
  topic.append("notify/alert/");
  topic.append(entry->m_subtype);

  const extram::string result = mp_encode(entry->GetValue());

  int id = m_msgid++;
  mg_mqtt_publish(m_mgconn, topic.c_str(), id,
    MG_MQTT_QOS(1), result.c_str(), result.length());
  ESP_LOGI(TAG,"Tx notify %s=%s",topic.c_str(),result.c_str());
  return id;
  }

int OvmsServerV3::TransmitNotificationData(OvmsNotifyEntry* entry)
  {
  char base[32];
  std::string topic(m_topic_prefix);
  topic.append("notify/data/");
  topic.append(entry->m_subtype);
  topic.append("/");
  topic.append(itoa(entry->m_id,base,10));
  topic.append("/");
  uint32_t now = esp_log_timestamp();
  topic.append(itoa(-((int)(now - entry->m_created) / 1000),base,10));

  // terminate payload at first LF:
  extram::string msg = entry->GetValue();
  size_t eol = msg.find('\n');
  if (eol != std::string::npos)
    msg.resize(eol);

  const char* result = msg.c_str();

  int id = m_msgid++;
  mg_mqtt_publish(m_mgconn, topic.c_str(), id,
    MG_MQTT_QOS(2), result, strlen(result));
  ESP_LOGI(TAG,"Tx notify %s=%s",topic.c_str(),result);
  return id;
  }

void OvmsServerV3::TransmitPendingNotificationsInfo()
  {
  // Find the type object
  OvmsNotifyType* info = MyNotify.GetType("info");
  if (info == NULL)
    {
    m_notify_info_pending = false;
    return;
    }

  // Find the first entry
  OvmsNotifyEntry* e = info->FirstUnreadEntry(MyOvmsServerV3Reader, 0);
  if (e == NULL)
    {
    m_notify_info_pending = false;
    return;
    }

  TransmitNotificationInfo(e);
  info->MarkRead(MyOvmsServerV3Reader, e);
  }

void OvmsServerV3::TransmitPendingNotificationsError()
  {
  // Find the type object
  OvmsNotifyType* error = MyNotify.GetType("error");
  if (error == NULL)
    {
    m_notify_error_pending = false;
    return;
    }

  // Find the first entry
  OvmsNotifyEntry* e = error->FirstUnreadEntry(MyOvmsServerV3Reader, 0);
  if (e == NULL)
    {
    m_notify_error_pending = false;
    return;
    }

  TransmitNotificationError(e);
  error->MarkRead(MyOvmsServerV3Reader, e);
  }

void OvmsServerV3::TransmitPendingNotificationsAlert()
  {
  // Find the type object
  OvmsNotifyType* alert = MyNotify.GetType("alert");
  if (alert == NULL)
    {
    m_notify_alert_pending = false;
    return;
    }

  // Find the first entry
  OvmsNotifyEntry* e = alert->FirstUnreadEntry(MyOvmsServerV3Reader, 0);
  if (e == NULL)
    {
    m_notify_alert_pending = false;
    return;
    }

  TransmitNotificationAlert(e);
  alert->MarkRead(MyOvmsServerV3Reader, e);
  }

void OvmsServerV3::TransmitPendingNotificationsData()
  {
  if (m_notify_data_waitcomp != 0) return;

  // Find the type object
  OvmsNotifyType* data = MyNotify.GetType("data");
  if (data == NULL)
    {
    m_notify_data_pending = false;
    return;
    }

  // Find the first entry
  OvmsNotifyEntry* e = data->FirstUnreadEntry(MyOvmsServerV3Reader, 0);
  if (e == NULL)
    {
    m_notify_data_pending = false;
    return;
    }

  m_notify_data_waittype = data;
  m_notify_data_waitentry = e;
  m_notify_data_waitcomp = TransmitNotificationData(e);
  m_notify_data_pending = false;
  }

void OvmsServerV3::IncomingMsg(std::string topic, std::string payload)
  {
  if (topic.compare(0,m_topic_prefix.length(),m_topic_prefix)==0)
    {
    topic = topic.substr(m_topic_prefix.length());
    if (topic.compare(0,7,"client/")==0)
      {
      topic = topic.substr(7);
      size_t delim = topic.find('/');
      if (delim != string::npos)
        {
        std::string clientid = topic.substr(0,delim);
        topic = topic.substr(delim+1);
        if (topic == "active")
          {
          if ((payload.empty())||(payload == "0"))
            RemoveClient(clientid);
          else
            AddClient(clientid);
          }
        else if (topic.compare(0,8,"command/") == 0)
          {
          topic = topic.substr(8);
          RunCommand(clientid, topic, payload);
          }
        }
      }
    }
  }

void OvmsServerV3::IncomingPubRec(int id)
  {
  mg_mqtt_pubrel(m_mgconn, id);
  if ((id == m_notify_data_waitcomp)&&
      (m_notify_data_waittype != NULL)&&
      (m_notify_data_waitentry != NULL))
    {
    m_notify_data_waittype->MarkRead(MyOvmsServerV3Reader, m_notify_data_waitentry);
    m_notify_data_waitcomp = 0;
    m_notify_data_waittype = NULL;
    m_notify_data_waitentry = NULL;
    m_notify_data_pending = true;
    TransmitPendingNotificationsData();
    }
  }

void OvmsServerV3::IncomingEvent(std::string event, void* data)
  {
  // Publish the event, if we are connected...
  if (m_mgconn == NULL) return;
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;

  std::string topic(m_topic_prefix);
  topic.append("event");

  ESP_LOGI(TAG,"Tx event %s",event.c_str());
  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
    MG_MQTT_QOS(0), event.c_str(), event.length());
  }

void OvmsServerV3::RunCommand(std::string client, std::string id, std::string command)
  {
  ESP_LOGI(TAG,"Run command: %s",command.c_str());

  BufferedShell* bs = new BufferedShell(false, COMMAND_RESULT_NORMAL);
  bs->SetSecure(true); // this is an authorized channel
  bs->ProcessChars(command.c_str(), command.length());
  bs->ProcessChar('\n');
  std::string val; bs->Dump(val);
  delete bs;

  std::string topic(m_topic_prefix);
  topic.append("client/");
  topic.append(client);
  topic.append("/response/");
  topic.append(id);
  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
    MG_MQTT_QOS(1), val.c_str(), val.length());
  }

void OvmsServerV3::AddClient(std::string id)
  {
  auto k = m_clients.find(id);
  if (k == m_clients.end())
    {
    // Create a new client
    m_clients[id] = monotonictime + 120;
    ESP_LOGI(TAG,"MQTT client %s has connected",id.c_str());
    }
  else
    {
    // Update existing client
    k->second = monotonictime + 120;
    }
  CountClients();
  }

void OvmsServerV3::RemoveClient(std::string id)
  {
  auto k = m_clients.find(id);
  if (k != m_clients.end())
    {
    // Remove the specified client
    m_clients.erase(k);
    ESP_LOGI(TAG,"MQTT client %s has disconnected",id.c_str());
    CountClients();
    }
  }

void OvmsServerV3::CountClients()
  {
  for (auto it = m_clients.begin(); it != m_clients.end();)
    {
    if (it->second < monotonictime)
      {
      // Expired, so delete it...
      ESP_LOGI(TAG,"MQTT client %s has timed out",it->first.c_str());
      it = m_clients.erase(it);
      }
    else
      {
      it++;
      }
    }

  int nc = m_clients.size();
  StandardMetrics.ms_s_v3_peers->SetValue(nc);
  if (nc > m_peers)
    ESP_LOGI(TAG, "One or more peers have connected");
  if ((nc == 0)&&(m_peers != 0))
    MyEvents.SignalEvent("app.disconnected",NULL);
  else if ((nc > 0)&&(nc != m_peers))
    MyEvents.SignalEvent("app.connected",NULL);
  if (nc != m_peers)
    m_lasttx = 0; // Our peers have changed, so force a transmission of status messages

  m_peers = nc;
  }

void OvmsServerV3::Connect()
  {
  m_msgid = 1;
  m_vehicleid = MyConfig.GetParamValue("vehicle", "id");
  m_server = MyConfig.GetParamValue("server.v3", "server");
  m_user = MyConfig.GetParamValue("server.v3", "user");
  m_password = MyConfig.GetParamValue("password", "server.v3");
  m_port = MyConfig.GetParamValue("server.v3", "port");

  m_topic_prefix = MyConfig.GetParamValue("server.v3", "topic.prefix", "");
  if(m_topic_prefix.empty())
    {
    m_topic_prefix = std::string("ovms/");
    m_topic_prefix.append(m_user);
    m_topic_prefix.append("/");

    if(!m_vehicleid.empty())
      {
      m_topic_prefix.append(m_vehicleid);
      m_topic_prefix.append("/");
      }
    }

  m_will_topic = std::string(m_topic_prefix);
  m_will_topic.append("metric/s/v3/connected");

  m_conn_topic[0] = std::string(m_topic_prefix);
  m_conn_topic[0].append("client/+/active");

  m_conn_topic[1] = std::string(m_topic_prefix);
  m_conn_topic[1].append("client/+/command/+");

  if (m_port.empty()) m_port = "1883";
  std::string address(m_server);
  address.append(":");
  address.append(m_port);

  ESP_LOGI(TAG, "Connection is %s:%s %s/%s",
    m_server.c_str(), m_port.c_str(),
    m_vehicleid.c_str(), m_user.c_str());

  if (m_server.empty())
    {
    SetStatus("Error: Parameter server.v3/server must be defined", true, WaitReconnect);
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
  OvmsMutexLock mg(&m_mgconn_mutex);
  if (m_mgconn)
    {
    m_mgconn->flags |= MG_F_CLOSE_IMMEDIATELY;
    m_mgconn = NULL;
    SetStatus("Disconnected from OVMS Server V3", false, Disconnected);
    }
  m_connretry = 0;
  StandardMetrics.ms_s_v3_connected->SetValue(false);
  }

void OvmsServerV3::SetStatus(const char* status, bool fault /*=false*/, State newstate /*=Undefined*/)
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
      case OvmsServerV3::WaitNetwork:
        MyEvents.SignalEvent("server.v3.waitnetwork", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::ConnectWait:
        MyEvents.SignalEvent("server.v3.connectwait", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::Connecting:
        MyEvents.SignalEvent("server.v3.connecting", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::Authenticating:
        MyEvents.SignalEvent("server.v3.authenticating", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::Connected:
        MyEvents.SignalEvent("server.v3.connected", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::Disconnected:
        MyEvents.SignalEvent("server.v3.disconnected", (void*)m_status.c_str(), m_status.length()+1);
        break;
      case OvmsServerV3::WaitReconnect:
        MyEvents.SignalEvent("server.v3.waitreconnect", (void*)m_status.c_str(), m_status.length()+1);
        break;
      default:
        break;
      }
    }
  }

void OvmsServerV3::MetricModified(OvmsMetric* metric)
  {
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;

  if (m_streaming)
    {
    OvmsMutexLock mg(&m_mgconn_mutex);
    if (!m_mgconn)
      return;
    metric->ClearModified(MyOvmsServerV3Modifier);
    TransmitMetric(metric);
    }
  }

bool OvmsServerV3::NotificationFilter(OvmsNotifyType* type, const char* subtype)
  {
  if (strcmp(type->m_name, "info") == 0 ||
      strcmp(type->m_name, "error") == 0 ||
      strcmp(type->m_name, "alert") == 0 ||
      strcmp(type->m_name, "data") == 0)
    return true;
  else
    return false;
  }

bool OvmsServerV3::IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  if (strcmp(type->m_name,"info")==0)
    {
    // Info notifications
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return false;
    TransmitNotificationInfo(entry);
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"error")==0)
    {
    // Error notification
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return false;
    TransmitNotificationError(entry);
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"alert")==0)
    {
    // Alert notifications
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return false;
    TransmitNotificationAlert(entry);
    return true; // Mark it as read, as we've managed to send it
    }
  else if (strcmp(type->m_name,"data")==0)
    {
    // Data notifications
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return false;
    m_notify_data_pending = true;
    return false; // We just flag it for later transmission
    }
  else
    return true; // Mark it read, as no interest to us
  }

void OvmsServerV3::EventListener(std::string event, void* data)
  {
  if (event == "config.changed" || event == "config.mounted")
    {
    ConfigChanged((OvmsConfigParam*) data);
    }
  }

/**
 * ConfigChanged: read new configuration
 *  - param: NULL = read all configurations
 */
void OvmsServerV3::ConfigChanged(OvmsConfigParam* param)
  {
  m_streaming = MyConfig.GetParamValueInt("vehicle", "stream", 0);
  m_updatetime_connected = MyConfig.GetParamValueInt("server.v3", "updatetime.connected", 60);
  m_updatetime_idle = MyConfig.GetParamValueInt("server.v3", "updatetime.idle", 600);
  }

void OvmsServerV3::NetUp(std::string event, void* data)
  {
  // workaround for wifi AP mode startup (manager up before interface)
  if ( (m_mgconn == NULL) && MyNetManager.MongooseRunning() )
    {
    ESP_LOGI(TAG, "Network is up, so attempt network connection");
    Connect(); // Kick off the connection
    }
  }

void OvmsServerV3::NetDown(std::string event, void* data)
  {
  if (m_mgconn)
    {
    ESP_LOGI(TAG, "Network is down, so disconnect network connection");
    Disconnect();
    }
  }

void OvmsServerV3::NetReconfigured(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Network was reconfigured: disconnect, and reconnect in 10 seconds");
  Disconnect();
  m_connretry = 10;
  }

void OvmsServerV3::NetmanInit(std::string event, void* data)
  {
  if ((m_mgconn == NULL)&&(MyNetManager.m_connected_any))
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
        return;
        }
      }
    }

  if (StandardMetrics.ms_s_v3_connected->AsBool())
    {
    if (m_sendall)
      {
      ESP_LOGI(TAG, "Subscribe to MQTT topics");
      struct mg_mqtt_topic_expression topics[MQTT_CONN_NTOPICS];
      for (int k=0;k<MQTT_CONN_NTOPICS;k++)
        {
        topics[k].topic = m_conn_topic[k].c_str();
        topics[k].qos = 1;
        }
      mg_mqtt_subscribe(m_mgconn, topics, MQTT_CONN_NTOPICS, m_msgid++);

      ESP_LOGI(TAG, "Transmit all metrics");
      TransmitAllMetrics();
      m_sendall = false;
      }

    if (m_notify_info_pending) TransmitPendingNotificationsInfo();
    if (m_notify_error_pending) TransmitPendingNotificationsError();
    if (m_notify_alert_pending) TransmitPendingNotificationsAlert();
    if ((m_notify_data_pending)&&(m_notify_data_waitcomp==0))
      TransmitPendingNotificationsData();

    bool caron = StandardMetrics.ms_v_env_on->AsBool();
    int now = StandardMetrics.ms_m_monotonic->AsInt();
    int next = (m_peers==0) ? m_updatetime_idle : m_updatetime_connected;
    if ((m_lasttx==0)||(now>(m_lasttx+next)))
      {
      TransmitModifiedMetrics();
      m_lasttx = m_lasttx_stream = now;
      }
    else if (m_streaming && caron && m_peers && now > m_lasttx_stream+m_streaming)
      {
      // TODO: transmit streaming metrics
      m_lasttx_stream = now;
      }
    }
  }

void OvmsServerV3::Ticker60(std::string event, void* data)
  {
  CountClients();
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
    switch (MyOvmsServerV3->m_state)
      {
      case OvmsServerV3::WaitNetwork:
        writer->puts("State: Waiting for network connectivity");
        break;
      case OvmsServerV3::ConnectWait:
        writer->printf("State: Waiting to connect (%d second(s) remaining)\n",MyOvmsServerV3->m_connretry);
        break;
      case OvmsServerV3::Connecting:
        writer->puts("State: Connecting...");
        break;
      case OvmsServerV3::Authenticating:
        writer->puts("State: Authenticating...");
        break;
      case OvmsServerV3::Connected:
        writer->puts("State: Connected");
        break;
      case OvmsServerV3::Disconnected:
        writer->puts("State: Disconnected");
        break;
      case OvmsServerV3::WaitReconnect:
        writer->printf("State: Waiting to reconnect (%d second(s) remaining)\n",MyOvmsServerV3->m_connretry);
        break;
      default:
        writer->puts("State: undetermined");
        break;
      }
    writer->printf("       %s\n",MyOvmsServerV3->m_status.c_str());
    }
  }

OvmsServerV3Init MyOvmsServerV3Init  __attribute__ ((init_priority (6200)));

OvmsServerV3Init::OvmsServerV3Init()
  {
  ESP_LOGI(TAG, "Initialising OVMS V3 Server (6200)");

  OvmsCommand* cmd_server = MyCommandApp.FindCommand("server");
  OvmsCommand* cmd_v3 = cmd_server->RegisterCommand("v3","OVMS Server V3 Protocol");
  cmd_v3->RegisterCommand("start","Start an OVMS V3 Server Connection",ovmsv3_start);
  cmd_v3->RegisterCommand("stop","Stop an OVMS V3 Server Connection",ovmsv3_stop);
  cmd_v3->RegisterCommand("status","Show OVMS V3 Server connection status",ovmsv3_status);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "*", std::bind(&OvmsServerV3Init::EventListener, this, _1, _2));

  MyConfig.RegisterParam("server.v3", "V3 Server Configuration", true, true);
  // Our instances:
  //   'server': The server name/ip
  //   'user': The server username
  //   'port': The port to connect to (default: 1883)
  // Also note:
  //  Parameter "vehicle", instance "id", is the vehicle ID
  //  Parameter "password", instance "server.v3", is the server password
  }

void OvmsServerV3Init::AutoInit()
  {
  if (MyConfig.GetParamValueBool("auto", "server.v3", false))
    MyOvmsServerV3 = new OvmsServerV3("oscv3");
  }

void OvmsServerV3Init::EventListener(std::string event, void* data)
  {
  if (event.compare(0,7,"ticker.") == 0) return; // Skip ticker.* events
  if (event.compare("system.event") == 0) return; // Skip event
  if (event.compare("system.wifi.scan.done") == 0) return; // Skip event

  if (MyOvmsServerV3)
    {
    MyOvmsServerV3->IncomingEvent(event, data);
    }
  }
