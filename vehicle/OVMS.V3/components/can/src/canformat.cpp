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
static const char *TAG = "canformat";

#include "canformat.h"

canformat::canformat_serve_mode_t GetFormatModeType(std::string name)
  {
  if (name.compare("simulate")==0)
    return canformat::Simulate;
  else if (name.compare("transmit")==0)
    return canformat::Transmit;
  else
    return canformat::Discard;
  }

OvmsCanFormatFactory MyCanFormatFactory __attribute__ ((init_priority (4500)));

OvmsCanFormatFactory::OvmsCanFormatFactory()
  {
  ESP_LOGI(TAG, "Initialising CAN Format Factory (4500)");
  }

OvmsCanFormatFactory::~OvmsCanFormatFactory()
  {
  }

canformat* OvmsCanFormatFactory::NewFormat(const char* FormatType)
  {
  OvmsCanFormatFactory::map_can_format_t::iterator iter = m_fmap.find(FormatType);
  if (iter != m_fmap.end())
    {
    return iter->second(FormatType);
    }
  return NULL;
  }

void OvmsCanFormatFactory::RegisterCommandSet(OvmsCommand* base, const char* title,
                        void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*),
                        const char *usage, int min, int max, bool secure,
                        int (*validate)(OvmsWriter*, OvmsCommand*, int, const char* const*, bool))
  {
  OvmsCanFormatFactory::map_can_format_t::iterator iter = m_fmap.begin();
  while (iter != m_fmap.end())
    {
    base->RegisterCommand(iter->first, title, execute, usage, min, max, secure, validate);
    ++iter;
    }
  }

canformat::canformat(const char* type, canformat_serve_mode_t mode)
  : m_buf(CANFORMAT_SERVE_BUFFERSIZE)
  {
  m_type = type;
  m_servemode = mode;
  m_servediscarding = false;
  m_putcallback_fn = NULL;
  }

canformat::~canformat()
  {
  }

const char* canformat::type()
  {
  return m_type;
  }

std::string canformat::get(CAN_log_message_t* message)
  {
  return std::string("");
  }

std::string canformat::getheader(struct timeval *time)
  {
  return std::string("");
  }

size_t canformat::put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata)
  {
  return 0;
  }

size_t canformat::Serve(uint8_t *buffer, size_t len, void* userdata)
  {
  if ((m_servediscarding)||(m_servemode == Discard))
    {
    return len; // Simply discard...
    }

  size_t consumed = 0;
  while(len>0)
    {
    CAN_log_message_t msg;
    memset(&msg,0,sizeof(msg));

    size_t used = put(&msg, buffer, len, userdata);
    if (used > 0)
      {
      consumed += used;
      buffer += used;
      len -= used;
      }
    else
      {
      len = 0;
      }

    if (msg.frame.origin != NULL)
      {
      switch (m_servemode)
        {
        case Simulate:
          MyCan.IncomingFrame(&msg.frame);
          break;
        case Transmit:
          msg.frame.origin->Write(&msg.frame);
          break;
        default:
          break;
        }
      }
    }

  return consumed;
  }

size_t canformat::Stuff(uint8_t *buffer, size_t len)
  {
  // Stuff incoming data into the put buffer
  if (len==0) return 0;
  size_t consumed = (len>m_buf.FreeSpace())?m_buf.FreeSpace():len;
  m_buf.Push(buffer,consumed);
  return consumed;
  }

canformat::canformat_serve_mode_t canformat::GetServeMode()
  {
  return m_servemode;
  }

const char* canformat::GetServeModeName()
  {
  switch (m_servemode)
    {
    case Simulate:
      return "simulate";
    case Transmit:
      return "transmit";
    default:
      return "discard";
    }
  }

void canformat::SetServeMode(canformat_serve_mode_t mode)
  {
  m_servemode = mode;
  }

bool canformat::IsServeDiscarding()
  {
  return ((m_servediscarding)||(m_servemode == Discard));
  }

void canformat::SetServeDiscarding(bool discarding)
  {
  m_servediscarding = discarding;
  }

void canformat::SetPutCallback(canformat_put_write_fn callback)
  {
  m_putcallback_fn = callback;
  }
