/* scpclient.c
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

#include <stdio.h>
#if !defined(USE_WINDOWS_API) && !defined(MICROCHIP_PIC32)
    #include <termios.h>
#endif
#include <wolfssh/ssh.h>
#include <wolfssh/wolfscp.h>
#include <wolfssh/test.h>
#include <wolfssh/port.h>

#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    #include <wolfssl/wolfcrypt/ecc.h>
#endif
#include "examples/scpclient/scpclient.h"


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


byte userPassword[256];

static int wsUserAuth(byte authType,
                      WS_UserAuthData* authData,
                      void* ctx)
{
    const char* defaultPassword = (const char*)ctx;
    word32 passwordSz = 0;
    int ret = WOLFSSH_USERAUTH_SUCCESS;

    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
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
            fflush(stdout);
        }

        if (ret == WOLFSSH_USERAUTH_SUCCESS) {
            authData->sf.password.password = userPassword;
            authData->sf.password.passwordSz = passwordSz;
        }
    }
    else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
        ret = WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
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


#define USAGE_WIDE "12"
static void ShowUsage(void)
{
    printf("wolfscp %s\n", LIBWOLFSSH_VERSION_STRING);
    printf(" -%c %-" USAGE_WIDE "s %s\n", 'h', "",
            "display this help and exit");
    printf(" -%c %-" USAGE_WIDE "s %s, default %s\n", 'H', "<host>",
            "host to connect to", wolfSshIp);
    printf(" -%c %-" USAGE_WIDE "s %s, default %u\n", 'p', "<num>",
            "port to connect on", wolfSshPort);
    printf(" -%c %-" USAGE_WIDE "s %s\n", 'u', "<username>",
            "username to authenticate as (REQUIRED)");
    printf(" -%c %-" USAGE_WIDE "s %s\n", 'P', "<password>",
            "password for username, prompted if omitted");
    printf(" -%c %-" USAGE_WIDE "s %s\n", 'L', "<from>:<to>",
            "copy from local to server");
    printf(" -%c %-" USAGE_WIDE "s %s\n", 'S', "<from>:<to>",
            "copy from server to local");
}


enum copyDir {copyNone, copyToSrv, copyFromSrv};


THREAD_RETURN WOLFSSH_THREAD scp_client(void* args)
{
    WOLFSSH_CTX* ctx = NULL;
    WOLFSSH* ssh = NULL;
    SOCKET_T sockFd = WOLFSSH_SOCKET_INVALID;
    SOCKADDR_IN_T clientAddr;
    socklen_t clientAddrSz = sizeof(clientAddr);
    int argc = ((func_args*)args)->argc;
    int ret = 0;
    char** argv = ((func_args*)args)->argv;
    const char* username = NULL;
    const char* password = NULL;
    char* host = (char*)wolfSshIp;
    char* path1 = NULL;
    char* path2 = NULL;
    word16 port = wolfSshPort;
    byte nonBlock = 0;
    enum copyDir dir = copyNone;
    char ch;

    ((func_args*)args)->return_code = 0;

    while ((ch = mygetopt(argc, argv, "H:L:NP:S:hp:u:")) != -1) {
        switch (ch) {
            case 'H':
                host = myoptarg;
                break;

            case 'L':
                dir = copyToSrv;
                path1 = myoptarg;
                break;

            case 'N':
                nonBlock = 1;
                break;

            case 'P':
                password = myoptarg;
                break;

            case 'S':
                dir = copyFromSrv;
                path1 = myoptarg;
                break;

            case 'h':
                ShowUsage();
                exit(EXIT_SUCCESS);

            case 'u':
                username = myoptarg;
                break;

            case 'p':
                port = (word16)atoi(myoptarg);
                #if !defined(NO_MAIN_DRIVER) || defined(USE_WINDOWS_API)
                    if (port == 0)
                        err_sys("port number cannot be 0");
                #endif
                break;

            default:
                ShowUsage();
                exit(MY_EX_USAGE);
                break;
        }
    }

    myoptind = 0;      /* reset for test cases */

    if (username == NULL)
        err_sys("client requires a username parameter.");

    if (dir == copyNone)
        err_sys("didn't specify a copy direction");

    /* split file path */
    if (path1 == NULL) {
        err_sys("Missing path value");
    }

    path2 = strchr(path1, ':');
    if (path2 == NULL) {
        err_sys("Missing colon separator");
    }

    *path2 = 0;
    path2++;

    if (strlen(path1) == 0 || strlen(path2) == 0) {
        err_sys("Empty path values");
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (ctx == NULL)
        err_sys("Couldn't create wolfSSH client context.");

    if (((func_args*)args)->user_auth == NULL)
        wolfSSH_SetUserAuth(ctx, wsUserAuth);
    else
        wolfSSH_SetUserAuth(ctx, ((func_args*)args)->user_auth);

    ssh = wolfSSH_new(ctx);
    if (ssh == NULL)
        err_sys("Couldn't create wolfSSH session.");

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

    if (ret != WS_SUCCESS)
        err_sys("Couldn't set the channel type.");

    if (dir == copyFromSrv)
        ret = wolfSSH_SCP_from(ssh, path1, path2);
    else if (dir == copyToSrv)
        ret = wolfSSH_SCP_to(ssh, path1, path2);
    if (ret != WS_SUCCESS)
        err_sys("Couldn't copy the file.");

    ret = wolfSSH_shutdown(ssh);
    WCLOSESOCKET(sockFd);
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
    if (ret != WS_SUCCESS)
        err_sys("Closing scp stream failed. Connection could have been closed by peer");

#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif

    return 0;
}


#ifndef NO_MAIN_DRIVER

int main(int argc, char* argv[])
{
    func_args args;

    args.argc = argc;
    args.argv = argv;
    args.return_code = 0;
    args.user_auth = NULL;

    #ifdef DEBUG_WOLFSSH
        wolfSSH_Debugging_ON();
    #endif

    wolfSSH_Init();

    scp_client(&args);

    wolfSSH_Cleanup();

    return args.return_code;
}


int myoptind = 0;
char* myoptarg = NULL;

#endif /* NO_MAIN_DRIVER */
