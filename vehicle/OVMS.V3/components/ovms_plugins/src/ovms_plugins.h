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

#ifndef __OVMS_PLUGINS_H__
#define __OVMS_PLUGINS_H__

#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "ovms_events.h"
#include "ovms_mutex.h"
#include <string>
#include <list>
#include <map>
#include "cJSON.h"

typedef enum
  {
  OP_EQUAL = 0,                 // =
  OP_NOTEQUAL,                  // !=
  OP_LESSTHAN,                  // <
  OP_LESSTHANEQUAL,             // <=
  OP_GREATERTHAN,               // >
  OP_GREATERTHANEQUAL,          // >=
  OP_EXISTS                     // anything else
} plugin_prerequisite_operator_t;

typedef enum
  {
  EL_MODULE = 0,                // A javascript module
  EL_WEB_PAGE,                  // A web page
  EL_WEB_HOOK,                  // A web hook
  EL_WEB_RSC                    // Other web resources
  } plugin_element_type_t;

class OvmsPluginPrerequisite
  {
  public:
    OvmsPluginPrerequisite();
    ~OvmsPluginPrerequisite();

  public:
    bool LoadJSON(cJSON *json);
    void Summarise(OvmsWriter* writer);

  public:
    std::string m_target;
    plugin_prerequisite_operator_t m_operator;
    std::string m_value;
  };

typedef std::map<std::string, std::string> plugin_element_attr_map_t;

class OvmsPluginElement
  {
  public:
    OvmsPluginElement();
    ~OvmsPluginElement();

  public:
    bool LoadJSON(cJSON *json);
    void Summarise(OvmsWriter* writer);

  public:
    plugin_element_type_t m_type;
    std::string m_name;
    std::string m_path;
    plugin_element_attr_map_t m_attr;
  };

typedef std::list<OvmsPluginPrerequisite*> plugin_prerequisite_list_t;
typedef std::list<OvmsPluginElement*> plugin_element_list_t;

class OvmsPlugin
  {
  public:
    OvmsPlugin();
    ~OvmsPlugin();

  public:
    bool LoadJSON(cJSON *json);
    void Summarise(OvmsWriter* writer);

  public:
    std::string m_name;
    std::string m_title;
    std::string m_description;
    std::string m_group;
    std::string m_info;
    std::string m_maintainer;
    std::string m_version;
    plugin_prerequisite_list_t m_prerequisites;
    plugin_element_list_t m_elements;
  };

typedef std::map<std::string, OvmsPlugin*> plugin_map_t;

class OvmsPluginStore
  {
  public:
    OvmsPluginStore();
    ~OvmsPluginStore();

  public:
    bool RetrieveStore();
    bool LoadPlugins(cJSON *json);

  public:
    void Summarise(OvmsWriter* writer);
    void ListPlugins(OvmsWriter* writer);
    void ShowPlugin(OvmsWriter* writer, std::string plugin);

  public:
    std::string m_version;
    plugin_map_t m_plugins;
  };

extern OvmsPluginStore MyPluginStore;

#endif //#ifndef __OVMS_PLUGINS_H__
