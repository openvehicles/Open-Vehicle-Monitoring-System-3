/* keygen.h
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


/*
 * The keygen module contains utility functions wrapping the wolfCrypt
 * key generation functions to product SSH friendly keys.
 */


#pragma once

#ifndef WOLFSSH_KEYGEN_H
#define WOLFSSH_KEYGEN_H

#include <wolfssh/settings.h>
#include <wolfssh/port.h>

#ifdef __cplusplus
extern "C" {
#endif


#define WOLFSSH_RSAKEY_DEFAULT_SZ 2048
#define WOLFSSH_RSAKEY_DEFAULT_E  65537


WOLFSSH_API int wolfSSH_MakeRsaKey(byte*, word32, word32, word32);


#ifdef __cplusplus
}
#endif

#endif
