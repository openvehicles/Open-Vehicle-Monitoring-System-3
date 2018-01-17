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

#include "ovms_utils.h"

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
    case 2: code = "range";         break;
    case 3: code = "performance";   break;
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
  else if (code == "range")         key = 2;
  else if (code == "performance")   key = 3;
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
