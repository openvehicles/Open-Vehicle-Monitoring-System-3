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
#include <vector>
#include <algorithm>
#include "ovms_server_v3.h"
#include "buffered_shell.h"
#include "ovms_command.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"
#include "esp_system.h"              // <- for esp_random() (jitter)
#include "id_filter.h"               // <- use IdFilter for pattern matching
#if CONFIG_MG_ENABLE_SSL
  #include "ovms_tls.h"
#endif

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
	      opts.keep_alive = MyOvmsServerV3->m_updatetime_keepalive;
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
          StandardMetrics.ms_s_v3_peers->SetValue(0);
          MyOvmsServerV3->SetStatus("Error: Connection failed", true, OvmsServerV3::WaitReconnect);
          MyOvmsServerV3->m_connretry = 30;
          MyOvmsServerV3->m_connection_counter = 0;
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
          MyOvmsServerV3->m_connretry = 30;
          MyOvmsServerV3->m_connection_counter = 0;
          }
        }
      else
        {
        if (MyOvmsServerV3)
          {
          MyOvmsServerV3->m_sendall = true;
          MyOvmsServerV3->m_notify_info_pending = true;
          MyOvmsServerV3->m_notify_error_pending = true;
          MyOvmsServerV3->m_notify_alert_pending = true;
          MyOvmsServerV3->m_notify_data_pending = true;
          MyOvmsServerV3->m_notify_data_waitcomp = 0;
          MyOvmsServerV3->m_notify_data_waittype = NULL;
          MyOvmsServerV3->m_notify_data_waitentry = NULL;
          StandardMetrics.ms_s_v3_connected->SetValue(true);
          MyOvmsServerV3->SetStatus("OVMS V3 MQTT login successful", false, OvmsServerV3::Connected);
          MyOvmsServerV3->m_connect_jitter = -1; // reset: allow new jitter selection in next network phase
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
        MyOvmsServerV3->m_connretry = 60;
        MyOvmsServerV3->m_connection_counter = 0;
        }
      break;
    default:
      break;
    }
  }

OvmsServerV3::OvmsServerV3(const char* name)
  : OvmsServer(name), m_metrics_filter(TAG), m_metrics_priority(TAG), m_metrics_immediately(TAG)
  {
  if (MyOvmsServerV3Modifier == 0)
    {
    MyOvmsServerV3Modifier = MyMetrics.RegisterModifier();
    ESP_LOGI(TAG, "OVMS Server V3 registered metric modifier is #%d",MyOvmsServerV3Modifier);
    }

  SetStatus("Server has been started", false, WaitNetwork);
  m_connretry = 0;
  m_connection_counter = 0;
  m_mgconn = NULL;
  m_sendall = false;
  m_lasttx = 0;
  m_lasttx_sendall = 0;
  m_lasttx_priority = 0;
  m_peers = 0;
  m_updatetime_idle = 600;
  m_updatetime_connected = 10;
  m_updatetime_on = 5;  
  m_updatetime_awake = 60;
  m_updatetime_charging = 10;
  m_updatetime_sendall = 1200;
  m_updatetime_keepalive = 29*60;
  m_legacy_event_topic = true;
  m_notify_info_pending = false;
  m_notify_error_pending = false;
  m_notify_alert_pending = false;
  m_notify_data_pending = false;
  m_notify_data_waitcomp = 0;
  m_notify_data_waittype = NULL;
  m_notify_data_waitentry = NULL;
  m_connection_available = false;
  m_conn_stable_wait = 10;          // seconds network must be stable before first connect
  m_conn_jitter_max  = 5;           // max random extra seconds
  m_connect_jitter   = -1;          // -1 = not yet chosen
  m_updatetime_priority = false;
  m_updatetime_immediately = false;
  m_max_per_call_sendall = 100;      // max messages to send per Ticker1 call in sendall mode, default 100
  m_max_per_call_modified = 150;     // max messages to send per Ticker1 call in modified mode, default 150

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
  else
    {
    MyNotify.RegisterReader(MyOvmsServerV3Reader, "ovmsv3", COMMAND_RESULT_NORMAL, std::bind(OvmsServerV3ReaderCallback, _1, _2),
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
  MyEvents.RegisterEvent(TAG,"location.alert.flatbed.moved", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"location.alert.valet.bounds",  std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"app.connected", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"app.disconnected", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.charge.start", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.charge.stop", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.charge.finished", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.awake", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.on", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.off", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.locked", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"vehicle.unlocked", std::bind(&OvmsServerV3::EventListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"server.v3.connected", std::bind(&OvmsServerV3::EventListener, this, _1, _2));

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
  static size_t s_next_index = 0;

  OvmsMutexLock mg(&m_mgconn_mutex);
  if (!m_mgconn)
    {
    s_next_index = 0;
    return;
    }  
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
  CountClients();

  // Locate starting metric for current index:
  OvmsMetric* m = MyMetrics.m_first;
  size_t pos = 0;
  while (m && pos < s_next_index)
    {
    m = m->m_next;
    pos++;
    }

  // If metrics list shrank (index out of range), restart from beginning:
  if (!m && s_next_index != 0)
    {
    s_next_index = 0;
    m = MyMetrics.m_first;
    pos = 0;
    }

  int sent = 0;
  while (m)
    {
    OvmsMetric* cur = m;
    m = m->m_next;

    // Only count into budget if metric is included:
    const std::string name(cur->m_name);
    const bool included = m_metrics_filter.CheckFilter(name);

    // Clear our modified slot for full-sync pass:
    cur->ClearModified(MyOvmsServerV3Modifier);

    if (included && cur->IsDefined())
      {
      TransmitMetric(cur); // applies filter again internally, harmless
      sent++;
      }

    s_next_index++;
    // Dynamically recalculate m_max_per_call_sendall on each iteration
    if (sent >= m_max_per_call_sendall)
      break;
    }

  if (!m)
    {
    // Completed a full pass
    s_next_index = 0;
    }
  else
    {
    // More remain; hint for rapid continuation (limited by caller behavior)
    m_lasttx_sendall = 0;
    }
  }

void OvmsServerV3::TransmitModifiedMetrics()
  {
  static size_t s_mod_next_index = 0;

  OvmsMutexLock mg(&m_mgconn_mutex);
  if (!m_mgconn)
    {
    s_mod_next_index = 0;
    return;
    }
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
  CountClients();

  // Locate starting point:
  OvmsMetric* m = MyMetrics.m_first;
  size_t pos = 0;
  while (m && pos < s_mod_next_index)
    {
    m = m->m_next;
    pos++;
    }

  // List shrank? restart:
  if (!m && s_mod_next_index != 0)
    {
    s_mod_next_index = 0;
    m = MyMetrics.m_first;
    pos = 0;
    }

  int sent = 0;
  while (m)
    {
    OvmsMetric* cur = m;
    m = m->m_next;
    // Check & clear modification flag for our modifier slot.
    if (cur->IsModifiedAndClear(MyOvmsServerV3Modifier) && cur->IsDefined())
      {
      TransmitMetric(cur);
      sent++;
      }
    // Advance scan position regardless of modification:
    s_mod_next_index++;

    // Dynamically recalculate m_max_per_call_modified on each iteration
    if (sent >= m_max_per_call_modified)
      break;
    }

  if (!m)
    {
    // Completed full pass; restart next call
    s_mod_next_index = 0;
    }
  else
    {
    m_lasttx = 0;
    }
  }

void OvmsServerV3::TransmitMetric(OvmsMetric* metric)
  {
  std::string metric_name(metric->m_name);

  if (!m_metrics_filter.CheckFilter(metric_name))
    return;

  std::string topic(m_topic_prefix);
  topic.append("metric/");
  topic.append(mqtt_topic(metric_name));

  std::string val = metric->AsString();

  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
    MG_MQTT_QOS(0) | MG_MQTT_RETAIN, val.c_str(), val.length());
  ESP_LOGD(TAG,"Tx metric %s=%s",topic.c_str(),val.c_str());
  }
  
void OvmsServerV3::TransmitPriorityMetrics()
  {
    // Default priority metrics (GPS + time)
    const char* s_priority_gps_metrics[] = {
      "v.p.latitude",
      "v.p.longitude",
      "v.p.altitude",
      "v.p.speed",
      "v.p.gpsspeed",
      "m.time.utc",
    };

    const size_t s_priority_gps_metrics_count =
      sizeof(s_priority_gps_metrics) / sizeof(s_priority_gps_metrics[0]);

    OvmsMutexLock mg(&m_mgconn_mutex);
    if (!m_mgconn) return;
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
    CountClients();

    // Helper to collect what we've already handled to avoid duplicates:
    std::vector<std::string> processed;
    auto already_processed = [&](const std::string& name) {
      return std::find(processed.begin(), processed.end(), name) != processed.end();
    };
    auto mark_processed = [&](const std::string& name) {
      processed.push_back(name);
    };

    auto send_metric_by_name = [&](const std::string& name) {
      if (name.empty() || already_processed(name)) return;
      OvmsMetric* m = MyMetrics.Find(name.c_str());
      if (!m) return;
      if (m->IsModifiedAndClear(MyOvmsServerV3Modifier) && m->IsDefined())
        TransmitMetric(m);
      mark_processed(name);
    };

    // 1) Send defaults:
    for (size_t i = 0; i < s_priority_gps_metrics_count; ++i)
      send_metric_by_name(s_priority_gps_metrics[i]);

    // 2) Add configurable extra priority metrics via IdFilter:
    //    Param: server.v3 / metrics.priority

    for (OvmsMetric* m = MyMetrics.m_first; m; m = m->m_next)
      {
      const std::string mname(m->m_name);
      if (m_metrics_priority.CheckFilter(mname))
        send_metric_by_name(mname);
      }
  }

int OvmsServerV3::TransmitNotificationInfo(OvmsNotifyEntry* entry)
  {
  std::string topic(m_topic_prefix);
  topic.append("notify/info/");
  topic.append(mqtt_topic(entry->m_subtype));

  const extram::string result = entry->GetValue();

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
  topic.append(mqtt_topic(entry->m_subtype));

  const extram::string result = entry->GetValue();

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
  topic.append(mqtt_topic(entry->m_subtype));

  const extram::string result = entry->GetValue();

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
  topic.append(mqtt_topic(entry->m_subtype));
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
  if (topic.compare(0,m_topic_prefix.length(),m_topic_prefix)!=0)
    return;

  topic = topic.substr(m_topic_prefix.length());
  if (topic.compare(0,7,"client/")!=0)
    return;

  topic = topic.substr(7);
  size_t delim = topic.find('/');
  if (delim == std::string::npos)
    return;

  std::string clientid = topic.substr(0,delim);
  topic = topic.substr(delim+1);

  if (topic == "active")
    {
    if ((payload.empty())||(payload=="0"))
      RemoveClient(clientid);
    else
      AddClient(clientid);
    }
  else if (topic.compare(0,8,"command/")==0)
    {
    std::string id = topic.substr(8);
    RunCommand(clientid, id, payload);
    }
  else if (topic.compare(0,8,"request/")==0)
    {
    std::string req = topic.substr(8);
    if (req == "metric")
      ProcessClientMetricRequest(clientid, payload);
    else if (req == "config")
      ProcessClientConfigRequest(clientid, payload);
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

  // Legacy: publish event name on fixed topic
  if (m_legacy_event_topic)
    {
    mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
      MG_MQTT_QOS(0), event.c_str(), event.length());
    }

  // Publish MQTT style event topic, payload reserved for event data serialization:
  topic.append("/");
  topic.append(mqtt_topic(event));
  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
    MG_MQTT_QOS(0), "", 0);

  ESP_LOGD(TAG,"Tx event %s",event.c_str());
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
  if (!m_connection_available)
    {
    ESP_LOGE(TAG, "No connection available, waiting for network");
    m_connretry = 10;
    m_connection_counter = 0;
    return;
    }
  m_msgid = 1;
  m_vehicleid = MyConfig.GetParamValue("vehicle", "id");
  m_server = MyConfig.GetParamValue("server.v3", "server");
  m_user = MyConfig.GetParamValue("server.v3", "user");
  m_password = MyConfig.GetParamValue("password", "server.v3");
  m_port = MyConfig.GetParamValue("server.v3", "port");
  m_tls = MyConfig.GetParamValueBool("server.v3","tls", false);

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

  // Request new Topics:
  m_conn_topic[2] = std::string(m_topic_prefix);
  m_conn_topic[2].append("client/+/request/metric");

  m_conn_topic[3] = std::string(m_topic_prefix);
  m_conn_topic[3].append("client/+/request/config");

  if (m_port.empty())
    {
    m_port = (m_tls)?"8883":"1883";
    }

  std::string address(m_server);
  address.append(":");
  address.append(m_port);

  ESP_LOGI(TAG, "Connection is %s:%s %s/%s topic %s",
    m_server.c_str(), m_port.c_str(),
    m_vehicleid.c_str(), m_user.c_str(),
    m_topic_prefix.c_str());

  if (m_server.empty())
    {
    SetStatus("Error: Parameter server.v3/server must be defined", true, WaitReconnect);
    m_connretry = 20; // Try again in 20 seconds...
    m_connection_counter = 0;
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
#if CONFIG_MG_ENABLE_SSL
    opts.ssl_ca_cert = MyOvmsTLS.GetTrustedList();
    opts.ssl_server_name = m_server.c_str();
#else
    ESP_LOGE(TAG, "mg_connect(%s) failed: SSL support disabled", address.c_str());
    SetStatus("Error: Connection failed (SSL support disabled)", true, Undefined);
    return;
#endif
    }
  if ((m_mgconn = mg_connect_opt(mgr, address.c_str(), OvmsServerV3MongooseCallback, opts)) == NULL)
    {
    ESP_LOGE(TAG, "mg_connect(%s) failed: %s", address.c_str(), err);
    m_connretry = 20; // Try again in 20 seconds...
    m_connection_counter = 0;
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
  StandardMetrics.ms_s_v3_peers->SetValue(0);
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
  if (metric == NULL) return;
  if (!m_updatetime_immediately) return;
  if (!m_mgconn) return;
  if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
  const std::string metric_name(metric->m_name);
  if (!m_metrics_filter.CheckFilter(metric_name))
    return;
    
  if (m_metrics_immediately.CheckFilter(metric_name) && 
      metric->IsModifiedAndClear(MyOvmsServerV3Modifier) &&
      metric->IsDefined())
    {
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
  int64_t now = StandardMetrics.ms_m_monotonic->AsInt();
  if (event == "config.changed" || event == "config.mounted")
    {
    ConfigChanged((OvmsConfigParam*) data);
    }
  if (event == "location.alert.flatbed.moved" || event == "location.alert.valet.bounds" ||
      event == "vehicle.charge.start" || event == "vehicle.charge.stop" ||
      event == "vehicle.charge.finished" || event == "vehicle.awake" ||
      event == "vehicle.on" || event == "vehicle.off" ||           
      event == "vehicle.locked" || event == "vehicle.unlocked" ||
      event == "app.disconnected")
    {
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
    m_lasttx_priority = now;
    m_lasttx = now;
    TransmitModifiedMetrics();
    }
  if (event == "app.connected" || event == "server.v3.connected")
    {
    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return;
    
    if ( (now - m_lasttx_sendall) > 30 )
      {
      m_lasttx_sendall = now;
      m_lasttx_priority = now;
      m_lasttx = now;
      TransmitAllMetrics();
      }
    else if ( (now - m_lasttx) > 20 )
      {
      m_lasttx_priority = now;
      m_lasttx = now;
      TransmitModifiedMetrics();
      }
    }
  }

/**
 * ConfigChanged: read new configuration
 *  - param: NULL = read all configurations
 */
void OvmsServerV3::ConfigChanged(OvmsConfigParam* param)
  {
  m_updatetime_connected = MyConfig.GetParamValueInt("server.v3", "updatetime.connected", m_updatetime_connected);
  m_updatetime_idle = MyConfig.GetParamValueInt("server.v3", "updatetime.idle", m_updatetime_idle);
  m_updatetime_on = MyConfig.GetParamValueInt("server.v3", "updatetime.on", m_updatetime_on);
  m_updatetime_awake = MyConfig.GetParamValueInt("server.v3", "updatetime.awake", m_updatetime_awake);
  m_updatetime_charging = MyConfig.GetParamValueInt("server.v3", "updatetime.charging", m_updatetime_charging);
  m_updatetime_sendall = MyConfig.GetParamValueInt("server.v3", "updatetime.sendall", m_updatetime_sendall);
  m_updatetime_keepalive = MyConfig.GetParamValueInt("server.v3", "updatetime.keepalive", m_updatetime_keepalive);
  m_legacy_event_topic = MyConfig.GetParamValueBool("server.v3", "events.legacy_topic", true);
  m_updatetime_priority = MyConfig.GetParamValueBool("server.v3", "updatetime.priority", false);
  m_updatetime_immediately = MyConfig.GetParamValueBool("server.v3", "updatetime.immediately", false);
  m_max_per_call_sendall = MyConfig.GetParamValueInt("server.v3", "queue.sendall", m_max_per_call_sendall);
  if (m_max_per_call_sendall < 1) m_max_per_call_sendall = 1;
  m_max_per_call_modified = MyConfig.GetParamValueInt("server.v3", "queue.modified", m_max_per_call_modified);
  if (m_max_per_call_modified < 1) m_max_per_call_modified = 1;

  m_metrics_filter.LoadFilters(MyConfig.GetParamValue("server.v3", "metrics.include"),
                               MyConfig.GetParamValue("server.v3", "metrics.exclude"));
  m_metrics_priority.LoadFilters(MyConfig.GetParamValue("server.v3", "metrics.priority"),
                               MyConfig.GetParamValue("server.v3", "metrics.exclude"));
  m_metrics_immediately.LoadFilters(MyConfig.GetParamValue("server.v3", "metrics.include.immediately"),
                               MyConfig.GetParamValue("server.v3", "metrics.exclude.immediately"));
  // New configurable timings:
  m_conn_stable_wait = MyConfig.GetParamValueInt("server.v3", "conn.stable_wait", m_conn_stable_wait);
  if (m_conn_stable_wait < 3) m_conn_stable_wait = 3;
  m_conn_jitter_max  = MyConfig.GetParamValueInt("server.v3", "conn.jitter.max", m_conn_jitter_max);
  if (m_conn_jitter_max < 0) m_conn_jitter_max = 0;
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
  m_connection_counter = 0;
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
  bool net_connected = StdMetrics.ms_m_net_connected->AsBool();
  bool net_ip        = StdMetrics.ms_m_net_ip->AsBool();
  bool net_good_sq   = true;
  if (StdMetrics.ms_m_net_good_sq->IsDefined())
    net_good_sq = StdMetrics.ms_m_net_good_sq->AsBool();

  m_connection_available = (net_connected && net_ip && net_good_sq);

  // Reset jitter & counters when network lost
  if (!m_connection_available)
    {
    if (m_mgconn)
      {
      ESP_LOGW(TAG,"Network lost (connected=%d ip=%d gSQ=%d) → disconnect",
               (int)net_connected,(int)net_ip,(int)net_good_sq);
      Disconnect();
      m_connretry = 10;
      }
    if (m_connection_counter != 0 || m_connect_jitter != -1)
      ESP_LOGD(TAG,"Reset counters (counter=%d jitter=%d)", m_connection_counter, m_connect_jitter);
    m_connection_counter = 0;
    m_connect_jitter = -1;
    }
  else
    {
    // Choose jitter once when network first becomes stable
    if (m_connection_counter == 0 && m_connect_jitter == -1)
      {
      m_connect_jitter = (m_conn_jitter_max > 0) ? (esp_random() % (m_conn_jitter_max+1)) : 0;
      ESP_LOGI(TAG,"Pick jitter=%d (stable_wait=%d)", m_connect_jitter, m_conn_stable_wait);
      }

    if (m_connection_counter < (m_conn_stable_wait + m_connect_jitter))
      {
      m_connection_counter++;
      }

    // Attempt connect only when:
    //  - no retry countdown active
    //  - stable period + jitter reached
    // Initial connect countdown
    if (m_connretry == 0 &&
        m_connection_counter >= (m_conn_stable_wait + m_connect_jitter) &&
        !StandardMetrics.ms_s_v3_connected->AsBool() &&
        m_mgconn == NULL)
      {
      ESP_LOGI(TAG,"Attempt connect: counter=%d need=%d jitter=%d",
               m_connection_counter, m_conn_stable_wait + m_connect_jitter, m_connect_jitter);
      Connect();
      return;
      }
    }
    if (m_connretry == 0 &&
        m_connection_counter >= (m_conn_stable_wait + m_connect_jitter) &&
        !StandardMetrics.ms_s_v3_connected->AsBool() &&
        m_mgconn == NULL)
      {
      ESP_LOGI(TAG,"Attempt connect: counter=%d need=%d jitter=%d",
               m_connection_counter, m_conn_stable_wait + m_connect_jitter, m_connect_jitter);
      Connect();
      return;
      }
    

  // Retry backoff countdown
  if (m_connretry > 0)
    {
    m_connretry--;
    ESP_LOGD(TAG, "Retry countdown: %d (counter=%d jitter=%d stable_wait=%d)",
             m_connretry, m_connection_counter, m_connect_jitter, m_conn_stable_wait);
    if (m_connretry == 0 &&
        m_connection_counter >= (m_conn_stable_wait + (m_connect_jitter<0?0:m_connect_jitter)))
      {
      if (m_mgconn) Disconnect();
      ESP_LOGI(TAG, "Retry timer elapsed -> reconnect (counter=%d jitter=%d)",
               m_connection_counter, m_connect_jitter);
      // If desired, pick a new jitter value for retries:
      if (m_conn_jitter_max > 0 && m_connect_jitter >= 0)
        {
        m_connect_jitter = esp_random() % (m_conn_jitter_max + 1);
        ESP_LOGD(TAG, "Re-pick jitter for retry: %d", m_connect_jitter);
        }
      Connect();
      return;
      }
    }

  if (StandardMetrics.ms_s_v3_connected->AsBool())
    {      
    bool carawake = StandardMetrics.ms_v_env_awake->AsBool();
    bool caron = StandardMetrics.ms_v_env_on->AsBool();
    bool carcharging = StandardMetrics.ms_v_charge_inprogress->AsBool();
    int64_t now = StandardMetrics.ms_m_monotonic->AsInt();
    int next = m_updatetime_idle;
    if (caron)
      next = m_updatetime_on;
    else if (carcharging)
      next = m_updatetime_charging;
    else if (m_peers > 0)
      next = m_updatetime_connected;
    else if (carawake)
      next = m_updatetime_awake;

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
      m_lasttx_sendall = now;
      m_lasttx_priority = now;
      m_lasttx = now;
      m_sendall = false;
      TransmitAllMetrics();
      }

    if (m_notify_info_pending)  TransmitPendingNotificationsInfo();
    if (m_notify_error_pending) TransmitPendingNotificationsError();
    if (m_notify_alert_pending) TransmitPendingNotificationsAlert();
    if (m_notify_data_pending && m_notify_data_waitcomp==0)
      TransmitPendingNotificationsData();
      
    if ((m_lasttx_sendall == 0) || (now > (m_lasttx_sendall + m_updatetime_sendall)))
      {
      //ESP_LOGI(TAG, "Transmit all metrics");
      m_lasttx_sendall = now;
      m_lasttx_priority = now;
      m_lasttx = now;
      TransmitAllMetrics();
      }
    else if ((m_lasttx_priority==0) || (m_updatetime_priority && carawake && (now > (m_lasttx_priority + m_updatetime_on))))
      {
      //ESP_LOGI(TAG, "Transmit priority metrics");
      m_lasttx_priority = now;
      TransmitPriorityMetrics();
      }
    else if ((m_lasttx==0) || (now > (m_lasttx + next)))
      {
      //ESP_LOGI(TAG, "Transmit modified metrics");
      m_lasttx = now;
      TransmitModifiedMetrics();
      }
    }
  }

void OvmsServerV3::RequestUpdate(const char* requested)
  {
  if (requested == NULL) return;
  if (strcmp(requested, "all") == 0)
    {
    m_lasttx_sendall = 0;
    }
  else if (strcmp(requested, "priority") == 0)
    {
    m_lasttx_priority = 0;
    }
  else if (strcmp(requested, "modified") == 0)
    {
    m_lasttx = 0;
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
    writer->puts("Launching OVMS Server V3 connection (oscv3)");
    MyOvmsServerV3 = new OvmsServerV3("oscv3");
    }
  }

void ovmsv3_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV3 != NULL)
    {
    writer->puts("Stopping OVMS Server V3 connection (oscv3)");
    OvmsServerV3 *instance = MyOvmsServerV3;
    MyOvmsServerV3 = NULL;
    delete instance;
    }
  else
    {
    writer->puts("OVMS v3 server has not been started");
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

void ovmsv3_update(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyOvmsServerV3 == NULL)
    {
    writer->puts("ERROR: OVMS v3 server has not been started");
    }
  else
    {
    const char* cname = cmd->GetName();
    const char* requested = "modified";

    if (strcmp(cname, "all") == 0)
      {
      requested = "all";
      }
    else if (strcmp(cname, "priority") == 0)
      {
      requested = "priority";
      }

    MyOvmsServerV3->RequestUpdate(requested);
    writer->printf("Server V3 data update for %s metrics has been scheduled\n", requested);
    }
  }

// Process metric request (payload: include patterns; use IdFilter)
void OvmsServerV3::ProcessClientMetricRequest(const std::string& clientid, const std::string& payload)
{
  if (payload.empty()) return;

  // Build a temporary include-only filter from the payload (supports *, prefix*)
  IdFilter reqfilter(TAG);
  reqfilter.LoadFilters(payload); // include only

  for (OvmsMetric* m = MyMetrics.m_first; m; m = m->m_next)
  {
  const std::string name(m->m_name);
  if (!reqfilter.CheckFilter(name))
    continue;
  if (m->IsDefined())
    TransmitMetric(m);
  }
}

// Process config request (payload: param/instance)
void OvmsServerV3::ProcessClientConfigRequest(const std::string& clientid, const std::string& payload)
  {
  if (payload.empty()) return;
  std::string req = payload;
  auto trim=[&](std::string& s){
    size_t a=s.find_first_not_of(" \t\r\n");
    size_t b=s.find_last_not_of(" \t\r\n");
    if (a==std::string::npos){ s.clear(); return; }
    s=s.substr(a,b-a+1);
  };
  trim(req);
  size_t sep=req.find('/');
  if (sep==std::string::npos) return;
  std::string param=req.substr(0,sep);
  std::string inst =req.substr(sep+1);
  if (param.empty()||inst.empty()) return;

  std::string value = MyConfig.GetParamValue(param.c_str(), inst.c_str());

  std::string topic(m_topic_prefix);
  topic.append("client/").append(clientid).append("/config/");
  topic.append(mqtt_topic(param)).append("/").append(mqtt_topic(inst));

  mg_mqtt_publish(m_mgconn, topic.c_str(), m_msgid++,
                  MG_MQTT_QOS(0), value.c_str(), value.length());
  ESP_LOGD(TAG,"Tx config %s=%s", topic.c_str(), value.c_str());
  }

OvmsServerV3Init MyOvmsServerV3Init  __attribute__ ((init_priority (6200)));

OvmsServerV3Init::OvmsServerV3Init()
  {
  ESP_LOGI(TAG, "Initialising OVMS V3 Server (6200)");

  OvmsCommand* cmd_server = MyCommandApp.FindCommand("server");
  OvmsCommand* cmd_v3 = cmd_server->RegisterCommand("v3","OVMS Server V3 Protocol", ovmsv3_status, "", 0, 0, false);
  cmd_v3->RegisterCommand("start","Start an OVMS V3 Server Connection",ovmsv3_start);
  cmd_v3->RegisterCommand("stop","Stop an OVMS V3 Server Connection",ovmsv3_stop);
  cmd_v3->RegisterCommand("status","Show OVMS V3 Server connection status",ovmsv3_status);

  OvmsCommand* cmd_update = cmd_v3->RegisterCommand("update", "Request OVMS V3 Server data update", ovmsv3_update);
  cmd_update->RegisterCommand("all", "Transmit all metrics", ovmsv3_update);
  cmd_update->RegisterCommand("modified", "Transmit modified metrics only", ovmsv3_update);
  cmd_update->RegisterCommand("priority", "Transmit priority metrics only", ovmsv3_update);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "*", std::bind(&OvmsServerV3Init::EventListenerInit, this, _1, _2));

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
  
void OvmsServerV3Init::EventListenerInit(std::string event, void* data)
  {
  if (event.compare(0,7,"ticker.") == 0) return; // Skip ticker.* events
  if (event.compare(0,6,"clock.") == 0) return; // Skip clock.* events
  if (event.compare("system.event") == 0) return; // Skip event
  if (event.compare("system.wifi.scan.done") == 0) return; // Skip event

  if (MyOvmsServerV3)
    {
    MyOvmsServerV3->IncomingEvent(event, data);
    }
  }
  
