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

#ifndef __ID_FILTER_H
#define __ID_FILTER_H

#include <string>
#include <array>
#include <vector>
#include <cstddef>

class IdFilter
  {
  public:
    /**
     * All IdFilter esp_log calls will be tagged with log_tag
     */
    IdFilter(const char* log_tag);

    /**
     * Parse a comma-separated list of filters, and assign them to a member of the class.
     * We have 3 kind of comparisons, and an unlimited list of filters.
     *
     * A filter can be:
     * - A "startsWith" comparison - when ending with '*',
     * - An "endsWith" comparison - when starting with '*',
     * - Invalid (and skipped) if empty, or with a '*' in any other position than beginning or end,
     * - A "string equal" comparison for all other cases
     */
    void LoadFilters(const std::string &value);

    /**
     * Return the number of filter entries 
     */
    size_t EntryCount() const;

    /**
     * Check if a value matches in a list of filters.
     *
     * Match can be a:
     * - startsWith match,
     * - endsWith match,
     * - equality match.
     */
    bool CheckFilter(const std::string &value) const;

  private:
    const char *m_log_tag;

    static constexpr auto OPERATOR_STARTSWITH   = 0;
    static constexpr auto OPERATOR_ENDSWITH     = 1;
    static constexpr auto OPERATOR_EQUALS       = 2;
    static constexpr auto COUNT_OF_OPERATORS    = 3;

    std::array<std::vector<std::string>, COUNT_OF_OPERATORS> m_entries;
    size_t                                                   m_entry_count{0};
  };

#endif // __ID_FILTER_H