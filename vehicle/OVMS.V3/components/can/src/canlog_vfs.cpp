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
#include "ovms_utils.h"
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
      { MyCan.AddLogger(logger, argc-1, &argv[1]); }
    else
      { MyCan.AddLogger(logger); }
    writer->printf("CAN logging to VFS active: %s\n", logger->GetInfo().c_str());
    MyCan.LogInfo(NULL, CAN_LogInfo_Config, logger->GetInfo().c_str());
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
          "<path> [filter1] ... [filterN]\n"
          "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
          "Example: 2:2a0-37f",
          1, 9);
        }
      }
    }
  }

#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

canlog_vfs_conn::canlog_vfs_conn(canlog* logger, std::string format, canformat::canformat_serve_mode_t mode)
  : canlogconnection(logger, format, mode)
  {
  m_file = NULL;
  }

canlog_vfs_conn::~canlog_vfs_conn()
  {
  if (m_file)
    {
    fclose(m_file);
    m_file = NULL;
    }
  }

void canlog_vfs_conn::OutputMsg(CAN_log_message_t& msg, std::string &result)
  {
  m_msgcount++;

  if ((m_filters != NULL) && (! m_filters->IsFiltered(&msg.frame)))
    {
    m_filtercount++;
    return;
    }

  if (result.length()>0)
    fwrite(result.c_str(),result.length(),1,m_file);
  }

#endif //#ifdef CONFIG_OVMS_SC_GPL_MONGOOSE

canlog_vfs::canlog_vfs(std::string path, std::string format)
  : canlog("vfs", format)
  {
  m_path = path;
  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(IDTAG, "sd.mounted", std::bind(&canlog_vfs::MountListener, this, _1, _2));
  MyEvents.RegisterEvent(IDTAG, "sd.unmounting", std::bind(&canlog_vfs::MountListener, this, _1, _2));
  }

canlog_vfs::~canlog_vfs()
  {
  MyEvents.DeregisterEvent(IDTAG);

  if (m_isopen)
    {
    Close();
    }
  }

bool canlog_vfs::Open()
  {
  OvmsRecMutexLock lock(&m_cmmutex);

  if (m_isopen)
    {
    for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
      {
      delete it->second;
      }
    m_connmap.clear();
    m_isopen = false;
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

  canlog_vfs_conn* clc = new canlog_vfs_conn(this, m_format, m_mode);
  clc->m_nc = NULL;
  clc->m_peer = m_path;

  clc->m_file = fopen(m_path.c_str(), "w");
  if (!clc->m_file)
    {
    ESP_LOGE(TAG, "Error: Can't write to '%s'", m_path.c_str());
    delete clc;
    return false;
    }

  ESP_LOGI(TAG, "Now logging CAN messages to '%s'", m_path.c_str());

  std::string header = m_formatter->getheader();
  if (header.length()>0)
    fwrite(header.c_str(),header.length(),1,clc->m_file);

  m_connmap[NULL] = clc;
  m_isopen = true;

  return true;
  }

void canlog_vfs::Close()
  {
  if (m_isopen)
    {
    ESP_LOGI(TAG, "Closed vfs log '%s': %s",
      m_path.c_str(), GetStats().c_str());

    OvmsRecMutexLock lock(&m_cmmutex);
    for (conn_map_t::iterator it=m_connmap.begin(); it!=m_connmap.end(); ++it)
      {
      delete it->second;
      }
    m_connmap.clear();

    m_isopen = false;
    }
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
