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

#include "id_filter.h"

#include <sstream>
#include <esp_log.h>

#include "ovms_utils.h"


IdFilter::IdFilter(const char* log_tag)
: m_log_tag(log_tag)
  {
  }

bool IdFilter::LoadFilters(const std::string &value)
  {
  // Optimization: only perform lock & load for actual filter definition change:
  std::size_t str_hash = std::hash<std::string>{}(value);
  if (str_hash == m_entry_hash) return false; // unchanged

  OvmsMutexLock lock(&m_mutex);

  // Empty all previously defined filters, for all operators
  for (int i=0; i<COUNT_OF_OPERATORS; i++)
    {
    m_entries[i].clear();
    }
  m_entry_count = 0;
  m_entry_hash = str_hash;

  if (!value.empty())
    {
    std::stringstream stream (value);
    std::string item;
    unsigned char comparison_operator;

    // Comma-separated list
    while (getline (stream, item, ','))
      {
      trim(item); // Removing leading and trailing spaces

      if (item.empty())
        {
        ESP_LOGW(m_log_tag, "LoadFilters: skipping empty value in the filter list");
        continue;
        }

      // Check if there is a wildcard ('*') in any other place than first or last position
      size_t wildcard_position = item.find('*', 1);
      if ((wildcard_position != std::string::npos) && (wildcard_position != item.size()-1))
        {
        ESP_LOGW(m_log_tag, "LoadFilters: skipping incorrect value (%s) in the filter list (wildcard in wrong position)", item.c_str());
        continue;
        }

      // Depending on the presence and position of the wildcard, push the filter
      // in the proper vector (without the wildcard)
      if (item.front() == '*')
        {
        comparison_operator = OPERATOR_ENDSWITH;
        item.erase(0, 1);
        }
      else if (item.back() == '*')
        {
        comparison_operator = OPERATOR_STARTSWITH;
        item.pop_back();
        }
      else
        {
        comparison_operator = OPERATOR_EQUALS;
        }
      m_entries[comparison_operator].push_back(item);
      m_entry_count++;
      }
    }

  // for (int i=0; i<COUNT_OF_OPERATORS; i++)
  //   {
  //   ESP_LOGI(m_log_tag, "LoadFilters: filters for operator %d:", i);
  //   for (std::vector<std::string>::iterator it=m_entries[i].begin(); it!=m_entries[i].end(); ++it)
  //     {
  //     ESP_LOGI(m_log_tag, "LoadFilters: filter value '%s'", it->c_str());
  //     }
  //   if (member[i].begin() == m_entries[i].end())
  //     {
  //     ESP_LOGI(m_log_tag, "LoadFilters: (empty filter list)");
  //     }
  //   }

    return true; // changed
    }

  size_t IdFilter::EntryCount() const
    {
    return m_entry_count;
    }

  bool IdFilter::CheckFilter(const std::string &value) const
    {
    if (m_entry_count == 0) return false;
    OvmsMutexLock lock(&m_mutex);
    for (int i=0; i<COUNT_OF_OPERATORS; i++)
        {
        for (std::vector<std::string>::const_iterator it=m_entries[i].begin(); it!=m_entries[i].end(); ++it)
        {
        if ((i == OPERATOR_STARTSWITH) && (startsWith(value, *it)))
            {
            return true;
            }
        else if ((i == OPERATOR_ENDSWITH) && (endsWith(value, *it)))
            {
            return true;
            }
        else if ((i == OPERATOR_EQUALS) && (value == *it))
            {
            return true;
            }
        }
        }
    return false;
    }
