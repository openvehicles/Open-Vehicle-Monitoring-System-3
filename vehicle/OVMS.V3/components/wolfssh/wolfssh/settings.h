/* settings.h
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
 * The settings header provides a method for developers to provide the
 * internal preprocessor defines that the configure script provides to
 * environments that do not use the configure script.
 */


#ifndef _WOLFSSH_SETTINGS_H_
#define _WOLFSSH_SETTINGS_H_

#include <wolfssh/visibility.h>

#ifdef WOLFSSL_USER_SETTINGS
    #include "user_settings.h"
#endif

#ifdef __cplusplus
extern "C" {
#endif


/* user settings here */
#ifndef WMALLOC_USER
    #define USE_WOLFSSH_MEMORY  /* default memory handlers */
#endif /* WMALLOC_USER */


#if defined (_WIN32)
    #define USE_WINDOWS_API
    #define _CRT_SECURE_NO_WARNINGS
#endif

#ifdef WOLFSSL_NUCLEUS
    #ifndef WOLFSSH_STOREHANDLE
    #define WOLFSSH_STOREHANDLE
    #endif
#endif

#ifdef FREESCALE_MQX
    #define NO_STDIO_FILESYSTEM
    #ifndef WOLFSSH_STOREHANDLE
        #define WOLFSSH_STOREHANDLE
    #endif

    #ifdef WOLFSSH_SCP
        #error wolfSSH SCP not ported to MQX yet
    #endif
#endif

#if defined(WOLFSSH_SCP) && defined(NO_WOLFSSH_SERVER)
    #error only SCP server side supported
#endif

#ifdef __cplusplus
}
#endif

#endif /* _WOLFSSH_SETTINGS_H_ */

