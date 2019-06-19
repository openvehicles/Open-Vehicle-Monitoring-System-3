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
static const char *TAG = "canlog-vfs";

#include "can.h"
#include "canformat.h"
#include "canlog_vfs.h"
#include "ovms_config.h"
#include "ovms_peripherals.h"

void can_log_vfs_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  std::string format(cmd->GetName());
  canlog_vfs* logger = new canlog_vfs(argv[0],format);
  logger->Open();

  if (logger->IsOpen())
    {
    if (argc>1)
      { MyCan.SetLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.SetLogger(logger); }
    writer->printf("CAN logging to VFS active: %s\n", logger->GetInfo().c_str());
    }
  else
    {
    writer->printf("Error: Could not start CAN logging to: %s\n", logger->GetInfo().c_str());
    delete logger;
    }
  }

class OvmsCanLogVFSInit
  {
  public: OvmsCanLogVFSInit();
} MyOvmsCanLogVFSInit  __attribute__ ((init_priority (4560)));

OvmsCanLogVFSInit::OvmsCanLogVFSInit()
  {
  ESP_LOGI(TAG, "Initialising CAN logging to VFS (4560)");

  OvmsCommand* cmd_can = MyCommandApp.FindCommand("can");
  if (cmd_can)
    {
    OvmsCommand* cmd_can_log = cmd_can->FindCommand("log");
    if (cmd_can_log)
      {
      OvmsCommand* cmd_can_log_start = cmd_can_log->FindCommand("start");
      if (cmd_can_log_start)
        {
        // We have a place to put our command tree..
        OvmsCommand* start = cmd_can_log_start->RegisterCommand("vfs", "CAN logging to VFS");
        MyCanFormatFactory.RegisterCommandSet(start, "Start CAN logging to VFS",
          can_log_vfs_start,
          "<format> <path> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  }

canlog_vfs::canlog_vfs(std::string path, std::string format)
  : canlog("vfs", format)
  {
  m_file = NULL;
  m_path = path;
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "sd.mounted", std::bind(&canlog_vfs::MountListener, this, _1, _2));
  MyEvents.RegisterEvent(TAG, "sd.unmounting", std::bind(&canlog_vfs::MountListener, this, _1, _2));
  }

canlog_vfs::~canlog_vfs()
  {
  MyEvents.DeregisterEvent(TAG);

  if (m_file != NULL)
    {
    Close();
    }
  }

bool canlog_vfs::Open()
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

  m_file = fopen(m_path.c_str(), "w");
  if (!m_file)
    {
    ESP_LOGE(TAG, "Error: Can't write to '%s'", m_path.c_str());
    return false;
    }

  LogInfo(NULL, CAN_LogInfo_Config, GetInfo().c_str());
  ESP_LOGI(TAG, "Now logging CAN messages to '%s'", m_path.c_str());

  std::string header = m_formatter->getheader();
  if (header.length()>0)
    fwrite(header.c_str(),header.length(),1,m_file);

  return true;
  }

void canlog_vfs::Close()
  {
  if (m_file)
    {
    fclose(m_file);
    m_file = NULL;
    ESP_LOGI(TAG, "Closed vfs log '%s': %s",
      m_path.c_str(), GetStats().c_str());
    }
  }

bool canlog_vfs::IsOpen()
  {
  return (m_file != NULL);
  }

std::string canlog_vfs::GetInfo()
  {
  std::string result = canlog::GetInfo();
  result.append(" Path:");
  result.append(m_path);
  return result;
  }

void canlog_vfs::MountListener(std::string event, void* data)
  {
  if (event == "sd.unmounting" && startsWith(m_path, "/sd"))
    Close();
  else if (event == "sd.mounted" && startsWith(m_path, "/sd"))
    Open();
  }

void canlog_vfs::OutputMsg(CAN_log_message_t& msg)
  {
  if (m_file == NULL) return;
  if (m_formatter == NULL) return;

  std::string result = m_formatter->get(&msg);
  if (result.length()>0)
    fwrite(result.c_str(),result.length(),1,m_file);
  }
