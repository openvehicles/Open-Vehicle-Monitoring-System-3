/* sftpclient.c
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
#include <wolfssh/wolfsftp.h>
#include <wolfssh/test.h>
#include <wolfssh/port.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/coding.h>
#include "examples/sftpclient/sftpclient.h"
#ifndef USE_WINDOWS_API
    #include <termios.h>
#endif

#ifdef WOLFSSH_SFTP

/* static so that signal handler can access and interrupt get/put */
static WOLFSSH* ssh = NULL;
static char* workingDir;
#define fin stdin
#define fout stdout
#define MAX_CMD_SZ 7


#define AUTOPILOT_OFF 0
#define AUTOPILOT_GET 1
#define AUTOPILOT_PUT 2


static void err_msg(const char* s)
{
    printf("%s\n", s);
}


#ifndef WOLFSSH_NO_TIMESTAMP
    #include <sys/time.h>

    static char   currentFile[WOLFSSH_MAX_FILENAME+1] = "";
    static word32 startTime;
    #define TIMEOUT_VALUE 120

    word32 current_time(int);

    /* return number of seconds*/
    word32 current_time(int reset)
    {
        struct timeval tv;

        (void)reset;

        gettimeofday(&tv, 0);
        return (word32)tv.tv_sec;
    }
#endif


static void myStatusCb(WOLFSSH* sshIn, word32* bytes, char* name)
{
    word32 currentTime;
    char buf[80];
    word64 longBytes = ((word64)bytes[1] << 32) | bytes[0];

#ifndef WOLFSSH_NO_TIMESTAMP
    if (WSTRNCMP(currentFile, name, WSTRLEN(name)) != 0) {
        startTime = current_time(1);
        WMEMSET(currentFile, 0, WOLFSSH_MAX_FILENAME);
        WSTRNCPY(currentFile, name, WOLFSSH_MAX_FILENAME);
    }
    currentTime = current_time(0) - startTime;
    WSNPRINTF(buf, sizeof(buf), "Processed %8llu\t bytes in %d seconds\r",
            (unsigned long long)longBytes, currentTime);
    if (currentTime > TIMEOUT_VALUE) {
        WSNPRINTF(buf, sizeof(buf), "\nProcess timed out at %d seconds, "
                "stopping\r", currentTime);
        WMEMSET(currentFile, 0, WOLFSSH_MAX_FILENAME);
        wolfSSH_SFTP_Interrupt(ssh);
    }
#else
    WSNPRINTF(buf, sizeof(buf), "Processed %8llu\t bytes \r",
            (unsigned long long)longBytes);
    (void)currentTime;
#endif
    WFPUTS(buf, fout);
    (void)name;
    (void)sshIn;
}


static int NonBlockSSH_connect(void)
{
    int ret;
    int error;
    SOCKET_T sockfd;
    int select_ret = 0;

    ret = wolfSSH_SFTP_connect(ssh);
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
            select_ret == WS_SELECT_ERROR_READY ||
            error == WS_WANT_WRITE)
        {
            ret = wolfSSH_SFTP_connect(ssh);
            error = wolfSSH_get_error(ssh);
        }
        else if (select_ret == WS_SELECT_TIMEOUT)
            error = WS_WANT_READ;
        else
            error = WS_FATAL_ERROR;
    }

    return ret;
}


#ifndef WS_NO_SIGNAL
/* for command reget and reput to handle saving offset after interrupt during
 * get and put */
#include <signal.h>
static byte interrupt = 0;

static void sig_handler(const int sig)
{
    (void)sig;

    interrupt = 1;
    wolfSSH_SFTP_Interrupt(ssh);
}
#endif /* WS_NO_SIGNAL */

/* cleans up absolute path */
static void clean_path(char* path)
{
    int  i;
    long sz = (long)WSTRLEN(path);
    byte found;

    /* remove any double '/' chars */
    for (i = 0; i < sz; i++) {
        if (path[i] == '/' && path[i+1] == '/') {
            WMEMMOVE(path + i, path + i + 1, sz - i);
            sz -= 1;
            i--;
        }
    }

    /* remove any trailing '/' chars */
    sz = (long)WSTRLEN(path);
    for (i = (int)sz - 1; i > 0; i--) {
        if (path[i] == '/') {
            path[i] = '\0';
        }
        else {
            break;
        }
    }

    if (path != NULL) {
        /* go through path until no cases are found */
        do {
            int prIdx = 0; /* begin of cut */
            int enIdx = 0; /* end of cut */
            sz = (long)WSTRLEN(path);

            found = 0;
            for (i = 0; i < sz; i++) {
                if (path[i] == '/') {
                    int z;

                    /* if next two chars are .. then delete */
                    if (path[i+1] == '.' && path[i+2] == '.') {
                        enIdx = i + 3;

                        /* start at one char before / and retrace path */
                        for (z = i - 1; z > 0; z--) {
                            if (path[z] == '/') {
                                prIdx = z;
                                break;
                            }
                        }

                        /* cut out .. and previous */
                        WMEMMOVE(path + prIdx, path + enIdx, sz - enIdx);
                        path[sz - (enIdx - prIdx)] = '\0';

                        if (enIdx == sz) {
                            path[prIdx] = '\0';
                        }

                        /* case of at / */
                        if (WSTRLEN(path) == 0) {
                           path[0] = '/';
                           path[1] = '\0';
                        }

                        found = 1;
                        break;
                    }
                }
            }
        } while (found);
    }
}

#define WS_MAX_EXAMPLE_RW 1024

static int SetEcho(int on)
{
#ifndef USE_WINDOWS_API
    static int echoInit = 0;
    static struct termios originalTerm;
    if (!echoInit) {
        if (tcgetattr(STDIN_FILENO, &originalTerm) != 0) {
            printf("Couldn't get the original terminal settings.\n");
            return -1;
        }
        echoInit = 1;
    }
    if (on) {
        if (tcsetattr(STDIN_FILENO, TCSANOW, &originalTerm) != 0) {
            printf("Couldn't restore the terminal settings.\n");
            return -1;
        }
    }
    else {
        struct termios newTerm;
        memcpy(&newTerm, &originalTerm, sizeof(struct termios));

        newTerm.c_lflag &= ~ECHO;
        newTerm.c_lflag |= (ICANON | ECHONL);

        if (tcsetattr(STDIN_FILENO, TCSANOW, &newTerm) != 0) {
            printf("Couldn't turn off echo.\n");
            return -1;
        }
    }
#else
    static int echoInit = 0;
    static DWORD originalTerm;
    HANDLE stdinHandle = GetStdHandle(STD_INPUT_HANDLE);
    if (!echoInit) {
        if (GetConsoleMode(stdinHandle, &originalTerm) == 0) {
            printf("Couldn't get the original terminal settings.\n");
            return -1;
        }
        echoInit = 1;
    }
    if (on) {
        if (SetConsoleMode(stdinHandle, originalTerm) == 0) {
            printf("Couldn't restore the terminal settings.\n");
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

static void ShowCommands(void)
{
    printf("\n\nCommands :\n");
    printf("\tcd  <string>                      change directory\n");
    printf("\tchmod <mode> <path>               change mode\n");
    printf("\tget <remote file> <local file>    pulls file(s) from server\n");
    printf("\tls                                list current directory\n");
    printf("\tmkdir <dir name>                  creates new directory on server\n");
    printf("\tput <local file> <remote file>    push file(s) to server\n");
    printf("\tpwd                               list current path\n");
    printf("\tquit                              exit\n");
    printf("\trename <old> <new>                renames remote file\n");
    printf("\treget <remote file> <local file>  resume pulling file\n");
    printf("\treput <remote file> <local file>  resume pushing file\n");
    printf("\t<crtl + c>                        interrupt get/put cmd\n");

}

static void ShowUsage(void)
{
    printf("client %s\n", LIBWOLFSSH_VERSION_STRING);
    printf(" -?            display this help and exit\n");
    printf(" -h <host>     host to connect to, default %s\n", wolfSshIp);
    printf(" -p <num>      port to connect on, default %d\n", wolfSshPort);
    printf(" -u <username> username to authenticate as (REQUIRED)\n");
    printf(" -P <password> password for username, prompted if omitted\n");
    printf(" -d <path>     set the default local path\n");
    printf(" -N            use non blocking sockets\n");
    printf(" -e            use ECC user authentication\n");
    /*printf(" -E            use ECC server authentication\n");*/
    printf(" -l <filename> local filename\n");
    printf(" -r <filename> remote filename\n");
    printf(" -g            put local filename as remote filename\n");
    printf(" -G            get remote filename as local filename\n");

    ShowCommands();
}


static byte userPassword[256];
static byte userPublicKeyType[32];
static byte userPublicKey[512];
static word32 userPublicKeySz;
static const byte* userPrivateKey;
static word32 userPrivateKeySz;

static const char hanselPublicRsa[] =
    "AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x";

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


static const char hanselPublicEcc[] =
    "AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAABBBNkI5JTP6D0lF42tbx"
    "X19cE87hztUS6FSDoGvPfiU0CgeNSbI+aFdKIzTP5CQEJSvm25qUzgDtH7oyaQROUnNvk=";

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


static int wsUserAuth(byte authType,
                      WS_UserAuthData* authData,
                      void* ctx)
{
    int ret = WOLFSSH_USERAUTH_INVALID_AUTHTYPE;

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
    if (authData->type & WOLFSSH_USERAUTH_PUBLICKEY &&
            authData->username != NULL &&
            authData->usernameSz > 0 &&
            XSTRNCMP((char*)authData->username, "hansel",
                authData->usernameSz) == 0) {
        if (authType == WOLFSSH_USERAUTH_PASSWORD) {
            printf("rejecting password type with hansel in favor of pub key\n");
            return WOLFSSH_USERAUTH_FAILURE;
        }
    }

    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
        const char* defaultPassword = (const char*)ctx;
        word32 passwordSz;

        ret = WOLFSSH_USERAUTH_SUCCESS;
        if (defaultPassword != NULL) {
            passwordSz = (word32)strlen(defaultPassword);
            memcpy(userPassword, defaultPassword, passwordSz);
        }
        else {
            printf("Password: ");
            fflush(stdout);
            SetEcho(0);
            if (WFGETS((char*)userPassword, sizeof(userPassword),
                        stdin) == NULL) {
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
    else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
        WS_UserAuthData_PublicKey* pk = &authData->sf.publicKey;

        /* we only have hansel's key loaded */
        if (authData->username != NULL && authData->usernameSz > 0 &&
                XSTRNCMP((char*)authData->username, "hansel",
                    authData->usernameSz) == 0) {
            pk->publicKeyType = userPublicKeyType;
            pk->publicKeyTypeSz = (word32)WSTRLEN((char*)userPublicKeyType);
            pk->publicKey = userPublicKey;
            pk->publicKeySz = userPublicKeySz;
            pk->privateKey = userPrivateKey;
            pk->privateKeySz = userPrivateKeySz;
            ret = WOLFSSH_USERAUTH_SUCCESS;
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


/* returns 0 on success */
static INLINE int SFTP_FPUTS(func_args* args, const char* msg)
{
    int ret;

    if (args && args->sftp_cb)
        ret = args->sftp_cb(msg, NULL, 0);
    else
        ret = WFPUTS(msg, fout);

    return ret;
}


/* returns pointer on success, NULL on failure */
static INLINE char* SFTP_FGETS(func_args* args, char* msg, int msgSz)
{
    char* ret = NULL;

    WMEMSET(msg, 0, msgSz);
    if (args && args->sftp_cb) {
        if (args->sftp_cb(NULL, msg, msgSz) == 0)
            ret = msg;
    }
    else
        ret = WFGETS(msg, msgSz, fin);

    return ret;
}


/* main loop for handling commands */
static int doCmds(func_args* args)
{
    byte quit = 0;
    int ret = WS_SUCCESS, err;
    byte resume = 0;
    int i;

    do {
        char msg[WOLFSSH_MAX_FILENAME * 2];
        char* pt;

        if (wolfSSH_get_error(ssh) == WS_SOCKET_ERROR_E) {
            if (SFTP_FPUTS(args, "peer disconnected\n") < 0) {
                err_msg("fputs error");
                return -1;
            }
            return WS_SOCKET_ERROR_E;
        }

        if (SFTP_FPUTS(args, "wolfSSH sftp> ") < 0) {
            err_msg("fputs error");
            return -1;
        }
        fflush(stdout);

        WMEMSET(msg, 0, sizeof(msg));
        if (SFTP_FGETS(args, msg, sizeof(msg) - 1) == NULL) {
            err_msg("fgets error");
            return -1;
        }
        msg[WOLFSSH_MAX_FILENAME * 2 - 1] = '\0';

        if ((pt = WSTRNSTR(msg, "mkdir", sizeof(msg))) != NULL) {
            WS_SFTP_FILEATRB atrb;
            int sz;
            char* f = NULL;

            pt += sizeof("mkdir");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';
            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL)
                    return WS_MEMORY_E;

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                WSTRNCAT(f, "/", maxSz);
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            do {
                err = WS_SUCCESS;
                if ((ret = wolfSSH_SFTP_MKDIR(ssh, pt, &atrb)) != WS_SUCCESS) {
                    err = wolfSSH_get_error(ssh);
                    if (ret == WS_PERMISSIONS) {
                        if (SFTP_FPUTS(args, "Insufficient permissions\n") < 0) {
                            err_msg("fputs error");
                            return -1;
                        }
                    }
                    else if (err != WS_WANT_READ && err != WS_WANT_WRITE) {
                        if (SFTP_FPUTS(args, "Error writing directory\n") < 0) {
                            err_msg("fputs error");
                            return -1;
                        }
                    }
                }
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }

        if ((pt = WSTRNSTR(msg, "reget", MAX_CMD_SZ)) != NULL) {
            resume = 1;
        }

        if ((pt = WSTRNSTR(msg, "get", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f  = NULL;
            char* to = NULL;

            pt += sizeof("get");

            sz = (int)WSTRLEN(pt);
            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';

            /* search for space delimiter */
            to = pt;
            for (i = 0; i < sz; i++) {
                to++;
                if (pt[i] == ' ') {
                    pt[i] = '\0';
                    break;
                }
            }

            /* check if local file path listed */
            if (WSTRLEN(to) <= 0) {

                to = pt;
                /* if local path not listed follow path till at the tail */
                for (i = 0; i < sz; i++) {
                    if (pt[i] == '/') {
                        to = pt + i + 1;
                    }
                }
            }

            if (pt[0] != '/') {
                int maxSz = (int)(WSTRLEN(workingDir) + sz + 2);
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            {
                char buf[WOLFSSH_MAX_FILENAME * 3];
                if (resume) {
                    WSNPRINTF(buf, sizeof(buf), "resuming %s to %s\n", pt, to);
                }
                else {
                    WSNPRINTF(buf, sizeof(buf), "fetching %s to %s\n", pt, to);
                }
                if (SFTP_FPUTS(args, buf) < 0) {
                    err_msg("fputs error");
                    return -1;
                }
            }

            do {
                ret = wolfSSH_SFTP_Get(ssh, pt, to, resume, &myStatusCb);
                if (ret != WS_SUCCESS) {
                    ret = wolfSSH_get_error(ssh);
                }
            } while (ret == WS_WANT_READ || ret == WS_WANT_WRITE);

            if (ret != WS_SUCCESS) {
                if (SFTP_FPUTS(args, "Error getting file\n")  < 0) {
                     err_msg("fputs error");
                     return -1;
                }
            }
            else {
                if (SFTP_FPUTS(args, "\n") < 0) { /* new line after status output */
                     err_msg("fputs error");
                     return -1;
                }
            }
            resume = 0;
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }


        if ((pt = WSTRNSTR(msg, "reput", MAX_CMD_SZ)) != NULL) {
            resume = 1;
        }

        if ((pt = WSTRNSTR(msg, "put", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f  = NULL;
            char* to = NULL;

            pt += sizeof("put");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';

            to = pt;
            for (i = 0; i < sz; i++) {
                to++;
                if (pt[i] == ' ') {
                    pt[i] = '\0';
                    break;
                }
            }

            /* check if local file path listed */
            if (WSTRLEN(to) <= 0) {

                to = pt;
                /* if local path not listed follow path till at the tail */
                for (i = 0; i < sz; i++) {
                    if (pt[i] == '/') {
                        to = pt + i + 1;
                    }
                }
            }

            if (to[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, to, maxSz);

                to = f;
            }

            {
                char buf[WOLFSSH_MAX_FILENAME * 3];
                if (resume) {
                    WSNPRINTF(buf, sizeof(buf), "resuming %s to %s\n", pt, to);
                }
                else {
                    WSNPRINTF(buf, sizeof(buf), "pushing %s to %s\n", pt, to);
                }
                if (SFTP_FPUTS(args, buf) < 0) {
                     err_msg("fputs error");
                     return -1;
                }

            }

            do {
                ret = wolfSSH_SFTP_Put(ssh, pt, to, resume, &myStatusCb);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (SFTP_FPUTS(args, "Error pushing file\n") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
            }
            else {
                if (SFTP_FPUTS(args, "\n") < 0) { /* new line after status output */
                    err_msg("fputs error");
                    return -1;
                }
            }
            resume = 0;
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }

        if ((pt = WSTRNSTR(msg, "cd", MAX_CMD_SZ)) != NULL) {
            WS_SFTP_FILEATRB atrb;
            int sz;
            char* f = NULL;

            pt += sizeof("cd");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';
            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            /* check directory is valid */
            do {
                ret = wolfSSH_SFTP_STAT(ssh, pt, &atrb);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (SFTP_FPUTS(args, "Error changing directory\n") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
            }

            if (ret == WS_SUCCESS) {
                sz = (int)WSTRLEN(pt);
                WFREE(workingDir, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                workingDir = (char*)WMALLOC(sz + 1, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (workingDir == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }
                WMEMCPY(workingDir, pt, sz);
                workingDir[sz] = '\0';

                clean_path(workingDir);
            }
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }

        if ((pt = WSTRNSTR(msg, "chmod", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f = NULL;
            char mode[WOLFSSH_MAX_OCTET_LEN];

            pt += sizeof("chmod");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';

            /* advance pointer to first location of non space character */
            for (i = 0; i < sz && pt[0] == ' '; i++, pt++);
            sz = (int)WSTRLEN(pt);

            /* get mode */
            sz = (sz < WOLFSSH_MAX_OCTET_LEN - 1)? sz :
                                                   WOLFSSH_MAX_OCTET_LEN -1;
            WMEMCPY(mode, pt, sz);
            mode[WOLFSSH_MAX_OCTET_LEN - 1] = '\0';
            for (i = 0; i < sz; i++) {
                if (mode[i] == ' ') {
                    mode[i] = '\0';
                    break;
                }
            }
            if (i == 0) {
                printf("error with getting mode\r\n");
                continue;
            }
            pt += (int)WSTRLEN(mode);
            sz = (int)WSTRLEN(pt);
            for (i = 0; i < sz && pt[0] == ' '; i++, pt++);

            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            /* update permissions */
            do {
                ret = wolfSSH_SFTP_CHMOD(ssh, pt, mode);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (SFTP_FPUTS(args, "Unable to change permissions of ") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
                if (SFTP_FPUTS(args, pt) < 0) {
                    err_msg("fputs error");
                    return -1;
                }
                if (SFTP_FPUTS(args, "\n") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
            }

            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }

        if ((pt = WSTRNSTR(msg, "rmdir", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f = NULL;

            pt += sizeof("rmdir");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';
            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            do {
                ret = wolfSSH_SFTP_RMDIR(ssh, pt);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (ret == WS_PERMISSIONS) {
                    if (SFTP_FPUTS(args, "Insufficient permissions\n") < 0) {
                        err_msg("fputs error");
                        return -1;
                    }
                }
                else {
                    if (SFTP_FPUTS(args, "Error writing directory\n") < 0) {
                        err_msg("fputs error");
                        return -1;
                    }
                }
            }
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }


        if ((pt = WSTRNSTR(msg, "rm", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f = NULL;

            pt += sizeof("rm");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';
            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }

            do {
                ret = wolfSSH_SFTP_Remove(ssh, pt);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (ret == WS_PERMISSIONS) {
                    if (SFTP_FPUTS(args, "Insufficient permissions\n") < 0) {
                        err_msg("fputs error");
                        return -1;
                    }
                }
                else {
                    if (SFTP_FPUTS(args, "Error writing directory\n") < 0) {
                        err_msg("fputs error");
                        return -1;
                    }
                }
            }
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;
        }

        if ((pt = WSTRNSTR(msg, "rename", MAX_CMD_SZ)) != NULL) {
            int sz;
            char* f   = NULL;
            char* fTo = NULL;
            char* to;
            int toSz;

            pt += sizeof("rename");
            sz = (int)WSTRLEN(pt);

            if (pt[sz - 1] == '\n') pt[sz - 1] = '\0';

            /* search for space delimiter */
            to = pt;
            for (i = 0; i < sz; i++) {
                to++;
                if (pt[i] == ' ') {
                    pt[i] = '\0';
                    break;
                }
            }
            if ((toSz = (int)WSTRLEN(to)) <= 0 || i == sz) {
                printf("bad usage, expected <old> <new> input\n");
                continue;
            }
            sz = (int)WSTRLEN(pt);

            if (pt[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + sz + 2;
                f = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);
                if (f == NULL) {
                    err_msg("Error malloc'ing");
                    return -1;
                }

                f[0] = '\0';
                WSTRNCAT(f, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(f, "/", maxSz);
                }
                WSTRNCAT(f, pt, maxSz);

                pt = f;
            }
            if (to[0] != '/') {
                int maxSz = (int)WSTRLEN(workingDir) + toSz + 2;
                fTo = (char*)WMALLOC(maxSz, NULL, DYNAMIC_TYPE_TMP_BUFFER);

                fTo[0] = '\0';
                WSTRNCAT(fTo, workingDir, maxSz);
                if (WSTRLEN(workingDir) > 1) {
                    WSTRNCAT(fTo, "/", maxSz);
                }
                WSTRNCAT(fTo, to, maxSz);

                to = fTo;
            }

            do {
                ret = wolfSSH_SFTP_Rename(ssh, pt, to);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && ret != WS_SUCCESS);
            if (ret != WS_SUCCESS) {
                if (SFTP_FPUTS(args, "Error with rename\n") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
            }
            WFREE(f, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            WFREE(fTo, NULL, DYNAMIC_TYPE_TMP_BUFFER);
            continue;

        }

        if ((pt = WSTRNSTR(msg, "ls", MAX_CMD_SZ)) != NULL) {
            WS_SFTPNAME* tmp;
            WS_SFTPNAME* current;

            do {
                current = wolfSSH_SFTP_LS(ssh, workingDir);
                err = wolfSSH_get_error(ssh);
            } while ((err == WS_WANT_READ || err == WS_WANT_WRITE)
                        && current == NULL && err != WS_SUCCESS);
            tmp = current;
            while (tmp != NULL) {
                if (SFTP_FPUTS(args, tmp->fName) < 0) {
                    err_msg("fputs error");
                    return -1;
                }
                if (SFTP_FPUTS(args, "\n") < 0) {
                    err_msg("fputs error");
                    return -1;
                }
                tmp = tmp->next;
            }
            wolfSSH_SFTPNAME_list_free(current);
            continue;
        }

        /* display current working directory */
        if ((pt = WSTRNSTR(msg, "pwd", MAX_CMD_SZ)) != NULL) {
            if (SFTP_FPUTS(args, workingDir) < 0 ||
                    SFTP_FPUTS(args, "\n") < 0) {
                err_msg("fputs error");
                return -1;
            }
            continue;
        }

        if (WSTRNSTR(msg, "help", MAX_CMD_SZ) != NULL) {
            ShowCommands();
            continue;
        }

        if (WSTRNSTR(msg, "quit", MAX_CMD_SZ) != NULL) {
            quit = 1;
            continue;
        }

        if (WSTRNSTR(msg, "exit", MAX_CMD_SZ) != NULL) {
            quit = 1;
            continue;
        }

        SFTP_FPUTS(args, "Unknown command\n");
    } while (!quit);

    return WS_SUCCESS;
}


/* alternate main loop for the autopilot get/receive */
static int doAutopilot(int cmd, char* local, char* remote)
{
    int err;
    int ret = WS_SUCCESS;
    char fullpath[128] = ".";
    WS_SFTPNAME* name;

    do {
        name = wolfSSH_SFTP_RealPath(ssh, fullpath);
        err = wolfSSH_get_error(ssh);
    } while ((err == WS_WANT_READ || err == WS_WANT_WRITE) &&
            ret != WS_SUCCESS);

    snprintf(fullpath, sizeof(fullpath), "%s/%s",
            name == NULL ? "." : name->fName,
            remote);

    do {
        if (cmd == AUTOPILOT_PUT) {
            ret = wolfSSH_SFTP_Put(ssh, local, fullpath, 0, NULL);
        }
        else if (cmd == AUTOPILOT_GET) {
            ret = wolfSSH_SFTP_Get(ssh, fullpath, local, 0, NULL);
        }
        err = wolfSSH_get_error(ssh);
    } while ((err == WS_WANT_READ || err == WS_WANT_WRITE) &&
            ret != WS_SUCCESS);

    if (ret != WS_SUCCESS) {
        if (cmd == AUTOPILOT_PUT) {
            fprintf(stderr, "Unable to copy local file %s to remote file %s\n",
                   local, fullpath);
        }
        else if (cmd == AUTOPILOT_GET) {
            fprintf(stderr, "Unable to copy remote file %s to local file %s\n",
                    fullpath, local);
        }
    }

    wolfSSH_SFTPNAME_list_free(name);
    return ret;
}


THREAD_RETURN WOLFSSH_THREAD sftpclient_test(void* args)
{
    WOLFSSH_CTX* ctx = NULL;
    SOCKET_T sockFd = WOLFSSH_SOCKET_INVALID;
    SOCKADDR_IN_T clientAddr;
    socklen_t clientAddrSz = sizeof(clientAddr);
    int ret;
    int ch;
    int userEcc = 0;
    /* int peerEcc = 0; */
    word16 port = wolfSshPort;
    char* host = (char*)wolfSshIp;
    const char* username = NULL;
    const char* password = NULL;
    const char* defaultSftpPath = NULL;
    byte nonBlock = 0;
    int autopilot = AUTOPILOT_OFF;
    char* apLocal = NULL;
    char* apRemote = NULL;

    int     argc = ((func_args*)args)->argc;
    char**  argv = ((func_args*)args)->argv;
    ((func_args*)args)->return_code = 0;

    while ((ch = mygetopt(argc, argv, "?d:egh:l:p:r:u:AEGNP:")) != -1) {
        switch (ch) {
            case 'd':
                defaultSftpPath = myoptarg;
                break;

            case 'e':
                userEcc = 1;
                break;

            case 'E':
                /* peerEcc = 1; */
                err_sys("wolfSFTP ECC server authentication "
                        "not yet supported.");
                break;

            case 'h':
                host = myoptarg;
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

            case 'l':
                apLocal = myoptarg;
                break;

            case 'r':
                apRemote = myoptarg;
                break;

            case 'g':
                autopilot = AUTOPILOT_PUT;
                break;

            case 'G':
                autopilot = AUTOPILOT_GET;
                break;

            case 'P':
                password = myoptarg;
                break;

            case 'N':
                nonBlock = 1;
                break;

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
    /* peerEcc = 1; */
#endif

    if (autopilot != AUTOPILOT_OFF) {
        if (apLocal == NULL || apRemote == NULL) {
            err_sys("Options -G and -g require both -l and -r.");
        }
    }

#ifdef WOLFSSH_TEST_BLOCK
    if (!nonBlock) {
        err_sys("Use -N when testing forced non blocking");
    }
#endif

    if (userEcc) {
        userPublicKeySz = (word32)sizeof(userPublicKey);
        Base64_Decode((byte*)hanselPublicEcc,
                (word32)WSTRLEN(hanselPublicEcc),
                (byte*)userPublicKey, &userPublicKeySz);

        WSTRNCPY((char*)userPublicKeyType, "ecdsa-sha2-nistp256",
                sizeof(userPublicKeyType));
        userPrivateKey = hanselPrivateEcc;
        userPrivateKeySz = hanselPrivateEccSz;
    }
    else {
        userPublicKeySz = (word32)sizeof(userPublicKey);
        Base64_Decode((byte*)hanselPublicRsa,
                (word32)WSTRLEN(hanselPublicRsa),
                (byte*)userPublicKey, &userPublicKeySz);

        WSTRNCPY((char*)userPublicKeyType, "ssh-rsa",
                sizeof(userPublicKeyType));
        userPrivateKey = hanselPrivateRsa;
        userPrivateKeySz = hanselPrivateRsaSz;
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (ctx == NULL)
        err_sys("Couldn't create wolfSSH client context.");

    if (((func_args*)args)->user_auth == NULL)
        wolfSSH_SetUserAuth(ctx, wsUserAuth);
    else
        wolfSSH_SetUserAuth(ctx, ((func_args*)args)->user_auth);

#ifndef WS_NO_SIGNAL
    /* handle interrupt with get and put */
    signal(SIGINT, sig_handler);
#endif

    ssh = wolfSSH_new(ctx);
    if (ssh == NULL)
        err_sys("Couldn't create wolfSSH session.");

    if (defaultSftpPath != NULL) {
        if (wolfSSH_SFTP_SetDefaultPath(ssh, defaultSftpPath)
                != WS_SUCCESS) {
            fprintf(stderr, "Couldn't store default sftp path.\n");
            exit(EXIT_FAILURE);
        }
    }

    if (password != NULL)
        wolfSSH_SetUserAuthCtx(ssh, (void*)password);

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

    if (!nonBlock)
        ret = wolfSSH_SFTP_connect(ssh);
    else
        ret = NonBlockSSH_connect();
    if (ret != WS_SUCCESS)
        err_sys("Couldn't connect SFTP");

    {
        /* get current working directory */
        WS_SFTPNAME* n = NULL;

        do {
            n = wolfSSH_SFTP_RealPath(ssh, (char*)".");
            ret = wolfSSH_get_error(ssh);
        } while (ret == WS_WANT_READ || ret == WS_WANT_WRITE);
        if (n == NULL) {
            err_sys("Unable to get real path for working directory");
        }

        workingDir = (char*)WMALLOC(n->fSz + 1, NULL, DYNAMIC_TYPE_TMP_BUFFER);
        if (workingDir == NULL) {
            err_sys("Unable to create working directory");
        }
        WMEMCPY(workingDir, n->fName, n->fSz);
        workingDir[n->fSz] = '\0';

        /* free after done with names */
        wolfSSH_SFTPNAME_list_free(n);
        n = NULL;
    }

    if (autopilot == AUTOPILOT_OFF) {
        ret = doCmds((func_args*)args);
    }
    else {
        ret = doAutopilot(autopilot, apLocal, apRemote);
    }

    WFREE(workingDir, NULL, DYNAMIC_TYPE_TMP_BUFFER);
    if (ret == WS_SUCCESS) {
        if (wolfSSH_shutdown(ssh) != WS_SUCCESS) {
            printf("error with wolfSSH_shutdown(), already disconnected?\n");
        }
    }
    WCLOSESOCKET(sockFd);
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
    if (ret != WS_SUCCESS) {
        printf("error %d encountered\n", ret);
        ((func_args*)args)->return_code = ret;
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

    return 0;
}

#else

THREAD_RETURN WOLFSSH_THREAD sftpclient_test(void* args)
{
    printf("Not compiled in!\nPlease recompile with WOLFSSH_SFTP or --enable-sftp");
    (void)args;
    return 0;
}
#endif /* WOLFSSH_SFTP */

#ifndef NO_MAIN_DRIVER

    int main(int argc, char** argv)
    {
        func_args args;

        args.argc = argc;
        args.argv = argv;
        args.return_code = 0;
        args.user_auth = NULL;
        #ifdef WOLFSSH_SFTP
            args.sftp_cb = NULL;
        #endif

        WSTARTTCP();

        #ifdef DEBUG_WOLFSSH
            wolfSSH_Debugging_ON();
        #endif

        wolfSSH_Init();

        ChangeToWolfSshRoot();
        sftpclient_test(&args);

        wolfSSH_Cleanup();

        return args.return_code;
    }

    int myoptind = 0;
    char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */
