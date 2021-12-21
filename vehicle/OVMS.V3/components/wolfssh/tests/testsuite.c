/* testsuite.c
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
#define WOLFSSH_TEST_SERVER
#define WOLFSSH_TEST_THREADING
#define WOLFSSH_TEST_LOCKING


#include <stdio.h>

#ifdef WOLFSSL_USER_SETTINGS
    #include <wolfssl/wolfcrypt/settings.h>
#else
    #include <wolfssl/options.h>
#endif

#include <wolfssh/settings.h>
#include <wolfssh/ssh.h>
#include <wolfssh/test.h>
#include "examples/echoserver/echoserver.h"
#include "examples/client/client.h"
#include "tests/testsuite.h"

#ifndef NO_TESTSUITE_MAIN_DRIVER

static int TestsuiteTest(int argc, char** argv);

int main(int argc, char** argv)
{
    return TestsuiteTest(argc, argv);
}


int myoptind = 0;
char* myoptarg = NULL;

#endif /* NO_TESTSUITE_MAIN_DRIVER */


#if !defined(NO_WOLFSSH_SERVER) && !defined(NO_WOLFSSH_CLIENT)

static int tsClientUserAuth(byte authType, WS_UserAuthData* authData, void* ctx)
{
    static char password[] = "upthehill";

    (void)authType;
    (void)ctx;

    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
        authData->sf.password.password = (byte*)password;
        authData->sf.password.passwordSz = (word32)WSTRLEN(password);
    }
    else {
        return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
    }

    return WOLFSSH_USERAUTH_SUCCESS;
}


#define NUMARGS 5
#define ARGLEN 32

int TestsuiteTest(int argc, char** argv)
{
    tcp_ready ready;
    THREAD_TYPE serverThread;
    func_args serverArgs;
    func_args clientArgs;
    char sA[NUMARGS][ARGLEN];
    char *serverArgv[NUMARGS] =
        { sA[0], sA[1], sA[2], sA[3], sA[4] };
    char cA[NUMARGS][ARGLEN];
    char *clientArgv[NUMARGS] =
        { cA[0], cA[1], cA[2], cA[3], cA[4] };
    int serverArgc = 0;
    int clientArgc = 0;

    (void)argc;
    (void)argv;

    WSTARTTCP();

    wolfSSH_Init();
    #if defined(DEBUG_WOLFSSH)
        wolfSSH_Debugging_ON();
    #endif
    #if !defined(WOLFSSL_TIRTOS)
        ChangeToWolfSshRoot();
    #endif

    InitTcpReady(&ready);

    WSTRNCPY(serverArgv[serverArgc++], "echoserver", ARGLEN);
    WSTRNCPY(serverArgv[serverArgc++], "-1", ARGLEN);
    WSTRNCPY(serverArgv[serverArgc++], "-f", ARGLEN);
    #ifndef USE_WINDOWS_API
        WSTRNCPY(serverArgv[serverArgc++], "-p", ARGLEN);
        WSTRNCPY(serverArgv[serverArgc++], "-0", ARGLEN);
    #endif

    serverArgs.argc = serverArgc;
    serverArgs.argv = serverArgv;
    serverArgs.return_code = EXIT_SUCCESS;
    serverArgs.signal = &ready;
    serverArgs.user_auth = NULL;
    ThreadStart(echoserver_test, &serverArgs, &serverThread);
    WaitTcpReady(&serverArgs);

    WSTRNCPY(cA[clientArgc++], "client", ARGLEN);
    WSTRNCPY(cA[clientArgc++], "-u", ARGLEN);
    WSTRNCPY(cA[clientArgc++], "jill", ARGLEN);
    #ifndef USE_WINDOWS_API
        WSTRNCPY(cA[clientArgc++], "-p", ARGLEN);
        WSNPRINTF(cA[clientArgc++], ARGLEN, "%d", ready.port);
    #endif

    clientArgs.argc = clientArgc;
    clientArgs.argv = clientArgv;
    clientArgs.return_code = EXIT_SUCCESS;
    clientArgs.signal = &ready;
    clientArgs.user_auth = tsClientUserAuth;

    client_test(&clientArgs);
    if (clientArgs.return_code != 0) {
        return clientArgs.return_code;
    }

    ThreadJoin(serverThread);

    wolfSSH_Cleanup();
    FreeTcpReady(&ready);

#ifdef WOLFSSH_SFTP
    printf("testing SFTP blocking\n");
    test_SFTP(0);
    printf("testing SFTP non blocking\n");
    test_SFTP(1);
#endif

    return EXIT_SUCCESS;
}

#else /* !NO_WOLFSSH_SERVER && !NO_WOLFSSH_CLIENT */

int TestsuiteTest(int argc, char** argv)
{
    (void)argc;
    (void)argv;
    return EXIT_SUCCESS;
}

#endif /* !NO_WOLFSSH_SERVER && !NO_WOLFSSH_CLIENT */


