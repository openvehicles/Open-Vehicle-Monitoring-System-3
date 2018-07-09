/* log.c 
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


#ifdef DEBUG_WOLFSSH
    #include <stdlib.h>
    #include <stdio.h>
    #include <stdarg.h>
    #include <time.h>

    static wolfSSH_LoggingCb logFunction  = NULL;
    static int loggingEnabled             = 0;
    static enum wolfSSH_LogLevel logLevel = WS_LOG_DEFAULT;
#endif /* DEBUG_WOLFSSH */


/* turn debugging on if supported */
int wolfSSH_Debugging_ON(void)
{
#ifdef DEBUG_WOLFSSH
    loggingEnabled = 1;
    return WS_SUCCESS;
#else
    return WS_NOT_COMPILED;
#endif
}


/* turn debugging off */
void wolfSSH_Debugging_OFF(void)
{
#ifdef DEBUG_WOLFSSH
    loggingEnabled = 0;
#endif
}


/* set logging callback function */
int wolfSSH_SetLoggingCb(wolfSSH_LoggingCb logF)
{
#ifdef DEBUG_WOLFSSH
    int ret = 0;

    if (logF)
        logFunction = logF;
    else
        ret = WS_BAD_ARGUMENT;

    return ret;
#else
    (void)logF;
    return WS_NOT_COMPILED;
#endif /* DEUBG_WOLFSSH */
}



#ifdef DEBUG_WOLFSSH


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

        default:
            return "UNKNOWN";
    }
}


/* our default logger */
void WLOG(enum wolfSSH_LogLevel level, const char *const fmt, ...)
{
    va_list vlist;
    char    timeStr[80];
    char    msgStr[80*2];

    if (loggingEnabled == 0)
        return;  /* not on */

    if (level < logLevel)
        return;   /* don't need to output */

    /* prefix strings */
    msgStr[0]  = '\0';
    timeStr[0] = '\0';

#ifndef NO_TIMESTAMP
    {
        time_t  current;
        struct  tm local;

        current = time(NULL);
        if (localtime_r(&current, &local)) {
            /* make pretty */
            strftime(timeStr, sizeof(timeStr), "%b %d %T %Y", &local);
        }        
        timeStr[sizeof(timeStr)-1] = '\0';
    }
#endif /* NO_TIMESTAMP */

    /* format msg */
    va_start(vlist, fmt);
    vsnprintf(msgStr, sizeof(msgStr), fmt, vlist);
    va_end(vlist);
    msgStr[sizeof(msgStr)-1] = '\0';

    if (logFunction)
        logFunction(level, msgStr);
    else
        printf("%s: [%s] %s\n", timeStr, GetLogStr(level), msgStr);
}

#endif /* DEBUG_WOLFSSH */

