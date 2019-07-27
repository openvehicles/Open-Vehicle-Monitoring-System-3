/*
;    Project:       Open Vehicle Monitor System
;    Date:          18th July 2019
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2019       Marko Juhanne
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

#define LOG_LOCAL_LEVEL ESP_LOG_VERBOSE

#define DEFAULT_PUSHOVER_RETRY 30 // Default retry interval in secs for unacknowledged emergency priority messages
#define DEFAULT_PUSHOVER_EXPIRE 1800 // Default expiration time in secs for unacknowledged emergency priority messages

#include "ovms_log.h"
static const char *TAG = "pushover";

#include <string.h>
#include "pushover.h"
#include "metrics_standard.h"
#include "ovms_netmanager.h"
#include "ovms_config.h"
#include "ovms_events.h"


#include "mongoose.h"

Pushover MyPushoverClient __attribute__ ((init_priority (8800)));



void pushover_send_message(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  int priority;
  std::string sound;
  if (argc<1) 
    {
    writer->puts("Error: No message given");
    return;
    }
  if (argc >= 2)
    {
    priority = atoi(argv[1]);

    if ((priority < -2) || (priority > 2))
      {
      writer->puts("Error: priority level should be between -2 and 2");
      return;
      }

    if (argc >= 3)
      sound = argv[2];
    } else
    {
      priority = 0;
      sound = "pushover";
    }

  writer->printf("Sending PUSHOVER message \"%s\" with priority %d and sound %s\n",argv[0],priority,sound);
  MyPushoverClient.SendMessage(argv[0], priority, sound);
  }

bool PushoverReaderCallback(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  return MyPushoverClient.IncomingNotification(type, entry);
  }

bool PushoverReaderFilterCallback(OvmsNotifyType* type, const char* subtype)
  {
  return MyPushoverClient.NotificationFilter(type, subtype);
  }

Pushover::Pushover()
  : pcp("pushover")
  {
  ESP_LOGI(TAG, "Initialising Pushover client (8800)");

  MyConfig.RegisterParam("pushover", "Pushover client configuration", true, true);

  m_buffer = new OvmsBuffer(1024);
  OvmsCommand* cmd_pushover = MyCommandApp.RegisterCommand("pushover","pushover notification framework");
  cmd_pushover->RegisterCommand("msg","message",pushover_send_message,"<\"message\"> [<priority>] [<sound>]",1,3);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "*", std::bind(&Pushover::EventListener, this, _1, _2));

  reader = MyNotify.RegisterReader("pushover", COMMAND_RESULT_NORMAL, std::bind(PushoverReaderCallback, _1, _2),
                                                   true, std::bind(PushoverReaderFilterCallback, _1, _2));
  }

Pushover::~Pushover()
  {
  if (m_buffer)
    {
    delete m_buffer;
    m_buffer = NULL;
    }
  MyNotify.ClearReader(reader);
  }


bool Pushover::NotificationFilter(OvmsNotifyType* type, const char* subtype)
  {
  if (strcmp(type->m_name, "info") == 0 ||
      strcmp(type->m_name, "error") == 0 ||
      strcmp(type->m_name, "alert") == 0 ||
      strcmp(type->m_name, "data") == 0)
    return true;
  else
    return false;
  }

bool Pushover::IncomingNotification(OvmsNotifyType* type, OvmsNotifyEntry* entry)
  {
  std::string nfy;
  std::string priority;
  std::string sound;
  std::string msg;

  if (!MyNetManager.m_connected_any)
    {
    ESP_LOGD(TAG,"IncomingNotification: Ignore notification (%s:%s:%s) because no network conn",type->m_name,entry->GetSubType(),entry->GetValue().c_str());
    return false;
    } 
  else if (MyConfig.GetParamValueBool("pushover","enable", false) == false)
    {
    ESP_LOGD(TAG,"IncomingNotification: Ignore notification (%s:%s:%s) (pushover not enabled)",type->m_name,entry->GetSubType(),entry->GetValue().c_str());
    return false;
    }
  ESP_LOGD(TAG,"IncomingNotification: Handling notification (%s:%s:%s)",type->m_name,entry->GetSubType(),entry->GetValue().c_str());      

  OvmsConfigParam* param = MyConfig.CachedParam("pushover");
  ConfigParamMap pmap;
  pmap = param->m_map;

  // try to find configuration for specific type/subtype
  nfy = "np.";
  nfy.append(type->m_name);
  nfy.append("/");
  nfy.append(entry->GetSubType());
  priority = pmap[nfy];
  if (priority == "")
  {
    // try more general type
    nfy = "np.";
    nfy.append(type->m_name);
    priority = pmap[nfy];
    if (priority == "")
      {
      ESP_LOGD(TAG,"IncomingNotification: No priority set -> ignore notification");
      return false;
      }
  }

  msg = std::string(entry->GetValue().c_str());
  if (priority == "0")
    sound = pmap["sound.normal"];
  else if (priority == "1")
    sound = pmap["sound.high"];
  else if (priority == "2")
    sound = pmap["sound.emergency"];
  SendMessage(msg, atoi(priority.c_str()), sound);


  /*
  if (strcmp(type->m_name,"info")==0)
    {
    // Info notifications
´    if (!StandardMetrics.ms_s_v3_connected->AsBool()) return false;
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
  else*/
    return true; // Mark it read, as no interest to us
  }


void Pushover::EventListener(std::string event, void* data)
  {
  std::string name, setting, pri, msg, sound;
  if ( (event == "ticker.1") || (event == "ticker.10") )
    return;
  ESP_LOGD(TAG,"EventListener %s",event.c_str());
  if (event == "config.changed" || event == "config.mounted")
    {
    ConfigChanged((OvmsConfigParam*) data);
    }

  if (!MyNetManager.m_connected_any)
    {
    ESP_LOGD(TAG,"EventListener: Ignore event (%s) because no network conn",event.c_str());
    return;
    } 
  else if (MyConfig.GetParamValueBool("pushover","enable", false) == false)
    {
    ESP_LOGD(TAG,"EventListener: Ignore event (%s) (pushover not enabled)",event.c_str());
    return;
    }
  ESP_LOGD(TAG,"EventListener: Handling event (%s)",event.c_str());      

  OvmsConfigParam* param = MyConfig.CachedParam("pushover");
  ConfigParamMap pmap;
  pmap = param->m_map;

  name = "ep.";
  name.append(event);
  setting = pmap[name];
  if (setting == "")
  {
    ESP_LOGD(TAG,"EventListener: No priority set -> ignore notification");
    return;
  }
  // Priority and message is saved as "priority/message" tuple
  if (setting[1]=='/') {
    pri = setting.substr(0,1);
    msg = setting.substr(2);
  } else
  if (setting[2]=='/') {
    pri = setting.substr(0,2);
    msg = setting.substr(3);
  }

  if (pri == "0")
    sound = pmap["sound.normal"];
  else if (pri == "1")
    sound = pmap["sound.high"];
  else if (pri == "2")
    sound = pmap["sound.emergency"];
  SendMessage(msg, atoi(pri.c_str()), sound);

  }


void Pushover::ConfigChanged(OvmsConfigParam* param)
  {
  }


size_t Pushover::IncomingData(uint8_t* data, size_t len)
  {
  if (m_buffer->Push(data, len))
    {
    while ((m_buffer->HasLine()>=0)&&(m_mgconn))
      {
      std::string line = m_buffer->ReadLine();
      ESP_LOGV(TAG, "Reply: %s",line.c_str());
      }
    return len;
    }
  return 0;
  }


static void PushoverMongooseCallback(struct mg_connection *nc, int ev, void *p)
  {
  switch (ev)
    {
    case MG_EV_CONNECT:
      {
      int *success = (int*)p;
      ESP_LOGI(TAG, "PushoverMongooseCallback(MG_EV_CONNECT=%d:%s)",*success, strerror(*success));
      if (*success == 0)
        {
        ESP_LOGD(TAG, "Connection successful");
        }
      else
        {
        ESP_LOGE(TAG, "Connection failed");
        }
      }
      break;
    case MG_EV_CLOSE:
      ESP_LOGD(TAG, "PushoverMongooseCallback(MG_EV_CLOSE)");
      MyPushoverClient.m_mgconn->flags |= MG_F_SEND_AND_CLOSE;
      MyPushoverClient.m_mgconn = 0;
      MyPushoverClient.m_buffer->EmptyAll();
      break;
    case MG_EV_SEND:
      ESP_LOGD(TAG, "PushoverMongooseCallback(MG_EV_SEND) %d bytes",*(uint32_t*)p);
      break;
    case MG_EV_RECV:
      ESP_LOGD(TAG, "PushoverMongooseCallback(MG_EV_RECV) %d bytes",*(uint32_t*)p);
      mbuf_remove(&nc->recv_mbuf, MyPushoverClient.IncomingData((uint8_t*)nc->recv_mbuf.buf, nc->recv_mbuf.len));
      break;
    case MG_EV_POLL:
      break;
    default:
      ESP_LOGE(TAG, "PushoverMongooseCallback - unhandled case: %d",ev);
      break;
    }
  }


bool Pushover::SendMessage( const std::string message, int priority, const std::string sound )
  {
  std::string _server = "api.pushover.net:443";
  bool parse_html = false;

  if (m_mgconn) 
    {
    ESP_LOGE(TAG,"Transmit pending!");
    return false;
    }

  ESP_LOGI(TAG,"Sending message %s with priority %d", message.c_str(), priority);

  extram::ostringstream* post = new extram::ostringstream();
  extram::ostringstream* http = new extram::ostringstream();

  // construct body
  *post << "token=" << MyConfig.GetParamValue("pushover", "token") 
        << "&user=" << MyConfig.GetParamValue("pushover", "user_key")
        << "&message=" << message 
        << "&priority=" << priority
        << "&title=" << MyConfig.GetParamValue("vehicle", "id") 
        << "&sound=" << sound;
  if (priority==2) 
    {
    int retry = MyConfig.GetParamValueInt("pushover", "retry", DEFAULT_PUSHOVER_RETRY);
    int expire = MyConfig.GetParamValueInt("pushover", "expire", DEFAULT_PUSHOVER_EXPIRE);
    *post << "&retry=" << retry
          << "&expire=" << expire;
    }
  if (parse_html == true) 
    *post << "&html=1";

  // construct header
  uint16_t len = post->str().length();
  *http << "POST /1/messages.json HTTP/1.1\r\nHost: api.pushover.net\r\nConnection: close\r\nAccept: text/html,application/xhtml+xml,application/xml;q=0.9,*/*;q=0.8\r\nContent-Length: "
    << len << "\r\n\r\n" << post->str();

  OvmsMutexLock mg(&m_mgconn_mutex);
  struct mg_mgr* mgr = MyNetManager.GetMongooseMgr();
  struct mg_connect_opts opts;
  const char* err;
  memset(&opts, 0, sizeof(opts));
  opts.error_string = &err;

  // TODO: Add root certificate. Currently Pushover server certificate is not validated!
  opts.ssl_ca_cert = "*";

  if ((m_mgconn = mg_connect_opt(mgr, _server.c_str(), PushoverMongooseCallback, opts)) == NULL)
    {
    ESP_LOGE(TAG, "mg_connect_opt(%s) failed: %s", _server.c_str(), err);
    return false;
    }

  ESP_LOGV(TAG,"msg: %s",http->str().c_str());
  mg_send(m_mgconn, http->str().c_str(), http->str().length());

  delete http;
  delete post;
  return true;
  }




