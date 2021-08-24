/* test.h
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


#ifndef _WOLFSSH_TEST_H_
#define _WOLFSSH_TEST_H_

#ifndef NO_STDIO_FILESYSTEM
#include <stdio.h>
/*#include <stdlib.h>*/
#include <ctype.h>
/*#include <wolfssh/error.h>*/
#endif

#ifdef USE_WINDOWS_API
    #ifndef _WIN32_WCE
        #include <process.h>
    #endif
    #include <assert.h>
    #ifdef TEST_IPV6            /* don't require newer SDK for IPV4 */
        #include <ws2tcpip.h>
        #include <wspiapi.h>
    #endif
    #define SOCKET_T SOCKET
    #define NUM_SOCKETS 5
#elif defined(WOLFSSL_VXWORKS)
    #include <unistd.h>
    #include <hostLib.h>
    #include <sockLib.h>
    #include <arpa/inet.h>
    #include <string.h>
    #include <selectLib.h>
    #include <sys/types.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <ipcom_sock.h>
    #include <fcntl.h>
    #include <sys/time.h>
    #include <netdb.h>
    #include <pthread.h>
    #define SOCKET_T int

    /*#define SINGLE_THREADED*/

    #ifndef STDIN_FILENO
    #define STDIN_FILENO   0
    #endif
    #ifndef STDOUT_FILENO
    #define STDOUT_FILENO   1
    #endif
    #ifndef STDERR_FILENO
    #define STDERR_FILENO   2
    #endif
    #define NUM_SOCKETS 2
#elif defined(MICROCHIP_MPLAB_HARMONY) || defined(MICROCHIP_TCPIP)
    #include "tcpip/tcpip.h"
    #include <signal.h>
    #ifdef MICROCHIP_MPLAB_HARMONY
        #ifndef htons
            #define htons TCPIP_Helper_htons
        #endif
        #define SOCKET_T TCP_SOCKET
        #define XNTOHS TCPIP_Helper_ntohs
    #endif
    #define socklen_t int
    #define NUM_SOCKETS 1
#elif defined(WOLFSSL_NUCLEUS)
    #include "nucleus.h"
    #include "networking/nu_networking.h"
    #define SOCKET_T int
    #define socklen_t int
    #define NUM_SOCKETS 1
    #define INADDR_ANY IP_ADDR_ANY
    #define AF_INET NU_FAMILY_IP
    #define SOCK_STREAM NU_TYPE_STREAM

    #define sin_addr id
    #define s_addr is_ip_addrs

    #define sin_family family
    #define sin_port port
#elif defined(FREESCALE_MQX)
    #ifndef SO_NOSIGPIPE
        #include <signal.h>  /* ignore SIGPIPE */
    #endif

    #define NUM_SOCKETS 5
#else /* USE_WINDOWS_API */
    #include <unistd.h>
    #include <netdb.h>
    #include <netinet/in.h>
    #include <netinet/tcp.h>
    #include <arpa/inet.h>
    #include <sys/ioctl.h>
    #include <sys/socket.h>
    #include <pthread.h>
    #include <fcntl.h>
    #ifndef SO_NOSIGPIPE
        #include <signal.h>  /* ignore SIGPIPE */
    #endif
    #define SOCKET_T int
    #define NUM_SOCKETS 5
#endif /* USE_WINDOWS_API */


/* Socket Handling */
#ifndef WOLFSSH_SOCKET_INVALID
#if defined(USE_WINDOWS_API) || defined(MICROCHIP_MPLAB_HARMONY)
    #define WOLFSSH_SOCKET_INVALID  ((SOCKET_T)INVALID_SOCKET)
#elif defined(WOLFSSH_TIRTOS)
    #define WOLFSSH_SOCKET_INVALID  ((SOCKET_T)-1)
#else
    #define WOLFSSH_SOCKET_INVALID  (SOCKET_T)(0)
#endif
#endif /* WOLFSSH_SOCKET_INVALID */

#ifndef WOLFSSL_SOCKET_IS_INVALID
#if defined(USE_WINDOWS_API) || defined(WOLFSSL_TIRTOS)
    #define WOLFSSL_SOCKET_IS_INVALID(s)  ((SOCKET_T)(s) == WOLFSSL_SOCKET_INVALID)
#else
    #define WOLFSSL_SOCKET_IS_INVALID(s)  ((SOCKET_T)(s) < WOLFSSL_SOCKET_INVALID)
#endif
#endif /* WOLFSSL_SOCKET_IS_INVALID */


#if defined(__MACH__) || defined(USE_WINDOWS_API) || defined(FREESCALE_MQX)
    #ifndef _SOCKLEN_T
        typedef int socklen_t;
    #endif
#endif


#ifdef USE_WINDOWS_API
    #define WCLOSESOCKET(s) closesocket(s)
    #define WSTARTTCP() do { WSADATA wsd; WSAStartup(0x0002, &wsd); } while(0)
#elif defined(MICROCHIP_TCPIP) || defined(MICROCHIP_MPLAB_HARMONY)
    #ifdef MICROCHIP_MPLAB_HARMONY
        #define WCLOSESOCKET(s) TCPIP_TCP_Close((s))
    #else
        #define WCLOSESOCKET(s) closesocket((s))
    #endif
    #define WSTARTTCP()
#elif defined(WOLFSSL_NUCLEUS)
    #define WCLOSESOCKET(s) NU_Close_Socket((s))
    #define WSTARTTCP()
#else
    #define WCLOSESOCKET(s) close(s)
    #define WSTARTTCP()
#endif


#ifdef SINGLE_THREADED
    typedef unsigned int  THREAD_RETURN;
    typedef void*         THREAD_TYPE;
    #define WOLFSSH_THREAD
#else
    #if defined(_POSIX_THREADS) && !defined(__MINGW32__)
        typedef void*         THREAD_RETURN;
        typedef pthread_t     THREAD_TYPE;
        #define WOLFSSH_THREAD
        #define INFINITE -1
        #define WAIT_OBJECT_0 0L
    #elif defined(WOLFSSL_NUCLEUS) || defined(FREESCALE_MQX)
        typedef unsigned int  THREAD_RETURN;
        typedef intptr_t      THREAD_TYPE;
        #define WOLFSSH_THREAD
    #else
        typedef unsigned int  THREAD_RETURN;
        typedef intptr_t      THREAD_TYPE;
        #define WOLFSSH_THREAD __stdcall
    #endif
#endif

#ifdef TEST_IPV6
    typedef struct sockaddr_in6 SOCKADDR_IN_T;
    #define AF_INET_V AF_INET6
#else
    #ifndef WOLFSSL_NUCLEUS
        typedef struct sockaddr_in SOCKADDR_IN_T;
    #endif
    #define AF_INET_V AF_INET
#endif


#define serverKeyRsaPemFile "./keys/server-key-rsa.pem"


#ifndef TEST_IPV6
    static const char* const wolfSshIp = "127.0.0.1";
#else /* TEST_IPV6 */
    static const char* const wolfSshIp = "::1";
#endif /* TEST_IPV6 */

#ifdef WOLFSSL_NUCLEUS
    /* port 8080 was open with QEMU */
    static const word16 wolfSshPort = 8080;
#else
    static const word16 wolfSshPort = 22222;
#endif

#ifdef __GNUC__
    #define WS_NORETURN __attribute__((noreturn))
#else
    #define WS_NORETURN
#endif

#ifdef WOLFSSL_VXWORKS
static INLINE void err_sys(const char* msg)
{
    printf("wolfSSH error: %s\n", msg);
    return;
}
#else
static INLINE WS_NORETURN void err_sys(const char* msg)
{
    printf("wolfSSH error: %s\n", msg);

#ifndef __GNUC__
    /* scan-build (which pretends to be gnuc) can get confused and think the
     * msg pointer can be null even when hardcoded and then it won't exit,
     * making null pointer checks above the err_sys() call useless.
     * We could just always exit() but some compilers will complain about no
     * possible return, with gcc we know the attribute to handle that with
     * WS_NORETURN. */
    if (msg)
#endif
    {
        exit(EXIT_FAILURE);
    }
}
#endif

#define MY_EX_USAGE 2

extern int   myoptind;
extern char* myoptarg;

static INLINE int mygetopt(int argc, char** argv, const char* optstring)
{
    static char* next = NULL;

    char  c;
    char* cp;

    if (myoptind == 0)
        next = NULL;   /* we're starting new/over */

    if (next == NULL || *next == '\0') {
        if (myoptind == 0)
            myoptind++;

        if (myoptind >= argc || argv[myoptind][0] != '-' ||
                                argv[myoptind][1] == '\0') {
            myoptarg = NULL;
            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        if (strcmp(argv[myoptind], "--") == 0) {
            myoptind++;
            myoptarg = NULL;

            if (myoptind < argc)
                myoptarg = argv[myoptind];

            return -1;
        }

        next = argv[myoptind];
        next++;                  /* skip - */
        myoptind++;
    }

    c  = *next++;
    /* The C++ strchr can return a different value */
    cp = (char*)strchr(optstring, c);

    if (cp == NULL || c == ':')
        return '?';

    cp++;

    if (*cp == ':') {
        if (*next != '\0') {
            myoptarg = next;
            next     = NULL;
        }
        else if (myoptind < argc) {
            myoptarg = argv[myoptind];
            myoptind++;
        }
        else
            return '?';
    }

    return c;
}


#ifdef USE_WINDOWS_API
    #pragma warning(push)
    #pragma warning(disable:4996)
    /* For Windows builds, disable compiler warnings for:
    * - 4996: deprecated function */
#endif

#if (defined(WOLFSSH_TEST_CLIENT) || defined(WOLFSSH_TEST_SERVER)) && !defined(FREESCALE_MQX)

#ifdef WOLFSSL_NUCLEUS
static INLINE void build_addr(struct addr_struct* addr, const char* peer,
                              word16 port)
{
    int useLookup = 0;
    (void)useLookup;

    memset(addr, 0, sizeof(struct addr_struct));

#ifndef TEST_IPV6
    /* peer could be in human readable form */
    if ( ((size_t)peer != INADDR_ANY) && isalpha((int)peer[0])) {

        NU_HOSTENT* entry;
        NU_HOSTENT h;
        entry = &h;
        NU_Get_Host_By_Name((char*)peer, entry);

        if (entry) {
            memcpy(&addr->id.is_ip_addrs, entry->h_addr_list[0],
                entry->h_length);
            useLookup = 1;
        }
        else
            err_sys("no entry for host");
    }
#endif

#ifndef TEST_IPV6
    addr->family = NU_FAMILY_IP;
    addr->port = port;

    /* @TODO always setting any ip addr here */
    PUT32(addr->id.is_ip_addrs, 0, IP_ADDR_ANY);
#endif
}
#else
static INLINE void build_addr(SOCKADDR_IN_T* addr, const char* peer,
                              word16 port)
{
    int useLookup = 0;
    (void)useLookup;

    memset(addr, 0, sizeof(SOCKADDR_IN_T));

#ifndef TEST_IPV6
    /* peer could be in human readable form */
    if ( ((size_t)peer != INADDR_ANY) && isalpha((int)peer[0])) {
        #ifdef CYASSL_MDK_ARM
            int err;
            struct hostent* entry = gethostbyname(peer, &err);
        #elif defined(MICROCHIP_MPLAB_HARMONY)
            struct hostent* entry = gethostbyname((char*)peer);
        #elif defined(WOLFSSL_NUCLEUS)
            NU_HOSTENT* entry;
            NU_HOSTENT h;
            entry = &h;
            NU_Get_Host_By_Name((char*)peer, entry);
        #else
            struct hostent* entry = gethostbyname(peer);
        #endif

        if (entry) {
#ifdef WOLFSSL_NUCLEUS
        memcpy(&addr->id.is_ip_addrs, entry->h_addr_list[0],
                entry->h_length);
#else
            memcpy(&addr->sin_addr.s_addr, entry->h_addr_list[0],
                   entry->h_length);
#endif
            useLookup = 1;
        }
        else
            err_sys("no entry for host");
    }
#endif

#ifndef TEST_IPV6
    #if defined(CYASSL_MDK_ARM)
        addr->sin_family = PF_INET;
    #else
        addr->sin_family = AF_INET_V;
    #endif
    addr->sin_port = htons(port);
    if ((size_t)peer == INADDR_ANY)
#ifdef WOLFSSL_NUCLEUS
        PUT32(addr->id.is_ip_addrs, 0, INADDR_ANY);
#else
        addr->sin_addr.s_addr = INADDR_ANY;
#endif
    else {
        if (!useLookup) {
    #ifdef MICROCHIP_MPLAB_HARMONY
            IPV4_ADDR ip4;
            TCPIP_Helper_StringToIPAddress(peer, &ip4);
            addr->sin_addr.s_addr = ip4.Val;
    #else
            addr->sin_addr.s_addr = inet_addr(peer);
    #endif
        }
    }
#else
    addr->sin6_family = AF_INET_V;
    addr->sin6_port = htons(port);
    if ((size_t)peer == INADDR_ANY)
        addr->sin6_addr = in6addr_any;
    else {
        #ifdef HAVE_GETADDRINFO
            struct addrinfo  hints;
            struct addrinfo* answer = NULL;
            int    ret;
            char   strPort[80];

            memset(&hints, 0, sizeof(hints));

            hints.ai_family   = AF_INET_V;
            hints.ai_socktype = udp ? SOCK_DGRAM : SOCK_STREAM;
            hints.ai_protocol = udp ? IPPROTO_UDP : IPPROTO_TCP;

            WSNPRINTF(strPort, sizeof(strPort), "%d", port);
            strPort[79] = '\0';

            ret = getaddrinfo(peer, strPort, &hints, &answer);
            if (ret < 0 || answer == NULL)
                err_sys("getaddrinfo failed");

            memcpy(addr, answer->ai_addr, answer->ai_addrlen);
            freeaddrinfo(answer);
        #else
            printf("no ipv6 getaddrinfo, loopback only tests/examples\n");
            addr->sin6_addr = in6addr_loopback;
        #endif
    }
#endif
}
#endif /* WOLFSSH_NUCLEUS */

#ifdef USE_WINDOWS_API
    #pragma warning(pop)
#endif


static INLINE void tcp_socket(WS_SOCKET_T* sockFd)
{
#ifdef MICROCHIP_MPLAB_HARMONY
    /* creates socket in listen or connect */
    *sockFd = 0;
#elif defined(MICROCHIP_TCPIP)
    *sockFd = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
#elif defined(WOLFSSL_NUCLEUS)
    *sockFd = NU_Socket(NU_FAMILY_IP, NU_TYPE_STREAM, 0);
#else
    *sockFd = socket(AF_INET_V, SOCK_STREAM, 0);
#endif

#ifdef USE_WINDOWS_API
    if (*sockFd == INVALID_SOCKET)
        err_sys("socket failed\n");
#else
    if (*sockFd < 0)
        err_sys("socket failed\n");
#endif

#ifndef USE_WINDOWS_API
#ifdef SO_NOSIGPIPE
    {
        int       on = 1;
        socklen_t len = sizeof(on);
        int       res = setsockopt(*sockFd, SOL_SOCKET, SO_NOSIGPIPE, &on, len);
        if (res < 0)
            err_sys("setsockopt SO_NOSIGPIPE failed\n");
    }
#elif defined(CYASSL_MDK_ARM)
    /* nothing to define */
#elif defined(MICROCHIP_MPLAB_HARMONY) && !defined(_FULL_SIGNAL_IMPLEMENTATION)
    /* not full signal implementation */
#elif defined(WOLFSSL_NUCLEUS)
    /* nothing to define */
#else  /* no S_NOSIGPIPE */
    signal(SIGPIPE, SIG_IGN);
#endif /* S_NOSIGPIPE */

#if defined(TCP_NODELAY)
    {
    #ifdef MICROCHIP_MPLAB_HARMONY
        if (!TCPIP_TCP_OptionsSet(*sockFd, TCP_OPTION_NODELAY, (void*)1)) {
            err_sys("setsockopt TCP_NODELAY failed\n");
        }
    #elif defined(WOLFSSL_NUCLEUS)
    #else
        int       on = 1;
        socklen_t len = sizeof(on);
        int       res = setsockopt(*sockFd, IPPROTO_TCP, TCP_NODELAY, &on, len);
        if (res < 0)
            err_sys("setsockopt TCP_NODELAY failed\n");
    #endif
    }
#endif
#endif  /* USE_WINDOWS_API */
}

#endif /* WOLFSSH_TEST_CLIENT || WOLFSSH_TEST_SERVER */


#ifndef XNTOHS
    #define XNTOHS(a) ntohs((a))
#endif


#if defined(WOLFSSH_TEST_SERVER) && !defined(FREESCALE_MQX)

static INLINE void tcp_listen(WS_SOCKET_T* sockfd, word16* port, int useAnyAddr)
{
#ifdef MICROCHIP_MPLAB_HARMONY
    /* does bind and listen and returns the socket */
    *sockfd = TCPIP_TCP_ServerOpen(IP_ADDRESS_TYPE_IPV4, *port, 0);
    return;
#else
    #ifdef WOLFSSL_NUCLEUS
        struct addr_struct addr;
    #else
        SOCKADDR_IN_T addr;
    #endif
    /* don't use INADDR_ANY by default, firewall may block, make user switch
       on */
    build_addr(&addr, (useAnyAddr ? INADDR_ANY : wolfSshIp), *port);
    tcp_socket(sockfd);

#if !defined(USE_WINDOWS_API) && !defined(WOLFSSL_MDK_ARM)\
                              && !defined(WOLFSSL_KEIL_TCP_NET)\
                              && !defined(WOLFSSL_NUCLEUS)
    {
        int res;
    #ifdef MICROCHIP_TCPIP
        const byte on = 1; /* account for function signature */
    #else
        int on  = 1;
    #endif
        socklen_t len = sizeof(on);
        res = setsockopt(*sockfd, SOL_SOCKET, SO_REUSEADDR, &on, len);
        if (res < 0)
            err_sys("setsockopt SO_REUSEADDR failed\n");
    }
#endif

#ifdef WOLFSSL_NUCLEUS
    /* any NU_Bind return greater than or equal to 0 is a success */
    if (NU_Bind(*sockfd, &addr, sizeof(addr)) < 0)
        err_sys("tcp bind failed");
    if (NU_Listen(*sockfd, NUM_SOCKETS) != NU_SUCCESS)
        err_sys("tcp listen failed");
#else
    if (bind(*sockfd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
        err_sys("tcp bind failed");
    if (listen(*sockfd, NUM_SOCKETS) != 0)
        err_sys("tcp listen failed");
#endif

    #if !defined(USE_WINDOWS_API) && !defined(WOLFSSL_TIRTOS) && !defined(WOLFSSL_NUCLEUS)
        if (*port == 0) {
            socklen_t len = sizeof(addr);
            if (getsockname(*sockfd, (struct sockaddr*)&addr, &len) == 0) {
                #ifndef TEST_IPV6
                    *port = XNTOHS(addr.sin_port);
                #else
                    *port = XNTOHS(addr.sin6_port);
                #endif
            }
        }
    #endif
#endif /* MICROCHIP_MPLAB_HARMONY */
}

#endif /* WOLFSSH_TEST_SERVER */

enum {
    WS_SELECT_FAIL,
    WS_SELECT_TIMEOUT,
    WS_SELECT_RECV_READY,
    WS_SELECT_ERROR_READY
};

#if (defined(WOLFSSH_TEST_SERVER) || defined(WOLFSSH_TEST_CLIENT)) && !defined(FREESCALE_MQX)

static INLINE void tcp_set_nonblocking(WS_SOCKET_T* sockfd)
{
    #ifdef USE_WINDOWS_API
        unsigned long blocking = 1;
        int ret = ioctlsocket(*sockfd, FIONBIO, &blocking);
        if (ret == SOCKET_ERROR)
            err_sys("ioctlsocket failed");
    #elif defined(WOLFSSL_MDK_ARM) || defined(WOLFSSL_KEIL_TCP_NET) \
        || defined (WOLFSSL_TIRTOS)|| defined(WOLFSSL_VXWORKS) || \
        defined(WOLFSSL_NUCLEUS)
         /* non blocking not supported, for now */
    #else
        int flags = fcntl(*sockfd, F_GETFL, 0);
        if (flags < 0)
            err_sys("fcntl get failed");
        flags = fcntl(*sockfd, F_SETFL, flags | O_NONBLOCK);
        if (flags < 0)
            err_sys("fcntl set failed");
    #endif
}


#ifdef WOLFSSL_NUCLEUS
	#define WFD_SET_TYPE FD_SET
	#define WFD_SET NU_FD_Set
	#define WFD_ZERO NU_FD_Init
	#define WFD_ISSET NU_FD_Check
#else
	#define WFD_SET_TYPE fd_set
	#define WFD_SET FD_SET
	#define WFD_ZERO FD_ZERO
	#define WFD_ISSET FD_ISSET
#endif

/* returns 1 or greater when something is ready to be read */
static INLINE int wSelect(int nfds, WFD_SET_TYPE* recvfds,
        WFD_SET_TYPE *writefds, WFD_SET_TYPE *errfds,  struct timeval* timeout)
{
#ifdef WOLFSSL_NUCLEUS
    int ret = NU_Select (nfds, recvfds,  writefds, errfds,
            (UNSIGNED)timeout->tv_sec);
    if (ret == NU_SUCCESS) {
        return 1;
    }
    return 0;
#else
    return select(nfds, recvfds, writefds, errfds, timeout);
#endif
}

static INLINE int tcp_select(SOCKET_T socketfd, int to_sec)
{
    WFD_SET_TYPE recvfds, errfds;
    int nfds = (int)socketfd + 1;
    struct timeval timeout = {(to_sec > 0) ? to_sec : 0, 0};
    int result;

    WFD_ZERO(&recvfds);
    WFD_SET(socketfd, &recvfds);
    WFD_ZERO(&errfds);
    WFD_SET(socketfd, &errfds);

    result = wSelect(nfds, &recvfds, NULL, &errfds, &timeout);

    if (result == 0)
        return WS_SELECT_TIMEOUT;
    else if (result > 0) {
        if (WFD_ISSET(socketfd, &recvfds))
            return WS_SELECT_RECV_READY;
        else if(WFD_ISSET(socketfd, &errfds))
            return WS_SELECT_ERROR_READY;
    }

    return WS_SELECT_FAIL;
}

#endif /* WOLFSSH_TEST_SERVER || WOLFSSH_TEST_CLIENT */


/* Wolf Root Directory Helper */
/* KEIL-RL File System does not support relative directory */
#if !defined(WOLFSSL_MDK_ARM) && !defined(WOLFSSL_KEIL_FS) && !defined(WOLFSSL_TIRTOS) \
    && !defined(NO_WOLFSSL_DIR) && !defined(WOLFSSL_NUCLEUS)
    /* Maximum depth to search for WolfSSL root */
    #define MAX_WOLF_ROOT_DEPTH 5

    static INLINE int ChangeToWolfSshRoot(void)
    {
        #if !defined(NO_FILESYSTEM)
            int depth, res;
            WFILE* file;
            for(depth = 0; depth <= MAX_WOLF_ROOT_DEPTH; depth++) {
                if (WFOPEN(&file, serverKeyRsaPemFile, "rb") == 0) {
                    WFCLOSE(file);
                    return depth;
                }
            #ifdef USE_WINDOWS_API
                res = SetCurrentDirectoryA("..\\");
            #else
                res = chdir("../");
            #endif
                if (res < 0) {
                    printf("chdir to ../ failed!\n");
                    break;
                }
            }

            err_sys("wolfSSH root not found");
            return -1;
        #else
            return 0;
        #endif
    }
#endif /* !defined(WOLFSSL_MDK_ARM) && !defined(WOLFSSL_KEIL_FS) && !defined(WOL
FSSL_TIRTOS) */


typedef struct tcp_ready {
    word16 ready;     /* predicate */
    word16 port;
    char* srfName;     /* server ready file name */
#if defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_mutex_t mutex;
    pthread_cond_t  cond;
#endif
} tcp_ready;


#ifdef WOLFSSH_SFTP
typedef int (*WS_CallbackSftpCommand)(const char* in, char* out, int outSz);
#endif

typedef struct func_args {
    int    argc;
    char** argv;
    int    return_code;
    tcp_ready* signal;
    WS_CallbackUserAuth user_auth;
#ifdef WOLFSSH_SFTP
    /* callback for example sftp client commands instead of WFGETS */
    WS_CallbackSftpCommand sftp_cb;
#endif
} func_args;


#ifdef WOLFSSH_TEST_LOCKING

static INLINE void InitTcpReady(tcp_ready* ready)
{
    ready->ready = 0;
    ready->port = 0;
    ready->srfName = NULL;
#ifdef SINGLE_THREADED
#elif defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_mutex_init(&ready->mutex, 0);
    pthread_cond_init(&ready->cond, 0);
#endif
}


static INLINE void FreeTcpReady(tcp_ready* ready)
{
#ifdef SINGLE_THREADED
    (void)ready;
#elif defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_mutex_destroy(&ready->mutex);
    pthread_cond_destroy(&ready->cond);
#else
    (void)ready;
#endif
}


static INLINE void WaitTcpReady(func_args* args)
{
#if defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_mutex_lock(&args->signal->mutex);

    if (!args->signal->ready)
        pthread_cond_wait(&args->signal->cond, &args->signal->mutex);
    args->signal->ready = 0; /* reset */

    pthread_mutex_unlock(&args->signal->mutex);
#else
    (void)args;
#endif
}


#endif /* WOLFSSH_TEST_LOCKING */


#ifdef WOLFSSH_TEST_THREADING

typedef THREAD_RETURN WOLFSSH_THREAD THREAD_FUNC(void*);


static INLINE void ThreadStart(THREAD_FUNC fun, void* args, THREAD_TYPE* thread)
{
#ifdef SINGLE_THREADED
    (void)fun;
    (void)args;
    (void)thread;
#elif defined(_POSIX_THREADS) && !defined(__MINGW32__)
    #ifdef WOLFSSL_VXWORKS
    {
        pthread_attr_t myattr;
        pthread_attr_init(&myattr);
        pthread_attr_setstacksize(&myattr, 0x10000);
        pthread_create(thread, &myattr, fun, args);
    }
    #else
        pthread_create(thread, 0, fun, args);
    #endif
    return;
#elif defined(WOLFSSL_TIRTOS)
    /* Initialize the defaults and set the parameters. */
    Task_Params taskParams;
    Task_Params_init(&taskParams);
    taskParams.arg0 = (UArg)args;
    taskParams.stackSize = 65535;
    *thread = Task_create((Task_FuncPtr)fun, &taskParams, NULL);
    if (*thread == NULL) {
        printf("Failed to create new Task\n");
    }
    Task_yield();
#else
    *thread = (THREAD_TYPE)_beginthreadex(0, 0, fun, args, 0, 0);
#endif
}


static INLINE void ThreadJoin(THREAD_TYPE thread)
{
#ifdef SINGLE_THREADED
    (void)thread;
#elif defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_join(thread, 0);
#elif defined(WOLFSSL_TIRTOS)
    while(1) {
        if (Task_getMode(thread) == Task_Mode_TERMINATED) {
            Task_sleep(5);
            break;
        }
        Task_yield();
    }
#else
    int res = WaitForSingleObject((HANDLE)thread, INFINITE);
    assert(res == WAIT_OBJECT_0);
    res = CloseHandle((HANDLE)thread);
    assert(res);
    (void)res; /* Suppress un-used variable warning */
#endif
}


static INLINE void ThreadDetach(THREAD_TYPE thread)
{
#ifdef SINGLE_THREADED
    (void)thread;
#elif defined(_POSIX_THREADS) && !defined(__MINGW32__)
    pthread_detach(thread);
#elif defined(WOLFSSL_TIRTOS)
#if 0
    while(1) {
        if (Task_getMode(thread) == Task_Mode_TERMINATED) {
            Task_sleep(5);
            break;
        }
        Task_yield();
    }
#endif
#else
    int res = CloseHandle((HANDLE)thread);
    assert(res);
    (void)res; /* Suppress un-used variable warning */
#endif
}

#endif /* WOLFSSH_TEST_THREADING */

#endif /* _WOLFSSH_TEST_H_ */

