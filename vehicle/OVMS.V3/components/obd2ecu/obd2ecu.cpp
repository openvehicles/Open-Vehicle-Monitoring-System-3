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

#include "esp_log.h"
static const char *TAG = "obd2ecu";

#include <string.h>
#include "obd2ecu.h"
#include "ovms_command.h"
#include "ovms_peripherals.h"
#include "ovms_metrics.h"
#include "metrics_standard.h"

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

obd2ecu::obd2ecu(std::string name, canbus* can)
  : pcp(name)
  { 
  m_can = can;
  xTaskCreatePinnedToCore(OBD2ECU_task, "OBDII ECU Task", 4096, (void*)this, 5, &m_task, 1);
  
  m_rxqueue = xQueueCreate(20,sizeof(CAN_frame_t));
    
  m_can->Start(CAN_MODE_ACTIVE,CAN_SPEED_500KBPS);
  m_can->SetPowerMode(On);
  
  m_starttime = time(NULL);
  m_private = PRIVACY;      /* default privacy mode (e.g. hiding VIN)  */
    
  MyCan.RegisterListener(m_rxqueue);
  }

obd2ecu::~obd2ecu()
  {
  m_can->SetPowerMode(Off);
  MyCan.DeregisterListener(m_rxqueue);

  vQueueDelete(m_rxqueue);
  vTaskDelete(m_task);
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
    std::string bus = cmd->GetName();
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
  
//
// Fill Mode 1 frames with data based on format specified
//
void obd2ecu::FillFrame(CAN_frame_t *frame,int reply,uint8_t pid,float data,uint8_t format)
  {
  uint8_t a,b;
  int i;

  /* Formats are defined as "Formula" on Wikipedia "OBD-II_PIDs" page. */
  /* Note that we need to apply the inverse, to derive the A and B values from the intended metric */

  switch(format)  
    {
    case 0:  /* no data (send zeros) */
      a = b = 0;
      break;
			
    case 1:  /* A only (one byte) */
      a = (int)data;
      b = 0;
      break;
		
    case 2:  /* Percent: 100*A/255 */
      a = (int)((data*255)/100);
      b = 0;
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
      b = 0;
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
			
    default:
      if (verbose) ESP_LOGI(TAG, "Unsupported format %d",format);
      a = b = 0;  /* default to empty data */
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
  frame->data.u8[5] = 0;
  frame->data.u8[6] = 0;			
  frame->data.u8[7] = 0x55;   /* pad 0x55 */

  return;			
  }

void obd2ecu::IncomingFrame(CAN_frame_t* p_frame)
  { 
  CAN_frame_t r_frame;  /* build the response frame here */
  int reply;
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
  if (p_frame->FIR.B.FF == CAN_frame_std) reply = RESPONSE_PID;
  else if (p_frame->FIR.B.FF == CAN_frame_ext) reply = RESPONSE_EXT_PID;
  else /* check for flow control frames - they're received on the response MsgID minus 8 */
    {
    if ((p_frame->MsgID == FLOWCONTROL_PID || p_frame->MsgID == FLOWCONTROL_EXT_PID) && p_frame->data.u8[0] == 0x30)
      {
      if (verbose) ESP_LOGI(TAG, "flow control frame");
	return;  /* ignore it.  We just sleep for a bit instead */
      }
    else ESP_LOGI(TAG, "unknown can_id %x",p_frame->MsgID);
      return;
    }

  jitter = time(NULL)&0xf;  /* 0-15 range for simulation purposes */
		
  switch(p_d[1])  /* switch on the incoming frame mode */
    {
    case 1:  /* Mode 1 (main real-time PIDs are here */
      /* Fetch what the HUD/Dongle request maps to for the simulated vehicle */
      /* For now, just use a static mapping for the common items, and simulate the rest. */
      /* Eventually, the link to Duktape will probably be in here, somehow */
      mapped_pid = p_d[2];  
      switch (mapped_pid)  /* switch on the what the requested PID maps to. */
        {
        case 0:  /* request capabilities */
          /* This is a bitmap of the Mode 1 PIDs that we will act on from the HUD/Dongle */
          r_frame.origin = NULL;
          r_frame.FIR.U = 0;
          r_frame.FIR.B.DLC = 8;
          r_frame.FIR.B.FF = CAN_frame_format_t (reply != RESPONSE_PID);
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
          /* For now, derive RPM from vehicle speed. Roadster ratio:  14,000 rpm div by 201 km/h = 70 */
          /* adding "jitter" to keep the dongle from going to sleep. ASSUME: speed is in km/h, not mph*/
          metric = StandardMetrics.ms_v_pos_speed->AsInt()*70+jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,3);
          m_can->Write(&r_frame);
          break;
        case 0x0d:	/* Vehicle speed */
          metric = StandardMetrics.ms_v_pos_speed->AsInt();
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
          m_can->Write(&r_frame);
          break;
        case 0x05:	/* Coolant Temp - Use Motor temp */
          metric = StandardMetrics.ms_v_temp_motor->AsInt();
          FillFrame(&r_frame,reply,mapped_pid,metric,4);
          m_can->Write(&r_frame);
          break;
        case 0x10:	/* MAF (Mass Air flow) rate - Map to SoC */
          /* For some reason, the HUD uses this param as a proxy for fuel rate */
          /* NO clue what the conversion formula is, but seems to have a display range of 0-19.9 */
          /* If configured with 11 as "Emission Setting" and 52% for "fuel connsumption" metrics */
          /* will display 0-10.0 for 0-100% with input of 0-33.  Use with display sete to L/hr (not L/km).  */
          metric = StandardMetrics.ms_v_bat_soc->AsInt()/33;
          FillFrame(&r_frame,reply,mapped_pid,metric,6);
          m_can->Write(&r_frame);
          break;
			
        //  Simulations below for additional PIDs requested by "SyncUp Drive" dongle.  Reply is apparently not critical                  
        case 0x04:	/* Engine load - simulate 45% */
          metric = 0x75-jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
          m_can->Write(&r_frame);
          break;
        case 0x0f:	/* Air intake temp - simulate 20c */
          metric = 0x3c+jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
          m_can->Write(&r_frame);
          break;
        case 0x11:	/* throttle position - simulate 15% */
          metric = 15+jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,2);
          m_can->Write(&r_frame);
          break;
        case 0x21:	/* Distance traveled with check engine - say none */
          metric = 0;
          FillFrame(&r_frame,reply,mapped_pid,metric,0);
          m_can->Write(&r_frame);
          break;
        case 0x2f:	/* Fuel tank level - simulate 80% */
          metric = 0xcc-jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
          m_can->Write(&r_frame);
          break;
        case 0x31:	/* Distance since codes cleared - simulate @ 60mph */
          since_start = (time(NULL)-m_starttime)/60;
          metric = since_start;
          FillFrame(&r_frame,reply,mapped_pid,metric,5);
          m_can->Write(&r_frame);
          break;
        case 0x0b:	/* Manifold pressure - simulate 10kpa */
          metric = 10+jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
          m_can->Write(&r_frame);
          break;
        case 0x1f:	/* Runtime since engine start - #secs since prog start */
          since_start = time(NULL)-m_starttime;
          if (verbose) ESP_LOGI(TAG, "Reporting running for %d seconds",(int)since_start);
          metric = since_start;
          FillFrame(&r_frame,reply,mapped_pid,metric,5);
          m_can->Write(&r_frame);
          break;
        case 0x0a:	/* fuel pressure - simulate 18kpa */
          metric = 18+jitter;
          FillFrame(&r_frame,reply,mapped_pid,metric,1);
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
        default:
          ESP_LOGI(TAG, "unknown capability requested %x",r_frame.data.u8[2]);
	}
      break;

    case 9: 
      switch (p_frame->data.u8[2])
        {
        case 2:
          if(verbose) ESP_LOGI(TAG, "Requested VIN");
          
          if(m_private) break;  /* ignore request for privacy's sake. Doesn't seem to matter to Dongle. */

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

          r_d[0] = 0x22;
          r_d[1] = 't';
          r_d[2] = 'e';
          r_d[3] = 'x';
          r_d[4] = 't';
          r_d[5] = ' ';
          r_d[6] = 'h';
          r_d[7] = 'e';
          m_can->Write(&r_frame);

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
  }
