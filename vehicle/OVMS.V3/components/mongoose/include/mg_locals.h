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

#ifndef __mg_locals_h__
#define __mg_locals_h__

#include "sdkconfig.h"
#include "ovms_log.h"
#include "ovms_malloc.h"

#define ESP_PLATFORM 1
#define MG_ENABLE_HTTP 1
#define MG_ENABLE_THREADSAFE_CONN_MBUFS 1

// Note: broadcast support not working reliably yet, do not enable for production!
// #define MG_ENABLE_BROADCAST 1
// #define MG_CTL_MSG_MESSAGE_SIZE 64    // Note: default is 8K, we only need this to trigger MG_EV_POLL

// #define CONFIG_MG_ENABLE_DEBUG 1
#ifdef CONFIG_MG_ENABLE_DEBUG
#define MG_ENABLE_DEBUG 1
#define CS_LOG_ENABLE_TS_DIFF 1
#endif

#ifdef CONFIG_MG_ENABLE_FILESYSTEM
#define MG_ENABLE_FILESYSTEM 1
#endif

#ifdef CONFIG_MG_ENABLE_DIRECTORY_LISTING
#define MG_ENABLE_DIRECTORY_LISTING 1
#endif

#ifdef CONFIG_MG_ENABLE_SSL
#define MG_ENABLE_SSL 1
#define MG_SSL_IF MG_SSL_IF_MBEDTLS
#define MG_SSL_MBED_DUMMY_RANDOM 1
#define MG_SSL_IF_MBEDTLS_MAX_FRAG_LEN 2048
#define MG_SSL_IF_MBEDTLS_FREE_CERTS 1
//#define CS_ENABLE_DEBUG
#endif

// Override memory allocation macros in mongoose.c
#define CS_COMMON_MG_MEM_H_
#define MG_MALLOC ExternalRamMalloc
#define MG_CALLOC ExternalRamCalloc
#define MG_REALLOC ExternalRamRealloc
#define MG_FREE free

// Let mongoose use LWIP getaddrinfo():
#define MG_ENABLE_SYNC_RESOLVER 1
#define MG_ENABLE_GETADDRINFO 1
// Note: if we can't stick to this solution due to blocking issues,
//  we can alternatively assign the first DNS stored by netmanager
//  to mongoose by calling mg_set_nameserver().

#endif // __mg_locals_h__
