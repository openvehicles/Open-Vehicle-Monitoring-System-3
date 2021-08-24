/* client.c
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

#define WOLFSSH_TEST_CLIENT

#include <wolfssh/ssh.h>
#include <wolfssh/test.h>
#ifdef WOLFSSH_AGENT
    #include <wolfssh/agent.h>
#endif
#include <wolfssl/wolfcrypt/ecc.h>
#include "examples/client/client.h"
#if !defined(USE_WINDOWS_API) && !defined(MICROCHIP_PIC32)
    #include <termios.h>
#endif

#ifdef WOLFSSH_SHELL
    #ifdef HAVE_PTY_H
        #include <pty.h>
    #endif
    #ifdef HAVE_UTIL_H
        #include <util.h>
    #endif
    #ifdef HAVE_TERMIOS_H
        #include <termios.h>
    #endif
    #include <pwd.h>
#endif /* WOLFSSH_SHELL */

#ifdef WOLFSSH_AGENT
    #include <errno.h>
    #include <stddef.h>
    #include <sys/socket.h>
    #include <sys/un.h>
#endif /* WOLFSSH_AGENT */

#ifdef HAVE_SYS_SELECT_H
    #include <sys/select.h>
#endif


#ifndef NO_WOLFSSH_CLIENT

static const char testString[] = "Hello, wolfSSH!";


/* type = 2 : shell / execute command settings
 * type = 0 : password
 * type = 1 : restore default
 * return 0 on success */
static int SetEcho(int type)
{
#if !defined(USE_WINDOWS_API) && !defined(MICROCHIP_PIC32)
    static int echoInit = 0;
    static struct termios originalTerm;

    if (!echoInit) {
        if (tcgetattr(STDIN_FILENO, &originalTerm) != 0) {
            printf("Couldn't get the original terminal settings.\n");
            return -1;
        }
        echoInit = 1;
    }
    if (type == 1) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &originalTerm) != 0) {
            printf("Couldn't restore the terminal settings.\n");
            return -1;
        }
    }
    else {
        struct termios newTerm;
        memcpy(&newTerm, &originalTerm, sizeof(struct termios));

        newTerm.c_lflag &= ~ECHO;
        if (type == 2) {
            newTerm.c_lflag &= ~(ICANON | ECHOE | ECHOK | ECHONL | ISIG);
        }
        else {
            newTerm.c_lflag |= (ICANON | ECHONL);
        }

        if (tcsetattr(STDIN_FILENO, TCSANOW, &newTerm) != 0) {
            printf("Couldn't turn off echo.\n");
            return -1;
        }
    }
#else
    static int echoInit = 0;
    static DWORD originalTerm;
    static CONSOLE_SCREEN_BUFFER_INFO screenOrig;
    HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (!echoInit) {
        if (GetConsoleMode(stdinHandle, &originalTerm) == 0) {
            printf("Couldn't get the original terminal settings.\n");
            return -1;
        }
        echoInit = 1;
    }
    if (type == 1) {
        if (SetConsoleMode(stdinHandle, originalTerm) == 0) {
            printf("Couldn't restore the terminal settings.\n");
            return -1;
        }
    }
    else if (type == 2) {
        DWORD newTerm = originalTerm;

        newTerm &= ~ENABLE_PROCESSED_INPUT;
        newTerm &= ~ENABLE_PROCESSED_OUTPUT;
        newTerm &= ~ENABLE_LINE_INPUT;
        newTerm &= ~ENABLE_ECHO_INPUT;
        newTerm &= ~(ENABLE_EXTENDED_FLAGS | ENABLE_INSERT_MODE);

        if (SetConsoleMode(stdinHandle, newTerm) == 0) {
            printf("Couldn't turn off echo.\n");
            return -1;
        }
    }
    else {
        DWORD newTerm = originalTerm;

        newTerm &= ~ENABLE_ECHO_INPUT;

        if (SetConsoleMode(stdinHandle, newTerm) == 0) {
            printf("Couldn't turn off echo.\n");
            return -1;
        }
    }
#endif

    return 0;
}


static void ShowUsage(void)
{
    printf("client %s\n", LIBWOLFSSH_VERSION_STRING);
    printf(" -?            display this help and exit\n");
    printf(" -h <host>     host to connect to, default %s\n", wolfSshIp);
    printf(" -p <num>      port to connect on, default %d\n", wolfSshPort);
    printf(" -u <username> username to authenticate as (REQUIRED)\n");
    printf(" -P <password> password for username, prompted if omitted\n");
    printf(" -e            use sample ecc key for user\n");
    printf(" -i <filename> filename for the user's private key\n");
    printf(" -j <filename> filename for the user's public key\n");
    printf(" -x            exit after successful connection without doing\n"
           "               read/write\n");
    printf(" -N            use non-blocking sockets\n");
#ifdef WOLFSSH_TERM
    printf(" -t            use psuedo terminal\n");
#endif
#if !defined(SINGLE_THREADED) && !defined(WOLFSSL_NUCLEUS)
    printf(" -c <command>  executes remote command and pipe stdin/stdout\n");
#ifdef USE_WINDOWS_API
    printf(" -R            raw untranslated output\n");
#endif
#endif
#ifdef WOLFSSH_AGENT
    printf(" -a            Attempt to use SSH-AGENT\n");
#endif
}


static byte userPassword[256];
static byte userPublicKey[512];
static const byte* userPublicKeyType = NULL;
static byte userPrivateKeyBuf[1191]; /* Size equal to hanselPrivateRsaSz. */
static byte* userPrivateKey = userPrivateKeyBuf;
static const byte* userPrivateKeyType = NULL;
static word32 userPublicKeySz = 0;
static word32 userPublicKeyTypeSz = 0;
static word32 userPrivateKeySz = sizeof(userPrivateKeyBuf);
static word32 userPrivateKeyTypeSz = 0;
static byte isPrivate = 0;


#ifndef NO_RSA
static const char* hanselPublicRsa =
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x hansel";
static const byte hanselPrivateRsa[] = {
  0x30, 0x82, 0x04, 0xa3, 0x02, 0x01, 0x00, 0x02, 0x82, 0x01, 0x01, 0x00,
  0xbd, 0x3f, 0x76, 0x45, 0xa3, 0x03, 0xac, 0x38, 0xd5, 0xc7, 0x0f, 0x93,
  0x30, 0x5a, 0x20, 0x9c, 0x89, 0x7c, 0xad, 0x05, 0x16, 0x46, 0x86, 0x83,
  0x0d, 0x8a, 0x2b, 0x16, 0x4a, 0x05, 0x2c, 0xe4, 0x77, 0x47, 0x70, 0x00,
  0xae, 0x1d, 0x83, 0xe2, 0xd9, 0x6e, 0x99, 0xd4, 0xf0, 0x45, 0x98, 0x15,
  0x93, 0xf6, 0x87, 0x4e, 0xac, 0x64, 0x63, 0xa1, 0x95, 0xc9, 0x7c, 0x30,
  0xe8, 0x3e, 0x2f, 0xa3, 0xf1, 0x24, 0x9f, 0x0c, 0x6b, 0x1c, 0xfe, 0x1b,
  0x02, 0x99, 0xcd, 0xc6, 0xa7, 0x6c, 0x84, 0x85, 0x46, 0x54, 0x12, 0x40,
  0xe1, 0xb4, 0xe5, 0xf2, 0xaa, 0x39, 0xec, 0xd6, 0x27, 0x24, 0x0b, 0xd1,
  0xa1, 0xe2, 0xef, 0x34, 0x69, 0x25, 0x6d, 0xc0, 0x74, 0x67, 0x25, 0x98,
  0x7d, 0xc4, 0xf8, 0x52, 0xab, 0x9b, 0x4b, 0x3a, 0x12, 0x1d, 0xe1, 0xe3,
  0xfa, 0xd6, 0xcf, 0x9a, 0xe6, 0x9c, 0x23, 0x4e, 0x39, 0xc4, 0x84, 0x16,
  0x88, 0x3d, 0x42, 0x4e, 0xd8, 0x2f, 0xcc, 0xd2, 0x91, 0x67, 0x9d, 0xb6,
  0x71, 0x2a, 0x02, 0x65, 0x5f, 0xbb, 0x75, 0x0e, 0x8c, 0xbb, 0x87, 0x97,
  0x97, 0xc6, 0xf8, 0xb2, 0x98, 0xe2, 0x2f, 0x68, 0x26, 0x4a, 0x53, 0xec,
  0x79, 0x3a, 0x8a, 0x5f, 0xcc, 0xcf, 0xf0, 0x16, 0x47, 0xb2, 0xd0, 0x43,
  0xd6, 0x36, 0x6c, 0xc8, 0xe7, 0x2f, 0xfe, 0xa7, 0x35, 0x39, 0x69, 0xfb,
  0x1d, 0x78, 0x45, 0x9d, 0x89, 0x00, 0xc8, 0x41, 0xcf, 0x34, 0x1f, 0xa3,
  0xf3, 0xf1, 0xfb, 0x28, 0x14, 0xfb, 0xd8, 0x48, 0x6f, 0xac, 0xe3, 0xfc,
  0x33, 0xd1, 0xdb, 0xae, 0xef, 0x27, 0x9e, 0x57, 0x56, 0x29, 0xa2, 0x1a,
  0x3a, 0xe5, 0x9a, 0xfe, 0xa4, 0x49, 0xc8, 0x7f, 0xb7, 0x4e, 0xd0, 0x1f,
  0x04, 0x6e, 0x58, 0x16, 0xb7, 0xeb, 0x9d, 0xf8, 0x92, 0x3c, 0xc2, 0xb0,
  0x21, 0x7c, 0x4e, 0x31, 0x02, 0x03, 0x01, 0x00, 0x01, 0x02, 0x82, 0x01,
  0x01, 0x00, 0x8d, 0xa4, 0x61, 0x06, 0x2f, 0xc3, 0x40, 0xf4, 0x6c, 0xf4,
  0x87, 0x30, 0xb8, 0x00, 0xcc, 0xe5, 0xbc, 0x75, 0x87, 0x1e, 0x06, 0x95,
  0x14, 0x7a, 0x23, 0xf9, 0x24, 0xd4, 0x92, 0xe4, 0x1a, 0xbc, 0x88, 0x95,
  0xfc, 0x3b, 0x56, 0x16, 0x1b, 0x2e, 0xff, 0x64, 0x2b, 0x58, 0xd7, 0xd8,
  0x8e, 0xc2, 0x9f, 0xb2, 0xe5, 0x84, 0xb9, 0xbc, 0x8d, 0x61, 0x54, 0x35,
  0xb0, 0x70, 0xfe, 0x72, 0x04, 0xc0, 0x24, 0x6d, 0x2f, 0x69, 0x61, 0x06,
  0x1b, 0x1d, 0xe6, 0x2d, 0x6d, 0x79, 0x60, 0xb7, 0xf4, 0xdb, 0xb7, 0x4e,
  0x97, 0x36, 0xde, 0x77, 0xc1, 0x9f, 0x85, 0x4e, 0xc3, 0x77, 0x69, 0x66,
  0x2e, 0x3e, 0x61, 0x76, 0xf3, 0x67, 0xfb, 0xc6, 0x9a, 0xc5, 0x6f, 0x99,
  0xff, 0xe6, 0x89, 0x43, 0x92, 0x44, 0x75, 0xd2, 0x4e, 0x54, 0x91, 0x58,
  0xb2, 0x48, 0x2a, 0xe6, 0xfa, 0x0d, 0x4a, 0xca, 0xd4, 0x14, 0x9e, 0xf6,
  0x27, 0x67, 0xb7, 0x25, 0x7a, 0x43, 0xbb, 0x2b, 0x67, 0xd1, 0xfe, 0xd1,
  0x68, 0x23, 0x06, 0x30, 0x7c, 0xbf, 0x60, 0x49, 0xde, 0xcc, 0x7e, 0x26,
  0x5a, 0x3b, 0xfe, 0xa6, 0xa6, 0xe7, 0xa8, 0xdd, 0xac, 0xb9, 0xaf, 0x82,
  0x9a, 0x3a, 0x41, 0x7e, 0x61, 0x21, 0x37, 0xa3, 0x08, 0xe4, 0xc4, 0xbc,
  0x11, 0xf5, 0x3b, 0x8e, 0x4d, 0x51, 0xf3, 0xbd, 0xda, 0xba, 0xb2, 0xc5,
  0xee, 0xfb, 0xcf, 0xdf, 0x83, 0xa1, 0x82, 0x01, 0xe1, 0x51, 0x9d, 0x07,
  0x5a, 0x5d, 0xd8, 0xc7, 0x5b, 0x3f, 0x97, 0x13, 0x6a, 0x4d, 0x1e, 0x8d,
  0x39, 0xac, 0x40, 0x95, 0x82, 0x6c, 0xa2, 0xa1, 0xcc, 0x8a, 0x9b, 0x21,
  0x32, 0x3a, 0x58, 0xcc, 0xe7, 0x2d, 0x1a, 0x79, 0xa4, 0x31, 0x50, 0xb1,
  0x4b, 0x76, 0x23, 0x1b, 0xb3, 0x40, 0x3d, 0x3d, 0x72, 0x72, 0x32, 0xec,
  0x5f, 0x38, 0xb5, 0x8d, 0xb2, 0x8d, 0x02, 0x81, 0x81, 0x00, 0xed, 0x5a,
  0x7e, 0x8e, 0xa1, 0x62, 0x7d, 0x26, 0x5c, 0x78, 0xc4, 0x87, 0x71, 0xc9,
  0x41, 0x57, 0x77, 0x94, 0x93, 0x93, 0x26, 0x78, 0xc8, 0xa3, 0x15, 0xbd,
  0x59, 0xcb, 0x1b, 0xb4, 0xb2, 0x6b, 0x0f, 0xe7, 0x80, 0xf2, 0xfa, 0xfc,
  0x8e, 0x32, 0xa9, 0x1b, 0x1e, 0x7f, 0xe1, 0x26, 0xef, 0x00, 0x25, 0xd8,
  0xdd, 0xc9, 0x1a, 0x23, 0x00, 0x26, 0x3b, 0x46, 0x23, 0xc0, 0x50, 0xe7,
  0xce, 0x62, 0xb2, 0x36, 0xb2, 0x98, 0x09, 0x16, 0x34, 0x18, 0x9e, 0x46,
  0xbc, 0xaf, 0x2c, 0x28, 0x94, 0x2f, 0xe0, 0x5d, 0xc9, 0xb2, 0xc8, 0xfb,
  0x5d, 0x13, 0xd5, 0x36, 0xaa, 0x15, 0x0f, 0x89, 0xa5, 0x16, 0x59, 0x5d,
  0x22, 0x74, 0xa4, 0x47, 0x5d, 0xfa, 0xfb, 0x0c, 0x5e, 0x80, 0xbf, 0x0f,
  0xc2, 0x9c, 0x95, 0x0f, 0xe7, 0xaa, 0x7f, 0x16, 0x1b, 0xd4, 0xdb, 0x38,
  0x7d, 0x58, 0x2e, 0x57, 0x78, 0x2f, 0x02, 0x81, 0x81, 0x00, 0xcc, 0x1d,
  0x7f, 0x74, 0x36, 0x6d, 0xb4, 0x92, 0x25, 0x62, 0xc5, 0x50, 0xb0, 0x5c,
  0xa1, 0xda, 0xf3, 0xb2, 0xfd, 0x1e, 0x98, 0x0d, 0x8b, 0x05, 0x69, 0x60,
  0x8e, 0x5e, 0xd2, 0x89, 0x90, 0x4a, 0x0d, 0x46, 0x7e, 0xe2, 0x54, 0x69,
  0xae, 0x16, 0xe6, 0xcb, 0xd5, 0xbd, 0x7b, 0x30, 0x2b, 0x7b, 0x5c, 0xee,
  0x93, 0x12, 0xcf, 0x63, 0x89, 0x9c, 0x3d, 0xc8, 0x2d, 0xe4, 0x7a, 0x61,
  0x09, 0x5e, 0x80, 0xfb, 0x3c, 0x03, 0xb3, 0x73, 0xd6, 0x98, 0xd0, 0x84,
  0x0c, 0x59, 0x9f, 0x4e, 0x80, 0xf3, 0x46, 0xed, 0x03, 0x9d, 0xd5, 0xdc,
  0x8b, 0xe7, 0xb1, 0xe8, 0xaa, 0x57, 0xdc, 0xd1, 0x41, 0x55, 0x07, 0xc7,
  0xdf, 0x67, 0x3c, 0x72, 0x78, 0xb0, 0x60, 0x8f, 0x85, 0xa1, 0x90, 0x99,
  0x0c, 0xa5, 0x67, 0xab, 0xf0, 0xb6, 0x74, 0x90, 0x03, 0x55, 0x7b, 0x5e,
  0xcc, 0xc5, 0xbf, 0xde, 0xa7, 0x9f, 0x02, 0x81, 0x80, 0x40, 0x81, 0x6e,
  0x91, 0xae, 0xd4, 0x88, 0x74, 0xab, 0x7e, 0xfa, 0xd2, 0x60, 0x9f, 0x34,
  0x8d, 0xe3, 0xe6, 0xd2, 0x30, 0x94, 0xad, 0x10, 0xc2, 0x19, 0xbf, 0x6b,
  0x2e, 0xe2, 0xe9, 0xb9, 0xef, 0x94, 0xd3, 0xf2, 0xdc, 0x96, 0x4f, 0x9b,
  0x09, 0xb3, 0xa1, 0xb6, 0x29, 0x44, 0xf4, 0x82, 0xd1, 0xc4, 0x77, 0x6a,
  0xd7, 0x23, 0xae, 0x4d, 0x75, 0x16, 0x78, 0xda, 0x70, 0x82, 0xcc, 0x6c,
  0xef, 0xaf, 0xc5, 0x63, 0xc6, 0x23, 0xfa, 0x0f, 0xd0, 0x7c, 0xfb, 0x76,
  0x7e, 0x18, 0xff, 0x32, 0x3e, 0xcc, 0xb8, 0x50, 0x7f, 0xb1, 0x55, 0x77,
  0x17, 0x53, 0xc3, 0xd6, 0x77, 0x80, 0xd0, 0x84, 0xb8, 0x4d, 0x33, 0x1d,
  0x91, 0x1b, 0xb0, 0x75, 0x9f, 0x27, 0x29, 0x56, 0x69, 0xa1, 0x03, 0x54,
  0x7d, 0x9f, 0x99, 0x41, 0xf9, 0xb9, 0x2e, 0x36, 0x04, 0x24, 0x4b, 0xf6,
  0xec, 0xc7, 0x33, 0x68, 0x6b, 0x02, 0x81, 0x80, 0x60, 0x35, 0xcb, 0x3c,
  0xd0, 0xe6, 0xf7, 0x05, 0x28, 0x20, 0x1d, 0x57, 0x82, 0x39, 0xb7, 0x85,
  0x07, 0xf7, 0xa7, 0x3d, 0xc3, 0x78, 0x26, 0xbe, 0x3f, 0x44, 0x66, 0xf7,
  0x25, 0x0f, 0xf8, 0x76, 0x1f, 0x39, 0xca, 0x57, 0x0e, 0x68, 0xdd, 0xc9,
  0x27, 0xb2, 0x8e, 0xa6, 0x08, 0xa9, 0xd4, 0xe5, 0x0a, 0x11, 0xde, 0x3b,
  0x30, 0x8b, 0xff, 0x72, 0x28, 0xe0, 0xf1, 0x58, 0xcf, 0xa2, 0x6b, 0x93,
  0x23, 0x02, 0xc8, 0xf0, 0x09, 0xa7, 0x21, 0x50, 0xd8, 0x80, 0x55, 0x7d,
  0xed, 0x0c, 0x48, 0xd5, 0xe2, 0xe9, 0x97, 0x19, 0xcf, 0x93, 0x6c, 0x52,
  0xa2, 0xd6, 0x43, 0x6c, 0xb4, 0xc5, 0xe1, 0xa0, 0x9d, 0xd1, 0x45, 0x69,
  0x58, 0xe1, 0xb0, 0x27, 0x9a, 0xec, 0x2b, 0x95, 0xd3, 0x1d, 0x81, 0x0b,
  0x7a, 0x09, 0x5e, 0xa5, 0xf1, 0xdd, 0x6b, 0xe4, 0xe0, 0x08, 0xf8, 0x46,
  0x81, 0xc1, 0x06, 0x8b, 0x02, 0x81, 0x80, 0x00, 0xf6, 0xf2, 0xeb, 0x25,
  0xba, 0x78, 0x04, 0xad, 0x0e, 0x0d, 0x2e, 0xa7, 0x69, 0xd6, 0x57, 0xe6,
  0x36, 0x32, 0x50, 0xd2, 0xf2, 0xeb, 0xad, 0x31, 0x46, 0x65, 0xc0, 0x07,
  0x97, 0x83, 0x6c, 0x66, 0x27, 0x3e, 0x94, 0x2c, 0x05, 0x01, 0x5f, 0x5c,
  0xe0, 0x31, 0x30, 0xec, 0x61, 0xd2, 0x74, 0x35, 0xb7, 0x9f, 0x38, 0xe7,
  0x8e, 0x67, 0xb1, 0x50, 0x08, 0x68, 0xce, 0xcf, 0xd8, 0xee, 0x88, 0xfd,
  0x5d, 0xc4, 0xcd, 0xe2, 0x86, 0x3d, 0x4a, 0x0e, 0x04, 0x7f, 0xee, 0x8a,
  0xe8, 0x9b, 0x16, 0xa1, 0xfc, 0x09, 0x82, 0xe2, 0x62, 0x03, 0x3c, 0xe8,
  0x25, 0x7f, 0x3c, 0x9a, 0xaa, 0x83, 0xf8, 0xd8, 0x93, 0xd1, 0x54, 0xf9,
  0xce, 0xb4, 0xfa, 0x35, 0x36, 0xcc, 0x18, 0x54, 0xaa, 0xf2, 0x90, 0xb7,
  0x7c, 0x97, 0x0b, 0x27, 0x2f, 0xae, 0xfc, 0xc3, 0x93, 0xaf, 0x1a, 0x75,
  0xec, 0x18, 0xdb
};
static const unsigned int hanselPrivateRsaSz = 1191;
#endif


#ifdef HAVE_ECC
#ifndef NO_ECC256
static const char* hanselPublicEcc =
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBNkI5JTP6D0lF42tbxX19cE87hztUS6FSDoGvPfiU0CgeNSbI+aFdKIzTP5CQEJSvm25"
    "qUzgDtH7oyaQROUnNvk= hansel";
static const byte hanselPrivateEcc[] = {
  0x30, 0x77, 0x02, 0x01, 0x01, 0x04, 0x20, 0x03, 0x6e, 0x17, 0xd3, 0xb9,
  0xb8, 0xab, 0xc8, 0xf9, 0x1f, 0xf1, 0x2d, 0x44, 0x4c, 0x3b, 0x12, 0xb1,
  0xa4, 0x77, 0xd8, 0xed, 0x0e, 0x6a, 0xbe, 0x60, 0xc2, 0xf6, 0x8b, 0xe7,
  0xd3, 0x87, 0x83, 0xa0, 0x0a, 0x06, 0x08, 0x2a, 0x86, 0x48, 0xce, 0x3d,
  0x03, 0x01, 0x07, 0xa1, 0x44, 0x03, 0x42, 0x00, 0x04, 0xd9, 0x08, 0xe4,
  0x94, 0xcf, 0xe8, 0x3d, 0x25, 0x17, 0x8d, 0xad, 0x6f, 0x15, 0xf5, 0xf5,
  0xc1, 0x3c, 0xee, 0x1c, 0xed, 0x51, 0x2e, 0x85, 0x48, 0x3a, 0x06, 0xbc,
  0xf7, 0xe2, 0x53, 0x40, 0xa0, 0x78, 0xd4, 0x9b, 0x23, 0xe6, 0x85, 0x74,
  0xa2, 0x33, 0x4c, 0xfe, 0x42, 0x40, 0x42, 0x52, 0xbe, 0x6d, 0xb9, 0xa9,
  0x4c, 0xe0, 0x0e, 0xd1, 0xfb, 0xa3, 0x26, 0x90, 0x44, 0xe5, 0x27, 0x36,
  0xf9
};
static const unsigned int hanselPrivateEccSz = 121;
#elif defined(HAVE_ECC521)
static const char* hanselPublicEcc =
    "ecdsa-sha2-nistp521 AAAAE2VjZHNhLXNoYTItbmlzdHA1MjEAAAAIbmlzdHA1MjEAAA"
    "CFBAET/BOzBb9Jx9b52VIHFP4g/uk5KceDpz2M+/Ln9WiDjsMfb4NgNCAB+EMNJUX/TNBL"
    "FFmqr7c6+zUH+QAo2qstvQDsReyFkETRB2vZD//nCZfcAe0RMtKZmgtQLKXzSlimUjXBM4"
    "/zE5lwE05aXADp88h8nuaT/X4bll9cWJlH0fUykA== hansel";
static const byte hanselPrivateEcc[] = {
  0x30, 0x81, 0xdc, 0x02, 0x01, 0x01, 0x04, 0x42, 0x01, 0x79, 0x40, 0xb8,
  0x33, 0xe5, 0x53, 0x5b, 0x9e, 0xfd, 0xed, 0xbe, 0x7c, 0x68, 0xe4, 0xb6,
  0xc3, 0x50, 0x00, 0x0d, 0x39, 0x64, 0x05, 0xf6, 0x5a, 0x5d, 0x41, 0xab,
  0xb3, 0xd9, 0xa7, 0xcb, 0x1c, 0x7d, 0x34, 0x46, 0x5c, 0x2d, 0x56, 0x26,
  0xa0, 0x6a, 0xc7, 0x3d, 0x4f, 0x78, 0x58, 0x14, 0x66, 0x6c, 0xfc, 0x86,
  0x3c, 0x8b, 0x5b, 0x54, 0x29, 0x89, 0x93, 0x48, 0xd9, 0x54, 0x8b, 0xbe,
  0x9d, 0x91, 0xa0, 0x07, 0x06, 0x05, 0x2b, 0x81, 0x04, 0x00, 0x23, 0xa1,
  0x81, 0x89, 0x03, 0x81, 0x86, 0x00, 0x04, 0x01, 0x13, 0xfc, 0x13, 0xb3,
  0x05, 0xbf, 0x49, 0xc7, 0xd6, 0xf9, 0xd9, 0x52, 0x07, 0x14, 0xfe, 0x20,
  0xfe, 0xe9, 0x39, 0x29, 0xc7, 0x83, 0xa7, 0x3d, 0x8c, 0xfb, 0xf2, 0xe7,
  0xf5, 0x68, 0x83, 0x8e, 0xc3, 0x1f, 0x6f, 0x83, 0x60, 0x34, 0x20, 0x01,
  0xf8, 0x43, 0x0d, 0x25, 0x45, 0xff, 0x4c, 0xd0, 0x4b, 0x14, 0x59, 0xaa,
  0xaf, 0xb7, 0x3a, 0xfb, 0x35, 0x07, 0xf9, 0x00, 0x28, 0xda, 0xab, 0x2d,
  0xbd, 0x00, 0xec, 0x45, 0xec, 0x85, 0x90, 0x44, 0xd1, 0x07, 0x6b, 0xd9,
  0x0f, 0xff, 0xe7, 0x09, 0x97, 0xdc, 0x01, 0xed, 0x11, 0x32, 0xd2, 0x99,
  0x9a, 0x0b, 0x50, 0x2c, 0xa5, 0xf3, 0x4a, 0x58, 0xa6, 0x52, 0x35, 0xc1,
  0x33, 0x8f, 0xf3, 0x13, 0x99, 0x70, 0x13, 0x4e, 0x5a, 0x5c, 0x00, 0xe9,
  0xf3, 0xc8, 0x7c, 0x9e, 0xe6, 0x93, 0xfd, 0x7e, 0x1b, 0x96, 0x5f, 0x5c,
  0x58, 0x99, 0x47, 0xd1, 0xf5, 0x32, 0x90
};
static const unsigned int hanselPrivateEccSz = 223;
#else
    #error "Enable an ECC Curve or disable ECC."
#endif
#endif


static int wsUserAuth(byte authType,
                      WS_UserAuthData* authData,
                      void* ctx)
{
    int ret = WOLFSSH_USERAUTH_SUCCESS;

#ifdef DEBUG_WOLFSSH
    /* inspect supported types from server */
    printf("Server supports ");
    if (authData->type & WOLFSSH_USERAUTH_PASSWORD) {
        printf("password authentication");
    }
    if (authData->type & WOLFSSH_USERAUTH_PUBLICKEY) {
        printf(" and public key authentication");
    }
    printf("\n");
    printf("wolfSSH requesting to use type %d\n", authType);
#endif

    /* We know hansel has a key, wait for request of public key */
    if ((authData->type & WOLFSSH_USERAUTH_PUBLICKEY) &&
            authData->username != NULL &&
            authData->usernameSz > 0 &&
            (XSTRNCMP((char*)authData->username, "hansel",
                authData->usernameSz) == 0)) {
        if (authType == WOLFSSH_USERAUTH_PASSWORD) {
            printf("rejecting password type with %s in favor of pub key\n",
                    (char*)authData->username);
            return WOLFSSH_USERAUTH_FAILURE;
        }
    }

    if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
        WS_UserAuthData_PublicKey* pk = &authData->sf.publicKey;

        pk->publicKeyType = userPublicKeyType;
        pk->publicKeyTypeSz = userPublicKeyTypeSz;
        pk->publicKey = userPublicKey;
        pk->publicKeySz = userPublicKeySz;
        pk->privateKey = userPrivateKey;
        pk->privateKeySz = userPrivateKeySz;

        ret = WOLFSSH_USERAUTH_SUCCESS;
    }
    else if (authType == WOLFSSH_USERAUTH_PASSWORD) {
        const char* defaultPassword = (const char*)ctx;
        word32 passwordSz = 0;

        if (defaultPassword != NULL) {
            passwordSz = (word32)strlen(defaultPassword);
            memcpy(userPassword, defaultPassword, passwordSz);
        }
        else {
            printf("Password: ");
            fflush(stdout);
            SetEcho(0);
            if (fgets((char*)userPassword, sizeof(userPassword), stdin) == NULL) {
                printf("Getting password failed.\n");
                ret = WOLFSSH_USERAUTH_FAILURE;
            }
            else {
                char* c = strpbrk((char*)userPassword, "\r\n");
                if (c != NULL)
                    *c = '\0';
            }
            passwordSz = (word32)strlen((const char*)userPassword);
            SetEcho(1);
            #ifdef USE_WINDOWS_API
                printf("\r\n");
            #endif
        }

        if (ret == WOLFSSH_USERAUTH_SUCCESS) {
            authData->sf.password.password = userPassword;
            authData->sf.password.passwordSz = passwordSz;
        }
    }

    return ret;
}


static int wsPublicKeyCheck(const byte* pubKey, word32 pubKeySz, void* ctx)
{
    #ifdef DEBUG_WOLFSSH
        printf("Sample public key check callback\n"
               "  public key = %p\n"
               "  public key size = %u\n"
               "  ctx = %s\n", pubKey, pubKeySz, (const char*)ctx);
    #else
        (void)pubKey;
        (void)pubKeySz;
        (void)ctx;
    #endif
    return 0;
}


static int NonBlockSSH_connect(WOLFSSH* ssh)
{
    int ret;
    int error;
    SOCKET_T sockfd;
    int select_ret = 0;

    ret = wolfSSH_connect(ssh);
    error = wolfSSH_get_error(ssh);
    sockfd = (SOCKET_T)wolfSSH_get_fd(ssh);

    while (ret != WS_SUCCESS &&
            (error == WS_WANT_READ || error == WS_WANT_WRITE))
    {
        if (error == WS_WANT_READ)
            printf("... client would read block\n");
        else if (error == WS_WANT_WRITE)
            printf("... client would write block\n");

        select_ret = tcp_select(sockfd, 1);
        if (select_ret == WS_SELECT_RECV_READY ||
            select_ret == WS_SELECT_ERROR_READY)
        {
            ret = wolfSSH_connect(ssh);
        }
        else if (select_ret == WS_SELECT_TIMEOUT)
            error = WS_WANT_READ;
        else
            error = WS_FATAL_ERROR;
    }

    return ret;
}

#if !defined(SINGLE_THREADED) && !defined(WOLFSSL_NUCLEUS)

typedef struct thread_args {
    WOLFSSH* ssh;
    wolfSSL_Mutex lock;
    byte rawMode;
} thread_args;

#ifdef _POSIX_THREADS
    #define THREAD_RET void*
    #define THREAD_RET_SUCCESS NULL
#elif defined(_MSC_VER)
    #define THREAD_RET DWORD WINAPI
    #define THREAD_RET_SUCCESS 0
#else
    #define THREAD_RET int
    #define THREAD_RET_SUCCESS 0
#endif

static THREAD_RET readInput(void* in)
{
    byte buf[256];
    int  bufSz = sizeof(buf);
    thread_args* args = (thread_args*)in;
    int ret = 0;
    word32 sz = 0;
#ifdef USE_WINDOWS_API
    HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
#endif

    while (ret >= 0) {
        WMEMSET(buf, 0, bufSz);
    #ifdef USE_WINDOWS_API
        /* Using A version to avoid potential 2 byte chars */
        ret = ReadConsoleA(stdinHandle, (void*)buf, bufSz - 1, (DWORD*)&sz,
                NULL);
    #else
        ret = (int)read(STDIN_FILENO, buf, bufSz -1);
        sz  = (word32)ret;
    #endif
        if (ret <= 0) {
            err_sys("Error reading stdin");
        }
        /* lock SSH structure access */
        wc_LockMutex(&args->lock);
        ret = wolfSSH_stream_send(args->ssh, buf, sz);
        wc_UnLockMutex(&args->lock);
        if (ret <= 0)
            err_sys("Couldn't send data");
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif
    return THREAD_RET_SUCCESS;
}


#ifdef WOLFSSH_AGENT
static inline void ato32(const byte* c, word32* u32)
{
    *u32 = (c[0] << 24) | (c[1] << 16) | (c[2] << 8) | c[3];
}
#endif


static THREAD_RET readPeer(void* in)
{
    byte buf[80];
    int  bufSz = sizeof(buf);
    thread_args* args = (thread_args*)in;
    int ret = 0;
    int fd = wolfSSH_get_fd(args->ssh);
    word32 bytes;
#ifdef USE_WINDOWS_API
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);
#endif
    fd_set readSet;
    fd_set errSet;

    FD_ZERO(&readSet);
    FD_ZERO(&errSet);
    FD_SET(fd, &readSet);
    FD_SET(fd, &errSet);

    while (ret >= 0) {
        bytes = select(fd + 1, &readSet, NULL, &errSet, NULL);
        wc_LockMutex(&args->lock);
        while (bytes > 0 && (FD_ISSET(fd, &readSet) || FD_ISSET(fd, &errSet))) {
            /* there is something to read off the wire */
            WMEMSET(buf, 0, bufSz);
            ret = wolfSSH_stream_read(args->ssh, buf, bufSz - 1);
            if (ret == WS_EXTDATA) { /* handle extended data */
                do {
                    WMEMSET(buf, 0, bufSz);
                    ret = wolfSSH_extended_data_read(args->ssh, buf, bufSz - 1);
                    if (ret < 0)
                        err_sys("Extended data read failed.");
                    buf[bufSz - 1] = '\0';
                    fprintf(stderr, "%s", buf);
                } while (ret > 0);
            }
            else if (ret <= 0) {
                #ifdef WOLFSSH_AGENT
                if (ret == WS_FATAL_ERROR) {
                    ret = wolfSSH_get_error(args->ssh);
                    if (ret == WS_CHAN_RXD) {
                        byte agentBuf[512];
                        int rxd, txd;
                        word32 channel = 0;

                        wolfSSH_GetLastRxId(args->ssh, &channel);
                        rxd = wolfSSH_ChannelIdRead(args->ssh, channel,
                                agentBuf, sizeof(agentBuf));
                        if (rxd > 4) {
                            word32 msgSz = 0;

                            ato32(agentBuf, &msgSz);
                            if (msgSz > (word32)rxd - 4) {
                                rxd += wolfSSH_ChannelIdRead(args->ssh, channel,
                                        agentBuf + rxd,
                                        sizeof(agentBuf) - rxd);
                            }

                            txd = rxd;
                            rxd = sizeof(agentBuf);
                            ret = wolfSSH_AGENT_Relay(args->ssh,
                                    agentBuf, (word32*)&txd,
                                    agentBuf, (word32*)&rxd);
                            if (ret == WS_SUCCESS) {
                                ret = wolfSSH_ChannelIdSend(args->ssh, channel,
                                        agentBuf, rxd);
                            }
                        }
                        WMEMSET(agentBuf, 0, sizeof(agentBuf));
                        continue;
                    }
                }
                #endif
                if (ret != WS_EOF) {
                    err_sys("Stream read failed.");
                }
            }
            else {
                buf[bufSz - 1] = '\0';

            #ifdef USE_WINDOWS_API
                if (args->rawMode == 0) {
                    ret = wolfSSH_ConvertConsole(args->ssh, stdoutHandle, buf,
                            ret);
                    if (ret != WS_SUCCESS && ret != WS_WANT_READ) {
                        err_sys("issue with print out");
                    }
                    if (ret == WS_WANT_READ) {
                        ret = 0;
                    }
                }
                else {
                    printf("%s", buf);
                    fflush(stdout);
                }
            #else
                printf("%s", buf);
                fflush(stdout);
            #endif
            }
            if (wolfSSH_stream_peek(args->ssh, buf, bufSz) <= 0) {
                bytes = 0; /* read it all */
            }
        }
        wc_UnLockMutex(&args->lock);
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

    return THREAD_RET_SUCCESS;
}
#endif /* !SINGLE_THREADED && !WOLFSSL_NUCLEUS */

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)

static int callbackGlobalReq(WOLFSSH *ssh, void *buf, word32 sz, int reply, void *ctx)
{
    char reqStr[] = "SampleRequest";

    if ((WOLFSSH *)ssh != *(WOLFSSH **)ctx)
    {
        printf("ssh(%x) != ctx(%x)\n", (unsigned int)ssh, (unsigned int)*(WOLFSSH **)ctx);
        return WS_FATAL_ERROR;
    }

    if (strlen(reqStr) == sz && (strncmp((char *)buf, reqStr, sz) == 0)
        && reply == 1){
        printf("Global Request\n");
        return WS_SUCCESS;
    } else {
        return WS_FATAL_ERROR;
    }

}
#endif


#ifdef WOLFSSH_AGENT
typedef struct WS_AgentCbActionCtx {
    struct sockaddr_un name;
    int fd;
    int state;
} WS_AgentCbActionCtx;

static const char EnvNameAuthPort[] = "SSH_AUTH_SOCK";

static int wolfSSH_AGENT_DefaultActions(WS_AgentCbAction action, void* vCtx)
{
    WS_AgentCbActionCtx* ctx = (WS_AgentCbActionCtx*)vCtx;
    int ret = WS_AGENT_SUCCESS;

    if (action == WOLFSSH_AGENT_LOCAL_SETUP) {
        const char* sockName;
        struct sockaddr_un* name = &ctx->name;
        size_t size;
        int err;

        sockName = getenv(EnvNameAuthPort);
        if (sockName == NULL)
            ret = WS_AGENT_NOT_AVAILABLE;

        if (ret == WS_AGENT_SUCCESS) {
            memset(name, 0, sizeof(struct sockaddr_un));
            name->sun_family = AF_LOCAL;
            strncpy(name->sun_path, sockName, sizeof(name->sun_path));
            name->sun_path[sizeof(name->sun_path) - 1] = '\0';
            size = strlen(sockName) +
                    offsetof(struct sockaddr_un, sun_path);

            ctx->fd = socket(AF_UNIX, SOCK_STREAM, 0);
            if (ctx->fd == -1) {
                ret = WS_AGENT_SETUP_E;
                err = errno;
                fprintf(stderr, "socket() = %d\n", err);
            }
        }

        if (ret == WS_AGENT_SUCCESS) {
            ret = connect(ctx->fd,
                    (struct sockaddr *)name, (socklen_t)size);
            if (ret < 0) {
                ret = WS_AGENT_SETUP_E;
                err = errno;
                fprintf(stderr, "connect() = %d", err);
            }
        }

        if (ret == WS_AGENT_SUCCESS)
            ctx->state = AGENT_STATE_CONNECTED;
    }
    else if (action == WOLFSSH_AGENT_LOCAL_CLEANUP) {
        int err;

        err = close(ctx->fd);
        if (err != 0) {
            err = errno;
            fprintf(stderr, "close() = %d", err);
            if (ret == 0)
                ret = WS_AGENT_SETUP_E;
        }
    }
    else
        ret = WS_AGENT_INVALID_ACTION;

    return ret;
}


static int wolfSSH_AGENT_IO_Cb(WS_AgentIoCbAction action,
        void* buf, word32 bufSz, void* vCtx)
{
    WS_AgentCbActionCtx* ctx = (WS_AgentCbActionCtx*)vCtx;
    int ret = WS_AGENT_INVALID_ACTION;

    if (action == WOLFSSH_AGENT_IO_WRITE) {
        const byte* wBuf = (const byte*)buf;
        ret = (int)write(ctx->fd, wBuf, bufSz);
        if (ret < 0) {
            ret = WS_CBIO_ERR_GENERAL;
        }
    }
    else if (action == WOLFSSH_AGENT_IO_READ) {
        byte* rBuf = (byte*)buf;
        ret = (int)read(ctx->fd, rBuf, bufSz);
        if (ret < 0) {
            ret = WS_CBIO_ERR_GENERAL;
        }
    }

    return ret;
}


#endif /* WOLFSSH_AGENT */


THREAD_RETURN WOLFSSH_THREAD client_test(void* args)
{
    WOLFSSH_CTX* ctx = NULL;
    WOLFSSH* ssh = NULL;
    SOCKET_T sockFd = WOLFSSH_SOCKET_INVALID;
    SOCKADDR_IN_T clientAddr;
    socklen_t clientAddrSz = sizeof(clientAddr);
    char rxBuf[80];
    int ret = 0;
    int ch;
    int userEcc = 0;
    word16 port = wolfSshPort;
    char* host = (char*)wolfSshIp;
    const char* username = NULL;
    const char* password = NULL;
    const char* cmd      = NULL;
    const char* privKeyName = NULL;
    const char* pubKeyName = NULL;
    byte imExit = 0;
    byte nonBlock = 0;
    byte keepOpen = 0;
#ifdef USE_WINDOWS_API
    byte rawMode = 0;
#endif
#ifdef WOLFSSH_AGENT
    byte useAgent = 0;
    WS_AgentCbActionCtx agentCbCtx;
#endif

    int     argc = ((func_args*)args)->argc;
    char**  argv = ((func_args*)args)->argv;
    ((func_args*)args)->return_code = 0;

    while ((ch = mygetopt(argc, argv, "?ac:eh:i:j:p:tu:xzNP:R")) != -1) {
        switch (ch) {
            case 'h':
                host = myoptarg;
                break;

            case 'z':
            #ifdef WOLFSSH_SHOW_SIZES
                wolfSSH_ShowSizes();
                exit(EXIT_SUCCESS);
            #endif
                break;

            case 'e':
                userEcc = 1;
                break;

            case 'p':
                port = (word16)atoi(myoptarg);
                #if !defined(NO_MAIN_DRIVER) || defined(USE_WINDOWS_API)
                    if (port == 0)
                        err_sys("port number cannot be 0");
                #endif
                break;

            case 'u':
                username = myoptarg;
                break;

            case 'P':
                password = myoptarg;
                break;

            case 'i':
                privKeyName = myoptarg;
                break;

            case 'j':
                pubKeyName = myoptarg;
                break;

            case 'x':
                /* exit after successful connection without read/write */
                imExit = 1;
                break;

            case 'N':
                nonBlock = 1;
                break;

        #if !defined(SINGLE_THREADED) && !defined(WOLFSSL_NUCLEUS)
            case 'c':
                cmd = myoptarg;
                break;
        #ifdef USE_WINDOWS_API
           case 'R':
                rawMode = 1;
                break;
        #endif /* USE_WINDOWS_API */
        #endif

        #ifdef WOLFSSH_TERM
            case 't':
                keepOpen = 1;
                break;
        #endif

        #ifdef WOLFSSH_AGENT
            case 'a':
                useAgent = 1;
                break;
        #endif

            case '?':
                ShowUsage();
                exit(EXIT_SUCCESS);

            default:
                ShowUsage();
                exit(MY_EX_USAGE);
        }
    }
    myoptind = 0;      /* reset for test cases */

    if (username == NULL)
        err_sys("client requires a username parameter.");

#ifdef NO_RSA
    userEcc = 1;
#endif

#ifdef SINGLE_THREADED
    if (keepOpen)
        err_sys("Threading needed for terminal session\n");
#endif

    if (pubKeyName == NULL && privKeyName != NULL) {
        err_sys("If setting priv key, need pub key.");
    }

    if (privKeyName == NULL) {
        if (userEcc) {
        #ifdef HAVE_ECC
            ret = wolfSSH_ReadKey_buffer(hanselPrivateEcc, hanselPrivateEccSz,
                    WOLFSSH_FORMAT_ASN1, &userPrivateKey, &userPrivateKeySz,
                    &userPrivateKeyType, &userPrivateKeyTypeSz, NULL);
        #endif
        }
        else {
        #ifndef NO_RSA
            ret = wolfSSH_ReadKey_buffer(hanselPrivateRsa, hanselPrivateRsaSz,
                    WOLFSSH_FORMAT_ASN1, &userPrivateKey, &userPrivateKeySz,
                    &userPrivateKeyType, &userPrivateKeyTypeSz, NULL);
        #endif
        }
        isPrivate = 1;
        if (ret != 0) err_sys("Couldn't load private key buffer.");
    }
    else {
    #ifndef NO_FILESYSTEM
        ret = wolfSSH_ReadKey_file(privKeyName,
                (byte**)&userPrivateKey, &userPrivateKeySz,
                (const byte**)&userPrivateKeyType, &userPrivateKeyTypeSz,
                &isPrivate, NULL);
    #else
        printf("file system not compiled in!\n");
        ret = -1;
    #endif
        if (ret != 0) err_sys("Couldn't load private key file.");
    }

    if (pubKeyName == NULL) {
        byte* p = userPublicKey;
        userPublicKeySz = sizeof(userPublicKey);

        if (userEcc) {
        #ifdef HAVE_ECC
            ret = wolfSSH_ReadKey_buffer((const byte*)hanselPublicEcc,
                    (word32)strlen(hanselPublicEcc), WOLFSSH_FORMAT_SSH,
                    &p, &userPublicKeySz,
                    &userPublicKeyType, &userPublicKeyTypeSz, NULL);
        #endif
        }
        else {
        #ifndef NO_RSA
            ret = wolfSSH_ReadKey_buffer((const byte*)hanselPublicRsa,
                    (word32)strlen(hanselPublicRsa), WOLFSSH_FORMAT_SSH,
                    &p, &userPublicKeySz,
                    &userPublicKeyType, &userPublicKeyTypeSz, NULL);
        #endif
        }
        isPrivate = 1;
        if (ret != 0) err_sys("Couldn't load public key buffer.");
    }
    else {
    #ifndef NO_FILESYSTEM
        byte* p = userPublicKey;
        userPublicKeySz = sizeof(userPublicKey);

        ret = wolfSSH_ReadKey_file(pubKeyName,
                &p, &userPublicKeySz,
                (const byte**)&userPublicKeyType, &userPublicKeyTypeSz,
                &isPrivate, NULL);
    #else
        printf("file system not compiled in!\n");
        ret = -1;
    #endif
        if (ret != 0) err_sys("Couldn't load public key file.");
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (ctx == NULL)
        err_sys("Couldn't create wolfSSH client context.");

    if (((func_args*)args)->user_auth == NULL)
        wolfSSH_SetUserAuth(ctx, wsUserAuth);
    else
        wolfSSH_SetUserAuth(ctx, ((func_args*)args)->user_auth);

#ifdef WOLFSSH_AGENT
    if (useAgent) {
        wolfSSH_CTX_set_agent_cb(ctx,
                wolfSSH_AGENT_DefaultActions, wolfSSH_AGENT_IO_Cb);
        wolfSSH_CTX_AGENT_enable(ctx, 1);
    }
#endif

    ssh = wolfSSH_new(ctx);
    if (ssh == NULL)
        err_sys("Couldn't create wolfSSH session.");

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    wolfSSH_SetGlobalReq(ctx, callbackGlobalReq);
    wolfSSH_SetGlobalReqCtx(ssh, &ssh); /* dummy ctx */
#endif

    if (password != NULL)
        wolfSSH_SetUserAuthCtx(ssh, (void*)password);

#ifdef WOLFSSH_AGENT
    if (useAgent) {
        memset(&agentCbCtx, 0, sizeof(agentCbCtx));
        agentCbCtx.state = AGENT_STATE_INIT;
        wolfSSH_set_agent_cb_ctx(ssh, &agentCbCtx);
    }
#endif

    wolfSSH_CTX_SetPublicKeyCheck(ctx, wsPublicKeyCheck);
    wolfSSH_SetPublicKeyCheckCtx(ssh, (void*)"You've been sampled!");

    ret = wolfSSH_SetUsername(ssh, username);
    if (ret != WS_SUCCESS)
        err_sys("Couldn't set the username.");

    build_addr(&clientAddr, host, port);
    tcp_socket(&sockFd);
    ret = connect(sockFd, (const struct sockaddr *)&clientAddr, clientAddrSz);
    if (ret != 0)
        err_sys("Couldn't connect to server.");

    if (nonBlock)
        tcp_set_nonblocking(&sockFd);

    ret = wolfSSH_set_fd(ssh, (int)sockFd);
    if (ret != WS_SUCCESS)
        err_sys("Couldn't set the session's socket.");

    if (cmd != NULL) {
        ret = wolfSSH_SetChannelType(ssh, WOLFSSH_SESSION_EXEC,
                            (byte*)cmd, (word32)WSTRLEN((char*)cmd));
        if (ret != WS_SUCCESS)
            err_sys("Couldn't set the channel type.");
    }

#ifdef WOLFSSH_TERM
    if (keepOpen) {
        ret = wolfSSH_SetChannelType(ssh, WOLFSSH_SESSION_TERMINAL, NULL, 0);
        if (ret != WS_SUCCESS)
            err_sys("Couldn't set the terminal channel type.");
    }
#endif

    if (!nonBlock)
        ret = wolfSSH_connect(ssh);
    else
        ret = NonBlockSSH_connect(ssh);
    if (ret != WS_SUCCESS)
        err_sys("Couldn't connect SSH stream.");

#if !defined(SINGLE_THREADED) && !defined(WOLFSSL_NUCLEUS)
    if (keepOpen) /* set up for psuedo-terminal */
        SetEcho(2);

    if (cmd != NULL || keepOpen == 1) {
    #if defined(_POSIX_THREADS)
        thread_args arg;
        pthread_t   thread[2];

        arg.ssh = ssh;
        wc_InitMutex(&arg.lock);
        pthread_create(&thread[0], NULL, readInput, (void*)&arg);
        pthread_create(&thread[1], NULL, readPeer, (void*)&arg);
        pthread_join(thread[1], NULL);
        pthread_cancel(thread[0]);
    #elif defined(_MSC_VER)
        thread_args arg;
        HANDLE thread[2];

        arg.ssh     = ssh;
        arg.rawMode = rawMode;
        wc_InitMutex(&arg.lock);
        thread[0] = CreateThread(NULL, 0, readInput, (void*)&arg, 0, 0);
        thread[1] = CreateThread(NULL, 0, readPeer, (void*)&arg, 0, 0);
        WaitForSingleObject(thread[1], INFINITE);
        CloseHandle(thread[0]);
        CloseHandle(thread[1]);
    #else
        err_sys("No threading to use");
    #endif
        if (keepOpen)
            SetEcho(1);
    }
    else
#endif

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
    while (!imExit) {
#else
    if (!imExit) {
#endif
        ret = wolfSSH_stream_send(ssh, (byte*)testString,
                                  (word32)strlen(testString));
        if (ret <= 0)
            err_sys("Couldn't send test string.");

        do {
            ret = wolfSSH_stream_read(ssh, (byte*)rxBuf, sizeof(rxBuf) - 1);
            if (ret <= 0) {
                ret = wolfSSH_get_error(ssh);
                if (ret != WS_WANT_READ && ret != WS_WANT_WRITE)
                    err_sys("Stream read failed.");
            }
        } while (ret == WS_WANT_READ || ret == WS_WANT_WRITE);

        rxBuf[ret] = '\0';
        printf("Server said: %s\n", rxBuf);

#if defined(WOLFSSL_PTHREADS) && defined(WOLFSSL_TEST_GLOBAL_REQ)
        sleep(10);
#endif
    }
    ret = wolfSSH_shutdown(ssh);
    WCLOSESOCKET(sockFd);
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
    if (ret != WS_SUCCESS)
        err_sys("Closing client stream failed. Connection could have been closed by peer");

#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

    return 0;
}

#endif /* NO_WOLFSSH_CLIENT */


#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;
        args.return_code = 0;
        args.user_auth = NULL;

        WSTARTTCP();

        #ifdef DEBUG_WOLFSSH
            wolfSSH_Debugging_ON();
        #endif

        wolfSSH_Init();

        ChangeToWolfSshRoot();
#ifndef NO_WOLFSSH_CLIENT
        client_test(&args);
#endif

        wolfSSH_Cleanup();

        return args.return_code;
    }

    int myoptind = 0;
    char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */
