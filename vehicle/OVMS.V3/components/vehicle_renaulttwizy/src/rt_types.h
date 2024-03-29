/**
 * Project:      Open Vehicle Monitor System
 * Module:       Renault Twizy battery monitor
 * 
 * (c) 2017  Michael Balzer <dexter@dexters-web.de>
 * 
 * Permission is hereby granted, free of charge, to any person obtaining a copy
 * of this software and associated documentation files (the "Software"), to deal
 * in the Software without restriction, including without limitation the rights
 * to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
 * copies of the Software, and to permit persons to whom the Software is
 * furnished to do so, subject to the following conditions:
 * 
 * The above copyright notice and this permission notice shall be included in
 * all copies or substantial portions of the Software.
 * 
 * THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
 * IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
 * FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
 * AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
 * LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
 * OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN
 * THE SOFTWARE.
 */

#ifndef __rt_types_h__
#define __rt_types_h__

#include <sys/param.h>


// Legacy type compatibility:

typedef uint8_t UINT8;
typedef unsigned int UINT;    // Note: fatfs defines this as 32 bit, so we stick to that
                              // Note: even if they are the same size (on 32-bit arch),
                              // int and long are differenciated by the compiler and
                              // are not interchangeable.
                              // UINT is defined as `typedef unsigned int UINT` in `fatfs/src/ff.h:49`
                              // while int32_t is defined as `unsigned long`.
                              // Same bit size - but different type, so we cannot redefine it differently
                              // Cf: https://docs.espressif.com/projects/esp-idf/en/latest/esp32/migration-guides/release-5.x/5.0/gcc.html#int32-t-and-uint32-t-for-xtensa-compiler
                              // Cf: https://github.com/espressif/esp-idf/issues/9511#issuecomment-1226251443
typedef uint32_t UINT32;

typedef int8_t INT8;
typedef int INT;      // 32 bit for consistency with UINT
typedef int32_t INT32;


// Math utils:

#define SQR(n) ((n)*(n))
#define ABS(n) (((n) < 0) ? -(n) : (n))

#define LIMIT_MIN(n,lim) ((n) < (lim) ? (lim) : (n))
#define LIMIT_MAX(n,lim) ((n) > (lim) ? (lim) : (n))

#define TRUNCPREC(fval,prec) (trunc((fval) * pow(10,(prec))) / pow(10,(prec)))

#endif
