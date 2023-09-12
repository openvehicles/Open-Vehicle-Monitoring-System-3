/*
;    Project:       Open Vehicle Monitor System
;    Subject:       Support for Wireguard VPN protocol (client)
;    (c)            Ludovic LANGE
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

#ifndef __OVMS_WIREGUARD_H__
#define __OVMS_WIREGUARD_H__

#include <functional>
// #include <map>
#include <string>
// #include "ovms_utils.h"
#include <esp_wireguard.h>

class OvmsWireguardClient {
 public:
  OvmsWireguardClient();
  ~OvmsWireguardClient();

 public:
  wireguard_ctx_t ctx = {0};
  bool started = false;

 public:
  void EventConfigChanged(std::string event, void* data);
  void EventSystemStart(std::string event, void* data);
  void EventTicker(std::string event, void* data);
  void EventNetUp(std::string event, void* data);
  void EventNetDown(std::string event, void* data);
  void EventNetReconfigured(std::string event, void* data);

 public:
  void TryStart();
  void TryStop();
  esp_err_t UpdateSetup();
};

extern OvmsWireguardClient MyWireguardClient;

#endif  // #ifndef __OVMS_WIREGUARD_H__
