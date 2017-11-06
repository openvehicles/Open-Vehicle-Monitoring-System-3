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
  m_metric = metric;
  }

obd2pid::~obd2pid()
  {
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
  m_script.clear();
  char buf[128];
  while(size_t n = fread(buf, sizeof(char), sizeof(buf), f))
    {
    m_script.append(buf,n);
    }
  fclose(f);
  }

float obd2pid::Execute()
  {
  // TODO:
  // For internal, we should put the code here to implement the PID
  // For external, we should run the script
  switch (m_type)
    {
    case Unimplemented:
      return 0;
    case Internal:
      return InternalPid();
    case Metric:
      if (m_metric)
        return m_metric->AsFloat();
      else
        return 0;
    case Script:
#ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      {
      duk_context *ctx = MyScripts.Duktape();
      duk_eval_string(ctx, m_script.c_str());
      return (float)duk_get_number(ctx,-1);
      }
#else // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
      return 0;
#endif // #ifdef CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE
    default:
      return 0;
    }
  }

float obd2pid::InternalPid()
  {
    if (m_metric) return m_metric->AsFloat();
    else
    { printf("InternalPID %x not a metric\n",m_pid);
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
  xTaskCreatePinnedToCore(OBD2ECU_task, "OBDII ECU Task", 4096, (void*)this, 5, &m_task, 1);
 
  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));

  m_can->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  m_can->SetPowerMode(On);

  m_starttime = time(NULL);

  LoadMap();

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

  writer->printf("%-7s %14s %12s %s\n","PID","Type","Value","Metric");

  for (PidMap::iterator it=MyPeripherals->m_obd2ecu->m_pidmap.begin(); it!=MyPeripherals->m_obd2ecu->m_pidmap.end(); ++it)
    {
    if ((argc==0)||(it->second->GetPid() == atoi(argv[0])))
      {
      const char *ms;
      if (it->second->GetMetric())
        ms = it->second->GetMetric()->m_name;
      else
        ms = "";
      writer->printf("%-7d %14s %12f %s\n",
        it->first,
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
10 = Bit Vector - A / B / C / D

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
 99,  // 26 O2 sensor 7
 99,  // 27 O2 sensor 8
 10,  // 28 OBD standards
 99,  // 29 O2 sensors present (huh?)
 99,  // 30 PTO status
  3,  // 31 Run time since engine start
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
      if (verbose && (data > 16383.75)) 
        {
        ESP_LOGI(TAG, "Data out of range %f",data);
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
      if (verbose && data > 655.35) 
        {
        ESP_LOGI(TAG, "Data out of range %f",data);
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
      if (verbose) ESP_LOGI(TAG, "Unsupported format %d",format);
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
  CAN_frame_t r_frame;  /* build the response frame here */
  uint32_t reply;
  int jitter;
  uint8_t mapped_pid;
  float metric;
  time_t since_start;
  char vin_string[18];

  uint8_t *p_d = p_frame->data.u8;  /* Incoming frame data from HUD / Dongle */
  uint8_t *r_d = r_frame.data.u8;  /* Response frame data being sent back to HUD / Dongle */

  if (verbose)
    ESP_LOGI(TAG, "Rcv %x: %x (%x %x %x %x %x %x %x %x)",
                      p_frame->MsgID,
                      p_frame->FIR.B.DLC,
                      p_d[0],p_d[1],p_d[2],p_d[3],p_d[4],p_d[5],p_d[6],p_d[7]);

  /* handle both standard and extended frame types - return like for like */
  if (p_frame->MsgID == REQUEST_PID) reply = RESPONSE_PID;
  else if (p_frame->MsgID == REQUEST_EXT_PID) reply = RESPONSE_EXT_PID;
       else
       { /* check for flow control frames - they're received on the response MsgID minus 8 */
         if ((p_frame->MsgID == FLOWCONTROL_PID || p_frame->MsgID == FLOWCONTROL_EXT_PID) && p_frame->data.u8[0] == 0x30)
         {  if (verbose) ESP_LOGI(TAG, "flow control frame - ignored");
           return;  /* ignore it.  We just sleep for a bit instead */
         }
         /* if none of the above, no idea what it is.  Ignore */
         ESP_LOGI(TAG, "unknown MsgID %x",p_frame->MsgID);
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
        m_pidmap[mapped_pid] = new obd2pid(mapped_pid); // Creates it as Unimplemented, by default
        metric = 0.0;
      }
      
      switch (mapped_pid)  /* switch on the what the requested PID maps to. */
        {
        case 0:  /* request capabilities */
          /* This is a bitmap of the Mode 1 PIDs that we will act on from the HUD/Dongle */
// TODO: need to create proper bitmap based on what PIDs are actually mapped          
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = p_frame->FIR.B.FF;
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x00;
          r_d[3] = 0x18;  /* 04, 05 */
          r_d[4] = 0x3b;  /* 0b, 0c, 0d, 0f, 10 */
          r_d[5] = 0x80;  /* 11 */
          r_d[6] = 0x02;  /* 1f */	
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
          
          metric = metric*70.0+jitter;  //TODO: remove this when real RPM metric is created
          
// TODO: test if metric is from a script; if so, don't do the dongle workarounds (script will do this if needed)     

          if(metric < 1.0) metric = 500+jitter; 
          FillFrame(&r_frame,reply,mapped_pid,metric,pid_format[mapped_pid]);
          m_can->Write(&r_frame);
          break;

        case 0x10:	/* MAF (Mass Air flow) rate - Map to SoC */
          /* For some reason, the HUD uses this param as a proxy for fuel rate */
          /* HUD devices seem to have a display range of 0-19.9 */
          /* Scaling provides a 1:1 metric pass-through, so be aware of limmits of the display device */
          /* Use with display set to L/hr (not L/km).  */
          metric = metric*3.0;
          FillFrame(&r_frame,reply,mapped_pid,metric,pid_format[mapped_pid]);
          m_can->Write(&r_frame);
          break;
			
        case 0x20:  /* request more capabilities */
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
          r_frame.MsgID = reply;
          r_d[0] = 6;		/* # additional bytes (ok to have extra) */
          r_d[1] = 0x41;  /* Mode 1 + 0x40 indicating a reply */
          r_d[2] = 0x20;
          r_d[3] = 0x08;  /* 21 */
          r_d[4] = 0x02;  /* 2f */
          r_d[5] = 0x80;  /* 31 */
          r_d[6] = 0x00;	
          r_d[7] = 0x55;  /* pad 0x55 */
          m_can->Write(&r_frame);
          break;
          
        case 0x40:  /* request more capabilities: none */
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
          
        case 0x1f:	/* Runtime since engine start - #secs since prog start */
          since_start = time(NULL)-m_starttime;
          if (verbose) ESP_LOGI(TAG, "Reporting running for %d seconds",(int)since_start);
          metric = since_start;
          FillFrame(&r_frame,reply,mapped_pid,metric,5);
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
          if(verbose) ESP_LOGI(TAG, "Requested VIN");
          
          if(MyConfig.GetParamValueBool("obd2ecu","private"))   /* ignore request for privacy's sake. Doesn't seem to matter to Dongle. */
          { if(verbose) ESP_LOGI(TAG, "VIN request ignored");
            break;
          }

          memcpy(vin_string,StandardMetrics.ms_v_vin->AsString().c_str(),17);
          vin_string[17] = '\0';  /* force null termination, just because */
            
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
          memcpy(&r_d[5],vin_string,3);  /* grab the first 3 bytes of VIN */

          m_can->Write(&r_frame);

          vTaskDelay(10 / portTICK_PERIOD_MS);  /* let the flow control frame pass */
          
          r_d[0] = 0x21;
          memcpy(&r_d[1],vin_string+3,7);  /* grab the next 7 bytes of VIN */
          
          m_can->Write(&r_frame);
          
          vTaskDelay(10 / portTICK_PERIOD_MS);

          r_d[0] = 0x22;
          memcpy(&r_d[1],vin_string+10,7);  /* grab the last 7 bytes of VIN */

          m_can->Write(&r_frame);
          
          break;
                       
        case 0x0a: /* ECU Name */
          if (verbose) ESP_LOGI(TAG, "ECU Name requested");
          /* Perhaps a good place for arbitrary text, e.g. fleet asset #?  20 char avail. */
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
          r_d[5] = 'S';
          r_d[6] = 'p';
          r_d[7] = 'a';
          m_can->Write(&r_frame);

          vTaskDelay(10 / portTICK_PERIOD_MS);  /* let the flow control frame pass */
          
          r_d[0] = 0x21;
          r_d[1] = 'c';
          r_d[2] = 'e';
          r_d[3] = ' ';
          r_d[4] = 'f';
          r_d[5] = 'o';
          r_d[6] = 'r';
          r_d[7] = ' ';
          m_can->Write(&r_frame);
          
          vTaskDelay(10 / portTICK_PERIOD_MS);

          r_d[0] = 0x22;
          r_d[1] = 't';
          r_d[2] = 'e';
          r_d[3] = 'x';
          r_d[4] = 't';
          r_d[5] = ' ';
          r_d[6] = 'h';
          r_d[7] = 'e';
          m_can->Write(&r_frame);
          
          vTaskDelay(10 / portTICK_PERIOD_MS);

          r_d[0] = 0x23;
          r_d[1] = 'r';
          r_d[2] = 'e';
          r_d[3] = 0x00;  /* end of text marker */
          r_d[4] = 0x55;  /* Pad... */
          r_d[5] = 0x55;
          r_d[6] = 0x55;
          r_d[7] = 0x55;
          m_can->Write(&r_frame);
          break;

        default:
          if (verbose) ESP_LOGI(TAG, "unknown ID=9 frame %x",p_d[2]);
        }
        break;

    case 0x03:  /* request DTCs */
      if (verbose) ESP_LOGI(TAG, "Request DTCs; ignored");
      break;

    case 0x07: /* pending DTCs */
      if (verbose) ESP_LOGI(TAG, "Pending DTCs; ignored");
      break;
				
    case 0x0a:  /* permanent / cleared DTCs */
      if (verbose) ESP_LOGI(TAG, "permanent / cleared DTCs; ignored");
      break;
				
    default:
      if (verbose) ESP_LOGI(TAG, "Unknown Mode %x",p_d[1]);
    }

  return;
  }

void obd2ecu::LoadMap()
  {
  ClearMap();
  // Create default PID maps
  m_pidmap[0x04] = new obd2pid(0x04,obd2pid::Internal,StandardMetrics.ms_v_bat_soc);    // Engine load (use as proxy for SoC)
  m_pidmap[0x05] = new obd2pid(0x05,obd2pid::Internal,StandardMetrics.ms_v_mot_temp);   // Coolant Temperature (use motor temp)
  m_pidmap[0x0c] = new obd2pid(0x0c,obd2pid::Internal,StandardMetrics.ms_v_pos_speed);  // Engine RPM TODO:  Need real metric for RPM
  m_pidmap[0x0d] = new obd2pid(0x0d,obd2pid::Internal,StandardMetrics.ms_v_pos_speed);  // Vehicle Speed
  m_pidmap[0x10] = new obd2pid(0x10,obd2pid::Internal,StandardMetrics.ms_v_bat_power);  // Mass Air Flow  (Note: display limited 0-19.9 on HUDs)

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
      else if(pid) ESP_LOGI(TAG, "Metric '%s' not found",it->second.c_str());
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
      ESP_LOGI(TAG, "Using custom scripting for pid#%d",pid);
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
  }

class obd2ecuInit
    {
    public: obd2ecuInit();
  } obd2ecuInit  __attribute__ ((init_priority (7000)));

obd2ecuInit::obd2ecuInit()
  {
  ESP_LOGI(TAG, "Initialising OBD2ECU (7000)");

  OvmsCommand* cmd_obdii = MyCommandApp.RegisterCommand("obdii","OBDII framework",NULL, "", 1);
  OvmsCommand* cmd_ecu = cmd_obdii->RegisterCommand("ecu","OBDII ECU framework",NULL, "", 1);
  OvmsCommand* cmd_start = cmd_ecu->RegisterCommand("start","Start an OBDII ECU",NULL, "", 1);
  cmd_start->RegisterCommand("can1","start an OBDII ECU on can1",obd2ecu_start, "", 0, 0);
  cmd_start->RegisterCommand("can2","Start an OBDII ECU on can2",obd2ecu_start, "", 0, 0);
  cmd_start->RegisterCommand("can3","Start an OBDII ECU on can3",obd2ecu_start, "", 0, 0);
  cmd_ecu->RegisterCommand("stop","Stop the OBDII ECU",obd2ecu_stop, "", 0, 0);
  cmd_ecu->RegisterCommand("list","Show OBDII ECU pid list",obd2ecu_list, "", 0, 1);
  cmd_ecu->RegisterCommand("reload","Reload OBDII ECU pid map",obd2ecu_reload, "", 0, 0);

  MyConfig.RegisterParam("obd2ecu", "OBD2ECU configuration", true, true);
  MyConfig.RegisterParam("obd2ecu.map", "OBD2ECU metric map", true, true);
  }
