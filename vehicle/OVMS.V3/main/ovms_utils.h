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
#include <algorithm>
#include <cstring>
#include <string>
#include <iomanip>
#include <vector>
#include <forward_list>
#include <functional>
#include <esp_timer.h>
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
  bool operator()(char const *a, char const *b) const
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
 * replace_substrings: replace all `from` substrings by `to` in `text`
 */
void replace_substrings(std::string &text, const std::string from, const std::string to);

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
 * hexdecode_u16: decode a hexadecimal encoded string of UTF-16 bytes
 *  Returns empty string on error
 */
std::u16string hexdecode_u16(const std::string encval);

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
    char ch = text[i];
    switch(ch)
      {
      case '\n':        buf += "\\n"; break;
      case '\r':        buf += "\\r"; break;
      case '\t':        buf += "\\t"; break;
      case '\b':        buf += "\\b"; break;
      case '\f':        buf += "\\f"; break;
      case '\"':        buf += "\\\""; break;
      case '\\':        buf += "\\\\"; break;
      default:
        if (iscntrl(ch))
          {
          sprintf(hex, "\\u%04x", (unsigned int)ch);
          buf += hex;
          }
        else
          {
          buf += ch;
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

/**
 * sign_extend: Sign extend an unsigned to a signed integer of the same or bigger size.
 *  Sign bit is not known at compile time.
 */
template<typename UINT, typename INT>
INT sign_extend( UINT uvalue, uint8_t signbit)
  {
  typedef typename std::make_unsigned<INT>::type uint_t;
  uint_t newuvalue = uvalue;
  UINT signmask = UINT(1U) << signbit;
  if ( newuvalue & signmask)
    newuvalue |= ~ (static_cast<uint_t>(signmask) - 1);
  return reinterpret_cast<INT &>(newuvalue);
  }

/**
 * sign_extend: Sign extend an unsigned to a signed integer of the same or bigger size.
 * Sign bit is known at compile time.
 */
template<typename UINT, typename INT, uint8_t SIGNBIT>
INT sign_extend( UINT uvalue)
  {
  typedef typename std::make_unsigned<INT>::type uint_t;
  uint_t newuvalue = uvalue;
  if ( newuvalue & ( UINT(1U) << SIGNBIT) )
    newuvalue |= ~((uint_t(1U) << SIGNBIT) - 1);
  return reinterpret_cast<INT &>(newuvalue);
  }

/**
 * get_bit: Get at a specific (compile-time defined) bit in a byte.
 */
template<uint8_t BIT>
bool get_bit(uint8_t data)
  {
  return (0 != (data & (1 << BIT)));
  }
/**
 * get_bit: Get at a specific (run-time defined) bit in a byte.
 */
inline bool get_bit(uint8_t data, uint8_t bit)
  {
  return (0 != (data & (1 << bit)));
  }
/**
 * get_uint_bits: Get at unsigned integer values within data.
 * Compile-time defined section.
 */
template<uint8_t BIT, uint8_t BITLEN>
uint16_t get_uint_bits(uint32_t data)
  {
  return (data >> BIT) & ((0x1U <<(BITLEN+1))-1);
  }
/**
 * get_uint_bits: Get at unsigned integer values within data.
 * Run-time defined section.
 */
inline uint16_t get_uint_bits(uint32_t data, uint8_t bit, uint8_t bitlen)
  {
  return (data >> bit) & ((0x1U <<(bitlen+1))-1);
  }

/**
 * get_uint_bits: Get at signed integer values within data.
 * Compile-time defined section.
 */
template<uint8_t BIT, uint8_t BITLEN>
int16_t get_int_bits(uint32_t data)
  {
  uint16_t unsigned_val = get_uint_bits<BIT,BITLEN>(data);
  return sign_extend<uint16_t,int16_t, BITLEN-1>(unsigned_val);
  }
/**
 * get_uint_bits: Get at signed integer values within data.
 * Run-time defined section.
 */
inline int16_t get_int_bits(uint32_t data, uint8_t bit, uint8_t bitlen)
  {
  uint16_t unsigned_val = get_uint_bits(data, bit, bitlen);
  return sign_extend<uint16_t,int16_t>(unsigned_val, bitlen-1);
  }

// helper to provide a big endian integer from a given number of bytes.
// The helper function checks the length before using this.
template<uint8_t BYTES, typename UINT = uint32_t>
struct ovms_bytes_impl_t
  {
  static UINT get_bytes_uint(const uint8_t *data, uint32_t index)
    {
    return (data[index + (BYTES - 1)])
      | (ovms_bytes_impl_t < BYTES - 1, UINT>::get_bytes_uint(data, index) << 8);
    }
  };
template<typename UINT>
struct ovms_bytes_impl_t<1,UINT>
  {
  static UINT get_bytes_uint(const uint8_t  *data, uint32_t index)
    {
    return (data[index]);
    }
  };

// Helper Class to access Little-Endian values.
template<uint8_t BYTES, typename UINT = uint32_t>
struct get_bytes_le_impl_t
  {
  static UINT get_bytes_uint( const uint8_t *data, uint32_t index)
    {
    return ((data[index + (BYTES - 1)]) << (UINT(8) * (BYTES - 1)))
      | get_bytes_le_impl_t < BYTES - 1 >::get_bytes_uint(data, index);
    }
  };
template<typename UINT>
struct get_bytes_le_impl_t<1, UINT>
  {
  static UINT get_bytes_uint(const uint8_t *data, uint32_t index)
    {
    return (data[index]);
    }
  };

/**
 * get_uint_bytes_be: Access to unsigned integer (big-endian) in a data buffer.
 *
 * @return true If within bounds
 */
template<uint8_t BYTES, typename UINT = uint32_t>
bool get_uint_bytes_be(const uint8_t  *data, uint32_t index, uint32_t length, UINT &res)
{
  if ((index + (BYTES - 1)) >= length) {
    return false;
  }
  res = ovms_bytes_impl_t<BYTES,UINT>::get_bytes_uint(data, index);
  return true;
}

/**
 * get_int_bytes_be: Access to signed integer (big-endian) in a data buffer.
 * @return true If within bounds
 */
template<uint8_t BYTES, typename INT = int32_t>
bool get_int_bytes_be(const uint8_t  *data, uint32_t index, uint32_t length, INT &res)
  {
  if ((index + (BYTES - 1)) >= length)
    return false;
  typedef typename std::make_unsigned<INT>::type uint_t;

  uint_t uresult = ovms_bytes_impl_t<BYTES,uint_t>::get_bytes_uint(data, index);
  res = sign_extend<uint_t, INT, BYTES * 8 - 1>(uresult);
  return true;
  }

/**
 * get_uint_buff_be: Access to unsigned integer (big-endian) in a vector data buffer.
 * @return true If successful
 */
template<uint8_t BYTES, typename UINT = uint32_t>
bool get_uint_buff_be(const std::string &data, uint32_t index,  UINT &ures)
  {
  if ((index + (BYTES - 1)) >= data.size())
    return false;
  ures = ovms_bytes_impl_t<BYTES,UINT>::get_bytes_uint(reinterpret_cast<const uint8_t *>(data.data()), index);
  return true;
  }

/**
 * get_buff_int_be: Access to signed integer (big-endian) in a std::string data buffer.
 * @return true If successful
 */
template<uint8_t BYTES, typename INT = int32_t>
bool get_buff_int_be(const std::string &data, uint32_t index,  INT &res)
  {
  if ((index + (BYTES - 1)) >= data.size())
    return false;
  typedef typename std::make_unsigned<INT>::type uint_t;
  uint_t ures = ovms_bytes_impl_t<BYTES,uint_t>::get_bytes_uint(reinterpret_cast<const uint8_t *>(data.data()), index);

  res = sign_extend<uint_t, INT, BYTES * 8 - 1>(ures);
  return true;
  }

/**
 * get_bytes_uint_le: Access to unsigned integer (little-endian) in a data buffer.
 * @return true If within bounds
 */
template<uint8_t BYTES, typename UINT = uint32_t>
bool get_bytes_uint_le(const uint8_t *data, uint32_t index, uint32_t length, UINT &res)
  {
  if ((index + (BYTES - 1)) >= length)
    return false;
  res = get_bytes_le_impl_t<BYTES,UINT>::get_bytes_uint(data, index);
  return true;
  }

/** Access to unsigned integer (little-endian) in a vector data buffer.
 * @return true If within bounds
 */
template<uint8_t BYTES, typename UINT = uint32_t>
bool get_buff_uint_le(const std::string &data, uint32_t index, UINT &res)
  {
  if ((index + (BYTES - 1)) >= data.size())
    return false;
  res = get_bytes_le_impl_t<BYTES,UINT>::get_bytes_uint(reinterpret_cast<const uint8_t *>(data.data()), index);
  return true;
  }

/** Access to signed integer (little-endian) in a vector data buffer.
 * @return true If within bounds
 */
template<uint8_t BYTES, typename INT = int32_t>
bool get_buff_int_le(const std::string &data, uint32_t index, INT &res)
  {
  if ((index + (BYTES - 1)) >= data.size())
    return false;
  typedef typename std::make_unsigned<INT>::type uint_t;
  uint_t uresult = ovms_bytes_impl_t<BYTES,uint_t>::get_bytes_uint(reinterpret_cast<const uint8_t *>(data.data()), index);
  res = sign_extend < uint_t, INT, BYTES * 8 - 1 > (uresult);
  return true;
  }


/** Access a string portion in a vector data buffer.
 * @return true if start is within bounds.
 */ 
bool get_buff_string(const uint8_t *data, uint32_t size, uint32_t index, uint32_t len, std::string &strret);

inline bool get_buff_string(const std::string &data, uint32_t index, uint32_t len, std::string &strret)
  {
  return get_buff_string(reinterpret_cast<const uint8_t *>(data.data()), data.size(), index, len, strret);
  }

// trim from start (in place)
static inline void ltrim(std::string &s) {
    s.erase(s.begin(), std::find_if(s.begin(), s.end(), [](unsigned char ch) {
        return !std::isspace(ch);
    }));
}

// trim from end (in place)
static inline void rtrim(std::string &s) {
    s.erase(std::find_if(s.rbegin(), s.rend(), [](unsigned char ch) {
        return !std::isspace(ch);
    }).base(), s.end());
}

// trim from both ends (in place)
static inline void trim(std::string &s) {
    rtrim(s);
    ltrim(s);
}

// trim from start (copying)
static inline std::string ltrim_copy(std::string s) {
    ltrim(s);
    return s;
}

// trim from end (copying)
static inline std::string rtrim_copy(std::string s) {
    rtrim(s);
    return s;
}

// trim from both ends (copying)
static inline std::string trim_copy(std::string s) {
    trim(s);
    return s;
}

/**
 * format_file_size: format a file size in human-readable format.
 * (like 1.5k 234.2M 2.1G)
 */
void format_file_size(char* buffer, std::size_t buf_size, std::size_t fsize);

/** Format to a std::string.
 */
std::string string_format(const char *fmt_str, ...) __attribute__ ((format (printf, 1, 2)));

/**
 * Return `std::string` in lower-case.
 *
 * Cf https://en.cppreference.com/w/cpp/string/byte/tolower
 *
 * Note: This may introduce unnecessary overhead as `std::tolower()` is locale aware.
 * Sometimes it may be better to use plain C `strncasecmp()`.
 */
static inline std::string str_tolower(std::string s) {
    std::transform(s.begin(), s.end(), s.begin(),
                   [](unsigned char c){ return std::tolower(c); }
                  );
    return s;
}

/**
 * Simple CSV line parser
 * Note: currently fixed to field separator ',', quoting '"', no line continuations
 * Credits: https://stackoverflow.com/a/30338543
 */
std::vector<std::string> readCSVRow(const std::string &row);
std::vector<std::vector<std::string>> readCSV(std::istream &in);

/**
 * Call-back register for registering named call-back procedures.
 *
 * The list does not shrink which is fine for our use-cases.
 * Can be made inexpensively threadsafe/re-entrant safe.
 */
template <typename FN>
class ovms_callback_register_t
  {
  private:
  class entry_t
    {
    public:
      entry_t(const std::string &caller, FN callback)
        {
        m_name = caller;
        m_callback = callback;
        }
      ~entry_t() {}
    public:
      std::string m_name;
      FN m_callback;
    };
    typedef std::forward_list<entry_t> callbacklist_t;
    callbacklist_t m_list;
  public:
    ~ovms_callback_register_t()
      {
      }
    void Register(const std::string &nametag, FN callback)
      {
      // Replace
      for (auto it = m_list.begin(); it != m_list.end(); ++it)
        {
        if ((*it).m_name == nametag)
          {
          (*it).m_callback = callback;
          return;
          }
        }
      if (!callback)
        return;
      for (auto it = m_list.begin(); it != m_list.end(); ++it)
        {
        if (!(*it).m_callback)
          {
          entry_t &entry = *it;
          entry.m_name = nametag;
          entry.m_callback = callback;
          return;
          }
        }
      m_list.push_front(entry_t(nametag, callback));
      }
    void Deregister(const std::string &nametag)
      {
      Register(nametag, nullptr);
      }
    typedef std::function<void (const std::string &nametag, FN callback)> visit_fn_t;
    void Call(visit_fn_t visit)
      {
      for (auto it = m_list.begin(); it != m_list.end(); ++it)
        {
        const entry_t &entry = *it;
        if (entry.m_callback)
          visit(entry.m_name, entry.m_callback);
        }
      }
  };


  /** Get the variable and null it in an atomic way.
   * Should probably be used sparingly.
   */
  template<typename T>
  T Atomic_Swap( volatile T &variable, T newVal)
    {
    return __atomic_exchange_n(&variable, newVal, __ATOMIC_SEQ_CST);
    }
  /** Get the variable and null it in an atomic way.
   * Should probably be used sparingly.
   * @return true if successful.
   */
  template<typename T>
  T Atomic_GetAndNull( volatile T &variable)
    {
    return Atomic_Swap<T>(variable, nullptr);
    }
  /** Swap newval into variable if variable is checkval.
   * @return true if successful.
   */
  template<typename T>
  bool Atomic_SwapIf( volatile T &variable, T checkval, T newVal)
    {
    return __atomic_compare_exchange_n(&variable, &checkval, newVal, false,  __ATOMIC_SEQ_CST, __ATOMIC_SEQ_CST);
    }

  template<typename T>
  T Atomic_Get( volatile const T &variable)
    {
    return __atomic_load_n(&variable, __ATOMIC_SEQ_CST);
    }
  template<typename T>
  T Atomic_Increment( volatile T &variable, T amt)
    {
    return __atomic_fetch_add(&variable, amt, __ATOMIC_SEQ_CST);
    }

  template<typename T>
  T Atomic_Subtract( volatile T &variable, T amt)
    {
    return __atomic_fetch_sub(&variable, amt, __ATOMIC_SEQ_CST);
    }

  /**
   * Calls a function if the lifetime of the object is less
   * than the specified constructed value.
   * Doesn't call the function if the tick count wraps.
   */
  class timer_util_t
    {
    public:
      typedef std::function<void(uint64_t start, uint64_t finish)> finish_proc_t;
    private:
      uint64_t m_start;
      finish_proc_t m_cb;
      inline static uint64_t getTime() {
        return esp_timer_get_time();
      }
    public:
      timer_util_t(const finish_proc_t &timeup_cb )
        : m_start(getTime()),
          m_cb(timeup_cb)
        {
        }
      ~timer_util_t();
    };
  constexpr unsigned floorlog2(unsigned x)
    {
    return x == 1 ? 0 : 1+floorlog2(x >> 1);
    }
  /* Maintain a smoothed average using shifts for division.
   * T should be an integer type
   * N needs to be a power of 2
   */
  template <typename T, unsigned N>
  class average_util_t
    {
    private:
      static const uint8_t _BITS = floorlog2(N);
      static const T _REST = (N-1);
      T m_avg;
      uint8_t m_n;
    public:
      average_util_t() : m_avg(0), m_n(0)
        {
        static_assert(N == (1 << _BITS), "N must be a power of 2");
        }

      void add(T val)
        {
        if (m_n == _BITS) // Optimise for templated values.
          m_avg = ((_REST * m_avg) + val) >> _BITS;
        else
          {
          // This is not quite as proper as m_n being the number of items,
          // but it is better than not ramping up at all and it works out
          // after a bit anyway.. and it's faster than using /.
          if (m_n == 0)
            m_avg = val; // Wear the cost of the if.
          else // Simplify to 2 shifts.
            m_avg = (((m_avg << m_n) - m_avg) + val) >> m_n;
          ++m_n;
          }
        }
      T get() const { return m_avg; }
      operator T() const { return m_avg; }
      void reset()
        {
        m_n = 0;
        m_avg = 0;
        }
      bool isEmpty() const
        {
        return m_n == 0;
        }
    };

  template <typename T>
  class average_util_t<T, 2>
    {
    private:
      static const T _INVALID = ~T(0);
      T m_avg;
    public:
      average_util_t() : m_avg(_INVALID)
        {
        }

      void add(T val)
        {
        if (m_avg==_INVALID)
          m_avg = val;
        else
          m_avg = (val + m_avg) >> 1;
        }
      T get() const { return (m_avg == _INVALID)?0:m_avg; }
      operator T() const { return get(); }
      void reset()
        {
        m_avg = _INVALID;
        }
      bool isEmpty() const
        {
        return m_avg == _INVALID;
        }
    };

  /* Assists in maintaining smoothed average for a period.
   * T should be an integer type
   * N needs to be a power of 2
   */
  template <typename T, unsigned N>
  class average_accum_util_t
    {
    private:
      T m_sum;
      average_util_t<T,N> ave;
    public:
      average_accum_util_t()
        : m_sum(0)
        {}

      // Add to the current sum.
      void add(T val)
        {
        m_sum += val;
        }
      // Pushes the current sum into the averager.
      void push()
        {
        ave.add(m_sum);
        m_sum = 0;
        }
      T get() { return ave.get(); }
      T sum() { return m_sum; }
      void reset()
        {
        m_sum = 0;
        ave.reset();
        }
    };

#endif // __OVMS_UTILS_H__
