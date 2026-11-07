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
static const char *TAG = "time";

#define STALETIMEOUT 120
static const char *ntp = "ntp";

#include <lwip/def.h>
#include <lwip/sockets.h>
#include <time.h>
#include "lwip/apps/sntp.h"
#include "ovms.h"
#include "ovms_time.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "ovms_netmanager.h"
#include "metrics_standard.h"
#include "ovms_http.h"
#include "cJSON.h"

OvmsTime MyTime __attribute__ ((init_priority (1500)));

void time_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  struct tm timeinfo;
  time_t rawtime;
  time ( &rawtime );
  char tb[64];
  char* tz = getenv("TZ");

  writer->printf("Time Zone:  %s\n", (tz!=NULL)?tz:"Not defined");
  if (MyConfig.GetParamValueBool("modem", "enable.autotz", false))
    {
    writer->printf("Auto TZ:    %s\n",
      MyTime.m_auto_tz.empty() ? "Pending (waiting for cellular network)" : MyTime.m_auto_tz.c_str());
    }

  struct tm* tmu = gmtime_r(&rawtime, &timeinfo);
  if (strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S UTC", tmu) > 0)
    writer->printf("UTC Time:   %s\n", tb);

  tmu = localtime_r(&rawtime, &timeinfo);
  if (strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S", tmu) > 0)
    {
    char zbuf[16];
    if (strftime(zbuf, sizeof(zbuf), "%Z", tmu) > 0 && strcmp(zbuf, "UTC") != 0 && zbuf[0] != '\0')
      writer->printf("Local Time: %s %s\n", tb, zbuf);
    else if (strftime(zbuf, sizeof(zbuf), "%z", tmu) > 0 && strcmp(zbuf, "+0000") != 0)
      {
      if (strlen(zbuf) == 5 && strcmp(zbuf + 3, "00") == 0)
        writer->printf("Local Time: %s (UTC%.3s)\n", tb, zbuf);
      else if (strlen(zbuf) == 5)
        writer->printf("Local Time: %s (UTC%.3s:%s)\n", tb, zbuf, zbuf + 3);
      else
        writer->printf("Local Time: %s (UTC%s)\n", tb, zbuf);
      }
    else
      writer->printf("Local Time: %s UTC\n", tb);
    }

  writer->printf("Provider:   %s\n",(MyTime.m_current)?MyTime.m_current->m_provider:"None");

  if (! MyTime.m_providers.empty())
    {
    writer->printf("\nPROVIDER             STRATUM  UPDATE TIME\n");
    for (OvmsTimeProviderMap_t::iterator itc=MyTime.m_providers.begin(); itc!=MyTime.m_providers.end(); itc++)
      {
      OvmsTimeProvider* tp = itc->second;
      time_t tim = tp->m_time + (monotonictime-tp->m_lastreport);
      struct tm tml;
      gmtime_r(&tim, &tml);

      char tmbuf[30];
      writer->printf("%s%-20.20s%7d%8" PRId32 " %s",
        (tp==MyTime.m_current)?"*":" ",
        tp->m_provider, tp->m_stratum, monotonictime-tp->m_lastreport,
        asctime_r( &tml, tmbuf));
      }
    }
  }

void time_set(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  struct tm tmu;
  if (strptime(argv[0], "%Y-%m-%d %H:%M:%S", &tmu) != NULL)
    {
    time_t tim = mktime(&tmu);
    MyTime.Set(TAG, 15, true, tim);
    writer->puts("Time set (at stratum 15)");
    }
  }

static std::string iana_to_posix_tz(const std::string& iana, int offset_sec)
  {
  if (iana.compare(0, 7, "Europe/") == 0)
    {
    if (iana == "Europe/London" || iana == "Europe/Dublin" || iana == "Europe/Lisbon")
      return "GMT0BST,M3.5.0/1,M10.5.0";
    if (iana == "Europe/Helsinki" || iana == "Europe/Athens" || iana == "Europe/Bucharest" ||
        iana == "Europe/Sofia" || iana == "Europe/Tallinn" || iana == "Europe/Riga" ||
        iana == "Europe/Vilnius" || iana == "Europe/Kyiv")
      return "EET-2EEST,M3.5.0/3,M10.5.0/4";
    return "CET-1CEST,M3.5.0,M10.5.0/3";
    }
  if (iana.compare(0, 8, "America/") == 0)
    {
    if (iana == "America/New_York" || iana == "America/Detroit" ||
        iana == "America/Toronto" || iana == "America/Montreal")
      return "EST5EDT,M3.2.0,M11.1.0";
    if (iana == "America/Chicago" || iana == "America/Winnipeg")
      return "CST6CDT,M3.2.0,M11.1.0";
    if (iana == "America/Denver" || iana == "America/Edmonton")
      return "MST7MDT,M3.2.0,M11.1.0";
    if (iana == "America/Los_Angeles" || iana == "America/Vancouver")
      return "PST8PDT,M3.2.0,M11.1.0";
    }
  if (iana.compare(0, 10, "Australia/") == 0)
    {
    if (iana == "Australia/Sydney" || iana == "Australia/Melbourne" || iana == "Australia/Hobart")
      return "AEST-10AEDT,M10.1.0,M4.1.0/3";
    if (iana == "Australia/Brisbane")
      return "AEST-10";
    if (iana == "Australia/Adelaide")
      return "ACST-9:30ACDT,M10.1.0,M4.1.0/3";
    if (iana == "Australia/Perth")
      return "AWST-8";
    }
  if (iana.compare(0, 5, "Asia/") == 0)
    {
    if (iana == "Asia/Tokyo") return "JST-9";
    if (iana == "Asia/Shanghai" || iana == "Asia/Hong_Kong" || iana == "Asia/Singapore" || iana == "Asia/Taipei")
      return "CST-8";
    if (iana == "Asia/Kolkata") return "IST-5:30";
    if (iana == "Asia/Dubai") return "GST-4";
    }

  int total_mins = offset_sec / 60;
  if (total_mins == 0)
    return "UTC";

  char buf[32];
  char posix_sign = (total_mins > 0) ? '-' : '+';
  int abs_mins = abs(total_mins);
  int hours = abs_mins / 60;
  int mins = abs_mins % 60;
  if (mins != 0)
    snprintf(buf, sizeof(buf), "UTC%c%d:%02d", posix_sign, hours, mins);
  else
    snprintf(buf, sizeof(buf), "UTC%c%d", posix_sign, hours);
  return std::string(buf);
  }

void time_autoset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  float lat = StdMetrics.ms_v_pos_latitude->AsFloat();
  float lon = StdMetrics.ms_v_pos_longitude->AsFloat();
  bool has_gps = (lat != 0.0f || lon != 0.0f);

  if (!has_gps)
    {
    // GPS not available - fall back to cellular network (NITZ) timezone
    if (!MyTime.m_auto_tz.empty())
      {
      MyConfig.SetParamValue("vehicle", "timezone", MyTime.m_auto_tz);
      writer->printf("GPS not available; Timezone set from cellular network (NITZ) to %s\n", MyTime.m_auto_tz.c_str());
      return;
      }
    else
      {
      writer->puts("Error: Neither GPS location fix nor cellular network timezone (NITZ) available");
      return;
      }
    }

  if (!MyNetManager.m_connected_any)
    {
    // Network offline for HTTP API - fall back to NITZ if available
    if (!MyTime.m_auto_tz.empty())
      {
      MyConfig.SetParamValue("vehicle", "timezone", MyTime.m_auto_tz);
      writer->printf("Network offline for API; Timezone set from cellular network (NITZ) to %s\n", MyTime.m_auto_tz.c_str());
      return;
      }
    writer->puts("Error: Network connection not available");
    return;
    }

  char url[160];
  snprintf(url, sizeof(url),
    "http://api.open-meteo.com/v1/forecast?latitude=%.4f&longitude=%.4f&timezone=auto",
    lat, lon);

  OvmsHttpClient http(5000);
  if (!http.Request(url))
    {
    if (!MyTime.m_auto_tz.empty())
      {
      MyConfig.SetParamValue("vehicle", "timezone", MyTime.m_auto_tz);
      writer->printf("HTTP API failed (code %d); Timezone set from cellular network (NITZ) to %s\n",
        http.ResponseCode(), MyTime.m_auto_tz.c_str());
      return;
      }
    writer->printf("Error: HTTP request failed (code %d)\n", http.ResponseCode());
    return;
    }

  std::string body = http.GetBodyAsString();
  cJSON* root = cJSON_Parse(body.c_str());
  if (!root)
    {
    if (!MyTime.m_auto_tz.empty())
      {
      MyConfig.SetParamValue("vehicle", "timezone", MyTime.m_auto_tz);
      writer->printf("API response parse failed; Timezone set from cellular network (NITZ) to %s\n", MyTime.m_auto_tz.c_str());
      return;
      }
    writer->puts("Error: Failed to parse API response JSON");
    return;
    }

  cJSON* offset_item = cJSON_GetObjectItem(root, "utc_offset_seconds");
  if (!offset_item)
    {
    cJSON_Delete(root);
    if (!MyTime.m_auto_tz.empty())
      {
      MyConfig.SetParamValue("vehicle", "timezone", MyTime.m_auto_tz);
      writer->printf("API response invalid; Timezone set from cellular network (NITZ) to %s\n", MyTime.m_auto_tz.c_str());
      return;
      }
    writer->puts("Error: Missing utc_offset_seconds in API response");
    return;
    }

  int offset_sec = offset_item->valueint;
  cJSON* tz_item = cJSON_GetObjectItem(root, "timezone");
  std::string tz_name = (tz_item && tz_item->valuestring) ? tz_item->valuestring : "";
  cJSON_Delete(root);

  std::string tz_str = iana_to_posix_tz(tz_name, offset_sec);

  MyTime.m_auto_tz = tz_str;
  MyConfig.SetParamValue("vehicle", "timezone", tz_str);
  writer->printf("Timezone set to %s (%s, offset %+d sec)\n", tz_str.c_str(), tz_name.c_str(), offset_sec);
  }

extern "C"
  {
  void sntp_setsystemtime_us(uint32_t s, uint32_t us)
    {
    MyTime.Set(ntp, 1, true, s, us);
    }
  }

OvmsTimeProvider::OvmsTimeProvider(const char* provider, int stratum, time_t tim)
  {
  m_provider = provider;
  m_time = tim;
  m_lastreport = monotonictime;
  m_stratum = stratum;
  }

OvmsTimeProvider::~OvmsTimeProvider()
  {
  }

void OvmsTimeProvider::Set(int stratum, time_t tim)
  {
  m_stratum = stratum;
  m_time = tim;
  m_lastreport = monotonictime;
  }

OvmsTime::OvmsTime()
  {
  ESP_LOGI(TAG, "Initialising TIME (1500)");

  m_current = NULL;
  m_tz.tz_minuteswest = 0;
  m_tz.tz_dsttime = 0;

  // SNTP defaults
  sntp_setoperatingmode(SNTP_OPMODE_POLL);
  sntp_setservername(0, (char*)"pool.ntp.org");

  // Register our commands
  OvmsCommand* cmd_time = MyCommandApp.RegisterCommand("time","TIME framework",time_status, "", 0, 0, false);
  cmd_time->RegisterCommand("status","Show time status",time_status,"", 0, 0, false);
  cmd_time->RegisterCommand("set","Set current UTC time",time_set,"<time>", 1, 1);
  cmd_time->RegisterCommand("autoset","Lookup and set timezone from GPS coordinates via HTTP API",time_autoset,"", 0, 0);

  #undef bind  // Kludgy, but works
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG,"system.start", std::bind(&OvmsTime::EventSystemStart, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"config.changed", std::bind(&OvmsTime::EventConfigChanged, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"ticker.60", std::bind(&OvmsTime::EventTicker60, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.up", std::bind(&OvmsTime::EventNetUp, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.down", std::bind(&OvmsTime::EventNetDown, this, _1, _2));
  MyEvents.RegisterEvent(TAG,"network.reconfigured", std::bind(&OvmsTime::EventNetReconfigured, this, _1, _2));
  }

OvmsTime::~OvmsTime()
  {
  }

void OvmsTime::UpdateTimezone()
  {
  std::string tz;
  bool autotz = MyConfig.GetParamValueBool("modem", "enable.autotz", false);
  if (autotz && !m_auto_tz.empty())
    tz = m_auto_tz;
  else
    tz = MyConfig.GetParamValue("vehicle", "timezone");

  setenv("TZ", tz.c_str(), 1);
  tzset();
  }

void OvmsTime::SetAutoTimezone(const std::string& tz)
  {
  if (m_auto_tz != tz)
    {
    ESP_LOGI(TAG, "Automatic timezone received: %s", tz.c_str());
    m_auto_tz = tz;
    if (MyConfig.GetParamValueBool("modem", "enable.autotz", false))
      {
      UpdateTimezone();
      }
    }
  }

void OvmsTime::EventConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* p = (OvmsConfigParam*)data;

  if (p->GetName().compare("vehicle")==0 || p->GetName().compare("modem")==0)
    {
    UpdateTimezone();
    }
  }

void OvmsTime::EventSystemStart(std::string event, void* data)
  {
  UpdateTimezone();
  }

void OvmsTime::EventTicker60(std::string event, void* data)
  {
  // Refresh SNTP, if possible
  if (sntp_enabled() && MyNetManager.m_connected_any)
    {
    auto k = m_providers.find(ntp);
    if (k != m_providers.end())
      {
      // Refresh it
      time_t rawtime;
      time ( &rawtime );
      k->second->m_time = rawtime;
      k->second->m_lastreport = monotonictime;
      }
    }
    Elect();
  }

void OvmsTime::EventNetUp(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Starting SNTP client");
  sntp_init();
  }

void OvmsTime::EventNetDown(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Stopping SNTP client");
  sntp_stop();
  }

void OvmsTime::EventNetReconfigured(std::string event, void* data)
  {
  ESP_LOGI(TAG, "Network was reconfigured: restarting SNTP client");
  sntp_stop();
  sntp_init();
  }

void OvmsTime::Elect()
  {
  int best=255;
  OvmsTimeProvider* c = NULL;

  for (OvmsTimeProviderMap_t::iterator itc=MyTime.m_providers.begin(); itc!=MyTime.m_providers.end(); itc++)
    {
    if ((itc->second->m_stratum < best)&&(itc->second->m_lastreport+STALETIMEOUT > monotonictime))
      {
      best = itc->second->m_stratum;
      c = itc->second;
      }
    }

  MyTime.m_current = c;
  }

void OvmsTime::Set(const char* provider, int stratum, bool trusted, time_t tim, suseconds_t timu)
  {
  if (!trusted) stratum=16;

  struct tm tmu;
  gmtime_r(&tim,&tmu);
  char timebuf[30];
  ESP_LOGD(TAG, "%s (stratum %d trusted %d) provides time %-24.24s (%06luus) UTC", provider, stratum, trusted,
    asctime_r(&tmu, timebuf),timu);

  auto k = m_providers.find(provider);
  if (k == m_providers.end())
    {
    m_providers[provider] = new OvmsTimeProvider(provider,stratum,tim);
    Elect();
    k = m_providers.find(provider);
    }

  if (k != m_providers.end())
    {
    k->second->Set(stratum,tim);

    // Check for stale master
    if ((m_current)&&(m_current->m_lastreport+STALETIMEOUT > monotonictime))
      {
      // Master is stale
      Elect();
      }

    if (k->second == m_current)
      {
      time_t rawtime;
      time ( &rawtime );
      if (tim != rawtime)
        {
        struct timeval tv;
        tv.tv_sec = tim;
        tv.tv_usec = timu;
        settimeofday(&tv, &m_tz);
        }
      }
    }
  }
