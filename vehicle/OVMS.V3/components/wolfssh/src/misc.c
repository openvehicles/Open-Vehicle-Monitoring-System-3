/* misc.c
 *
 * Copyright (C) 2014-2016 wolfSSL Inc.
 *
 * This file is part of wolfSSH.
 *
 * wolfSSH is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 3 of the License, or
 * (at your option) any later version.
 *
 * wolfSSH is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with wolfSSH.  If not, see <http://www.gnu.org/licenses/>.
 */

/*
 * The misc module contains inline functions. This file is either included
 * into source files or built separately depending on the inline configure
 * option.
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif


#include <wolfssh/settings.h>


#ifndef WOLFSSH_MISC_C
#define WOLFSSH_MISC_C


#include <wolfssh/misc.h>


#ifdef NO_INLINE
    #define STATIC
#else
    #define STATIC static
#endif


/* Check for if compiling misc.c when not needed. */
#if !defined(WOLFSSH_MISC_INCLUDED) && !defined(NO_INLINE)

    #warning misc.c does not need to be compiled when using inline (NO_INLINE not defined)

#else /* !WOLFSSL_MISC_INCLUDED && !NO_INLINE */


#ifndef min
STATIC INLINE uint32_t min(uint32_t a, uint32_t b)
{
    return a > b ? b : a;
}
#endif /* min */


/* convert opaque to 32 bit integer */
STATIC INLINE void ato32(const uint8_t* c, uint32_t* u32)
{
    *u32 = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}


/* convert 32 bit integer to opaque */
STATIC INLINE void c32toa(uint32_t u32, uint8_t* c)
{
    c[0] = (u32 >> 24) & 0xff;
    c[1] = (u32 >> 16) & 0xff;
    c[2] = (u32 >>  8) & 0xff;
    c[3] =  u32 & 0xff;
}


/* Make sure compiler doesn't skip */
STATIC INLINE void ForceZero(const void* mem, uint32_t length)
{
    volatile uint8_t* z = (volatile uint8_t*)mem;

    while (length--) *z++ = 0;
}


/* check all length bytes for equality, return 0 on success */
STATIC INLINE int ConstantCompare(const uint8_t* a, const uint8_t* b,
                                  uint32_t length)
{
    uint32_t i;
    uint32_t compareSum = 0;

    for (i = 0; i < length; i++) {
        compareSum |= a[i] ^ b[i];
    }

    return compareSum;
}


#undef STATIC


#endif /* !WOLFSSL_MISC_INCLUDED && !NO_INLINE */


#endif /* WOLFSSH_MISC_C */
