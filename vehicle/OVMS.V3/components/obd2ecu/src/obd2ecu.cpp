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
;    (C) 2017       Gregory Dolkas
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
static const char *TAG = "obd2ecu";

#include <string.h>
#include <dirent.h>
#include "obd2ecu.h"
#include "ovms_script.h"
#include "ovms_config.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "metrics_standard.h"


obd2pid::obd2pid(int pid, pid_t type, OvmsMetric* metric)
  {
  m_pid = pid;
  m_type = type;
  m_script = NULL;
  m_metric = metric;
  }

obd2pid::~obd2pid()
  {
  if (m_script)
    {
    free(m_script);
    m_script = NULL;
    }
  }

int obd2pid::GetPid()
  {
  return m_pid;
  }

obd2pid::pid_t obd2pid::GetType()
  {
  return m_type;
  }

const char* obd2pid::GetTypeString()
  {
  switch (m_type)
    {
    case Unimplemented:  return "unimplemented";
    case Internal:       return "internal";
    case Metric:         return "metric";
    case Script:         return "script";
    default:             return "unknown";
    }
  }

OvmsMetric* obd2pid::GetMetric()
  {
  return m_metric;
  }

void obd2pid::SetType(pid_t type)
  {
  m_type = type;
  }

void obd2pid::SetMetric(OvmsMetric* metric)
  {
  m_metric = metric;
  }

void obd2pid::LoadScript(std::string path)
  {
  FILE* f = fopen(path.c_str(), "r");
  if (f == NULL) return;

  fseek(f, 0L, SEEK_END);
  size_t fsz = ftell(f);
  fseek(f, 0L, SEEK_SET);

  if (m_script)
    {
    free(m_script);
    m_script = NULL;
    }
  m_script = (char*)ExternalRamMalloc(fsz+1);
  fread(m_script, 1, fsz, f);
  m_script[fsz] = 0;

  fclose(f);
  }

float obd2pid::Execute()
  {
  switch (m_type)
    {
    case Unimplemented:   // PIDs requested by device but not already known
      return 0;
    case Internal:        // Pre-configured PIDs
    case Metric:          // PIDs defined or redefined by Config command
      if (m_metric)
        return m_metric->AsFloat();
      else
        return 0.0;
    case Script:
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      {
      return MyScripts.DuktapeEvalFloatResult(m_script);
      }
#else // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      return 0;
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    default:
      return 0;
    }
  }


static void OBD2ECU_task(void *pvParameters)
  {
  obd2ecu *me = (obd2ecu*)pvParameters;

  CAN_frame_t frame;
  while(1)
    {
    if (xQueueReceive(me->m_rxqueue, &frame, (portTickType)portMAX_DELAY)==pdTRUE)
      {
      // Only handle incoming frames on our CAN bus
      if (frame.origin == me->m_can) me->IncomingFrame(&frame);
      }
    }
  }

obd2ecu::obd2ecu(const char* name, canbus* can)
  : pcp(name)
  {
  m_can = can;
  m_can->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  m_can->SetPowerMode(On);

  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));

  m_starttime = time(NULL);
  LoadMap();

  xTaskCreatePinnedToCore(OBD2ECU_task, "OVMS OBDII ECU", 6144, (void*)this, 5, &m_task, CORE(1));

  MyCan.RegisterListener(m_rxqueue);
  }

obd2ecu::~obd2ecu()
  {
  m_can->SetPowerMode(Off);
  MyCan.DeregisterListener(m_rxqueue);

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_task);

  ClearMap();
  }

void obd2ecu::SetPowerMode(PowerMode powermode)
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

void obd2ecu_start(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (! MyPeripherals->m_obd2ecu)
    {
    const char* bus = cmd->GetName();
    MyPeripherals->m_obd2ecu = new obd2ecu("OBD2ECU", (canbus*)MyPcpApp.FindDeviceByName(bus));
    writer->puts("OBDII ECU has been started");
    }
  }

void obd2ecu_stop(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_obd2ecu)
    {
    delete MyPeripherals->m_obd2ecu;
    MyPeripherals->m_obd2ecu = NULL;
    writer->puts("OBDII ECU has been stopped");
    }
  }


void obd2ecu_list(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_obd2ecu == NULL)
    {
    writer->puts("Need to start ecu process first");
    return;
    }

  writer->printf("%-7s %14s %12s %s\n","  PID","Type","Value","   Metric");

  for (PidMap::iterator it=MyPeripherals->m_obd2ecu->m_pidmap.begin(); it!=MyPeripherals->m_obd2ecu->m_pidmap.end(); ++it)
    {
    if ((argc==0)||(it->second->GetPid() == atoi(argv[0])))
      {
      const char *ms;
      if (it->second->GetMetric() && (it->second->GetType() != obd2pid::Script))
        ms = it->second->GetMetric()->m_name;
      else
        ms = "";

      writer->printf("%-3d (0x%02x) %14s %12f %s\n",
        it->first, it->first,
        it->second->GetTypeString(),
        it->second->Execute(),
        ms);
      }
    }
  }

void obd2ecu_reload(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  if (MyPeripherals->m_obd2ecu == NULL)
    {
    writer->puts("Need to start ecu process first");
    return;
    }

  MyPeripherals->m_obd2ecu->LoadMap();
  writer->puts("OBDII ECU pid map reloaded");
  }


  /* PID Format Table  (see: https://en.wikipedia.org/widi/OBD-II_PIDs )

00 = no data (return zeros)
01 = A (one byte)
02 = Percent:  100*A/255
03 = 16 bit integer divided by 4:  (256*A+B)/4
04 = A offset by 40:  A-40
05 = 16 bit unsigned integer:  256*A+B
06 = 0 to 655.35:  (256*A+B)/100
07 = 3A:  A*3
08 = A/2 - 64:  (A/2)-64
09 = 100A/128-100
10 = Bit Vector - A / B / C / D  (NOTE: only have 6 digits of precision!)

99 = not implemented

*/

/* Index this table by PID */

static uint8_t pid_format[] =
{10,  // 00 PIDs supported
 10,  // 01 Monitor status since DTCs cleared (bit vector)
  0,  // 02 Freeze DTC
 10,  // 03 Fuel System Status
  2,  // 04 Engine load
  4,  // 05 Coolant Temperature
  9,  // 06 short term fuel trim bank 1
  9,  // 07 long term fuel trim bank 1
  9,  // 08 short term fuel trim bank 2
  9,  // 09 long term fuel trim bank 2
  7,  // 10 Fuel pressure
  1,  // 11 Intake manifold pressure
  3,  // 12 Engine RPM
  1,  // 13 vehicle speed
  8,  // 14 Timing Advance
  4,  // 15 Intake air Temperature
  6,  // 16 MAF Air flow rate
  2,  // 17 Throttle position
 10,  // 18 Commanded secondary air status
 99,  // 19 O2 sensors present (huh?)
 99,  // 20 O2 sensor 1
 99,  // 21 O2 sensor 2
 99,  // 22 O2 sensor 3
 99,  // 23 O2 sensor 4
 99,  // 24 O2 sensor 5
 99,  // 25 O2 sensor 6
 99,  // 26 O2 sensor 7  (PIDs 40 and above not supported)
 99,  // 27 O2 sensor 8
 10,  // 28 OBD standards
 99,  // 29 O2 sensors present (huh?)
 99,  // 30 PTO status
  5,  // 31 Run time since engine start
 10,  // 32 PIDs supported
  5,  // 33 Distance traveled with check engine light on
 99,  // 34
 99,  // 35
 99,  // 36
 99,  // 37
 99,  // 38
 99,  // 39
 99,  // 40
 99,  // 41
 99,  // 42
 99,  // 43
 99,  // 44
 99,  // 45
 99,  // 46
  2,  // 47 Fuel tank level
 99,  // 48
 99,  // 49
 99,  // 50
  1,  // 51 Barometric pressure
 99,  // 52
 99,  // 53
 99,  // 54
 99,  // 55
 99,  // 56
 99,  // 57
 99,  // 58
 99,  // 59
 99,  // 60
 99,  // 61
 99,  // 62
 99,  // 63
 10   // 64 PIDs supported
};

//
// Fill Mode 1 frames with data based on format specified
//
void obd2ecu::FillFrame(CAN_frame_t *frame,int reply,uint8_t pid,float data,uint8_t format)
  {
  uint8_t a,b,c,d;
  int i;

  /* Formats are defined as "Formula" on Wikipedia "OBD-II_PIDs" page. */
  /* Note that we need to apply the inverse, to derive the A and B values from the intended metric */

  a=b=c=d=0;

  switch(format)
    {
    case 0:  /* no data (send zeros) */
      break;

    case 1:  /* A only (one byte) */
      a = (int)data;
      break;

    case 2:  /* Percent: 100*A/255 */
      a = (int)((data*255)/100);
      break;

    case 3:  /* 16 bit integer divided by 4: (256*A+B)/4  */
      if (data > 16383.75)
        {
        ESP_LOGD(TAG, "Data out of range %f",data);
        a = b = 0xff;  /* use max value */
        break;
        }
      i = (int)data*4;
      a = (i&0xff00)>>8;
      b = i&0xff;
      break;

    case 4:  /* A offset by 40:  A-40  */
      a = (int)data+40;
      break;

    case 5:  /* 16 bit unsigned data:  256*A+B */
      a = ((int)data&0xff00)>>8;
      b = (int)data&0xff;
      break;

    case 6:  /* 0 to 655.35:  (256*A+B)/100 */
      if (data > 655.35)
        {
        ESP_LOGD(TAG, "Data out of range %f",data);
        a = b = 0xff;  /* use max value */
        break;
        }
      i = (int)data*100;
      a = (i&0xff00)>>8;
      b = i&0xff;
      break;

    case 7:  /* 3A:  A*3  */
      a = data/3.0;
      break;

    case 8:  /* A/2 - 64:  (A/2)-64 */
      a = (data+64)*2.0;
      break;

    case 9:  /* (100*A)/128 - 100 */
      a = (data+100.0)*1.28;
      break;

    case 10:  /* A, B, C, D Bit vector */

      a = (((int)data) >> 24) & 0xff;
      b = (((int)data) >> 16) & 0xff;
			c = (((int)data) >> 8) & 0xff;
      d = ((int)data) & 0xff;
      break;

    case 99:  /* Unimplemented but not an error. return zeros */
      break;

    default:
      ESP_LOGD(TAG, "Unsupported PID format %d",format);
      /* default to empty data */
      break;
    }

  frame->origin = NULL;
  frame->FIR.U = 0;
  frame->FIR.B.DLC = 8;
  frame->FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
  frame->MsgID = reply;

  frame->data.u8[0] = 6;		/* # additional bytes (ok to have extra) */
  frame->data.u8[1] = 0x41;   /* Mode 1 + 0x40 indicating a reply */
  frame->data.u8[2] = pid;
  frame->data.u8[3] = a;
  frame->data.u8[4] = b;
  frame->data.u8[5] = c;
  frame->data.u8[6] = d;
  frame->data.u8[7] = 0x55;   /* pad 0x55 */

  return;
  }


void obd2ecu::IncomingFrame(CAN_frame_t* p_frame)
  {
  CAN_frame_t r_frame = {};  /* build the response frame here */
  uint32_t reply;
  int jitter;
  uint8_t mapped_pid;
  float metric;
  char rtn_string[21];

  uint8_t *p_d = p_frame->data.u8;  /* Incoming frame data from HUD / Dongle */
  uint8_t *r_d = r_frame.data.u8;  /* Response frame data being sent back to HUD / Dongle */

  ESP_LOGD(TAG, "Rcv %x: %x (%x %x %x %x %x %x %x %x)",
                      p_frame->MsgID,
                      p_frame->FIR.B.DLC,
                      p_d[0],p_d[1],p_d[2],p_d[3],p_d[4],p_d[5],p_d[6],p_d[7]);

  /* handle both standard and extended frame types - return like for like */
  if (p_frame->MsgID == REQUEST_PID) reply = RESPONSE_PID;
  else if (p_frame->MsgID == REQUEST_EXT_PID) reply = RESPONSE_EXT_PID;
       else
       { /* check for flow control frames - they're received on the response MsgID minus 8 */
         if ((p_frame->MsgID == FLOWCONTROL_PID || p_frame->MsgID == FLOWCONTROL_EXT_PID) && p_frame->data.u8[0] == 0x30)
         {  ESP_LOGD(TAG, "flow control frame - ignored");
           return;  /* ignore it.  We just sleep for a bit instead */
         }
         /* if none of the above, no idea what it is.  Ignore */
         ESP_LOGD(TAG, "unknown MsgID %x",p_frame->MsgID);
         return;
       }

  jitter = time(NULL)&0xf;  /* 0-15 range for simulation purposes */

  switch(p_d[1])  /* switch on the incoming frame mode */
    {
    case 1:  /* Mode 1 (main real-time PIDs are here */

      mapped_pid = p_d[2];
      if (m_pidmap.find(mapped_pid) != m_pidmap.end()) // m_pidmap[pid] contains the obd2pid object to work with
      { metric = m_pidmap[mapped_pid]->Execute();
      }
      else
      { if (MyConfig.GetParamValueBool("obd2ecu","autocreate"))
          m_pidmap[mapped_pid] = new obd2pid(mapped_pid); // Creates it as Unimplemented, if enabled
          // note: don't 'Addpid' the PID to the supported vectors.  Only done when support set by config.
        metric = 0.0;
      }

      switch (mapped_pid)  /* switch on the what the requested PID was (before mapping!) */
        {
        case 0:  /* request capabilities PIDs 01-0x20 */

          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = p_frame->FIR.B.FF;
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x00;
          r_d[3] = (m_supported_01_20 >> 24) & 0xff;
          r_d[4] = (m_supported_01_20 >> 16) & 0xff;
          r_d[5] = (m_supported_01_20 >> 8) & 0xff;
          r_d[6] =  m_supported_01_20 & 0xff;
          r_d[7] = 0x55;  /* pad 0x55 */
          m_can->Write(&r_frame);
          break;

        case 1: /* request status since DTC Cleared */
          /* Note: Even setting [7]=0xff and DTC count=0, the dongle still requests DTC stuff. */
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x01;
          r_d[3] = 0x00;  /* report all clear, no tests */
          r_d[4] = 0x00;
          r_d[5] = 0x00;
          r_d[6] = 0x00;
          r_d[7] = 0xff;  /* nothing ready yet (or ever) */
          m_can->Write(&r_frame);
          break;

        case 0x0c:	/* Engine RPM */
          /* This item (only) needs to vary to prevent SyncUp Drive dongle from going to sleep */
          /* Also a Minimum "idle" RPM, but only if not moving, for HUD device */

          // Test if metric is from a script; if so, don't do the dongle workarounds (script will do this if needed)
          if(m_pidmap[mapped_pid]->GetType() != obd2pid::Script)
          { metric = metric+jitter;
            if(StandardMetrics.ms_v_pos_speed->AsFloat() < 1.0) metric = 500+jitter;
          }

          FillFrame(&r_frame,reply,mapped_pid,metric,pid_format[mapped_pid]);
          m_can->Write(&r_frame);
          break;

        case 0x10:	/* MAF (Mass Air flow) rate - Map to SoC */
          /* For some reason, the HUD uses this param as a proxy for fuel rate */
          /* HUD devices seem to have a display range of 0-19.9 */
          /* Scaling provides a 1:1 metric pass-through, so be aware of limmits of the display device */
          /* Use with display set to L/hr (not L/km).  Note: scripting this metric is not pre-scaled. */

          if(m_pidmap[mapped_pid]->GetType() != obd2pid::Script) metric = metric*3.0;
          FillFrame(&r_frame,reply,mapped_pid,metric,pid_format[mapped_pid]);
          m_can->Write(&r_frame);
          break;

        case 0x20:  /* request more capabilities, PIDs 0x21 - 0x40 */
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x20;
          r_d[3] = (m_supported_21_40 >> 24) & 0xff;
          r_d[4] = (m_supported_21_40 >> 16) & 0xff;
          r_d[5] = (m_supported_21_40 >> 8) & 0xff;
          r_d[6] =  m_supported_21_40 & 0xff;
          r_d[7] = 0x55;  /* pad 0x55 */
          m_can->Write(&r_frame);
          break;

        case 0x40:  /* request more capabilities: none
            (would need to expand the pid_format table to do so) */
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x40;
          r_d[3] = 0x00;
          r_d[4] = 0x00;
          r_d[5] = 0x00;
          r_d[6] = 0x00;
          r_d[7] = 0x55;  /* pad 0x55 */
          m_can->Write(&r_frame);
          break;

        default:  /* most PIDs get processed here */
          if(mapped_pid > sizeof(pid_format))
          { ESP_LOGI(TAG, "unknown capability requested %x",r_frame.data.u8[2]);
            break;
          }

          FillFrame(&r_frame,reply,mapped_pid,metric,pid_format[mapped_pid]);
          m_can->Write(&r_frame);

	}
      break;

    case 9:
      switch (p_frame->data.u8[2])
        {
        case 2:
          ESP_LOGD(TAG, "Requested VIN");

          if(MyConfig.GetParamValueBool("obd2ecu","private"))   /* ignore request for privacy's sake. Doesn't seem to matter to Dongle. */
          { ESP_LOGD(TAG, "VIN request ignored");
            break;
          }

          memcpy(rtn_string,StandardMetrics.ms_v_vin->AsString().c_str(),17);
          rtn_string[17] = '\0';  /* force null termination, just because */

          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 0x10;  /* not sure what this is for */
          r_d[1] = 0x14;  /* overall length, starting with next byte, not including pad */
          r_d[2] = 0x49;  /* Mode 9 reply */
          r_d[3] = 0x02;  /* PID */
          r_d[4] = 0x01;  /* not sure what this is for */
          memcpy(&r_d[5],rtn_string,3);  /* grab the first 3 bytes of VIN */

          m_can->Write(&r_frame);

          vTaskDelay(10 / portTICK_PERIOD_MS);  /* let the flow control frame pass */

          r_d[0] = 0x21;
          memcpy(&r_d[1],rtn_string+3,7);  /* grab the next 7 bytes of VIN */

          m_can->Write(&r_frame);

          r_d[0] = 0x22;
          memcpy(&r_d[1],rtn_string+10,7);  /* grab the last 7 bytes of VIN */

          m_can->Write(&r_frame);

          break;

        case 0x0a: /* ECU Name */
          ESP_LOGD(TAG, "ECU Name requested");
          /* Perhaps a good place for arbitrary text, e.g. fleet asset #?  20 char avail. */

          memcpy(rtn_string,MyConfig.GetParamValue("vehicle","id","").c_str(),20);
          rtn_string[20] = '\0';

          for(int i=strlen(rtn_string);i<20;i++) rtn_string[i] = '\0';  // zero pad string, per spec

          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 0x10;  /* not sure what this is for */
          r_d[1] = 0x17;  /* overall length, starting with next byte, not including pad */
          r_d[2] = 0x49;  /* Mode 9 reply */
          r_d[3] = 0x0a;  /* PID */
          r_d[4] = 0x01;  /* not sure what this is for */
          memcpy(&r_d[5],rtn_string,3);  /* grab the first 3 bytes of Vehicle ID */

          m_can->Write(&r_frame);

          vTaskDelay(10 / portTICK_PERIOD_MS);  /* let the flow control frame pass */

          r_d[0] = 0x21;
          memcpy(&r_d[1],rtn_string+3,7);  /* grab the next 7 bytes of Vehicle ID */

          m_can->Write(&r_frame);

          r_d[0] = 0x22;
          memcpy(&r_d[1],rtn_string+10,7);  /* grab next 7 bytes of Vehicle ID */

          m_can->Write(&r_frame);

          r_d[0] = 0x23;
          memcpy(&r_d[1],rtn_string+17,3);  /* grab last 3 bytes of Vehicle ID */

          r_d[4] = 0x00;  /* Spec says 0 Pad... */
          r_d[5] = 0x00;
          r_d[6] = 0x00;
          r_d[7] = 0x00;

          m_can->Write(&r_frame);

          break;

        default:
          ESP_LOGD(TAG, "unknown ID=9 frame %x",p_d[2]);
        }
        break;

    case 0x03:  /* request DTCs */
      ESP_LOGD(TAG, "Request DTCs; ignored");
      break;

    case 0x07: /* pending DTCs */
      ESP_LOGD(TAG, "Pending DTCs; ignored");
      break;

    case 0x0a:  /* permanent / cleared DTCs */
      ESP_LOGD(TAG, "permanent / cleared DTCs; ignored");
      break;

    default:
      ESP_LOGD(TAG, "Unknown Mode %x",p_d[1]);
    }

  return;
  }

void obd2ecu::LoadMap()
  {
  ClearMap();
  // Create default PID maps
  m_pidmap[0x00] = new obd2pid(0x00,obd2pid::Internal);                                 // PIDs 1-20 supported (internally)
  // PID 00 is assumed; don't addpid it
  m_pidmap[0x04] = new obd2pid(0x04,obd2pid::Internal,StandardMetrics.ms_v_bat_soc);    // Engine load (use as proxy for SoC)
  Addpid(0x04);
  m_pidmap[0x05] = new obd2pid(0x05,obd2pid::Internal,StandardMetrics.ms_v_mot_temp);   // Coolant Temperature (use motor temp)
  Addpid(0x05);
  m_pidmap[0x0c] = new obd2pid(0x0c,obd2pid::Internal,StandardMetrics.ms_v_mot_rpm);    // Engine RPM
  Addpid(0x0c);
  m_pidmap[0x0d] = new obd2pid(0x0d,obd2pid::Internal,StandardMetrics.ms_v_pos_speed);  // Vehicle Speed
  Addpid(0x0d);
  m_pidmap[0x10] = new obd2pid(0x10,obd2pid::Internal,StandardMetrics.ms_v_bat_12v_voltage);  // Mass Air Flow  (Note: display limited 0-19.9 on HUDs)
  Addpid(0x10);
  m_pidmap[0x20] = new obd2pid(0x20,obd2pid::Internal);                                 // PIDs 21-40 supported (internally)
  Addpid(0x20);

  // Look for metric overrides...
  OvmsConfigParam* cm = MyConfig.CachedParam("obd2ecu.map");
  for (ConfigParamMap::iterator it=cm->m_map.begin(); it!=cm->m_map.end(); ++it)
    {
    int pid = atoi(it->first.c_str());

    OvmsMetric* m = MyMetrics.Find(it->second.c_str());
    if ((pid>0) && m)
      {
      ESP_LOGI(TAG, "Using custom metric for pid #%d (0x%02x)",pid,pid);
      if (m_pidmap.find(pid) == m_pidmap.end())
      { m_pidmap[pid] = new obd2pid(pid,obd2pid::Metric,m);
      }
      else
        { ESP_LOGI(TAG, "Updating custom metric for pid #%d (0x%02x)",pid,pid);
          m_pidmap[pid]->SetType(obd2pid::Metric);
          m_pidmap[pid]->SetMetric(m);
        }
      }
      else if(pid) ESP_LOGI(TAG, "Metric '%s' not found; ignored.",it->second.c_str());
    }

  // Look for scripts (if javascript enabled)...
  #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  DIR *dir;
  struct dirent *dp;
  if ((dir = opendir ("/store/obd2ecu")) != NULL)
    {
    while ((dp = readdir (dir)) != NULL)
      {
      int pid = atoi(dp->d_name);
      ESP_LOGI(TAG, "Using custom scripting for pid #%d (0x%02x)",pid,pid);
      if (pid)
        {
        std::string fpath("/store/obd2ecu/");
        fpath.append(dp->d_name);
        if (m_pidmap.find(pid) == m_pidmap.end())
          m_pidmap[pid] = new obd2pid(pid,obd2pid::Script);
        else
          m_pidmap[pid]->SetType(obd2pid::Script);
        m_pidmap[pid]->LoadScript(fpath);
        }
      }
    closedir(dir);
    }
  #endif //#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
  }

void obd2ecu::ClearMap()
  {
  for (PidMap::iterator it=m_pidmap.begin(); it!=m_pidmap.end(); ++it)
    {
    delete it->second;
    }
  m_pidmap.clear();
  m_supported_01_20 = 0;
  m_supported_21_40 = 0;

  }
/* procedure to add a PID to the vectors of supported PIDS, used with PID 0 & 0x20 */

void obd2ecu::Addpid(uint8_t pid)
{
  if(pid == 0) return;  // pid 0 assumed

  if(pid <= 0x20)       // PIDs 1-20
  { m_supported_01_20 |= 1 << (32-pid);
    ESP_LOGD(TAG, "Added 0x%02x resulting 0x%08x",pid,m_supported_01_20);
    return;
  }

  if(pid <= 0x40)        // PIDs 21-40
  { m_supported_21_40 |= 1 << (32-(pid-0x20));
    return;
  }

  ESP_LOGI(TAG, "PID %d (0x%02x) unsupportable",pid,pid);
  return;
}

class obd2ecuInit obd2ecuInit __attribute__ ((init_priority (7000)));

obd2ecuInit::obd2ecuInit()
  {
  ESP_LOGI(TAG, "Initialising OBD2ECU (7000)");

  OvmsCommand* cmd_obdii = MyCommandApp.RegisterCommand("obdii","OBDII framework");
  OvmsCommand* cmd_ecu = cmd_obdii->RegisterCommand("ecu","OBDII ECU framework");
  OvmsCommand* cmd_start = cmd_ecu->RegisterCommand("start","Start an OBDII ECU");
  cmd_start->RegisterCommand("can1","start an OBDII ECU on can1",obd2ecu_start);
  cmd_start->RegisterCommand("can2","Start an OBDII ECU on can2",obd2ecu_start);
  cmd_start->RegisterCommand("can3","Start an OBDII ECU on can3",obd2ecu_start);
  cmd_ecu->RegisterCommand("stop","Stop the OBDII ECU",obd2ecu_stop);
  cmd_ecu->RegisterCommand("list","Show OBDII ECU pid list",obd2ecu_list, "", 0, 1);
  cmd_ecu->RegisterCommand("reload","Reload OBDII ECU pid map",obd2ecu_reload);

  MyConfig.RegisterParam("obd2ecu", "OBD2ECU configuration", true, true);
  MyConfig.RegisterParam("obd2ecu.map", "OBD2ECU metric map", true, true);
  }

void obd2ecuInit::AutoInit()
  {
  std::string busname = MyConfig.GetParamValue("auto", "obd2ecu", "");
  if (!busname.empty())
    {
    canbus* bus = (canbus*) MyPcpApp.FindDeviceByName(busname.c_str());
    if (bus)
      MyPeripherals->m_obd2ecu = new obd2ecu("OBD2ECU", bus);
    else
      ESP_LOGE(TAG, "AutoInit: unknown CAN bus name '%s'", busname.c_str());
    }
  }
