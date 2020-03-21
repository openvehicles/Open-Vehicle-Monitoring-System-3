/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN logging framework
;    Date:          18th January 2018
;
;    (C) 2018       Michael Balzer
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
static const char *TAG = "canplay";

#include "can.h"
#include "canplay.h"
#include <sys/param.h>
#include <ctype.h>
#include <string.h>
#include <string>
#include <sstream>
#include <iomanip>
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_events.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"

////////////////////////////////////////////////////////////////////////
// Command Processing
////////////////////////////////////////////////////////////////////////

void can_play_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasPlayer())
    {
    writer->puts("Error: No players running");
    return;
    }

  if (argc==0)
    {
    // Stop all players
    writer->puts("Stopping all players");
    MyCan.RemovePlayers();
    return;
    }

  if (MyCan.RemovePlayer(atoi(argv[0])))
    {
    writer->puts("Stopped player");
    }
  else
    {
    writer->puts("Error: Cannot find specified player");
    }
  }

void can_play_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasPlayer())
    {
    writer->puts("CAN playing inactive");
    return;
    }

  if (argc>0)
    {
    canplay* cl = MyCan.GetPlayer(atoi(argv[0]));
    if (cl)
      {
      writer->printf("CAN playing active: %s\n  Statistics: %s\n", cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    else
      {
      writer->puts("Error: Cannot find specified can player");
      }
    return;
    }
  else
    {
    // Show the status of all players
    OvmsMutexLock lock(&MyCan.m_playermap_mutex);
    for (can::canplay_map_t::iterator it=MyCan.m_playermap.begin(); it!=MyCan.m_playermap.end(); ++it)
      {
      canplay* cl = it->second;
      writer->printf("CAN player #%d: %s\n  Statistics: %s\n",
        it->first, cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    }
  }

void can_play_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasPlayer())
    {
    writer->puts("CAN playing inactive");
    return;
    }
  else
    {
    // Show the list of all players
    OvmsMutexLock lock(&MyCan.m_playermap_mutex);
    for (can::canplay_map_t::iterator it=MyCan.m_playermap.begin(); it!=MyCan.m_playermap.end(); ++it)
      {
      canplay* cl = it->second;
      writer->printf("#%d: %s\n", it->first, cl->GetInfo().c_str());
      }
    }
  }

void can_play_speed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyCan.HasPlayer())
    {
    writer->puts("CAN playing inactive");
    return;
    }

  if (argc==2)
    {
    canplay* cl = MyCan.GetPlayer(atoi(argv[1]));
    if (cl)
      {
      cl->SetSpeed(atoi(argv[0]));
      writer->printf("CAN playing active: %s\n  Statistics: %s\n", cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    else
      {
      writer->puts("Error: Cannot find specified can player");
      }
    return;
    }
  else
    {
    // Set the speed for all players
    OvmsMutexLock lock(&MyCan.m_playermap_mutex);
    for (can::canplay_map_t::iterator it=MyCan.m_playermap.begin(); it!=MyCan.m_playermap.end(); ++it)
      {
      canplay* cl = it->second;
      cl->SetSpeed(atoi(argv[0]));
      writer->printf("CAN player #%d: %s\n  Statistics: %s\n",
        it->first, cl->GetInfo().c_str(), cl->GetStats().c_str());
      }
    }
  }

////////////////////////////////////////////////////////////////////////
// CAN Play System initialisation
////////////////////////////////////////////////////////////////////////

class OvmsCanPlayInit
  {
  public: OvmsCanPlayInit();
} MyOvmsCanPlayInit  __attribute__ ((init_priority (4570)));

OvmsCanPlayInit::OvmsCanPlayInit()
  {
  ESP_LOGI(TAG, "Initialising CAN play framework (4570)");

  OvmsCommand* cmd_can = MyCommandApp.FindCommand("can");
  if (cmd_can == NULL)
    {
    ESP_LOGE(TAG,"Cannot find CAN command - aborting play command registration");
    return;
    }

  OvmsCommand* cmd_canplay = cmd_can->RegisterCommand("play", "CAN play framework");
  cmd_canplay->RegisterCommand("stop", "Stop playing", can_play_stop,"[<id>]",0,1);
  cmd_canplay->RegisterCommand("speed", "Set playback speed", can_play_speed,"<speed> [<id>]",1,2);
  cmd_canplay->RegisterCommand("status", "Playing status", can_play_status,"[<id>]",0,1);
  cmd_canplay->RegisterCommand("list", "Playing list", can_play_list);
  cmd_canplay->RegisterCommand("start", "CAN play start framework");
  }

////////////////////////////////////////////////////////////////////////
// CAN Play class
////////////////////////////////////////////////////////////////////////

canplay::canplay(const char* type, std::string format, canformat::canformat_serve_mode_t mode)
  {
  m_type = type;
  m_format = format;
  m_formatter = MyCanFormatFactory.NewFormat(format.c_str());
  m_formatter->SetServeMode(mode);
  m_filter = NULL;
  m_speed = 1;

  m_msgcount = 0;
  xTaskCreatePinnedToCore(PlayTask, "OVMS CanPlay", 4096, (void*)this, 10, &m_task, CORE(1));
  }

canplay::~canplay()
  {
  if (m_task)
    {
    vTaskDelete(m_task);
    m_task = NULL;
    }

  if (m_formatter)
    {
    delete m_formatter;
    m_formatter = NULL;
    }

  if (m_filter)
    {
    delete m_filter;
    m_filter = NULL;
    }
  }

void canplay::PlayTask(void *context)
  {
//  canplay* me = (canplay*) context;

//  CAN_log_message_t msg;
//  while (me->InputMsg(&msg))
//    {
//    vTaskDelay(pdMS_TO_TICKS(1000));
//    }

//  me->Close();
  while(1)
    {
    vTaskDelay(pdMS_TO_TICKS(1000));
    }
  }

const char* canplay::GetType()
  {
  return m_type;
  }

const char* canplay::GetFormat()
  {
  return m_format.c_str();
  }

void canplay::SetSpeed(uint32_t speed)
  {
  m_speed = speed;
  }

bool canplay::InputMsg(CAN_log_message_t* msg)
  {
  return false;
  }

std::string canplay::GetInfo()
  {
  std::ostringstream buf;

  buf << "Type:" << m_type << " Format:" << m_format;
  if (m_formatter)
    {
    buf << "(" << m_formatter->GetServeModeName() << ")";
    }

  buf << " Speed:" << m_speed << "x";

  if (m_filter)
    {
    buf << " Filter:" << m_filter->Info();
    }
  else
    {
    buf << " Filter:off";
    }

  return buf.str();
  }

std::string canplay::GetStats()
  {
  std::ostringstream buf;

  buf << "total messages: " << m_msgcount;

  return buf.str();
  }

void canplay::SetFilter(canfilter* filter)
  {
  if (m_filter)
    {
    delete m_filter;
    }
  m_filter = filter;
  }

void canplay::ClearFilter()
  {
  if (m_filter)
    {
    delete m_filter;
    m_filter = NULL;
    }
  }
