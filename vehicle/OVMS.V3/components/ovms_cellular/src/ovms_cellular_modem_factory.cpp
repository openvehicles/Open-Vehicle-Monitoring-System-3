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

#include "ovms_log.h"
static const char *TAG = "cellular-modem-factory";

#include <string.h>

#include "ovms_cellular.h"

////////////////////////////////////////////////////////////////////////////////
// The modem factory
// Responsible for registering modem drivers, and providing a factory
// function to create on demand.

OvmsCellularModemFactory MyCellularModemFactory __attribute__ ((init_priority (4610)));

OvmsCellularModemFactory::OvmsCellularModemFactory()
  {
  ESP_LOGI(TAG, "Initialising CELLULAR MODEM Factory (4601)");
  }

OvmsCellularModemFactory::~OvmsCellularModemFactory()
  {
  }

modemdriver* OvmsCellularModemFactory::NewCellularModemDriver(const char* ModelType)
  {
  OvmsCellularModemFactory::map_modemdriver_t::iterator iter = m_drivermap.find(ModelType);
  if (iter != m_drivermap.end())
    {
    return iter->second.construct();
    }
  return NULL;
  }
