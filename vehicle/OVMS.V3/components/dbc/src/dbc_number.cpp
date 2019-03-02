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

#include "ovms_log.h"
//static const char *TAG = "dbc-number";

#include <math.h>
#include <string.h>
#include "dbc_number.h"

dbcNumber::dbcNumber()
  {
  m_type = DBC_NUMBER_NONE;
  }

dbcNumber::dbcNumber(int32_t value)
  {
  Set(value);
  }

dbcNumber::dbcNumber(uint32_t value)
  {
  Set(value);
  }

dbcNumber::dbcNumber(double value)
  {
  Set(value);
  }

dbcNumber::~dbcNumber()
  {
  }

void dbcNumber::Clear()
  {
  m_type = DBC_NUMBER_NONE;
  }

bool dbcNumber::IsDefined()
  {
  return (m_type != DBC_NUMBER_NONE);
  }

bool dbcNumber::IsSignedInteger()
  {
  return (m_type == DBC_NUMBER_INTEGER_SIGNED);
  }

bool dbcNumber::IsUnsignedInteger()
  {
  return (m_type == DBC_NUMBER_INTEGER_UNSIGNED);
  }

bool dbcNumber::IsDouble()
  {
  return (m_type == DBC_NUMBER_DOUBLE);
  }

void dbcNumber::Set(int32_t value)
  {
  m_type = DBC_NUMBER_INTEGER_SIGNED;
  m_value.sintval = value;
  }

void dbcNumber::Set(uint32_t value)
  {
  m_type = DBC_NUMBER_INTEGER_UNSIGNED;
  m_value.uintval = value;
  }

void dbcNumber::Set(double value)
  {
  if (ceil(value)==value)
    {
    if (value<0)
      {
      m_type = DBC_NUMBER_INTEGER_SIGNED;
      m_value.sintval = (int32_t)value;
      }
    else
      {
      m_type = DBC_NUMBER_INTEGER_UNSIGNED;
      m_value.uintval = (uint32_t)value;
      }
    }
  else
    {
    m_type = DBC_NUMBER_DOUBLE;
    m_value.doubleval = value;
    }
  }

void dbcNumber::Cast(uint32_t value, dbcNumberType_t type)
  {
  switch(type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
    case DBC_NUMBER_INTEGER_UNSIGNED:
      m_value.uintval = value;
      m_type = type;
      break;
    default:
      break;
    }
  }

int32_t dbcNumber::GetSignedInteger()
  {
  switch (m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      return m_value.sintval;
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      return (int32_t)m_value.uintval;
      break;
    case DBC_NUMBER_DOUBLE:
      return (int32_t)m_value.doubleval;
      break;
    default:
      return 0;
      break;
    }
  }

uint32_t dbcNumber::GetUnsignedInteger()
  {
  switch (m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      return (uint32_t)m_value.sintval;
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      return m_value.uintval;
      break;
    case DBC_NUMBER_DOUBLE:
      return (uint32_t)m_value.doubleval;
      break;
    default:
      return 0;
      break;
    }
  }

double dbcNumber::GetDouble()
  {
  switch (m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      return (double)m_value.sintval;
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      return (double)m_value.uintval;
      break;
    case DBC_NUMBER_DOUBLE:
      return m_value.doubleval;
      break;
    default:
      return 0;
      break;
    }
  }

std::ostream& operator<<(std::ostream& os, const dbcNumber& me)
  {
  switch (me.m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      os << me.m_value.sintval;
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      os << me.m_value.uintval;
      break;
    case DBC_NUMBER_DOUBLE:
      os << me.m_value.doubleval;
      break;
    default:
      os << 0;
      break;
    }

    return os;
  }

dbcNumber& dbcNumber::operator=(const int32_t value)
  {
  m_type = DBC_NUMBER_INTEGER_SIGNED;
  m_value.sintval = value;
  return *this;
  }

dbcNumber& dbcNumber::operator=(const uint32_t value)
  {
  m_type = DBC_NUMBER_INTEGER_UNSIGNED;
  m_value.uintval = value;
  return *this;
  }

dbcNumber& dbcNumber::operator=(const double value)
  {
  m_type = DBC_NUMBER_DOUBLE;
  m_value.doubleval = value;
  return *this;
  }

dbcNumber& dbcNumber::operator=(const dbcNumber& value)
  {
  if (this != &value)
    {
    m_type = value.m_type;
    memcpy(&m_value,&value.m_value,sizeof(m_value));
    }
  return *this;
  }

dbcNumber dbcNumber::operator*(const dbcNumber& value)
  {
  switch (value.m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber(m_value.sintval * value.m_value.sintval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber((int32_t)m_value.uintval * value.m_value.sintval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval * value.m_value.sintval);
          break;
        default:
          break;
        }
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber(m_value.sintval * value.m_value.uintval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber(m_value.uintval * value.m_value.uintval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval * value.m_value.uintval);
          break;
        default:
          break;
        }
      break;
    case DBC_NUMBER_DOUBLE:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber((double)m_value.sintval * value.m_value.doubleval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber((double)m_value.uintval * value.m_value.doubleval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval * value.m_value.doubleval);
          break;
        default:
          break;
        }
      break;
    default:
      break;
    }
  return dbcNumber((uint32_t)0);
  }

dbcNumber dbcNumber::operator+(const dbcNumber& value)
  {
  switch (value.m_type)
    {
    case DBC_NUMBER_INTEGER_SIGNED:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber(m_value.sintval + value.m_value.sintval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber((int32_t)m_value.uintval + value.m_value.sintval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval + value.m_value.sintval);
          break;
        default:
          break;
        }
      break;
    case DBC_NUMBER_INTEGER_UNSIGNED:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber(m_value.sintval + value.m_value.uintval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber(m_value.uintval + value.m_value.uintval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval + value.m_value.uintval);
          break;
        default:
          break;
        }
      break;
    case DBC_NUMBER_DOUBLE:
      switch (m_type)
        {
        case DBC_NUMBER_INTEGER_SIGNED:
          return dbcNumber((double)m_value.sintval + value.m_value.doubleval);
          break;
        case DBC_NUMBER_INTEGER_UNSIGNED:
          return dbcNumber((double)m_value.uintval + value.m_value.doubleval);
          break;
        case DBC_NUMBER_DOUBLE:
          return dbcNumber(m_value.doubleval + value.m_value.doubleval);
          break;
        default:
          break;
        }
      break;
    default:
      break;
    }
  return dbcNumber((uint32_t)0);
  }

bool dbcNumber::operator==(const int32_t value)
  {
  return ((m_type == DBC_NUMBER_INTEGER_SIGNED)&&(m_value.sintval==value));
  }

bool dbcNumber::operator==(const uint32_t value)
  {
  return ((m_type == DBC_NUMBER_INTEGER_UNSIGNED)&&(m_value.uintval==value));
  }

bool dbcNumber::operator==(const double value)
  {
  return ((m_type == DBC_NUMBER_DOUBLE)&&(m_value.doubleval==value));
  }
