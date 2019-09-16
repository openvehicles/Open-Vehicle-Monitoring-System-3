/*
;    Project:       Open Vehicle Monitor System
;    Date:          14th March 2017
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2011       Michael Stegen / Stegen Electronics
;    (C) 2011-2017  Mark Webb-Johnson
;    (C) 2011       Sonny Chen @ EPRO/DX
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

#ifndef __DBC_NUMBER_H__
#define __DBC_NUMBER_H__

#include <stdint.h>
#include <string>
#include <iostream>

typedef enum
  {
  DBC_NUMBER_NONE = 0,
  DBC_NUMBER_INTEGER_SIGNED,
  DBC_NUMBER_INTEGER_UNSIGNED,
  DBC_NUMBER_DOUBLE
} dbcNumberType_t;

class dbcNumber
  {
  public:
    dbcNumber();
    dbcNumber(int32_t value);
    dbcNumber(uint32_t value);
    dbcNumber(double value);
    ~dbcNumber();

  public:
    void Clear();
    bool IsDefined();
    bool IsSignedInteger();
    bool IsUnsignedInteger();
    bool IsDouble();
    void Set(int32_t value);
    void Set(uint32_t value);
    void Set(double value);
    void Cast(uint32_t value, dbcNumberType_t type);
    int32_t GetSignedInteger();
    uint32_t GetUnsignedInteger();
    double GetDouble();
    friend std::ostream& operator<<(std::ostream& os, const dbcNumber& me);
    dbcNumber& operator=(const int32_t value);
    dbcNumber& operator=(const uint32_t value);
    dbcNumber& operator=(const double value);
    dbcNumber& operator=(const dbcNumber& value);
    dbcNumber operator*(const dbcNumber& value);
    dbcNumber operator+(const dbcNumber& value);
    bool operator==(const int32_t value);
    bool operator==(const uint32_t value);
    bool operator==(const double value);

  protected:
    dbcNumberType_t m_type;
    union
      {
      uint32_t uintval;
      int32_t sintval;
      double doubleval;
      } m_value;
  };

#endif //#ifndef __DBC_NUMBER_H__
