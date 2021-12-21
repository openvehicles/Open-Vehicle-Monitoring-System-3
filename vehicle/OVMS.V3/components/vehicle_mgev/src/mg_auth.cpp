/*
;    Project:       Open Vehicle Monitor System
;    Date:          25th February 2021
;
;    Changes:
;    1.0  Initial release
;
;    (C) 2021       Chris Staite
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

#include "mg_auth.h"

uint32_t MgEvPasscode::umul_lsr45(uint32_t a, uint32_t b)
{
    uint32_t i = a & 0xffffu;
    a >>= 16u;
    uint32_t j = b & 0xffffu;
    b >>= 16u;
    return (((((i * j) >> 16u) + (i * b + j * a)) >> 16u) + (a * b)) >> 13u;
}

uint32_t MgEvPasscode::iterate(uint32_t seed, uint32_t count)
{
    while (count--)
    {
        seed = (seed << 1u) | ((((((((seed >> 6u) ^ seed) >> 12u) ^ seed) >> 10u) ^ seed) >> 2u) & 1u);
    }
    return seed;
}

uint32_t MgEvPasscode::GWMKey1(uint32_t seed)
{
    uint32_t i = seed & 0xffffu;
    uint32_t i2 = 1u;
    uint32_t i3 = 0x12e5u;
    while (i3)
    {
        if (i3 & 1u)
        {
            uint32_t tmp = i2 * i;
            i2 = (tmp - (umul_lsr45(tmp, 0x82b87f05u) * 0x3eabu));
        }
        uint32_t tmp2 = i * i;
        i = (tmp2 - (umul_lsr45(tmp2, 0x82b87f05u) * 0x3eabu));
        i3 >>= 1u;
    }
    uint32_t i5 = ((i2 >> 8u) + i2) ^ 0x0fu;
    uint32_t i6 = (i2 ^ (i5 << 8u)) & 0xff00u;
    uint32_t i7 = ((i2 ^ i5) & 0xffu) | i6;
    return (i7 | i7 << 16u) ^ 0xad0779e2u;
}

uint32_t MgEvPasscode::GWMKey2(uint32_t seed)
{
    uint32_t count = 0x25u + (((seed >> 0x18u) & 0x1cu) ^ 0x08u);
    return iterate(seed, count) ^ 0xdc8fe1aeu;
}

uint32_t MgEvPasscode::BCMKey(uint32_t seed)
{
    uint32_t count = 0x2bu + (((seed >> 0x18u) & 0x17u) ^ 0x02u);
    return iterate(seed, count) ^ 0x594e348au;
}

        