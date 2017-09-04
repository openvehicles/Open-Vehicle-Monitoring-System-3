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

#ifndef __CONFIG_H__
#define __CONFIG_H__

#include "string"
#include "map"
#include "esp_err.h"

class OvmsConfigParam
  {
  public:
    OvmsConfigParam(std::string name, std::string title, bool writable, bool readable);
    ~OvmsConfigParam();

  public:
    void SetValue(std::string instance, std::string value);
    void DeleteParam();
    void DeleteInstance(std::string instance);
    std::string GetValue(std::string instance);

  protected:
    void RewriteConfig();

  protected:
    std::string m_name;
    std::string m_title;
    bool m_writable;
    bool m_readable;
    std::map<std::string, std::string> m_map;
  };

class OvmsConfig
  {
  public:
    OvmsConfig();
    ~OvmsConfig();

  public:
    void RegisterParam(std::string name, std::string title, bool writable=true, bool readable=true);
    void DeregisterParam(std::string name);

  public:
    void SetParamValue(std::string param, std::string instance, std::string value);
    void DeleteInstance(std::string param, std::string instance);
    std::string GetParamValue(std::string param, std::string instance);
    bool ProtectedPath(std::string path);

  public:
    esp_err_t mount();
    esp_err_t unmount();
    bool ismounted();

  protected:
    bool m_mounted;
    std::map<std::string, OvmsConfigParam*> m_map;

  protected:
    OvmsConfigParam* CachedParam(std::string param);
  };

extern OvmsConfig MyConfig;

#endif //#ifndef __CONFIG_H__
