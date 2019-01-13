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

#ifndef __UTILS_H__
#define __UTILS_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <string>
#include "ovms.h"

// Macro utils:
// see https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
// STR(x) = string of x, x expanded if preprocessor macro
#ifndef STR
#define STRX(x)   #x
#define STR(x)    STRX(x)
#endif

struct CmpStrOp
  {
  bool operator()(char const *a, char const *b)
    {
    return std::strcmp(a, b) < 0;
    }
  };

inline bool strtobool(const std::string& str)
  {
  return (str == "yes" || str == "1" || str == "true");
  }

/**
 * chargestate_code: convert legacy chargestate key to code
 * chargestate_key: convert chargestate code to legacy key
 */
std::string chargestate_code(const int key);
int chargestate_key(const std::string code);

/**
 * chargesubstate_code: convert legacy charge substate key to code
 * chargesubstate_key: convert charge substate code to legacy key
 */
std::string chargesubstate_code(const int key);
int chargesubstate_key(const std::string code);

/**
 * chargemode_code: convert legacy chargemode key to code
 * chargemode_key: convert chargemode code to legacy key
 */
std::string chargemode_code(const int key);
int chargemode_key(const std::string code);


/**
 * mp_encode: encode string for MP transport;
 *  - replace '\r\n' by '\r'
 *  - replace '\n' by '\r'
 *  - replace ',' by ';'
 */
std::string mp_encode(const std::string text);
extram::string mp_encode(const extram::string text);

/**
 * stripcr:
 *  - replace '\r\n' by '\n'
 */
extram::string stripcr(const extram::string& text);

/**
 * startsWith: std::string prefix check
 */
bool startsWith(const std::string& haystack, const std::string& needle);
bool startsWith(const std::string& haystack, const char needle);

/**
 * endsWith: std::string suffix check
 */
bool endsWith(const std::string& haystack, const std::string& needle);
bool endsWith(const std::string& haystack, const char needle);

/**
 * FormatHexDump: create/fill hexdump buffer including printable representation
 * Note: allocates buffer as necessary in *bufferp, caller must free.
 */
int FormatHexDump(char** bufferp, const char* data, size_t rlength, size_t colsize=16);


/**
 * json_encode: encode string for JSON transport (see http://www.json.org/)
 */
template <class src_string>
std::string json_encode(const src_string text)
  {
  std::string buf;
  for (int i=0; i<text.size(); i++)
    {
    switch(text[i])
      {
      case '\n':        buf += "\\n"; break;
      case '\r':        buf += "\\r"; break;
      case '\t':        buf += "\\t"; break;
      case '\b':        buf += "\\b"; break;
      case '\f':        buf += "\\f"; break;
      case '\"':        buf += "\\\""; break;
      case '\\':        buf += "\\\\"; break;
      default:          buf += text[i]; break;
      }
    }
	return buf;
  }


/**
 * mqtt_topic: convert dotted string (e.g. notification subtype) to MQTT topic
 *  - replace '.' by '/'
 */
std::string mqtt_topic(const std::string text);


/**
 * pwgen: simple password generator
 * Note: take care to seed the pseudo random generator i.e. by
 *  srand48(StdMetrics.ms_m_monotonic->AsInt() * StdMetrics.ms_m_freeram->AsInt());
 */
std::string pwgen(int length);

#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
#define HAVE_TaskGetHandle
/**
 * TaskGetHandle: get task handle by name
 * (FreeRTOS xTaskGetHandle() is not available)
 */
TaskHandle_t TaskGetHandle(const char *name);
#endif // CONFIG_FREERTOS_USE_TRACE_FACILITY


/**
 * mkpath: mkdir -p
 */
int mkpath(std::string path, mode_t mode = 0);

/**
 * rmtree: rmdir -r
 */
int rmtree(const std::string path);

/**
 * path_exists: check if filesystem path exists
 */
bool path_exists(const std::string path);

#endif //#ifndef __UTILS_H__
