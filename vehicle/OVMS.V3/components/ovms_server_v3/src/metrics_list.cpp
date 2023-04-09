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

#include "metrics_list.h"

#include <regex>
#include <iterator>

MetricsList::MetricsList(EmptyBehavior empty_behavior)
: m_empty_behavior(empty_behavior)
  {
  }

void MetricsList::LoadFromConfigValue(std::string const& config_value)
  {
    using std::sregex_token_iterator;
    
    static const std::regex SEPERATOR_REGEX("[,\\s]");

    m_list.clear();

    sregex_token_iterator entries_begin(config_value.begin(), config_value.end(), SEPERATOR_REGEX, -1);
    sregex_token_iterator entries_end;

    for (auto entry_iter = entries_begin; entry_iter != entries_end; entry_iter++)
      {
        std::string entry(*entry_iter);
        if (!entry.empty())
          m_list.insert(entry);
      }
  }

size_t MetricsList::Count()
  {
    return m_list.size();
  }

bool MetricsList::MatchesMetricName(const char* metric_name)
  {
    if (m_list.empty())
      return m_empty_behavior == EmptyBehavior::MATCH_ALL;

    return m_list.find(metric_name) != m_list.end();
  }
