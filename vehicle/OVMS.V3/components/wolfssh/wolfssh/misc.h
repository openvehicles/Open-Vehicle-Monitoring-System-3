/* misc.h
 *
 * Copyright (C) 2014-2020 wolfSSL Inc.
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

#pragma once

#ifndef WOLFSSH_MISC_H
#define WOLFSSH_MISC_H


#ifdef __cplusplus
    extern "C" {
#endif


#include <wolfssl/wolfcrypt/types.h>
#include <wolfssh/settings.h>
#include <wolfssh/port.h>


#ifdef NO_INLINE


#ifndef min
WOLFSSH_LOCAL word32 min(word32, word32);
#endif /* min */

WOLFSSH_LOCAL void ato32(const byte*, word32*);
WOLFSSH_LOCAL void c32toa(word32, byte*);
WOLFSSH_LOCAL void ForceZero(const void*, word32);
WOLFSSH_LOCAL int ConstantCompare(const byte*, const byte*, word32);
WOLFSSH_LOCAL void CreateMpint(byte*, word32*, byte*);


#endif /* NO_INLINE */


#ifdef __cplusplus
    }   /* extern "C" */
#endif


#endif /* WOLFSSH_MISC_H */

