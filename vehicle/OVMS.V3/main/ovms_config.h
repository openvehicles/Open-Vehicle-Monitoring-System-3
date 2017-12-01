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
#include "esp_vfs_fat.h"
#include "wear_levelling.h"

typedef std::map<std::string, std::string> ConfigParamMap;

class OvmsConfigParam
  {
  public:
    OvmsConfigParam(std::string name, std::string title, bool writable, bool readable);
    ~OvmsConfigParam();

  public:
    void SetValue(std::string instance, std::string value);
    void DeleteParam();
    bool DeleteInstance(std::string instance);
    std::string GetValue(std::string instance);
    bool IsDefined(std::string instance);
    bool Writable();
    bool Readable();
    std::string GetName();
    const char* GetTitle() { return m_title.c_str(); }
    void Load();

  protected:
    void RewriteConfig();
    void LoadConfig();

  protected:
    std::string m_name;
    std::string m_title;
    bool m_writable;
    bool m_readable;
    bool m_loaded;

  public:
    ConfigParamMap m_map;
  };

typedef std::map<std::string, OvmsConfigParam*> ConfigMap;

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
    void SetParamValueBinary(std::string param, std::string instance, std::string value);
    void SetParamValueInt(std::string param, std::string instance, int value);
    void SetParamValueFloat(std::string param, std::string instance, float value);
    void SetParamValueBool(std::string param, std::string instance, bool value);
    void DeleteInstance(std::string param, std::string instance);
    std::string GetParamValue(std::string param, std::string instance, std::string defvalue = "");
    std::string GetParamValueBinary(std::string param, std::string instance, std::string defvalue = "");
    int GetParamValueInt(std::string param, std::string instance, int defvalue = 0);
    float GetParamValueFloat(std::string param, std::string instance, float defvalue = 0);
    bool GetParamValueBool(std::string param, std::string instance, bool defvalue = false);
    bool IsDefined(std::string param, std::string instance);
    bool ProtectedPath(std::string path);
    OvmsConfigParam* CachedParam(std::string param);

  public:
    esp_err_t mount();
    esp_err_t unmount();
    bool ismounted();

  protected:
    bool m_mounted;
    esp_vfs_fat_mount_config_t m_store_fat;
    wl_handle_t m_store_wlh;

  public:
    ConfigMap m_map;
  };

extern OvmsConfig MyConfig;

#endif //#ifndef __CONFIG_H__
