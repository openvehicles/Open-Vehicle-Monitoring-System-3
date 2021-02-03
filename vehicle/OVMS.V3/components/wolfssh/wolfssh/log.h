/* log.h
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
 * The log module contains the interface to the logging function. When
 * debugging is enabled and turned on, the logger will output to STDOUT.
 * A custom logging callback may be installed.
 */


#pragma once

#include <wolfssh/settings.h>

#ifdef __cplusplus
extern "C" {
#endif


#ifdef NO_TIMESTAMP
    /* The NO_TIMESTAMP tag is deprecated. Convert to new name. */
    #define WOLFSSH_NO_TIMESTAMP
#endif


enum wolfSSH_LogLevel {
    WS_LOG_AGENT = 8,
    WS_LOG_SCP   = 7,
    WS_LOG_SFTP  = 6,
    WS_LOG_USER  = 5,
    WS_LOG_ERROR = 4,
    WS_LOG_WARN  = 3,
    WS_LOG_INFO  = 2,
    WS_LOG_DEBUG = 1,
    WS_LOG_DEFAULT = WS_LOG_DEBUG
};


typedef void (*wolfSSH_LoggingCb)(enum wolfSSH_LogLevel,
                                  const char *const logMsg);
WOLFSSH_API void wolfSSH_SetLoggingCb(wolfSSH_LoggingCb logF);
WOLFSSH_API int wolfSSH_LogEnabled(void);


#ifdef __GNUC__
    #define FMTCHECK __attribute__((format(printf,2,3)))
#else
    #define FMTCHECK
#endif /* __GNUC__ */


WOLFSSH_API void wolfSSH_Log(enum wolfSSH_LogLevel,
                             const char *const, ...) FMTCHECK;

#define WLOG(...) do { \
                      if (wolfSSH_LogEnabled()) \
                          wolfSSH_Log(__VA_ARGS__); \
                  } while (0)


#ifdef __cplusplus
}
#endif

