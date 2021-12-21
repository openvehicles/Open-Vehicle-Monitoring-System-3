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

#ifndef __OVMS_UTILS_H__
#define __OVMS_UTILS_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <cstring>
#include <string>
#include <iomanip>
#include "ovms.h"

// Macro utils:
// see https://gcc.gnu.org/onlinedocs/cpp/Stringizing.html#Stringizing
// STR(x) = string of x, x expanded if preprocessor macro
#ifndef STR
#define STRX(x)   #x
#define STR(x)    STRX(x)
#endif

// Math utils:
#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))
#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

// Value precision utils:
#define TRUNCPREC(fval,prec) (trunc((fval) * pow(10,(prec))) / pow(10,(prec)))
#define ROUNDPREC(fval,prec) (round((fval) * pow(10,(prec))) / pow(10,(prec)))
#define CEILPREC(fval,prec)  (ceil((fval)  * pow(10,(prec))) / pow(10,(prec)))

// Standard array size (number of elements):
#if __cplusplus < 201703L
  template <class T, std::size_t N>
  constexpr std::size_t sizeof_array(const T (&array)[N]) noexcept
    {
    return N;
    }
#else
  #define sizeof_array(array) (std::size(array))
#endif

// container.insert() helper:
#define endof_array(array) ((array)+sizeof_array(array))

// C string sorting for std::map et al:
struct CmpStrOp
  {
  bool operator()(char const *a, char const *b)
    {
    return std::strcmp(a, b) < 0;
    }
  };

// Tolerant boolean string analyzer:
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
 * stripesc: remove terminal escape sequences from (log) string
 */
std::string stripesc(const char* s);

/**
 * startsWith: std::string et al prefix check
 */
template <class string_t>
bool startsWith(const string_t& haystack, const std::string& needle)
  {
  return needle.length() <= haystack.length()
    && std::equal(needle.begin(), needle.end(), haystack.begin());
  }
template <class string_t>
bool startsWith(const string_t& haystack, const char needle)
  {
  return !haystack.empty() && haystack.front() == needle;
  }

/**
 * endsWith: std::string et al suffix check
 */
template <class string_t>
bool endsWith(const string_t& haystack, const std::string& needle)
  {
  return needle.length() <= haystack.length()
    && std::equal(needle.begin(), needle.end(), haystack.end() - needle.length());
  }
template <class string_t>
bool endsWith(const string_t& haystack, const char needle)
  {
  return !haystack.empty() && haystack.back() == needle;
  }

/**
 * HexByte: Write a single byte as two hexadecimal characters
 * Returns new pointer to end of string (p + 2)
 */
char* HexByte(char* p, uint8_t byte);

/**
 * FormatHexDump: create/fill hexdump buffer including printable representation
 * Note: allocates buffer as necessary in *bufferp, caller must free.
 * Returns new remaining length
 */
size_t FormatHexDump(char** bufferp, const char* data, size_t rlength, size_t colsize=16);

/**
 * hexencode: encode a string of bytes into hexadecimal form
 */
std::string hexencode(const std::string value);

/**
 * hexdecode: decode a hexadecimal encoded string of bytes
 *  Returns empty string on error
 */
std::string hexdecode(const std::string encval);

/**
 * int_to_hex: hex encode an integer value
 *  Source: https://kodlogs.com/68574/int-to-hex-string-c
 */
template <typename T>
std::string int_to_hex(T i)
  {
  std::stringstream stream;
  stream << std::setfill('0') << std::setw(sizeof(T)*2) << std::hex << (unsigned)i;
  return stream.str();
  }

/**
 * json_encode: encode string for JSON transport (see http://www.json.org/)
 */
template <class src_string>
std::string json_encode(const src_string text)
  {
  std::string buf;
  char hex[10];
  buf.reserve(text.size() + (text.size() >> 3));
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
      default:
        if (iscntrl(text[i]))
          {
          sprintf(hex, "\\u%04x", (unsigned int)text[i]);
          buf += hex;
          }
        else
          {
          buf += text[i];
          }
        break;
      }
    }
  return buf;
  }

/**
 * display_encode: encode string displaying unprintablel characters
 * Emulates (linux) "cat -t" semantics
 */
template <class src_string>
std::string display_encode(const src_string text)
  {
  std::string buf;
  buf.reserve(text.size() + (text.size() >> 3));
  for (int i = 0; i < text.size(); i++)
    {
    char ch = text[i];
    if (!isascii(ch))
      {
      buf += "M-";
      ch = toascii(ch);
      }
    if (ch == '\177')
      {
      buf += "^?";
      continue;
      }
    if (ch == '\t')
      {
      buf += "^I";
      continue;
      }
    if (ch == '\n')
      {
      // deviation from "cat -t"
      buf += "^J";
      continue;
      }
    if (!isprint(ch))
      {
      ch = ch ^ 0x40;
      buf += "^";
      }
    buf += ch;
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

/**
 * load & save file to/from string
 *  - saving creates missing directories automatically & signals system.vfs.file.changed
 *  - return value: 0 = ok / errno
 */
int load_file(const std::string &path, extram::string &content);
int save_file(const std::string &path, extram::string &content);

/**
 * get_user_agent: create User-Agent string from OVMS versions & vehicle ID
 *  Scheme: "ovms/v<hw_version> (<vehicle_id> <sw_version>)"
 */
std::string get_user_agent();

/**
 * float2double: minimize precision errors on float→double conversion
 */
double float2double(float f);

/**
 * idtag: create object instance tag for registrations
 */
std::string idtag(const char* tag, void* instance);
#define IDTAG idtag(TAG,this)

#endif // __OVMS_UTILS_H__
