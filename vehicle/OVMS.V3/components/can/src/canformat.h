/*
;    Project:       Open Vehicle Monitor System
;    Module:        CAN dump framework
;    Date:          18th January 2018
;
;    (C) 2018       Mark Webb-Johnson
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

#ifndef __CANFORMAT_H__
#define __CANFORMAT_H__

#include <string>
#include <sys/time.h>
#include <string.h>
#include <map>
#include "can.h"
#include "ovms_utils.h"
#include "ovms_command.h"
#include "ovms_buffer.h"
using namespace std;

#define CANFORMAT_SERVE_BUFFERSIZE 1024

typedef void (*canformat_put_write_fn)(uint8_t *buffer, size_t len, void* data);

class canformat
  {
  public:
    typedef enum { Discard, Simulate, Transmit } canformat_serve_mode_t;

  public:
    canformat(const char* type, canformat_serve_mode_t mode = Discard);
    virtual ~canformat();

  public:
    const char* type();

  public: // Conversion from OVMS CAN log messages to specific format
    virtual std::string get(CAN_log_message_t* message);
    virtual std::string getheader(struct timeval *time = NULL);

  public: // Conversion from specific format to OVMS CAN log messages
    virtual size_t put(CAN_log_message_t* message, uint8_t *buffer, size_t len, void* userdata=NULL);

  private:
    const char* m_type;

  public:
    canformat_serve_mode_t GetServeMode();
    const char* GetServeModeName();
    void SetServeMode(canformat_serve_mode_t mode);
    bool IsServeDiscarding();
    void SetServeDiscarding(bool discarding);
    void SetPutCallback(canformat_put_write_fn callback);
    virtual size_t Serve(uint8_t *buffer, size_t len, void* userdata=NULL);
    virtual size_t Stuff(uint8_t *buffer, size_t len);

  protected:
    canformat_put_write_fn m_putcallback_fn;
    canformat_serve_mode_t m_servemode;
    bool m_servediscarding;
    OvmsBuffer m_buf;
  };

template<typename Type> canformat* CreateCanFormat(const char* type)
  {
  return new Type(type);
  }

canformat::canformat_serve_mode_t GetFormatModeType(std::string name);

class OvmsCanFormatFactory
  {
  public:
    OvmsCanFormatFactory();
    ~OvmsCanFormatFactory();

  public:
    typedef canformat* (*FactoryFuncPtr)(const char*);
    typedef map<const char*, FactoryFuncPtr, CmpStrOp> map_can_format_t;
    map_can_format_t m_fmap;

  public:
    template<typename Type>
    short RegisterCanFormat(const char* FormatType)
      {
      FactoryFuncPtr function = &CreateCanFormat<Type>;
        m_fmap.insert(std::make_pair(FormatType, function));
      return 0;
      };

  public:
    canformat* NewFormat(const char* FormatType);
    void RegisterCommandSet(OvmsCommand* base, const char* title,
                            void (*execute)(int, OvmsWriter*, OvmsCommand*, int, const char* const*) = NULL,
                            const char *usage = "", int min = 0, int max = 0, bool secure = true,
                            int (*validate)(OvmsWriter*, OvmsCommand*, int, const char* const*, bool) = NULL);
  };

extern OvmsCanFormatFactory MyCanFormatFactory;

#endif // __CANFORMAT_H__
