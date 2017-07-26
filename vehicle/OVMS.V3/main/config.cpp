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

#include "config.h"
#include "command.h"
#include "spiffs_vfs.h"

OvmsConfig MyConfig __attribute__ ((init_priority (1010)));

void spiffs_mount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.mount();
  writer->puts("Mounted SPIFFS");
  }

void spiffs_unmount(int verbosity, OvmsWriter* writer, OvmsCommand* cmd, int argc, const char* const* argv)
  {
  MyConfig.unmount();
  writer->puts("Unmounted SPIFFS");
  }

OvmsConfig::OvmsConfig()
  {
  puts("Initialising SPIFFS Framework");
  OvmsCommand* cmd_spiffs = MyCommandApp.RegisterCommand("spiffs","SPIFFS framework",NULL,"<$C>",1,1);
  cmd_spiffs->RegisterCommand("mount","Mount SPIFFS",spiffs_mount,"",0,0);
  cmd_spiffs->RegisterCommand("unmount","Unmount SPIFFS",spiffs_unmount,"",0,0);
  }

OvmsConfig::~OvmsConfig()
  {
  }

esp_err_t OvmsConfig::mount()
  {
  if (!spiffs_is_registered)
    vfs_spiffs_register();

  if (!spiffs_is_mounted)
    spiffs_mount();

  m_mounted = true;
  return ESP_OK;
  }

esp_err_t OvmsConfig::unmount()
  {
  if (spiffs_is_mounted)
    spiffs_unmount(0);

  m_mounted = false;
  return ESP_OK;
  }

bool OvmsConfig::ismounted()
  {
  return m_mounted;
  }

