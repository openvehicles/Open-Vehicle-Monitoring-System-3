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

OvmsTime MyTime __attribute__ ((init_priority (1500)));

void time_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  time_t rawtime;
  time ( &rawtime );
  char tb[64];
  char* tz = getenv("TZ");

  writer->printf("Time Zone:  %s\n", (tz!=NULL)?tz:"Not defined");

  struct tm* tmu = gmtime(&rawtime);
  if (strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S UTC", tmu) > 0)
    writer->printf("UTC Time:   %s\n", tb);

  tmu = localtime(&rawtime);
  if (strftime(tb, sizeof(tb), "%Y-%m-%d %H:%M:%S %Z", tmu) > 0)
    writer->printf("Local Time: %s\n", tb);

  writer->printf("Provider:   %s\n",(MyTime.m_current)?MyTime.m_current->m_provider:"None");

  if (! MyTime.m_providers.empty())
    {
    writer->printf("\nPROVIDER             STRATUM  UPDATE TIME\n");
    for (OvmsTimeProviderMap_t::iterator itc=MyTime.m_providers.begin(); itc!=MyTime.m_providers.end(); itc++)
      {
      OvmsTimeProvider* tp = itc->second;
      time_t tim = tp->m_time + (monotonictime-tp->m_lastreport);
      struct tm* tml = gmtime(&tim);
      writer->printf("%s%-20.20s%7d%8d %s",
        (tp==MyTime.m_current)?"*":" ",
        tp->m_provider, tp->m_stratum, monotonictime-tp->m_lastreport, asctime(tml));
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

void OvmsTime::EventConfigChanged(std::string event, void* data)
  {
  OvmsConfigParam* p = (OvmsConfigParam*)data;

  if (p->GetName().compare("vehicle")==0)
    {
    std::string tz = MyConfig.GetParamValue("vehicle","timezone");
    setenv("TZ", tz.c_str(), 1);
    tzset();
    }
  }

void OvmsTime::EventSystemStart(std::string event, void* data)
  {
  std::string tz = MyConfig.GetParamValue("vehicle","timezone");
  setenv("TZ", tz.c_str(), 1);
  tzset();
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

  struct tm* tmu = gmtime(&tim);
  ESP_LOGD(TAG, "%s (stratum %d trusted %d) provides time %-24.24s (%06luus) UTC", provider, stratum, trusted, asctime(tmu),timu);

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
