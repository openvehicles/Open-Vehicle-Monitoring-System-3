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

#ifndef __METRICS_LIST_H__
#define __METRICS_LIST_H__

#include <cstddef>
#include <string>
#include <unordered_set>

class MetricsList
  {
    public:
      enum class EmptyBehavior
      {
        MATCH_ALL,  // If empty, all metrics match
        MATCH_NONE  // If empty, no metrics match
      };

      MetricsList(EmptyBehavior empty_behavior);

      // Clear the current list and add the comma or whitespace-delimted list 
      // of metrics names in config_value
      //
      // TODO: allow suffix wildcarding with trailing '*'
      void LoadFromConfigValue(std::string const& config_value);

      size_t Count();

      // Return true iff metric_name is included in the list
      bool MatchesMetricName(const char* metric_name);
    
    private:
      const EmptyBehavior m_empty_behavior;

      std::unordered_set<std::string> m_list;
  };

#endif //#ifndef __METRICS_LIST_H__