/*
;    Project:       Open Vehicle Monitor System
;    Date:          12th November 2022
;
;    Changes:
;    1.0  Initial release - based on public domain code from Espressif (examples)
;
;    (C) 2022       Ludovic LANGE
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

#ifndef __ONVS_H__
#define __ONVS_H__

#include <stdio.h>
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_idf_version.h"

class OvmsNonVolatileStorage
  {
  public:
    OvmsNonVolatileStorage();
    ~OvmsNonVolatileStorage();

  public:
    void Init();
    void Start();
    uint64_t GetRestartCount();

  private:
    uint64_t m_restart_counter;
  };

extern OvmsNonVolatileStorage MyNonVolatileStorage;

#endif //#ifndef __ONVS_H__
