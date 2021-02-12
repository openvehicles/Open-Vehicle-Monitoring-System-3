/* visibility.h
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
 * The visibility header handles the visibility of function prototypes
 * between the local (used between modules in the library) and public
 * (exported for the library user) APIs.
 */


#pragma once


#ifdef __cplusplus
extern "C" {
#endif

/* WOLFSSH_API is used for the public API symbols.
        It either imports or exports (or does nothing for static builds)

   WOLFSSH_LOCAL is used for non-API symbols (private).
*/

#if defined(BUILDING_WOLFSSH)
    #if defined(_MSC_VER) || defined(__CYGWIN__)
        #ifdef WOLFSSH_DLL
            #define WOLFSSH_API extern __declspec(dllexport)
        #else
            #define WOLFSSH_API
        #endif
        #define WOLFSSH_LOCAL
    #elif defined(HAVE_VISIBILITY) && HAVE_VISIBILITY
        #define WOLFSSH_API   __attribute__ ((visibility("default")))
        #define WOLFSSH_LOCAL __attribute__ ((visibility("hidden")))
    #elif defined(__SUNPRO_C) && (__SUNPRO_C >= 0x550)
        #define WOLFSSH_API   __global
        #define WOLFSSH_LOCAL __hidden
    #else
        #define WOLFSSH_API
        #define WOLFSSH_LOCAL
    #endif /* HAVE_VISIBILITY */
#else /* BUILDING_WOLFSSH */
    #if defined(_MSC_VER) || defined(__CYGWIN__)
        #ifdef WOLFSSH_DLL
            #define WOLFSSH_API extern __declspec(dllimport)
        #else
            #define WOLFSSH_API
        #endif
        #define WOLFSSH_LOCAL
    #else
        #define WOLFSSH_API
        #define WOLFSSH_LOCAL
    #endif
#endif /* BUILDING_WOLFSSH */



#ifdef __cplusplus
}
#endif

