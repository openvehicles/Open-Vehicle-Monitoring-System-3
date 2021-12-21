/* log.c
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


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>
#include <wolfssh/log.h>
#include <wolfssh/error.h>

#include <stdlib.h>
#include <stdarg.h>
#ifndef FREESCALE_MQX
    #include <stdio.h>
    #ifndef WOLFSSH_NO_TIMESTAMP
        #include <time.h>
    #endif
#endif


#ifndef WOLFSSH_DEFAULT_LOG_WIDTH
    #define WOLFSSH_DEFAULT_LOG_WIDTH 120
#endif


#ifndef WOLFSSL_NO_DEFAULT_LOGGING_CB
    static void DefaultLoggingCb(enum wolfSSH_LogLevel, const char *const);
    static wolfSSH_LoggingCb logFunction = DefaultLoggingCb;
#else /* WOLFSSH_NO_DEFAULT_LOGGING_CB */
    static wolfSSH_LoggingCb logFunction = NULL;
#endif /* WOLFSSH_NO_DEFAULT_LOGGING_CB */


static enum wolfSSH_LogLevel logLevel = WS_LOG_DEFAULT;
#ifdef DEBUG_WOLFSSH
    static int logEnable = 0;
#endif


/* turn debugging on if supported */
void wolfSSH_Debugging_ON(void)
{
#ifdef DEBUG_WOLFSSH
    logEnable = 1;
#endif
}


/* turn debugging off */
void wolfSSH_Debugging_OFF(void)
{
#ifdef DEBUG_WOLFSSH
    logEnable = 0;
#endif
}


/* set logging callback function */
void wolfSSH_SetLoggingCb(wolfSSH_LoggingCb logF)
{
    if (logF)
        logFunction = logF;
}


int wolfSSH_LogEnabled(void)
{
#ifdef DEBUG_WOLFSSH
    return logEnable;
#else
    return 0;
#endif
}


#ifndef WOLFSSH_NO_DEFAULT_LOGGING_CB
/* log level string */
static const char* GetLogStr(enum wolfSSH_LogLevel level)
{
    switch (level) {
        case WS_LOG_INFO:
            return "INFO";

        case WS_LOG_WARN:
            return "WARNING";

        case WS_LOG_ERROR:
            return "ERROR";

        case WS_LOG_DEBUG:
            return "DEBUG";

        case WS_LOG_USER:
            return "USER";

        case WS_LOG_SFTP:
            return "SFTP";

        case WS_LOG_SCP:
            return "SCP";

        case WS_LOG_AGENT:
            return "AGENT";

        default:
            return "UNKNOWN";
    }
}


void DefaultLoggingCb(enum wolfSSH_LogLevel level, const char *const msgStr)
{
    char timeStr[24];
    timeStr[0] = '\0';
#ifndef WOLFSSH_NO_TIMESTAMP
    {
        time_t  current;
        struct  tm local;

        current = WTIME(NULL);
        if (WLOCALTIME(&current, &local)) {
            /* make pretty */
            strftime(timeStr, sizeof(timeStr), "%F %T ", &local);
        }
    }
#endif /* WOLFSSH_NO_TIMESTAMP */
    #ifndef WOLFSSH_LOG_PRINTF
    fprintf(stdout, "%s[%s] %s\r\n", timeStr, GetLogStr(level), msgStr);
    #else
    printf("%s[%s] %s\r\n", timeStr, GetLogStr(level), msgStr);
    #endif
}
#endif /* WOLFSSH_NO_DEFAULT_LOGGING_CB */


/* our default logger */
void wolfSSH_Log(enum wolfSSH_LogLevel level, const char *const fmt, ...)
{
    va_list vlist;
    char    msgStr[WOLFSSH_DEFAULT_LOG_WIDTH];

    if (level < logLevel)
        return;   /* don't need to output */

    /* format msg */
    va_start(vlist, fmt);
    WVSNPRINTF(msgStr, sizeof(msgStr), fmt, vlist);
    va_end(vlist);

    if (logFunction)
        logFunction(level, msgStr);
}
