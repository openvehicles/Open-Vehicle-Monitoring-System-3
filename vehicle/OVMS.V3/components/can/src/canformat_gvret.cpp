/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump framework
;    Date:          18th January 2018
;
;    (C) 2018       Mark Webb-Johnson
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
static const char *TAG = "canformat-gvret";

#include "canformat_gvret.h"
#include "canlog.h"
#include <errno.h>
#include <endian.h>
#include "pcp.h"

////////////////////////////////////////////////////////////////////////
// Initialisation and Registration
////////////////////////////////////////////////////////////////////////

class OvmsCanFormatGVRETInit
  {
  public: OvmsCanFormatGVRETInit();
} MyOvmsCanFormatGVRETInit  __attribute__ ((init_priority (4505)));

OvmsCanFormatGVRETInit::OvmsCanFormatGVRETInit()
  {
  ESP_LOGI(TAG, "Registering CAN Format: GVRET (4505)");

  MyCanFormatFactory.RegisterCanFormat<canformat_gvret_ascii>("gvret-a");
  MyCanFormatFactory.RegisterCanFormat<canformat_gvret_binary>("gvret-b");
  }

////////////////////////////////////////////////////////////////////////
// Base GVRET implementation (utility)
////////////////////////////////////////////////////////////////////////

canformat_gvret::canformat_gvret(const char* type)
  : canformat(type)
  {
  }

canformat_gvret::~canformat_gvret()
  {
  }

std::string canformat_gvret::get(CAN_log_message_t* message)
  {
  return std::string("");
  }

std::string canformat_gvret::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat_gvret::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, bool* hasmore, canlogconnection* clc)
  {
  return len;
  }

////////////////////////////////////////////////////////////////////////
// ASCII format GVRET
////////////////////////////////////////////////////////////////////////

canformat_gvret_ascii::canformat_gvret_ascii(const char* type)
  : canformat_gvret(type)
  {
  }

std::string canformat_gvret_ascii::get(CAN_log_message_t* message)
  {
  char buf[CANFORMAT_GVRET_MAXLEN];

  if ((message->type != CAN_LogFrame_RX)&&
      (message->type != CAN_LogFrame_TX))
    {
    return std::string("");
    }

  char busnumber = (message->origin != NULL)?message->origin->m_busnumber + '0':'0';

  sprintf(buf,"%u - %x %s %c %d",
    (uint32_t)((message->timestamp.tv_sec * 1000000) + message->timestamp.tv_usec),
    message->frame.MsgID,
    (message->frame.FIR.B.FF == CAN_frame_std) ? "S" : "X",
    busnumber,
    message->frame.FIR.B.DLC);
    for (int k=0; k<message->frame.FIR.B.DLC; k++)
      sprintf(buf+strlen(buf)," %02x", message->frame.data.u8[k]);

  strcat(buf,"\n");
  return std::string(buf);
  }

size_t canformat_gvret_ascii::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, bool* hasmore, canlogconnection* clc)
  {
  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible

  if (m_buf.HasLine()<0)
    {
    return consumed; // No line, so quick exit
    }
  else
    {
    std::string line = m_buf.ReadLine();
    char *b = strdup(line.c_str());

    // We look for something like
    // 1000 - 100 S 0 4 01 02 03 04
    // timestamp, message ID (hex), S or X, length, data bytes

    message->type = CAN_LogFrame_RX;

    uint32_t timestamp = strtol(b,&b,10);
    message->timestamp.tv_sec = timestamp % 1000000;
    message->timestamp.tv_usec = timestamp / 1000000;

    b += 2; // Skip the '-'

    message->frame.MsgID = strtol(b,&b,16);
    if (b[1] == 'S')
      {
      message->frame.FIR.B.FF = CAN_frame_std;
      }
    else if (b[1] == 'X')
      {
      message->frame.FIR.B.FF = CAN_frame_ext;
      }
    else
      {
      // Bad frame type - discard
      free(b);
      return consumed;
      }

    b += 2; // Skip the frame type

    uint32_t busnumber = strtol(b,&b,10);

    message->frame.FIR.B.DLC = strtol(b,&b,10);
    if (message->frame.FIR.B.DLC > 8)
      {
      // Bad frame length - discard
      free(b);
      return consumed;
      }

    for (size_t x=0;x<message->frame.FIR.B.DLC;x++)
      {
      message->frame.data.u8[x] = strtol(b,&b,16);
      }

    message->origin = MyCan.GetBus(busnumber-'0');

    free(b);
    return consumed;
    }
  }

////////////////////////////////////////////////////////////////////////
// BINARY format GVRET
////////////////////////////////////////////////////////////////////////

canformat_gvret_binary::canformat_gvret_binary(const char* type)
  : canformat_gvret(type)
  {
  }

std::string canformat_gvret_binary::get(CAN_log_message_t* message)
  {
  gvret_binary_frame_t frame;
  memset(&frame,0,sizeof(frame));

  if ((message->type != CAN_LogFrame_RX)&&
      (message->type != CAN_LogFrame_TX))
    {
    return std::string("");
    }

  char busnumber = (message->origin != NULL)?message->origin->m_busnumber:0;

  frame.startbyte = GVRET_START_BYTE;
  frame.command = BUILD_CAN_FRAME;
  frame.microseconds = (uint32_t)((message->timestamp.tv_sec * 1000000) + message->timestamp.tv_usec);
  frame.id = (uint32_t)message->frame.MsgID |
              ((message->frame.FIR.B.FF == CAN_frame_std)? 0 : 0x80000000);
  frame.lenbus = message->frame.FIR.B.DLC + (busnumber<<4);
  for (int k=0; k<message->frame.FIR.B.DLC; k++)
    frame.data[k] = message->frame.data.u8[k];
  return std::string((const char*)&frame,12 + message->frame.FIR.B.DLC);
  }

std::string canformat_gvret_binary::getheader(struct timeval *time)
  {
  // Nasty kludge workaround to SavvyCAN issue
  // (when SavvyCAN reconnects, it doesn't request GET_CANBUS_PARAMS, but if
  // it doesn't get a GET_CANBUS_PARAMS reply it times out and disconnects;
  // so we send it unsolicited, upon initial connection).
  gvret_replymsg_t r;
  PopulateBusList12(&r);
  return std::string((char*)&r,12);
  }

size_t canformat_gvret_binary::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, bool* hasmore, canlogconnection* clc)
  {
  if (m_buf.FreeSpace()==0) SetServeDiscarding(true); // Buffer full, so discard from now on
  if (IsServeDiscarding()) return len;  // Quick return if discarding

  size_t consumed = Stuff(buffer,len);  // Stuff m_buf with as much as possible

  uint8_t firstbyte;
  while ((m_buf.Peek(1,&firstbyte) == 1)&&(firstbyte != GVRET_START_BYTE))
    {
    if (firstbyte == GVRET_SET_BINARY)
      {
      ESP_LOGV(TAG,"GVRET request to set binary mode");
      }
    m_buf.Pop(1,&firstbyte);
    }

  if (m_buf.UsedSpace() >= 2)
    {
    gvret_replymsg_t r;
    gvret_commandmsg_t m;
    memset(&m,0,sizeof(m));
    memset(&r,0,sizeof(r));
    m_buf.Peek(2,(uint8_t*)&m);
    r.startbyte = m.startbyte;
    r.command = m.command;
    switch (m.command)
      {
      case BUILD_CAN_FRAME:
        if (m_buf.UsedSpace() >= 8)
          {
          m_buf.Peek(8,(uint8_t*)&m);
          if (m_buf.UsedSpace() >= 8 + m.body.build_can_frame.length)
            {
            m_buf.Pop(8 + m.body.build_can_frame.length,(uint8_t*)&m);
            CAN_frame_t msg;
            memset(&msg,0,sizeof(msg));
            msg.origin = MyCan.GetBus(m.body.build_can_frame.bus);
            if (m.body.build_can_frame.id & 0x8000000)
              {
              msg.MsgID = m.body.build_can_frame.id & 0x7fffffff;
              msg.FIR.B.FF = CAN_frame_ext;
              }
            else
              {
              msg.MsgID = m.body.build_can_frame.id;
              msg.FIR.B.FF = CAN_frame_std;
              }
            msg.FIR.B.DLC = m.body.build_can_frame.length;
            memcpy(&msg.data, &m.body.build_can_frame.data, m.body.build_can_frame.length);

            ESP_LOGD(TAG,"Rx BUILD_CAN_FRAME ID=%0x",msg.MsgID);
            *hasmore = true;  // Call us again to see if we have more frames to process
            // We have a frame to be transmitted / simulated
            switch (m_servemode)
              {
              case Transmit:
                if (msg.origin) msg.origin->Write(&msg);
                break;
              case Simulate:
                if (msg.origin) MyCan.IncomingFrame(&msg);
                break;
              default:
                break;
              }
            }
          }
        break;
      case TIME_SYNC:
        ESP_LOGD(TAG,"Rx %02x TIME_SYNC",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        r.body.time_sync.microseconds = 0;
        if (clc) clc->TransmitCallback((uint8_t*)&r,6);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_DIG_INPUTS:
        ESP_LOGD(TAG,"Rx %02x GET_DIG_INPUTS",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        if (clc) clc->TransmitCallback((uint8_t*)&r,4);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_ANALOG_INPUTS:
        ESP_LOGD(TAG,"Rx %02x GET_ANALOG_INPUTS",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        if (clc) clc->TransmitCallback((uint8_t*)&r,11);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case SET_DIG_OUTPUTS:
        ESP_LOGD(TAG,"Rx %02x GET_DIG_OUTPUTS",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case SETUP_CANBUS:
        ESP_LOGD(TAG,"Rx %02x SETUP_CANBUS",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_CANBUS_PARAMS:
        ESP_LOGD(TAG,"Rx %02x GET_CANBUS_PARAMS",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        PopulateBusList12(&r);
        if (clc) clc->TransmitCallback((uint8_t*)&r,12);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_DEVICE_INFO:
        ESP_LOGD(TAG,"Rx %02x GET_DEVICE_INFO",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        r.body.get_device_info.build = 0;
        r.body.get_device_info.eeprom = 0;
        r.body.get_device_info.filetype = 0;
        r.body.get_device_info.autolog = 0;
        r.body.get_device_info.singlewire = 0;
        if (clc) clc->TransmitCallback((uint8_t*)&r,8);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case SET_SINGLEWIRE_MODE:
        ESP_LOGD(TAG,"Rx %02x SET_SINGLEWIRE_MODE",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case KEEP_ALIVE:
        // Don't log keepalive, as we get four of these a second
        //ESP_LOGD(TAG,"Rx %02x KEEP_ALIVE",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        r.body.keep_alive.notdead1 = GVRET_NOTDEAD_1;
        r.body.keep_alive.notdead2 = GVRET_NOTDEAD_2;
        if (clc) clc->TransmitCallback((uint8_t*)&r,4);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case SET_SYSTEM_TYPE:
        ESP_LOGD(TAG,"Rx %02x SET_SYSTEM_TYPE",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case ECHO_CAN_FRAME:
        ESP_LOGD(TAG,"Rx %02x ECHO_CAN_FRAME",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_NUM_BUSES:
        ESP_LOGD(TAG,"Rx %02x GET_NUM_BUSES",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        r.body.get_num_buses.buses = 3;
        if (clc) clc->TransmitCallback((uint8_t*)&r,3);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      case GET_EXT_BUSES:
        ESP_LOGD(TAG,"Rx %02x GET_EXT_BUSES",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        PopulateBusList3(&r);
        if (clc) clc->TransmitCallback((uint8_t*)&r,17);
        *hasmore = true;  // Call us again to see if we have more frames to process
        break;
      default:
        ESP_LOGW(TAG,"Rx %02x command unrecognised - skipping",m.command);
        m_buf.Pop(2,(uint8_t*)&m);
        *hasmore = true;  // Call us again to see if we have more frames to process
      }
    }

  return consumed;
  }

void canformat_gvret_binary::PopulateBusList12(gvret_replymsg_t* r)
  {
  memset(r,0,sizeof(*r));
  r->startbyte = GVRET_START_BYTE;
  r->command = GET_CANBUS_PARAMS;

  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName("can1");
  r->body.get_canbus_params.can1_mode = (bus->m_mode != CAN_MODE_OFF) | ((bus->m_mode == CAN_MODE_LISTEN)<<4);
  r->body.get_canbus_params.can1_speed = MAP_CAN_SPEED(bus->m_speed);

  bus = (canbus*)MyPcpApp.FindDeviceByName("can2");
  r->body.get_canbus_params.can2_mode = (bus->m_mode != CAN_MODE_OFF) | ((bus->m_mode == CAN_MODE_LISTEN)<<4);
  r->body.get_canbus_params.can2_speed = MAP_CAN_SPEED(bus->m_speed);
  }

void canformat_gvret_binary::PopulateBusList3(gvret_replymsg_t* r)
  {
  canbus* bus = (canbus*)MyPcpApp.FindDeviceByName("can3");
  r->body.get_ext_buses.swcan_mode = (bus->m_mode != CAN_MODE_OFF) | ((bus->m_mode == CAN_MODE_LISTEN)<<4);
  r->body.get_ext_buses.swcan_speed = MAP_CAN_SPEED(bus->m_speed);
  }
