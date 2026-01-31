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

#ifndef __ID_INCLUDE_EXCLUDE_FILTER_H
#define __ID_INCLUDE_EXCLUDE_FILTER_H

#include <string>

#include "id_filter.h"

class IdIncludeExcludeFilter
  {
  public:
    IdIncludeExcludeFilter(const char *log_tag);

    /**
     * Parse comma-separated lists of include and exclude filter entries.
     * 
     * See IdFilter::LoadFilter()
     */
    bool LoadFilters(const std::string &include_value, const std::string &exclude_value);

    /**
     * Check if a value matches the include/exclude filters.  A value will match if it matches the
     * include filter (or the include filter has no entries) and does not match the exclude filter.
     * 
     * See IdFilter::CheckFilter()
     */
    bool CheckFilter(const std::string &value) const;

  private:
    const char *m_log_tag;

    IdFilter m_include_filter;
    IdFilter m_exclude_filter;
  };

#endif // __ID_INCLUDE_EXCLUDE_FILTER_H
