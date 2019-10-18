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
#include "dbc_app.h"
#include "ovms.h"
#include "ovms_peripherals.h"
#include "ovms_events.h"
#include "ovms_utils.h"
#include "ovms_notify.h"

re *MyRE = NULL;

const char* re_green[2][2] = { { "\x1b" "[32m", "\x1b" "[39m" }, { "<b>", "</b>" } };
const char* re_red[2][2] = { { "\x1b" "[31m", "\x1b" "[39m" }, { "<u>", "</u>" } };

void HighlightDump(char* bufferp, const char* data, size_t rlength, uint8_t red, uint8_t green, int mode = 0)
  {
  const char *s = data;
  const char *os = data;
  size_t colsize = 8;

  for (int k=0;k<colsize;k++)
    {
    uint8_t mask = (k==0)?1:(1<<k);
    if (k<rlength)
      {
      if (green & mask)
        bufferp += sprintf(bufferp, "%s%2.2x%s ", re_green[mode][0], *s, re_green[mode][1]);
      else if (red & mask)
        bufferp += sprintf(bufferp, "%s%2.2x%s ", re_red[mode][0], *s, re_red[mode][1]);
      else
        bufferp += sprintf(bufferp, "%2.2x ", *s);
      s++;
      }
    else
      {
      bufferp += sprintf(bufferp,"   ");
      }
    }
  bufferp += sprintf(bufferp,"| ");
  s = os;
  for (int k=0;k<colsize;k++)
    {
    uint8_t mask = (k==0)?1:(1<<k);
    if (k<rlength)
      {
      if (green & mask)
        bufferp += sprintf(bufferp, "%s%c%s", re_green[mode][0], isprint((int)*s) ? *s : '.', re_green[mode][1]);
      else if (red & mask)
        bufferp += sprintf(bufferp, "%s%c%s", re_red[mode][0], isprint((int)*s) ? *s : '.', re_red[mode][1]);
      else
        *bufferp++ = isprint((int)*s) ? *s : '.';
      s++;
      }
    else
      {
      *bufferp++ = ' ';
      }
    }
  *bufferp = 0;
  }

static void RE_task(void *pvParameters)
  {
  re *me = (re*)pvParameters;
  me->Task();
  }

void re::Task()
  {
  CAN_log_message_t message;

  while(1)
    {
    if (xQueueReceive(m_rxqueue, &message.frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      if (MyRE != NULL) // Protect against MyRE not set (during init)
        {
        switch (m_mode)
          {
          case Analyse:
          case Discover:
            if ((m_filter)&&(!m_filter->IsFiltered(&message.frame)))
              {
              // Frame is filtered, just drop it...
              }
            else
              {
              DoAnalyse(&message.frame);
              }
            break;
          }
        m_finished = monotonictime;
        }
      }
    }
  }

void re::DoAnalyse(CAN_frame_t* frame)
  {
  char vbuf[256];

  OvmsMutexLock lock(&m_mutex);
  std::string key = GetKey(frame);
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
      case Analyse:
        break;
      case Discover:
        r->attr.b.Discovered = 1;
        r->attr.dd = 0xff;
        HighlightDump(vbuf, (const char*)frame->data.u8, frame->FIR.B.DLC, r->attr.dc, r->attr.dd);
        ESP_LOGV(TAG, "Discovered new %s%s%s %s",
          re_green[0][0], key.c_str(), re_green[0][1], vbuf);
        break;
      }
    m_rmap[key] = r;
    }
  else
    {
    r = k->second;
    switch (MyRE->m_mode)
      {
      case Analyse:
        for (int k=0;k<r->last.FIR.B.DLC;k++)
          {
          if (r->last.data.u8[k] != frame->data.u8[k])
            r->attr.dc |= (1<<k); // Mark the byte as changed
          }
        break;
      case Discover:
        {
        bool found = false;
        for (int j=0;j<r->last.FIR.B.DLC;j++)
          {
          uint8_t mask = (j==0)?1:(1<<j);
          if (((r->attr.dc & mask)==0) &&
              (r->last.data.u8[j] != frame->data.u8[j]))
            {
            r->attr.dc |= mask; // Mark the byte as changed
            r->attr.dd |= mask; // Mark the byte as discovered
            found = true;
            }
          }
        if (found)
          {
          HighlightDump(vbuf, (const char*)frame->data.u8, frame->FIR.B.DLC, r->attr.dc, r->attr.dd);
          ESP_LOGV(TAG, "Discovered change %s %s", k->first.c_str(), vbuf);
          }
        break;
        }
      }
    }
  memcpy(&r->last,frame,sizeof(CAN_frame_t));
  r->rxcount++;
  }

std::string re::GetKey(CAN_frame_t* frame)
  {
  std::string key;
  if (frame->origin != NULL)
    key = std::string(frame->origin->GetName());
  else
    key = std::string("can?");
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

  // Check for, and process, multiplexed signal
  if (frame->origin != NULL)
    {
    dbcfile* dbc = frame->origin->GetDBC();
    if (dbc != NULL)
      {
      dbcMessage* m = dbc->m_messages.FindMessage(frame->FIR.B.FF, frame->MsgID);
      if ((m != NULL)&&(m->IsMultiplexor()))
        {
        // We have a multiplexed signal
        dbcSignal* s = m->GetMultiplexorSignal();
        dbcNumber muxn = s->Decode(frame);
        uint32_t mux = muxn.GetUnsignedInteger();
        char b[8];
        sprintf(b,":%04x",mux);
        key.append(b);
        }
      }
    }

  return key;
  }

re::re(const char* name, canfilter* filter)
  : pcp(name)
  {
  m_filter = filter;
  m_obdii_std_min = 0;
  m_obdii_std_max = 0;
  m_obdii_ext_min = 0;
  m_obdii_ext_max = 0;
  m_started = monotonictime;
  m_finished = monotonictime;
  m_mode = Analyse;
  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
  xTaskCreatePinnedToCore(RE_task, "OVMS RE", 4096, (void*)this, 5, &m_task, CORE(1));
  MyCan.RegisterListener(m_rxqueue, true);
  }

re::~re()
  {
  MyCan.DeregisterListener(m_rxqueue);

  Clear();
  vQueueDelete(m_rxqueue);
  vTaskDelete(m_task);
  if (m_filter)
    {
    delete m_filter;
    m_filter = NULL;
    }
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
    {
    canfilter* filter = NULL;
    if (argc>0)
      {
      filter = new canfilter();
      for (int k=0;k<argc;k++)
        {
        filter->AddFilter(argv[k]);
        }
      }
    MyRE = new re("re", filter);
    MyEvents.SignalEvent("retools.started", NULL);
    }
  }

void re_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    writer->puts("Error: RE tools not running");
  else
    {
    delete MyRE;
    MyRE = NULL;
    MyEvents.SignalEvent("retools.stopped", NULL);
    }
  }

void re_clear(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    writer->puts("Error: RE tools not running");
  else
    {
    MyRE->Clear();
    MyEvents.SignalEvent("retools.cleared.all", NULL);
    }
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
    if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
      {
      char vbuf[48];
      char *s = vbuf;
      FormatHexDump(&s, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, 8);
      writer->printf("%-20s %10d %6d %s\n",
        it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
      }
    }
  }

void re_stream_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("[]");
    return;
    }

  uint32_t tdiff = (MyRE->m_finished - MyRE->m_started)*1000;
  if (tdiff == 0) tdiff = 1000;

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("[");
  int cnt = 0;
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
      {
      char vbuf[48];
      char *s = vbuf;
      FormatHexDump(&s, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, 8);
      vbuf[24] = 0;
      writer->printf("%s[\"%s\",%d,%d,\"%s\",\"%s\"]\n",
        cnt ? "," : "",
        json_encode(it->first).c_str(), it->second->rxcount, (tdiff/it->second->rxcount),
        json_encode(std::string(vbuf)).c_str(),
        json_encode(std::string(vbuf+25)).c_str());
      cnt++;
      }
    }
  writer->puts("]");
  }

void re_dbc_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
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
    if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
      {
      char vbuf[48];
      char *s = vbuf;
      FormatHexDump(&s, (const char*)it->second->last.data.u8, it->second->last.FIR.B.DLC, 8);
      writer->printf("%-20s %10d %6d %s\n",
        it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
      if (it->second->last.origin)
        {
        dbcfile* dbc = it->second->last.origin->GetDBC();
        if (dbc)
          {
          // We have a DBC attached.
          dbcMessage* msg = dbc->m_messages.FindMessage(it->second->last.FIR.B.FF, it->second->last.MsgID);
          if (msg)
            {
            // Let's look for signals...
            dbcSignal* mux = msg->GetMultiplexorSignal();
            uint32_t muxval;
            if (mux)
              {
              dbcNumber r = mux->Decode(&it->second->last);
              muxval = r.GetSignedInteger();
              std::ostringstream ss;
              ss << "  dbc/mux/";
              ss << mux->GetName();
              ss << ": ";
              ss << r;
              ss << " ";
              ss << mux->GetUnit();
              writer->puts(ss.str().c_str());
              }
            for (dbcSignal* sig : msg->m_signals)
              {
              if ((mux==NULL)||(sig->GetMultiplexSwitchvalue() == muxval))
                {
                dbcNumber r = sig->Decode(&it->second->last);
                std::ostringstream ss;
                ss << "  dbc/";
                ss << sig->GetName();
                ss << ": ";
                ss << r;
                ss << " ";
                ss << sig->GetUnit();
                writer->puts(ss.str().c_str());
                }
              }
            }
          }
        }
      }
    }
  }

void re_status(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("RE: tools not running");
    return;
    }

  switch (MyRE->m_mode)
    {
    case Analyse:
      writer->puts("Mode:    Analysing");
      break;
    case Discover:
      writer->puts("Mode:    Discovering");
      break;
    }

  if (MyRE->m_filter)
    {
    writer->printf("Filter:  %s\n", MyRE->m_filter->Info().c_str());
    }

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("Key Map: %d entries\n",MyRE->m_rmap.size());
  if (MyRE->m_rmap.size() > 0)
    {
    int nignored = 0;
    int nchanged = 0;
    int bchanged = 0;
    int ndiscovered = 0;
    int bdiscovered = 0;
    for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
      {
      re_record_t *r = it->second;
      if (r->attr.b.Ignore) nignored++;
      if (r->attr.b.Changed) nchanged++;
      if (r->attr.b.Discovered) ndiscovered++;
      for (int j=0;j<8;j++)
        {
        uint8_t mask = (1<<j);
        if (r->attr.dc & mask) bchanged++;
        if (r->attr.dd & mask) bdiscovered++;
        }
      }
    writer->printf("         %d keys are ignored\n",nignored);
    writer->printf("         %d keys are new and changed\n",nchanged);
    writer->printf("         %d bytes are changed\n",bchanged);
    writer->printf("         %d keys are new and discovered\n",ndiscovered);
    writer->printf("         %d bytes are discovered\n",bdiscovered);
    }
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

void re_mode_analyse(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (!MyRE)
    {
    writer->puts("Error: RE tools not running");
    return;
    }

  MyRE->m_mode = Analyse;
  writer->puts("Now running in analyse mode");
  MyEvents.SignalEvent("retools.mode.analyse", NULL);
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
  MyEvents.SignalEvent("retools.mode.discover", NULL);
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
  MyEvents.SignalEvent("retools.cleared.changed", NULL);
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
  MyEvents.SignalEvent("retools.cleared.discovered", NULL);
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
      HighlightDump(vbuf, (const char*)it->second->last.data.u8,
        it->second->last.FIR.B.DLC, it->second->attr.dc, it->second->attr.dd);
      if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
        {
        writer->printf("%-20s %10d %6d %s\n",
          it->first.c_str(),it->second->rxcount,(tdiff/it->second->rxcount),vbuf);
        }
      }
    }
  }

void re_stream_changed(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  char vbuf[256];
  if (!MyRE)
    {
    writer->puts("[]");
    return;
    }

  uint32_t tdiff = (MyRE->m_finished - MyRE->m_started)*1000;
  if (tdiff == 0) tdiff = 1000;

  OvmsMutexLock lock(&MyRE->m_mutex);
  writer->printf("[");
  int cnt = 0;
  for (re_record_map_t::iterator it=MyRE->m_rmap.begin(); it!=MyRE->m_rmap.end(); ++it)
    {
    if ((it->second->attr.b.Changed)||(it->second->attr.dc))
      {
      HighlightDump(vbuf, (const char*)it->second->last.data.u8,
        it->second->last.FIR.B.DLC, it->second->attr.dc, it->second->attr.dd, 1);
      if ((argc==0)||(strstr(it->first.c_str(),argv[0])))
        {
        char *asc = strchr(vbuf, '|');
        *asc = 0;
        writer->printf("%s[\"%s\",%d,%d,\"%s\",\"%s\"]\n",
          cnt ? "," : "",
          json_encode(it->first).c_str(), it->second->rxcount, (tdiff/it->second->rxcount),
          json_encode(std::string(vbuf)).c_str(),
          json_encode(std::string(asc+2)).c_str());
        cnt++;
        }
      }
    }
  writer->puts("]");
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
      HighlightDump(vbuf, (const char*)it->second->last.data.u8,
        it->second->last.FIR.B.DLC, it->second->attr.dc, it->second->attr.dd);
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
  public:
    REInit();
    void Ticker1(std::string event, void* data);
} REInit  __attribute__ ((init_priority (8800)));

void REInit::Ticker1(std::string event, void* data)
  {
  if (MyRE && MyNotify.HasReader("stream", "retools.status"))
    {
    StringWriter buf;
    re_status(COMMAND_RESULT_VERBOSE, &buf, NULL, 0, NULL);
    MyNotify.NotifyString("stream", "retools.status", buf.c_str());
    }
  if (MyRE && MyNotify.HasReader("stream", "retools.list.update"))
    {
    StringWriter buf;
    re_stream_changed(COMMAND_RESULT_VERBOSE, &buf, NULL, 0, NULL);
    MyNotify.NotifyString("stream", "retools.list.update", buf.c_str());
    }
  }

REInit::REInit()
  {
  ESP_LOGI(TAG, "Initialising RE Tools (8800)");

  OvmsCommand* cmd_re = MyCommandApp.RegisterCommand("re","RE framework");
  cmd_re->RegisterCommand("start","Start RE tools",
    re_start,
    "[filter1] ... [filterN]\n"
    "Filter: <bus> | <id>[-<id>] | <bus>:<id>[-<id>]\n"
    "Example: 2:2a0-37f",
    0, 9);
  cmd_re->RegisterCommand("stop","Stop RE tools",re_stop);
  cmd_re->RegisterCommand("clear","Clear RE records",re_clear);
  cmd_re->RegisterCommand("list","List RE records",re_list, "", 0, 1);
  cmd_re->RegisterCommand("status","Show RE status",re_status);

  OvmsCommand* cmd_dbc = cmd_re->RegisterCommand("dbc","RE DBC framework");
  cmd_dbc->RegisterCommand("list","List RE DBC records",re_dbc_list, "", 0, 1);

  OvmsCommand* cmd_reobdii = cmd_re->RegisterCommand("obdii","RE OBDII framework");
  cmd_reobdii->RegisterCommand("standard","Set OBDII standard ID range",re_obdii_std, "<min> <max>", 2, 2);
  cmd_reobdii->RegisterCommand("extended","Set OBDII extended ID range",re_obdii_ext, "<min> <max>", 2, 2);

  OvmsCommand* cmd_mode = cmd_re->RegisterCommand("mode","RE mode framework");
  cmd_mode->RegisterCommand("analyse","Set mode to analyse",re_mode_analyse);
  cmd_mode->RegisterCommand("discover","Set mode to discover",re_mode_discover);

  OvmsCommand* cmd_discover = cmd_re->RegisterCommand("discover","RE discover framework");
  OvmsCommand* cmd_discover_list = cmd_discover->RegisterCommand("list","RE discover list framework");
  cmd_discover_list->RegisterCommand("changed","List changed records",re_list_changed, "", 0, 1);
  cmd_discover_list->RegisterCommand("discovered","List discovered records",re_list_discovered, "", 0, 1);
  OvmsCommand* cmd_discover_clear = cmd_discover->RegisterCommand("clear","RE discover clear framework");
  cmd_discover_clear->RegisterCommand("changed","Clear changed flags",re_clear_changed);
  cmd_discover_clear->RegisterCommand("discovered","Clear discovered flags",re_clear_discovered);

  OvmsCommand* cmd_stream = cmd_re->RegisterCommand("stream","RE JSON streaming");
  cmd_stream->RegisterCommand("list","Output array of all RE records",re_stream_list, "[<filter>]", 0, 1);
  cmd_stream->RegisterCommand("changed","Output array of changed RE records",re_stream_changed, "[<filter>]", 0, 1);

  using std::placeholders::_1;
  using std::placeholders::_2;
  MyEvents.RegisterEvent(TAG, "ticker.1", std::bind(&REInit::Ticker1, this, _1, _2));
  }
