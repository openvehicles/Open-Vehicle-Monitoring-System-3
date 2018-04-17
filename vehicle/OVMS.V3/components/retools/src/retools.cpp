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
static const char *TAG = "re";

#include <string.h>
#include "retools.h"
#include "ovms_peripherals.h"
#include "ovms.h"
#include "ovms_utils.h"

re *MyRE = NULL;

const char* re_green = "\x1b" "[32m";
const char* re_red = "\x1b" "[31m";
const char* re_off = "\x1b" "[39m";

int HighlightDump(char* bufferp, const char* data, size_t rlength, uint8_t red, uint8_t green)
  {
  const char *s = data;
  size_t colsize = 8;

  if (rlength>0)
    {
    const char *os = s;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        if (green & (1<<k))
          {
          strcat(bufferp,re_green);
          bufferp += strlen(re_green);
          }
        else if (red & (1<<k))
          {
          strcat(bufferp,re_red);
          bufferp += strlen(re_red);
          }
        sprintf(bufferp,"%2.2x ",*s);
        bufferp += 3;
        if ((green | red) & (1<<k))
          {
          strcat(bufferp,re_off);
          bufferp += strlen(re_off);
          }
        s++;
        }
      else
        {
        sprintf(bufferp,"   ");
        bufferp += 3;
        }
      }
    sprintf(bufferp,"| ");
    bufferp += 2;
    s = os;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        if (green & (1<<k))
          {
          strcat(bufferp,re_green);
          bufferp += strlen(re_green);
          }
        else if (red & (1<<k))
          {
          strcat(bufferp,re_red);
          bufferp += strlen(re_red);
          }
        if (isprint((int)*s))
          { *bufferp = *s; }
        else
          { *bufferp = '.'; }
        bufferp++;
        if ((green | red) & (1<<k))
          {
          strcat(bufferp,re_off);
          bufferp += strlen(re_off);
          }
        s++;
        }
      else
        {
        *bufferp = ' ';
        bufferp++;
        }
      }
    *bufferp = 0;
    rlength -= colsize;
    }

  return rlength;
  }

static void RE_task(void *pvParameters)
  {
  re *me = (re*)pvParameters;
  me->Task();
  }

void re::Task()
  {
  CAN_frame_t frame;
  char vbuf[256];

  while(1)
    {
    if (xQueueReceive(m_rxqueue, &frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      OvmsMutexLock lock(&m_mutex);
      std::string key = GetKey(&frame);
      auto k = m_rmap.find(key);
      re_record_t* r;
      if (m_rmap.size() == 0) m_started = monotonictime;
      if (k == m_rmap.end())
        {
        r = k->second;
        r = new re_record_t;
        memset(r,0,sizeof(re_record_t));
        r->attr.b.Changed = 1; // Mark the whole ID as changed
        r->attr.dc = 0xff;
        switch (MyRE->m_mode)
          {
          case Record:
            break;
          case Discover:
            r->attr.b.Discovered = 1;
            r->attr.dd = 0xff;
            HighlightDump(vbuf, (const char*)frame.data.u8, frame.FIR.B.DLC, r->attr.dc, r->attr.dd);
            ESP_LOGV(TAG, "Discovered new %s %s", key.c_str(), vbuf);
            break;
          }
        m_rmap[key] = r;
        }
      else
        {
        r = k->second;
        switch (MyRE->m_mode)
          {
          case Record:
            for (int k=0;k<r->last.FIR.B.DLC;k++)
              {
              if (r->last.data.u8[k] != frame.data.u8[k])
                r->attr.dc |= (1<<k); // Mark the byte as changed
              }
            break;
          case Discover:
            for (int j=0;j<r->last.FIR.B.DLC;j++)
              {
              if (((r->attr.dc & (1<<j))==0) && (r->last.data.u8[j] != frame.data.u8[j]))
                {
                r->attr.dc |= (1<<j); // Mark the byte as changed
                r->attr.dd |= (1<<j); // Mark the byte as discovered
                }
              }
            if (r->attr.dd != 0)
              {
              HighlightDump(vbuf, (const char*)frame.data.u8, frame.FIR.B.DLC, r->attr.dc, r->attr.dd);
              ESP_LOGV(TAG, "Discovered change %s %s", k->first.c_str(), vbuf);
              }
            break;
          }
        }
      memcpy(&r->last,&frame,sizeof(frame));
      r->rxcount++;
      m_finished = monotonictime;
      // ESP_LOGI(TAG,"rx Key=%s Count=%d",key.c_str(),r->rxcount);
      }
    }
  }

std::string re::GetKey(CAN_frame_t* frame)
  {
  std::string key(frame->origin->GetName());
  key.append("/");

  char id[9];
  if (frame->FIR.B.FF == CAN_frame_std)
    sprintf(id,"%03x",frame->MsgID);
  else
    sprintf(id,"%08x",frame->MsgID);
  key.append(id);

  if (((m_obdii_std_min>0) &&
       (frame->FIR.B.FF == CAN_frame_std) &&
       (frame->MsgID >= m_obdii_std_min) &&
       (frame->MsgID <= m_obdii_std_max))
      ||
       ((m_obdii_ext_min>0) &&
       (frame->FIR.B.FF == CAN_frame_ext) &&
       (frame->MsgID >= m_obdii_ext_min) &&
       (frame->MsgID <= m_obdii_ext_max)))
    {
    // It is an OBDII request
    if (frame->data.u8[0] > 8)
      {
      // Probably just a continuation frame. Ignore it.
      return key;
      }
    uint8_t mode = frame->data.u8[1];
    char req[16];
    if (mode > 0x4a)
      {
      sprintf(req,":O2Pm%d:%d",mode-0x40,((int)frame->data.u8[2]<<8)+frame->data.u8[3]);
      }
    else if (mode > 0x40)
      {
      sprintf(req,":O2Pm%d:%d",mode-0x40,(int)frame->data.u8[2]);
      }
    else if (mode > 0x0a)
      {
      sprintf(req,":O2Qm%d:%d",mode,((int)frame->data.u8[2]<<8)+frame->data.u8[3]);
      }
    else
      {
      sprintf(req,":O2Qm%d:%d",mode,(int)frame->data.u8[2]);
      }
    key.append(req);
    return key;
    }

  auto k = m_idmap.find(frame->MsgID);
  if (k != m_idmap.end())
    {
    uint8_t bytes = m_idmap[frame->MsgID];
    for (int j=0;j<8;j++)
      {
      if (bytes & (1<<j))
        {
        char b[4];
        sprintf(b,":%02x",frame->data.u8[j]);
        key.append(b);
        }
      }
    }

  return key;
  }

re::re(const char* name)
  : pcp(name)
  {
  m_obdii_std_min = 0;
  m_obdii_std_max = 0;
  m_obdii_ext_min = 0;
  m_obdii_ext_max = 0;
  m_started = monotonictime;
  m_finished = monotonictime;
  m_mode = Record;
  xTaskCreatePinnedToCore(RE_task, "OVMS RE", 4096, (void*)this, 5, &m_task, 1);
  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
  MyCan.RegisterListener(m_rxqueue);
  }

re::~re()
  {
  MyCan.DeregisterListener(m_rxqueue);

  Clear();
  vQueueDelete(m_rxqueue);
  vTaskDelete(m_task);
  }

void re::SetPowerMode(PowerMode powermode)
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

void re::Clear()
  {
  OvmsMutexLock lock(&m_mutex);
  for (re_record_map_t::iterator it=m_rmap.begin(); it!=m_rmap.end(); ++it)
    {
    delete it->second;
    }
  m_rmap.clear();
  m_started = monotonictime;
  m_finished = monotonictime;
  }

void re_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyRE)
    writer->puts("Error: RE tools already running");
  else
    MyRE = new re("re");
  }

void re_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    writer->puts("Error: RE tools not running");
  else
    {
    delete MyRE;
    MyRE = NULL;
    }
  }

void re_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    writer->puts("Error: RE tools not running");
  else
    MyRE->Clear();
  }

void re_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  uint32_t tdiff = (MyRE->m_finished - MyRE->m_started)*1000;
  if (tdiff == 0) tdiff = 1000;

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("%-20.20s %10s %6s %s\n","key","records","ms","last");
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    char vbuf[48];
    char *s = vbuf;
    FormatHexDump(&s, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, 8);
    if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
      {
      writer->printf("%-20s %10d %6d %s\n",
        it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
      }
    }
  }

void re_keyclear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  uint32_t id = (uint32_t)strtol(argv[0],NULL,16);
  OvmsMutexLock lock(&MyRE->m_mutex);
  auto k = MyRE->m_idmap.find(id);
  if (k != MyRE->m_idmap.end())
    {
    MyRE->m_idmap.erase(k);
    writer->puts("Cleared ID key");
    }
  }

void re_keyset(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  uint32_t id = (uint32_t)strtol(argv[0],NULL,16);
  uint8_t bytes = 0;
  for (int k=1;k<argc;k++)
    {
    int b = atoi(argv[k]);
    if ((b>=1)&&(b<=8))
      {
      bytes |= (1<<(b-1));
      }
    }
  OvmsMutexLock lock(&MyRE->m_mutex);
  MyRE->m_idmap[id] = bytes;
  writer->printf("Set ID %x to bytes 0x%02x\n",id);
  }

void re_obdii_std(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  MyRE->m_obdii_std_min = (uint32_t)strtol(argv[0],NULL,16);
  MyRE->m_obdii_std_max = (uint32_t)strtol(argv[1],NULL,16);

  writer->printf("Set OBDII standard ID range %03x-%03x\n",MyRE->m_obdii_std_min,MyRE->m_obdii_std_max);
  }

void re_obdii_ext(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  MyRE->m_obdii_ext_min = (uint32_t)strtol(argv[0],NULL,16);
  MyRE->m_obdii_ext_max = (uint32_t)strtol(argv[1],NULL,16);

  writer->printf("Set OBDII extended ID range %08x-%08x\n",MyRE->m_obdii_ext_min,MyRE->m_obdii_ext_max);
  }

void re_mode_record(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  MyRE->m_mode = Record;
  writer->puts("Now running in record mode");
  }

void re_mode_discover(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  OvmsMutexLock lock(&MyRE->m_mutex);
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    it->second->attr.b.Discovered = 0;
    it->second->attr.dd = 0;
    }

  MyRE->m_mode = Discover;
  writer->puts("Now running in discover mode");
  }

void re_clear_changed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  OvmsMutexLock lock(&MyRE->m_mutex);
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    it->second->attr.b.Changed = 0;
    it->second->attr.dc = 0;
    }

  writer->puts("Cleared all change flags");
  }

void re_clear_discovered(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  OvmsMutexLock lock(&MyRE->m_mutex);
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    it->second->attr.b.Discovered = 0;
    it->second->attr.dd = 0;
    }

  writer->puts("Cleared all discover flags");
  }

void re_list_changed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  char vbuf[256];
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  uint32_t tdiff = (MyRE->m_finished - MyRE->m_started)*1000;
  if (tdiff == 0) tdiff = 1000;

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("%-20.20s %10s %6s %s\n","key","records","ms","last");
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    if ((it->second->attr.b.Changed)||(it->second->attr.dc))
      {
      HighlightDump(vbuf, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, it->second->attr.dc, 0);
      if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
        {
        writer->printf("%-20s %10d %6d %s\n",
          it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
        }
      }
    }
  }

void re_list_discovered(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  char vbuf[256];
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  uint32_t tdiff = (MyRE->m_finished - MyRE->m_started)*1000;
  if (tdiff == 0) tdiff = 1000;

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("%-20.20s %10s %6s %s\n","key","records","ms","last");
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    if ((it->second->attr.b.Discovered)||(it->second->attr.dd))
      {
      HighlightDump(vbuf, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, 0, it->second->attr.dd);
      if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
        {
        writer->printf("%-20s %10d %6d %s\n",
          it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
        }
      }
    }
  }

class REInit
  {
  public: REInit();
} REInit  __attribute__ ((init_priority (8800)));

REInit::REInit()
  {
  ESP_LOGI(TAG, "Initialising RE Tools (8800)");

  OvmsCommand* cmd_re = MyCommandApp.RegisterCommand("re","RE framework",NULL, "", 0, 0, true);
  cmd_re->RegisterCommand("start","Start RE tools",re_start, "", 0, 0, true);
  cmd_re->RegisterCommand("stop","Stop RE tools",re_stop, "", 0, 0, true);
  cmd_re->RegisterCommand("clear","Clear RE records",re_clear, "", 0, 0, true);
  cmd_re->RegisterCommand("list","List RE records",re_list, "", 0, 1, true);
  OvmsCommand* cmd_rekey = cmd_re->RegisterCommand("key","RE KEY framework",NULL, "", 0, 0, true);
  cmd_rekey->RegisterCommand("clear","Clear RE key",re_keyclear, "<id>", 1, 1, true);
  cmd_rekey->RegisterCommand("set","Set RE key",re_keyset, "<id> {<bytes>}", 2, 9, true);
  OvmsCommand* cmd_reobdii = cmd_re->RegisterCommand("obdii","RE OBDII framework",NULL, "", 0, 0, true);
  cmd_reobdii->RegisterCommand("standard","Set OBDII standard ID range",re_obdii_std, "<min> <max>", 2, 2, true);
  cmd_reobdii->RegisterCommand("extended","Set OBDII extended ID range",re_obdii_ext, "<min> <max>", 2, 2, true);
  OvmsCommand* cmd_mode = cmd_re->RegisterCommand("mode","RE mode framework",NULL, "", 0, 0, true);
  cmd_mode->RegisterCommand("record","Set mode to record",re_mode_record, "", 0, 0, true);
  cmd_mode->RegisterCommand("discover","Set mode to discover",re_mode_discover, "", 0, 0, true);
  OvmsCommand* cmd_discover = cmd_re->RegisterCommand("discover","RE discover framework",NULL, "", 0, 0, true);
  OvmsCommand* cmd_discover_list = cmd_discover->RegisterCommand("list","RE discover list framework",NULL, "", 0, 0, true);
  cmd_discover_list->RegisterCommand("changed","List changed records",re_list_changed, "", 0, 1, true);
  cmd_discover_list->RegisterCommand("discovered","List discovered records",re_list_discovered, "", 0, 1, true);
  OvmsCommand* cmd_discover_clear = cmd_discover->RegisterCommand("clear","RE discover clear framework",NULL, "", 0, 0, true);
  cmd_discover_clear->RegisterCommand("changed","Clear changed flags",re_clear_changed, "", 0, 0, true);
  cmd_discover_clear->RegisterCommand("discovered","Clear discovered flags",re_clear_discovered, "", 0, 0, true);
  }
