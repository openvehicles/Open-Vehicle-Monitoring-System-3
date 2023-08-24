/*
;    Project:       Open Vehicle Monitor System
;    Date:          8th April 2023
;
;    Changes:
;    1.0  Initial release
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

#include "id_include_exclude_filter.h"

#include <esp_log.h>

IdIncludeExcludeFilter::IdIncludeExcludeFilter(const char *log_tag)
: m_log_tag(log_tag), m_include_filter(log_tag), m_exclude_filter(log_tag)
  {      
  }

void IdIncludeExcludeFilter::LoadFilters(const std::string &include_value, const std::string &exclude_value)
  {
  m_include_filter.LoadFilters(include_value);
  m_exclude_filter.LoadFilters(exclude_value);

  ESP_LOGI(m_log_tag, "%d include entries / %d exclude entries",
           m_include_filter.EntryCount(), m_exclude_filter.EntryCount());
  }

bool IdIncludeExcludeFilter::CheckFilter(const std::string &value) const
  {
  if (m_include_filter.EntryCount() > 0 && !m_include_filter.CheckFilter(value))
      return false;

  return !m_exclude_filter.CheckFilter(value);
  }
