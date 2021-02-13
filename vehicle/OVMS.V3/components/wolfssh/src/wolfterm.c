/* wolfterm.c
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


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>

#ifdef USE_WINDOWS_API

#include <wolfssh/internal.h>
#include <wolfssh/log.h>

#ifdef NO_INLINE
    #include <wolfssh/misc.h>
#else
    #define WOLFSSH_MISC_INCLUDED
#endif


/* Linux console codes values reference
 * https://www.systutorials.com/docs/linux/man/4-console_codes/
 */


/* macros used to abstract some of the Linux commands and Windows API */
#define WS_WRITECONSOLE(h, b, sz, szOut, r) \
    WriteConsole((h), (b), (sz), (szOut), (r))

/* r, g, b mask of fore/back ground bits*/
#define WS_MASK_RBGFG (FOREGROUND_RED | FOREGROUND_GREEN | FOREGROUND_BLUE)
#define WS_MASK_RBGBG (BACKGROUND_RED | BACKGROUND_GREEN | BACKGROUND_BLUE)
#define WS_MASK_FG (WS_MASK_RBGFG | FOREGROUND_INTENSITY)
#define WS_MASK_BG (WS_MASK_RBGBG | BACKGROUND_INTENSITY)

#define ESC 0x1B
#define CSI 0x9B
#define OFST 1

#ifndef WOLFSSH_MAX_CONSOLE_ARGS
/* NPAR(16) */
#define WOLFSSH_MAX_CONSOLE_ARGS 16
#endif

enum WS_ESC_STATE {
    WC_ESC_NONE = 0,
    WS_ESC_START,
    WS_ESC_CSI,
    WS_ESC_OSC,
};

/* adds x and y to current cursor location
 * if stomp is 1 the function overrides the current cursor location with new location */
static void wolfSSH_CursorMove(WOLFSSH_HANDLE h, word32 x, word32 y, byte stomp)
{
    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
    COORD cor = { x, y };

    GetConsoleScreenBufferInfo(h, &info);
    if (stomp == 0) {
        cor.X += info.dwCursorPosition.X;
        cor.Y += info.dwCursorPosition.Y;
    }

    /* sanity checks on coordinates */
    if (cor.X > info.dwMaximumWindowSize.X) {
        cor.X = info.dwMaximumWindowSize.X;
    }
    if (cor.X < 0) {
        cor.X = 0;
    }

    if (cor.Y > info.dwMaximumWindowSize.Y) {
        cor.Y = info.dwMaximumWindowSize.Y;
    }
    if (cor.Y < 0) {
        cor.Y = 0;
    }

    SetConsoleCursorPosition(h, cor);
}


/* clear screen (put ' ' chars) from (x1,y1) to (x2,y2) */
static void wolfSSH_ClearScreen(WOLFSSH_HANDLE handle, word32 x1, word32 y1, word32 x2, word32 y2)
{
        CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
        COORD start;
        DWORD w;
        word32 fill = 0; /* number of cells to write to */
        word32 maxX = 0;

        GetConsoleScreenBufferInfo(handle, &info);
        maxX = info.dwMaximumWindowSize.X;
        start.X = x1;
        start.Y = y1;

        /* get number of cells  */
        if (y1 == y2) { /* on same line so is x2 - x1 */
            fill = x2 - x1;
        }
        else { /* | y1 - y2 | * maxX - x1 + x2 */
            fill = y1 - y2;
            if (fill < 0)
                fill += fill * 2;
            fill = fill * maxX - x1 + x2;
        }

        FillConsoleOutputCharacterA(handle, ' ', fill, start, &w);
}


static void doDisplayAttributes(WOLFSSH* ssh, WOLFSSH_HANDLE handle, word32* args, word32 numArgs)
{
    word32 i = 0;
    word32 atr = 0;
    CONSOLE_SCREEN_BUFFER_INFO info;

    GetConsoleScreenBufferInfo(handle, &info);
    atr = info.wAttributes;

    /* interpret empty parameter as zero */
    if (numArgs == 0) {
        args[0] = 0;
        numArgs = 1;
    }

    for (i = 0; i < numArgs; i++) {
        switch (args[i]) {
            case 0: /* reset all to defaults */
                SetConsoleTextAttribute(handle, ssh->defaultAttr);
                break;

            case 1: /* set bold */
                SetConsoleTextAttribute(handle, (atr | FOREGROUND_INTENSITY));
                break;

            case 2: /* set half-bright */
                break;

            case 4: /* set undersocre */
                SetConsoleTextAttribute(handle, atr | BACKGROUND_INTENSITY);
                break;

            case 10: /* reset mapping and display control flag */
                break;

            case 24: /* turn off underline */
                SetConsoleTextAttribute(handle, atr & ~BACKGROUND_INTENSITY);
                break;

            case 21:
            case 22: /* normal intensity */
                atr &= ~BACKGROUND_INTENSITY;
                atr &= ~FOREGROUND_INTENSITY;
                SetConsoleTextAttribute(handle, atr);
                break;

            case 25: /* blink off */
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_INTENSITY));
                break;

            case 30: /* set black foreground */
                SetConsoleTextAttribute(handle, (atr & ~(WS_MASK_RBGBG)));
                break;

            case 31: /* red foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, atr | FOREGROUND_RED);
                break;

            case 32: /* green foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, atr | FOREGROUND_GREEN);
                break;

            case 33: /* brown foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, atr | FOREGROUND_GREEN |
                        FOREGROUND_RED);
                break;

            case 34: /* blue foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, atr | FOREGROUND_BLUE);
                break;

            case 35: /* magenta foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, (atr | FOREGROUND_RED |
                            FOREGROUND_BLUE));
                break;

            case 36: /* cyan foreground */
                atr &= ~WS_MASK_RBGFG;
                SetConsoleTextAttribute(handle, atr | FOREGROUND_BLUE |
                        FOREGROUND_GREEN);
                break;

            case 37: /* white foreground */
                SetConsoleTextAttribute(handle, (atr | WS_MASK_RBGFG));
                break;

            case 40: /* black background */
                SetConsoleTextAttribute(handle, atr &= ~WS_MASK_RBGBG);
                break;

            case 41: /* red background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_RED));
                break;

            case 42: /* green background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_GREEN));
                break;

            case 43: /* brown background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_RED |
                            BACKGROUND_GREEN));
                break;

            case 44: /* blue background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_BLUE));
                break;

            case 45: /* magenta background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_RED |
                            BACKGROUND_BLUE));
                break;

            case 46: /* cyan background */
                atr &= ~WS_MASK_RBGBG;
                SetConsoleTextAttribute(handle, (atr | BACKGROUND_BLUE |
                            BACKGROUND_GREEN));
                break;

            case 47: /* white background */
                SetConsoleTextAttribute(handle, (atr | WS_MASK_RBGBG));
                break;

            case 49: /* default background */
                atr &= ~WS_MASK_BG;
                atr |= (ssh->defaultAttr & WS_MASK_BG);
                SetConsoleTextAttribute(handle, atr);
                break;

            default:
               WLOG(WS_LOG_DEBUG, "Unknown display attribute %d", args[i]);
        }
    }
}


/* returns 1 if is a command and 0 if not */
static int isCommand(byte c)
{
    switch (c) {
        case '@':
        case 'A':
        case 'B':
        case 'C':
        case 'D':
        case 'E':
        case 'F':
        case 'G':
        case 'H':
        case 'J':
        case 'K':
        case 'L':
        case 'M':
        case 'P':
        case 'X':
        case 'a':
        case 'c':
        case 'd':
        case 'e':
        case 'f':
        case 'g':
        case 'h':
        case 'l':
        case 'm':
        case 'n':
        case 'q':
        case 'r':
        case 's':
        case 'u':
        case '`':
            return 1;

        default:
            return 0;
    }
}


/* returns the number of args found */
static int getArgs(byte* buf, word32 bufSz, word32* idx, word32* out)
{
    word32 i = 0, numArgs = 0;
    word32 maxIdx = WOLFSSH_MAX_CONSOLE_ARGS * 4;
    byte tmpBuf[WOLFSSH_MAX_CONSOLE_ARGS * 4];
    word32 tmpBufIdx = 0;

    /* parse out arguments */
    while (i < WOLFSSH_MAX_CONSOLE_ARGS && *idx + i < bufSz &&
        !isCommand(buf[*idx + i])) {

        if (buf[*idx + i] == ';') {
            tmpBuf[tmpBufIdx] = '\0';
            out[numArgs] = atoi(tmpBuf);
            numArgs++;
            tmpBufIdx = 0;
        }
        else {
            tmpBuf[tmpBufIdx++] = buf[*idx + i];
        }
        i++;
    }

    if (i > 0 && tmpBufIdx > 0) {
        tmpBuf[tmpBufIdx] = '\0';
        out[numArgs] = atoi(tmpBuf);
        numArgs++;
    }

    *idx += i;
    return numArgs;
}


/* return WS_SUCCESS on success */
static int wolfSSH_DoOSC(WOLFSSH* ssh, WOLFSSH_HANDLE handle, byte* buf,
        word32 bufSz, word32* idx)
{
    if (buf[*idx] == 'P') { /* color pallet */
        WLOG(WS_LOG_DEBUG, "Color pallet not yet supported");
        return WS_SUCCESS;
    }

    if (buf[*idx] == 'R') { /* reset color pallet */
        WLOG(WS_LOG_DEBUG, "Reset color pallet not yet supported");
        return WS_SUCCESS;
    }

    /* xterm commands */
    switch (buf[*idx]) {
        case 0x30: /* '0' set icon name and window title to string */
        {
            char* tmpBuf;
            int sz;
            word32 i;

            *idx += 1;
            if (buf[*idx] == ';') {
                *idx += 1;
            }
            else {
                break;
            }

            /* BEL (0x07) ends string */
            i = *idx;
            while (i < bufSz && buf[i] != 0x07) i++;
            if (buf[i] != 0x07) {
                break; /*bell not found, possible want read? */
            }

            /* sanity check on underflow */
            if (*idx > i) {
                return WS_FATAL_ERROR;
            }

            /* make sure is null terminated */
            sz = i - *idx;
            tmpBuf = (char*)WMALLOC(sz + 1, ssh->ctx->heap, DYNTYPE_BUFFER);
            if (tmpBuf == NULL) {
                return WS_MEMORY_E;
            }
            WMEMCPY(tmpBuf, buf + *idx, sz);
            tmpBuf[sz] = '\0';
            *idx += sz + 1;
            WLOG(WS_LOG_INFO, "terminal name = %s", tmpBuf);
            WFREE(tmpBuf, ssh->ctx->heap, DYNTYPE_BUFFER);
        }
    }
    return WS_SUCCESS;
}


/* CSI sequence actions
 * return WS_WANT_READ if more input is needed
 * return WS_SUCCESS on success */
static int wolfSSH_DoControlSeq(WOLFSSH* ssh, WOLFSSH_HANDLE handle, byte* buf, word32 bufSz, word32* idx)
{
    word32 i = *idx;
    byte c;
    word32 numArgs = 0;
    word32 args[WOLFSSH_MAX_CONSOLE_ARGS] = { 0 };

    if (ssh->escState == WS_ESC_CSI || ssh->escBufSz > 0) {
        /* check for left overs */
        if (ssh->escBufSz > 0) {
            word32 tmpSz = min(bufSz - *idx,
                    WOLFSSH_MAX_CONSOLE_ARGS - (word32)ssh->escBufSz);
            WMEMCPY(ssh->escBuf + ssh->escBufSz, buf + *idx, tmpSz);
            i = 0;
            numArgs = getArgs(ssh->escBuf, ssh->escBufSz + tmpSz, &i, args);
            c = ssh->escBuf[i++];
            if (!isCommand(c)) {
                /* expecting a command when in CSI state */
                if (ssh->escBufSz + tmpSz >= WOLFSSH_MAX_CONSOLE_ARGS) {
                    WLOG(WS_LOG_ERROR,
                            "Bad state when trying to return to CSI command");
                    return WS_FATAL_ERROR;
                }
                return WS_WANT_READ;
            }
            if (i >= ssh->escBufSz) {
                i = *idx + (i - ssh->escBufSz);
            }
            else {
                i = *idx;
            }
        }
        else {
            numArgs = getArgs(buf, bufSz, &i, args);
            c = buf[i]; i++;
        }
    }
    else {
        ssh->escState = WS_ESC_CSI;
        numArgs = getArgs(buf, bufSz, &i, args);

        if (i >= bufSz) {
            /* save left overs for next call */
            WMEMCPY(ssh->escBuf, buf + *idx, bufSz - *idx);
            ssh->escBufSz = bufSz - *idx;
            return WS_WANT_READ;
        }
        c = buf[i]; i++;
    }

    switch (c) {
        case 'H': /* move curser to indicated row and column  -1 to account
                   * for 1,1 on linux vs 0,0 on windows */
            wolfSSH_CursorMove(handle, args[1] - OFST, args[0] - OFST, 1);
            break;

        case 'C': /* move curser right */
            wolfSSH_CursorMove(handle, args[0], 0, 0);
            break;

        case 'J': /* erase display */
            if (numArgs == 0) {
                WLOG(WS_LOG_DEBUG, "CSI 'J' expects an argument");
                args[0] = 2; /* default to erase screen */
            }

            switch (args[0]) {
                case 1: /* erase from start to cursor */
                {
                    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                    GetConsoleScreenBufferInfo(handle, &info);
                    wolfSSH_ClearScreen(handle, 0, info.dwCursorPosition.Y,
                            info.dwCursorPosition.X, info.dwCursorPosition.Y);
                }
                    break;

                case 2: /* erase whole display */
                {
                    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                    GetConsoleScreenBufferInfo(handle, &info);
                    wolfSSH_ClearScreen(handle, 0, 0, info.dwSize.X, info.dwSize.Y);
                }
                    break;

                case 3: /* erase whole display + */
                {
                    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                    GetConsoleScreenBufferInfo(handle, &info);
                    wolfSSH_ClearScreen(handle, 0, 0, info.dwMaximumWindowSize.X,
                            info.dwMaximumWindowSize.Y);
                    wolfSSH_CursorMove(handle, 0, 0, 1);
                }
                    break;

                default:
                    WLOG(WS_LOG_DEBUG, "Unexpected erase value %d", args[0]);
            }

        case 'K':
            if (numArgs == 0) { /* erase start of cursor to end of line */
                {
                    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                    GetConsoleScreenBufferInfo(handle, &info);
                    wolfSSH_ClearScreen(handle, info.dwCursorPosition.X,
                            info.dwCursorPosition.Y, info.dwMaximumWindowSize.X,
                            info.dwCursorPosition.Y);
                }
                break;
            }
            else {
                switch (args[0]) {
                    case 1: /* erase start of line to cursor */
                    {
                        CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                        GetConsoleScreenBufferInfo(handle, &info);
                        wolfSSH_ClearScreen(handle, 0, info.dwCursorPosition.Y,
                              info.dwCursorPosition.X, info.dwCursorPosition.Y);
                    }
                    break;

                    case 2: /* erase whole line */
                    {
                        CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                        GetConsoleScreenBufferInfo(handle, &info);
                        wolfSSH_ClearScreen(handle, 0, info.dwCursorPosition.Y,
                           info.dwMaximumWindowSize.X, info.dwCursorPosition.Y);
                    }
                    break;
                }
            }
            break;

        case 'm':
            /* set display attributes */
            doDisplayAttributes(ssh, handle, args, numArgs);
            break;

        case 'l': /* reset mode */
        case 'h': /* set mode */
            /* will come back in with a '?' */
            break;

        case 'r': /* @TODO DECSTBM */
            WLOG(WS_LOG_DEBUG, "DECSTBM not yet supported");
            if (numArgs == 0) {
                /* reset scroll window */
                break;
            }
            if (numArgs == 2) {
                /* set top / bottom for scroll window */
                break;
            }

            /* unknown number of args found */
            break;

        case '?':
            numArgs = getArgs(buf, bufSz, &i, args);

            switch (args[0]) {
                case 1: /* cursor keys send ESC O */
                case 25: /* Make curseor visible */
                    break;
                default:
                    break;
            }
            i += 1; /* for 'h' or 'l' */
            break;

        default:
            WLOG(WS_LOG_DEBUG, "Unknown control sequence char:%c", c);
            i = *idx;
            return WS_FATAL_ERROR;
    }
    *idx = i;

    return WS_SUCCESS;
}


/* Used to translate Linux console commands into something that a Windows
 * terminal can understand. Can make changes to the attributes of handle and
 * prints directly to handle.
 *
 * handle A handle to set changes i.e. color, cursor location, clear and print
 *        to.
 * buf    Unaltered buffer that could have Linux console commands.
 * bufSz  Size of "buf" passed in.
 *
 * returns WS_SUCCESS on success
 */
int wolfSSH_ConvertConsole(WOLFSSH* ssh, WOLFSSH_HANDLE handle, byte* buf,
        word32 bufSz)
{
    word32 i = 0, z = 0;
    DWORD  wrt = 0;
    int ret;

    if (ssh == NULL || buf == NULL) {
        return WS_BAD_ARGUMENT;
    }

    if (bufSz == 0) { /* nothing to do */
        return WS_SUCCESS;
    }

    if (ssh->defaultAttrSet == 0) { /* store default attributes */
        CONSOLE_SCREEN_BUFFER_INFO info;
        GetConsoleScreenBufferInfo(handle, &info);
        ssh->defaultAttr = info.wAttributes;
        ssh->defaultAttrSet = 1;
    }

    /* check if already in a state */
    switch (ssh->escState) {
        case WS_ESC_CSI:
            ret = wolfSSH_DoControlSeq(ssh, handle, buf, bufSz, &i);
            if (ret == WS_WANT_READ) {
                return ret;
            }
            ssh->escBufSz = 0;
            break;

        case WS_ESC_OSC:
            ret = wolfSSH_DoOSC(ssh, handle, buf, bufSz, &i);
            if (ret == WS_WANT_READ) {
                return ret;
            }
            ssh->escBufSz = 0;
            break;

        default:
            break;
    }

    while (i < bufSz) {
        /* is escape char? */
        if (buf[i] == ESC || ssh->escState == WS_ESC_START) {
            byte c;

            if (ssh->escState == WS_ESC_START) {
                if (ssh->escBufSz > 0) {
                    c = ssh->escBuf[0];
                    ssh->escBufSz -= 1;
                    if (ssh->escBufSz > 0) {
                        WMEMMOVE(ssh->escBuf, ssh->escBuf + 1, ssh->escBufSz);
                        z = 0; /* read from start of esc buffer */
                    }
                }
                else {
                    c = buf[i];
                    z = i + 1;
                }
            }
            else {
                ssh->escState = WS_ESC_START;
                if (i + 1 >= bufSz) {
                    /* not enough data to complete operation */
                    if (bufSz - i > WOLFSSL_MAX_ESCBUF) {
                        WLOG(WS_LOG_ERROR,
                            "Terminal found more left over data then expected");
                        return WS_FATAL_ERROR;
                    }
                    else {
                        ssh->escBufSz = bufSz - (i + 1);
                        WMEMCPY(ssh->escBuf, buf + (i + 1), ssh->escBufSz);
                    }

                    return WS_WANT_READ;
                }
                c = buf[i + 1];
                z = i + 2;
            }

            /* get type of sequence */
            switch (c) {
                case 'c': /* reset */
                {
                    CONSOLE_SCREEN_BUFFER_INFO info = { 0 };
                    GetConsoleScreenBufferInfo(handle, &info);
                    wolfSSH_ClearScreen(handle, 0, 0, info.dwMaximumWindowSize.X,
                            info.dwMaximumWindowSize.Y);
                    wolfSSH_CursorMove(handle, 0, 0, 1);
                }
                    break;

                case 'D':
                    /* linefeed */
                    if (WS_WRITECONSOLE(handle, "\n", sizeof("\n"), &wrt, NULL)
                        == 0) {
                        WLOG(WS_LOG_DEBUG, "Error writing newline to handle");
                        return WS_FATAL_ERROR;
                    }
                    break;

                case 'E': /* newline */
                    if (WS_WRITECONSOLE(handle, "\n", sizeof("\n"), &wrt, NULL)
                            == 0) {
                        WLOG(WS_LOG_DEBUG, "Error writing newline to handle");
                        return WS_FATAL_ERROR;
                    }
                    break;

                case '[': /* control sequence */
                    ret = wolfSSH_DoControlSeq(ssh, handle, buf, bufSz, &z);
                    if (ret == WS_WANT_READ)
                        return ret;
                    /* if do control seq failed then ignore the sequence */
                    break;

                case ']': /* OSC command */
                    ret = wolfSSH_DoOSC(ssh, handle, buf, bufSz, &z);
                    if (ret == WS_WANT_READ)
                        return ret;
                    break;

                default:
                    WLOG(WS_LOG_DEBUG,
                            "unknown special console code char:%c hex:%02X",
                             c, c);
            }
            i = z; /* update index */

            ssh->escState = WC_ESC_NONE;
            ssh->escBufSz = 0;
        }
        else if (buf[i] == CSI) {
            z = i + 1;
            ret = wolfSSH_DoControlSeq(ssh, handle, buf, bufSz, &z);
            if (ret == WS_WANT_READ)
                return ret;
            i = z;
        }
        else {
            byte out[2] = { buf[i], 0 };
            WS_WRITECONSOLE(handle, out, 1, &wrt, NULL);
            i++;
        }
    }
    return WS_SUCCESS;
}
#endif /* USE_WINDOWS_API */

