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
static const char *TAG = "canplay-vfs";

#include "can.h"
#include "canformat.h"
#include "canplay_vfs.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"

void can_play_vfs_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  canplay_vfs* player = new canplay_vfs(argv[0],format);
  player->Open();

  if (player->IsOpen())
    {
    if (argc>1)
      { MyCan.AddPlayer(player, argc-1, &argv[1]); }
    else
      { MyCan.AddPlayer(player); }
    writer->printf("CAN playing from VFS active: %s\n", player->GetInfo().c_str());
    }
  else
    {
    writer->printf("Error: Could not start CAN playing from: %s\n", player->GetInfo().c_str());
    delete player;
    }
  }

class OvmsCanPlayVFSInit
  {
  public: OvmsCanPlayVFSInit();
} MyOvmsCanPlayVFSInit  __attribute__ ((init_priority (4580)));

OvmsCanPlayVFSInit::OvmsCanPlayVFSInit()
  {
  ESP_LOGI(TAG, "Initialising CAN playing from VFS (4580)");

  OvmsCommand* cmd_can = MyCommandApp.FindCommand("can");
  if (cmd_can)
    {
    OvmsCommand* cmd_can_play = cmd_can->FindCommand("play");
    if (cmd_can_play)
      {
      OvmsCommand* cmd_can_play_start = cmd_can_play->FindCommand("start");
      if (cmd_can_play_start)
        {
        // We have a place to put our command tree..
        OvmsCommand* start = cmd_can_play_start->RegisterCommand("vfs", "CAN playing from VFS");
        MyCanFormatFactory.RegisterCommandSet(start, "Start CAN playing from VFS",
          can_play_vfs_start,
          "<path> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  }

canplay_vfs::canplay_vfs(std::string path, std::string format)
  : canplay("vfs", format)
  {
  m_file = NULL;
  m_path = path;
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "sd.mounted", std::bind(&canplay_vfs::MountListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "sd.unmounting", std::bind(&canplay_vfs::MountListener, this, _1, _2));
  }

canplay_vfs::~canplay_vfs()
  {
  MyEvents.DeregisterEvent(TAG);

  if (m_file != NULL)
    {
    Close();
    }
  }

bool canplay_vfs::Open()
  {
  if (m_file)
    {
    fclose(m_file);
    m_file = NULL;
    }

  if (MyConfig.ProtectedPath(m_path))
    {
    ESP_LOGE(TAG, "Error: Path '%s' is protected and cannot be opened", m_path.c_str());
    return false;
    }

#ifdef CONFIG_OVMS_COMP_SDCARD
  if (startsWith(m_path, "/sd") && (!MyPeripherals || !MyPeripherals->m_sdcard || !MyPeripherals->m_sdcard->isavailable()))
    {
    ESP_LOGE(TAG, "Error: Cannot open '%s' as SD filesystem not available", m_path.c_str());
    return false;
    }
#endif // #ifdef CONFIG_OVMS_COMP_SDCARD

  m_file = fopen(m_path.c_str(), "r");
  if (!m_file)
    {
    ESP_LOGE(TAG, "Error: Can't read from '%s'", m_path.c_str());
    return false;
    }

  ESP_LOGI(TAG, "Now playing CAN messages from '%s'", m_path.c_str());

  return true;
  }

void canplay_vfs::Close()
  {
  if (m_file)
    {
    fclose(m_file);
    m_file = NULL;
    ESP_LOGI(TAG, "Closed vfs playback '%s': %s",
      m_path.c_str(), GetStats().c_str());
    }
  }

bool canplay_vfs::IsOpen()
  {
  return (m_file != NULL);
  }

std::string canplay_vfs::GetInfo()
  {
  std::string result = canplay::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canplay_vfs::MountListener(std::string event, void* data)
  {
  if (event == "sd.unmounting" && startsWith(m_path, "/sd"))
    Close();
  else if (event == "sd.mounted" && startsWith(m_path, "/sd"))
    Open();
  }

bool canplay_vfs::InputMsg(CAN_log_message_t* msg)
  {
  if (m_file == NULL) return false;
  if (m_formatter == NULL) return false;

  return false;
  }
