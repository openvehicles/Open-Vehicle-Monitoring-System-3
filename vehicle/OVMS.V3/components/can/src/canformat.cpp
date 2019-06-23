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

canformat::canformat(const char* type)
  {
  m_type = type;
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

size_t canformat::put(CAN_log_message_t* message, uint8_t *buffer, size_t len)
  {
  return 0;
  }
