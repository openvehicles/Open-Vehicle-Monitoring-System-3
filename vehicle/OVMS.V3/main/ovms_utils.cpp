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

#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <dirent.h>
#include <stdarg.h>
#include <memory>
#include <fstream>
#include <istream>
#include "ovms_utils.h"
#include "ovms_config.h"
#include "ovms_events.h"
#include "metrics_standard.h"
#include "ovms_version.h"
#include "esp_idf_version.h"

/**
 * chargestate_code: convert legacy chargestate key to code
 */
std::string chargestate_code(const int key)
  {
  std::string code;
  switch (key)
    {
    case  1: code = "charging";   break;
    case  2: code = "topoff";     break;
    case  4: code = "done";       break;
    case 13: code = "prepare";    break;
    case 14: code = "timerwait";  break;
    case 15: code = "heating";    break;
    case 21: code = "stopped";    break;
    default: code = "";
    }
  return code;
  }

/**
 * chargestate_key: convert chargestate code to legacy key
 */
int chargestate_key(const std::string code)
  {
  int key;
  if      (code == "charging")    key = 1;
  else if (code == "topoff")      key = 2;
  else if (code == "done")        key = 4;
  else if (code == "prepare")     key = 13;
  else if (code == "timerwait")   key = 14;
  else if (code == "heating")     key = 15;
  else if (code == "stopped")     key = 21;
  else key = 0;
  return key;
  }

/**
 * chargesubstate_code: convert legacy charge substate key to code
 */
std::string chargesubstate_code(const int key)
  {
  std::string code;
  switch (key)
    {
    case 0x01: code = "scheduledstop";    break;
    case 0x02: code = "scheduledstart";   break;
    case 0x03: code = "onrequest";        break;
    case 0x05: code = "timerwait";        break;
    case 0x07: code = "powerwait";        break;
    case 0x0d: code = "stopped";          break;
    case 0x0e: code = "interrupted";      break;
    default: code = "";
    }
  return code;
  }

/**
 * chargesubstate_key: convert charge substate code to legacy key
 */
int chargesubstate_key(const std::string code)
  {
  int key;
  if      (code == "scheduledstop")       key = 0x01;
  else if (code == "scheduledstart")      key = 0x02;
  else if (code == "onrequest")           key = 0x03;
  else if (code == "timerwait")           key = 0x05;
  else if (code == "powerwait")           key = 0x07;
  else if (code == "stopped")             key = 0x0d;
  else if (code == "interrupted")         key = 0x0e;
  else key = 0;
  return key;
  }

/**
 * chargemode_code: convert legacy chargemode key to code
 */
std::string chargemode_code(const int key)
  {
  std::string code;
  switch (key)
    {
    case 0: code = "standard";      break;
    case 1: code = "storage";       break;
    case 3: code = "range";         break;
    case 4: code = "performance";   break;
    default: code = "";
    }
  return code;
  }

/**
 * chargemode_key: convert chargemode code to legacy key
 */
int chargemode_key(const std::string code)
  {
  int key;
  if      (code == "standard")      key = 0;
  else if (code == "storage")       key = 1;
  else if (code == "range")         key = 3;
  else if (code == "performance")   key = 4;
  else key = -1;
  return key;
  }

/**
 * mp_encode: encode string for MP transport;
 *  - replace '\r\n' by '\r'
 *  - replace '\n' by '\r'
 *  - replace ',' by ';'
 */
std::string mp_encode(const std::string text)
  {
  std::string res;
  char lc = 0;
  res.reserve(text.length());
  for (int i=0; i<text.length(); i++)
    {
    if (text[i] == '\n')
      {
      if (lc != '\r')
        res += '\r';
      }
    else if (text[i] == ',')
      {
      res += ';';
      }
    else
      {
      res += text[i];
      }

    lc = text[i];
    }
  return res;
  }

/**
 * mp_encode: encode string for MP transport;
 *  - replace '\r\n' by '\r'
 *  - replace '\n' by '\r'
 *  - replace ',' by ';'
 */
extram::string mp_encode(const extram::string text)
  {
  extram::string res;
  char lc = 0;
  res.reserve(text.length());
  for (int i=0; i<text.length(); i++)
    {
    if (text[i] == '\n')
      {
      if (lc != '\r')
        res += '\r';
      }
    else if (text[i] == ',')
      {
      res += ';';
      }
    else
      {
      res += text[i];
      }

    lc = text[i];
    }
  return res;
  }

/**
 * stripcr:
 *  - replace '\r\n' by '\n'
 */
extram::string stripcr(const extram::string& text)
  {
  extram::string res;
  res.reserve(text.length());
  for (int i = 0; i < text.length(); i++)
    {
    if (text[i] != '\r' || (i < text.length()-1 && text[i+1] != '\n'))
      res += text[i];
    }
  return res;
  }

/**
 * stripesc: remove terminal escape sequences from (log) string
 */
std::string stripesc(const char* s)
  {
  std::string res;
  res.reserve(200);
  bool skip = false;
  while (s && *s)
    {
    if (*s == '\033' && *(s+1) == '[')
      skip = true;
    else if (!skip)
      res += *s;
    else if (*s == 'm')
      skip = false;
    ++s;
    }
  return res;
  }

/**
 * replace_substrings: replace all `from` substrings by `to` in `text`
 */
void replace_substrings(std::string &text, const std::string from, const std::string to)
  {
  if (from.length() > 0)
    {
    size_t pos = 0;
    size_t len = from.length();
    while ((pos = text.find(from, pos)) != std::string::npos)
      {
      text.replace(pos, len, to);
      pos += to.length();
      }
    }
  }


/**
 * HexByte: Write a single byte as two hexadecimal characters
 * Returns new pointer to end of string (p + 2)
 */
char* HexByte(char* p, uint8_t byte)
  {
  uint8_t nibble;

  nibble = byte >> 4;   // high nibble
  nibble += '0'; if (nibble>'9') nibble += 39;
  *p = nibble; p++;

  nibble = byte & 0x0f; // low nibble
  nibble += '0'; if (nibble>'9') nibble += 39;
  *p = nibble; p++;
  return p;
  }

/**
 * FormatHexDump: create/fill hexdump buffer including printable representation
 * Note: allocates buffer as necessary in *bufferp, caller must free.
 * Returns new remaining length
 */
size_t FormatHexDump(char** bufferp, const char* data, size_t rlength, size_t colsize /*=16*/)
  {
  const char *s = data;

  if (rlength>0)
    {
    if (!*bufferp)
      *bufferp = (char*) ExternalRamMalloc(colsize*4 + 4); // space for 16x3 + 2 + 16 + 1(\0)

    char *p = *bufferp;
    const char *os = s;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        sprintf(p,"%2.2x ",*s);
        s++;
        }
      else
        {
        sprintf(p,"   ");
        }
      p+=3;
      }
    sprintf(p,"| ");
    p += 2;
    s = os;
    for (int k=0;k<colsize;k++)
      {
      if (k<rlength)
        {
        if (isprint((int)*s))
          { *p = *s; }
        else
          { *p = '.'; }
        s++;
        }
      else
        {
        *p = ' ';
        }
      p++;
      }
    *p = 0;
    if (rlength > colsize)
      rlength -= colsize;
    else
      rlength = 0;
    }

  return rlength;
  }


/**
 * hexencode: encode a string of bytes into hexadecimal form
 */
std::string hexencode(const std::string value)
  {
  std::string encval;
  size_t len = value.length();
  encval.reserve(len * 2);
  char buf[3] = {0};
  for (size_t i = 0; i < len; ++i)
    {
    unsigned char c = value.at(i);
    HexByte(buf, c);
    encval += buf;
    }
  return encval;
  }


/**
 * hexdecode: decode a hexadecimal encoded string of bytes
 *  Returns empty string on error
 */
std::string hexdecode(const std::string encval)
  {
  std::string value;
  size_t len = encval.length();
  if (len == 0)
    return value;
  if (encval.find_first_not_of("0123456789ABCDEFabcdef", 0) != std::string::npos || (len & 1))
    return value;
  value.reserve(len / 2);
  char buf[3] = {0};
  for (size_t i = 0; i < len; i += 2)
    {
    buf[0] = encval.at(i);
    buf[1] = encval.at(i + 1);
    unsigned char c = strtoul(buf, NULL, 16);
    value += c;
    }
  return value;
  }

/**
 * hexdecode_u16: decode a hexadecimal encoded string of UTF-16 bytes
 *  Returns empty string on error
 */
std::u16string hexdecode_u16(const std::string encval)
  {
  std::u16string value;
  size_t len = encval.length();
  if (len == 0)
    return value;
  if (encval.find_first_not_of("0123456789ABCDEFabcdef", 0) != std::string::npos || (len & 3))
    return value;
  value.reserve(len / 4);
  char buf[5] = {0};
  for (size_t i = 0; i < len; i += 4)
    {
    buf[0] = encval.at(i);
    buf[1] = encval.at(i + 1);
    buf[2] = encval.at(i + 2);
    buf[3] = encval.at(i + 3);
    char16_t c = strtoul(buf, NULL, 16);
    value += c;
    }
  return value;
  }


/**
 * pwgen: simple password generator
 * Note: take care to seed the pseudo random generator i.e. by
 *  srand48(StdMetrics.ms_m_monotonic->AsInt() * StdMetrics.ms_m_freeram->AsInt());
 */
std::string pwgen(int length)
  {
  const char cs1[] = "abcdefghijklmnopqrstuvwxyz";
  const char cs2[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789";
  const char cs3[] = "!@#$%^&*()_-=+;:,.?";
  std::string res;
  for (int i=0; i < length; i++)
    {
    double r = drand48();
    if (r > 0.4)
      res.push_back((char)cs1[(int)(drand48()*(sizeof(cs1)-1))]);
    else if (r > 0.2)
      res.push_back((char)cs2[(int)(drand48()*(sizeof(cs2)-1))]);
    else
      res.push_back((char)cs3[(int)(drand48()*(sizeof(cs3)-1))]);
    }
  return res;
  }


#ifdef CONFIG_FREERTOS_USE_TRACE_FACILITY
/**
 * TaskGetHandle: get task handle by name
 * (FreeRTOS xTaskGetHandle() is not available)
 */
TaskHandle_t TaskGetHandle(const char *name)
  {
  TaskHandle_t res = 0;
  TaskStatus_t* pxTaskStatusArray;
  volatile UBaseType_t uxArraySize, x;

  uxArraySize = uxTaskGetNumberOfTasks();
  pxTaskStatusArray = (TaskStatus_t*)pvPortMalloc(uxArraySize * sizeof(TaskStatus_t));
  if (pxTaskStatusArray != NULL)
    {
    uxArraySize = uxTaskGetSystemState(pxTaskStatusArray, uxArraySize, NULL);
    for (x = 0; x < uxArraySize; x++)
      {
      if (strcmp(pxTaskStatusArray[x].pcTaskName, name) == 0)
        {
        res = pxTaskStatusArray[x].xHandle;
        break;
        }
      }
    vPortFree( pxTaskStatusArray );
    }
  return res;
  }
#endif // CONFIG_FREERTOS_USE_TRACE_FACILITY


/**
 * mkpath: mkdir -p
 *
 * Original: https://stackoverflow.com/a/12904145
 */
int mkpath(std::string path, mode_t mode /*=0*/)
  {
  size_t pre = 0, pos;
  std::string dir;
  int mdret;

  if (!endsWith(path, '/'))
    {
    // force trailing / so we can handle everything in loop
    path.append("/");
    }

  while ((pos = path.find_first_of('/', pre)) != std::string::npos)
    {
    dir = path.substr(0, pos++);
    pre = pos;
    if (dir.size() == 0)
      continue; // if leading / first time is 0 length
    if ((mdret = mkdir(dir.c_str(), mode)) && errno != EEXIST)
      return mdret;
    }

  return 0;
  }

/**
 * rmtree: rmdir -r
 */
int rmtree(const std::string path)
  {
  DIR *dir = opendir(path.c_str());
  if (!dir)
    return 0;

  struct dirent *dp;
  struct stat st;
  std::string sub;
  bool ok = true;

  while ((dp = readdir(dir)) != NULL) {
    sub = path + "/" + dp->d_name;
    if (stat(sub.c_str(), &st)) {
      ok = false;
      break;
    }
    if (S_ISDIR(st.st_mode))
      ok = (rmtree(sub) == 0);
    else
      ok = (unlink(sub.c_str()) == 0);
    if (!ok)
      break;
  }

  closedir(dir);
  ok = (rmdir(path.c_str()) == 0);
  return ok ? 0 : -1;
  }

/**
 * path_exists: check if filesystem path exists
 */
bool path_exists(const std::string path)
  {
  struct stat st;
  return (stat(path.c_str(), &st) == 0);
  }

/**
 * load file into string:
 *  - return value: 0 = ok / errno
 */
int load_file(const std::string &path, extram::string &content)
  {
  std::ifstream file(path, std::ios::in | std::ios::binary | std::ios::ate);
  if (file.is_open())
    {
    auto size = file.tellg();
    if (size > 0)
      {
      content.resize(size, '\0');
      file.seekg(0);
      file.read(&content[0], size);
      }
    }
  if (file.fail())
    return errno;
  else
    return 0;
  }

/**
 * save file from string:
 *  - creates missing directories automatically & signals system.vfs.file.changed
 *  - return value: 0 = ok / errno
 */
int save_file(const std::string &path, extram::string &content)
  {
  // create path:
  size_t n = path.rfind('/');
  if (n != 0 && n != std::string::npos)
    {
    std::string dir = path.substr(0, n);
    if (!path_exists(dir))
      {
      if (mkpath(dir) != 0)
        return errno;
      }
    }

  // write file:
  std::ofstream file(path, std::ios::out | std::ios::binary | std::ios::trunc);
  if (file.is_open())
    file.write(content.data(), content.size());
  if (file.fail())
    {
    return errno;
    }
  else
    {
    MyEvents.SignalEvent("system.vfs.file.changed", (void*)path.c_str(), path.size()+1);
    return 0;
    }
  }


/**
 * mqtt_topic: convert dotted string (e.g. notification subtype) to MQTT topic
 *  - replace '.' by '/'
 */
std::string mqtt_topic(const std::string text)
  {
  std::string buf;
  buf.reserve(text.size());
  for (int i=0; i<text.size(); i++)
    {
    switch(text[i])
      {
      case '.':         buf += '/'; break;
      default:          buf += text[i]; break;
      }
    }
	return buf;
  }

/**
 * get_user_agent: create User-Agent string from OVMS versions & vehicle ID
 *  Scheme: "ovms/v<hw_version> (<vehicle_id> <sw_version>)"
 */
std::string get_user_agent()
  {
  std::string ua;
  ua = "ovms/";
  ua.append(GetOVMSProduct());
  ua.append(" (");
  ua.append(MyConfig.GetParamValue("vehicle","id",""));
  ua.append(" ");
  ua.append(StandardMetrics.ms_m_version->AsString());
  ua.append(")");
  return ua;
  }

/**
 * float2double: minimize precision errors on float→double conversion
 *  Casting a float to double sets the additional precision bits to 0, resulting in
 *  the double to have a significant offset from the rounded float; e.g.
 *  11.08 becomes 11.079999923706055.
 *  (Is there a better implementation for this than sprintf/atof?)
 */
double float2double(float f)
  {
  char buf[16];
  snprintf(buf, sizeof buf, "%g", f);
  return atof(buf);
  }

/**
 * idtag: create object instance tag for registrations
 */
std::string idtag(const char* tag, void* instance)
  {
  std::ostringstream buf;
  buf << tag << "-" << instance;
  std::string res = buf.str();
  return res;
  }

/**
 * get_buff_string: Helper function to get at string value from sized buffer.
 */
bool get_buff_string(const uint8_t *data, uint32_t size, uint32_t index, uint32_t len, std::string &strret)
  {
  int remain = size - index;
  if (remain < 0)
    return false;
  if (remain < len)
    len = remain;
  const char *begin = reinterpret_cast<const char *>(data + index) ;
  const char *end   = begin + len;
  strret.assign<const char *>(begin, end);
  return true;
  }

#if ESP_IDF_VERSION < ESP_IDF_VERSION_VAL(4, 4, 0)
/**
 * realpath: this implementation is from (a newer version of) newlib, in particular
 * https://github.com/espressif/esp-idf/commit/d83ce227aa987d648e1a0dddba32b88846374815#diff-91b1abbc42db113c5dcf9c11c594f0b3e75cae5f6dfba9ef8b68e26bbd422f4a
 * - Note: strchrnul has been replaced with strchr as it's also non implemented
 * in our versions.
 */
char * realpath(const char *file_name, char *resolved_name)
{
    char * out_path = resolved_name;
    if (out_path == NULL) {
        /* allowed as an extension, allocate memory for the output path */
        out_path = (char *)malloc(PATH_MAX);
        if (out_path == NULL) {
            errno = ENOMEM;
            return NULL;
        }
    }

    /* canonical path starts with / */
    strlcpy(out_path, "/", PATH_MAX);

    /* pointers moving over the input and output path buffers */
    const char* in_ptr = file_name;
    char* out_ptr = out_path + 1;
    /* number of path components in the output buffer */
    size_t out_depth = 0;


    while (*in_ptr) {
        /* "path component" is the part between two '/' path separators.
         * locate the next path component in the input path:
         */
        const char* end_of_path_component = strchr(in_ptr, '/');
        if (NULL == end_of_path_component) {
          end_of_path_component = in_ptr + strlen(in_ptr);
        }
        size_t path_component_len = end_of_path_component - in_ptr;

        if (path_component_len == 0 ||
            (path_component_len == 1 && in_ptr[0] == '.')) {
            /* empty path component or '.' - nothing to do */
        } else if (path_component_len == 2 && in_ptr[0] == '.' && in_ptr[1] == '.') {
            /* '..' - remove one path component from the output */
            if (out_depth == 0) {
                /* nothing to remove */
            } else if (out_depth == 1) {
                /* there is only one path component in output;
                 * remove it, but keep the leading separator
                 */
                out_ptr = out_path + 1;
                *out_ptr = '\0';
                out_depth = 0;
            } else {
                /* remove last path component and the separator preceding it */
                char * prev_sep = strrchr(out_path, '/');
                assert(prev_sep > out_path);  /* this shouldn't be the leading separator */
                out_ptr = prev_sep;
                *out_ptr = '\0';
                --out_depth;
            }
        } else {
            /* copy path component to output; +1 is for the separator  */
            if (out_ptr - out_path + 1 + path_component_len > PATH_MAX - 1) {
                /* output buffer insufficient */
                errno = E2BIG;
                goto fail;
            } else {
                /* add separator if necessary */
                if (out_depth > 0) {
                    *out_ptr = '/';
                    ++out_ptr;
                }
                memcpy(out_ptr, in_ptr, path_component_len);
                out_ptr += path_component_len;
                *out_ptr = '\0';
                ++out_depth;
            }
        }
        /* move input pointer to separator right after this path component */
        in_ptr += path_component_len;
        if (*in_ptr != '\0') {
            /* move past it unless already at the end of the input string */
            ++in_ptr;
        }
    }
    return out_path;

fail:
    if (resolved_name == NULL) {
        /* out_path was allocated, free it */
        free(out_path);
    }
    return NULL;
}
#endif

/**
 * Format string with std::string result (sprintf for std::string).
 */
std::string string_format(const char * fmt_str, ...)
  {
  va_list ap;
  char *fp = NULL;
  va_start(ap, fmt_str);
  int ret = vasprintf(&fp, fmt_str, ap);
  va_end(ap);
  std::unique_ptr<char[]> formatted(fp);
  if (ret >= 0)
    return std::string(formatted.get());
  return "";
  }

/**
 * format_file_size: format a file size in human-readable format.
 * (like 1.5k 234.2M 2.1G)
 */
void format_file_size(char* buffer, std::size_t buf_size, std::size_t fsize) {
  if (fsize < 1024) {
    snprintf(buffer, buf_size, "%d", (int) fsize);
  } else if (fsize < 0x100000) {
    snprintf(buffer, buf_size, "%.1fk", (double) fsize / 1024.0);
  } else if (fsize < 0x40000000) {
    snprintf(buffer, buf_size, "%.1fM", (double) fsize / 1048576);
  } else {
    snprintf(buffer, buf_size, "%.1fG", (double) fsize / 1073741824);
  }
}

timer_util_t::~timer_util_t()
  {
  if (m_cb)
    {
    m_cb(m_start, getTime());
    }
  }


/**
 * Simple CSV line parser
 * Note: currently fixed to field separator ',', quoting '"', no line continuations
 * Credits: https://stackoverflow.com/a/30338543
 */

enum class CSVState {
  UnquotedField,
  QuotedField,
  QuotedQuote
};

std::vector<std::string> readCSVRow(const std::string &row) {
  CSVState state = CSVState::UnquotedField;
  std::vector<std::string> fields {""};
  size_t i = 0; // index of the current field
  for (char c : row) {
    switch (state) {
      case CSVState::UnquotedField:
        switch (c) {
          case ',': // end of field
                    fields.push_back(""); i++;
                    break;
          case '"': state = CSVState::QuotedField;
                    break;
          default:  fields[i].push_back(c);
                    break; }
        break;
      case CSVState::QuotedField:
        switch (c) {
          case '"': state = CSVState::QuotedQuote;
                    break;
          default:  fields[i].push_back(c);
                    break; }
        break;
      case CSVState::QuotedQuote:
        switch (c) {
          case ',': // , after closing quote
                    fields.push_back(""); i++;
                    state = CSVState::UnquotedField;
                    break;
          case '"': // "" -> "
                    fields[i].push_back('"');
                    state = CSVState::QuotedField;
                    break;
          default:  // end of quote
                    state = CSVState::UnquotedField;
                    break; }
        break;
    }
  }
  return fields;
}

// Read CSV file, Excel dialect. Accept "quoted fields ""with quotes"""
std::vector<std::vector<std::string>> readCSV(std::istream &in) {
  std::vector<std::vector<std::string>> table;
  std::string row;
  while (!in.eof()) {
    std::getline(in, row);
    if (in.bad() || in.fail()) {
      break;
    }
    auto fields = readCSVRow(row);
    table.push_back(fields);
  }
  return table;
}
