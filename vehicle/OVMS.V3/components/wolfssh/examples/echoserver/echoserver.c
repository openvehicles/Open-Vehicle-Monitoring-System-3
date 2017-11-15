/* echoserver.c
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


#include <ctype.h>
#include <string.h>
#include <unistd.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <arpa/inet.h>
#include <sys/ioctl.h>
#include <sys/time.h>
#include <sys/socket.h>
#include <stdio.h>
#include <pthread.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssh/ssh.h>
#ifndef SO_NOSIGPIPE
    #include <signal.h>
#endif


static const char echoserverBanner[] = "wolfSSH Example Echo Server\n";

typedef int SOCKET_T;
#ifdef TEST_IPV6
    typedef struct sockaddr_in6 SOCKADDR_IN_T;
    #define AF_INET_V           AF_INET6
    static const char*          wolfsshIP = "::1";
#else
    typedef struct sockaddr_in  SOCKADDR_IN_T;
    #define AF_INET_V           AF_INET
    static const char*          wolfsshIP = "127.0.0.1";
#endif
#define SERVER_PORT_NUMBER      22222
#define SCRATCH_BUFFER_SIZE     1200

#if defined(__MACH__) || defined(USE_WINDOWS_API)
    #ifndef _SOCKLEN_T
        typedef int socklen_t;
    #endif
#endif
/* HPUX doesn't use socklent_t for third parameter to accept, unless
   _XOPEN_SOURCE_EXTENDED is defined */
#if !defined(__hpux__) && !defined(CYASSL_MDK_ARM) && !defined(CYASSL_IAR_ARM)
    typedef socklen_t SOCKLEN_T;
#else
    #if defined _XOPEN_SOURCE_EXTENDED
        typedef socklen_t SOCKLEN_T;
    #else
        typedef int       SOCKLEN_T;
    #endif
#endif


#if defined(_POSIX_THREADS) && !defined(__MINGW32__)
    typedef void*         THREAD_RETURN;
    typedef pthread_t     THREAD_TYPE;
    #define CYASSL_THREAD
    #define INFINITE -1
    #define WAIT_OBJECT_0 0L
#elif defined(CYASSL_MDK_ARM)
    typedef unsigned int  THREAD_RETURN;
    typedef int           THREAD_TYPE;
    #define CYASSL_THREAD
#else
    typedef unsigned int  THREAD_RETURN;
    typedef intptr_t      THREAD_TYPE;
    #define CYASSL_THREAD __stdcall
#endif

typedef struct {
    WOLFSSH* ssh;
    SOCKET_T fd;
    uint32_t id;
} thread_ctx_t;


#ifndef EXAMPLE_HIGHWATER_MARK
    #define EXAMPLE_HIGHWATER_MARK 0x3FFF8000 /* 1GB - 32kB */
#endif
#ifndef EXAMPLE_BUFFER_SZ
    #define EXAMPLE_BUFFER_SZ 4096
#endif


#ifdef __GNUC__
    #define WS_NORETURN __attribute__((noreturn))
#else
    #define WS_NORETURN
#endif


static INLINE WS_NORETURN void err_sys(const char* msg)
{
    printf("server error: %s\n", msg);

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


static INLINE void build_addr(SOCKADDR_IN_T* addr, const char* peer,
                              uint16_t port)
{
    int useLookup = 0;
    (void)useLookup;

    memset(addr, 0, sizeof(SOCKADDR_IN_T));

#ifndef TEST_IPV6
    /* peer could be in human readable form */
    if ( (peer != INADDR_ANY) && isalpha((int)peer[0])) {
        #ifdef CYASSL_MDK_ARM
            int err;
            struct hostent* entry = gethostbyname(peer, &err);
        #else
            struct hostent* entry = gethostbyname(peer);
        #endif

        if (entry) {
            memcpy(&addr->sin_addr.s_addr, entry->h_addr_list[0],
                   entry->h_length);
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
    if (peer == INADDR_ANY)
        addr->sin_addr.s_addr = INADDR_ANY;
    else {
        if (!useLookup)
            addr->sin_addr.s_addr = inet_addr(peer);
    }
#else
    addr->sin6_family = AF_INET_V;
    addr->sin6_port = htons(port);
    if (peer == INADDR_ANY)
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

            SNPRINTF(strPort, sizeof(strPort), "%d", port);
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


static INLINE void tcp_socket(SOCKET_T* sockFd)
{
    *sockFd = socket(AF_INET_V, SOCK_STREAM, 0);

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
#else  /* no S_NOSIGPIPE */
    signal(SIGPIPE, SIG_IGN);
#endif /* S_NOSIGPIPE */

#if defined(TCP_NODELAY)
    {
        int       on = 1;
        socklen_t len = sizeof(on);
        int       res = setsockopt(*sockFd, IPPROTO_TCP, TCP_NODELAY, &on, len);
        if (res < 0)
            err_sys("setsockopt TCP_NODELAY failed\n");
    }
#endif
#endif  /* USE_WINDOWS_API */
}


static INLINE void tcp_bind(SOCKET_T* sockFd, uint16_t port, int useAnyAddr)
{
    SOCKADDR_IN_T addr;

    /* don't use INADDR_ANY by default, firewall may block, make user switch
       on */
    build_addr(&addr, (useAnyAddr ? INADDR_ANY : wolfsshIP), port);
    tcp_socket(sockFd);

#if !defined(USE_WINDOWS_API) && !defined(CYASSL_MDK_ARM)
    {
        int       res, on  = 1;
        socklen_t len = sizeof(on);
        res = setsockopt(*sockFd, SOL_SOCKET, SO_REUSEADDR, &on, len);
        if (res < 0)
            err_sys("setsockopt SO_REUSEADDR failed\n");
    }
#endif

    if (bind(*sockFd, (const struct sockaddr*)&addr, sizeof(addr)) != 0)
        err_sys("tcp bind failed");
}


static uint8_t find_char(const uint8_t* str, const uint8_t* buf, uint32_t bufSz)
{
    const uint8_t* cur;

    while (bufSz) {
        cur = str;
        while (*cur != '\0') {
            if (*cur == *buf)
                return *cur;
            cur++;
        }
        buf++;
        bufSz--;
    }

    return 0;
}


static int dump_stats(thread_ctx_t* ctx)
{
    char stats[1024];
    uint32_t statsSz;
    uint32_t txCount, rxCount, seq, peerSeq;

    wolfSSH_GetStats(ctx->ssh, &txCount, &rxCount, &seq, &peerSeq);

    sprintf(stats,
            "Statistics for Thread #%u:\r\n"
            "  txCount = %u\r\n  rxCount = %u\r\n"
            "  seq = %u\r\n  peerSeq = %u\r\n",
            ctx->id, txCount, rxCount, seq, peerSeq);
    statsSz = (uint32_t)strlen(stats);

    fprintf(stderr, "%s", stats);
    return wolfSSH_stream_send(ctx->ssh, (uint8_t*)stats, statsSz);
}

static THREAD_RETURN CYASSL_THREAD server_worker(void* vArgs)
{
    thread_ctx_t* threadCtx = (thread_ctx_t*)vArgs;

    if (wolfSSH_accept(threadCtx->ssh) == WS_SUCCESS) {
        uint8_t* buf = NULL;
        uint8_t* tmpBuf;
        int bufSz, backlogSz = 0, rxSz, txSz, stop = 0, txSum;

        do {
            bufSz = EXAMPLE_BUFFER_SZ + backlogSz;

            tmpBuf = realloc(buf, bufSz);
            if (tmpBuf == NULL)
                stop = 1;
            else
                buf = tmpBuf;

            if (!stop) {
                rxSz = wolfSSH_stream_read(threadCtx->ssh,
                                           buf + backlogSz,
                                           EXAMPLE_BUFFER_SZ);
                if (rxSz > 0) {
                    backlogSz += rxSz;
                    txSum = 0;
                    txSz = 0;

                    while (backlogSz != txSum && txSz >= 0 && !stop) {
                        txSz = wolfSSH_stream_send(threadCtx->ssh,
                                                   buf + txSum,
                                                   backlogSz - txSum);

                        if (txSz > 0) {
                            uint8_t c;
                            const uint8_t matches[] = { 0x03, 0x04, 0x05, 0x00 };

                            c = find_char(matches, buf + txSum, txSz);
                            switch (c) {
                                case 0x03:
                                    stop = 1;
                                    break;
                                case 0x05:
                                    if (dump_stats(threadCtx) <= 0)
                                        stop = 1;
                                default:
                                    txSum += txSz;
                            }
                        }
                        else if (txSz != WS_REKEYING)
                            stop = 1;
                    }

                    if (txSum < backlogSz)
                        memmove(buf, buf + txSum, backlogSz - txSum);
                    backlogSz -= txSum;
                }
                else
                    stop = 1;
            }
        } while (!stop);

        free(buf);
    }
    close(threadCtx->fd);
    wolfSSH_free(threadCtx->ssh);
    free(threadCtx);

    return 0;
}


static int load_file(const char* fileName, uint8_t* buf, uint32_t bufSz)
{
    FILE* file;
    uint32_t fileSz;
    uint32_t readSz;

    if (fileName == NULL) return 0;

    file = fopen(fileName, "rb");
    if (file == NULL) return 0;
    fseek(file, 0, SEEK_END);
    fileSz = (uint32_t)ftell(file);
    rewind(file);

    if (fileSz > bufSz) {
        fclose(file);
        return 0;
    }

    readSz = (uint32_t)fread(buf, 1, fileSz, file);
    if (readSz < fileSz) {
        fclose(file);
        return 0;
    }

    fclose(file);

    return fileSz;
}


static inline void c32toa(uint32_t u32, uint8_t* c)
{
    c[0] = (u32 >> 24) & 0xff;
    c[1] = (u32 >> 16) & 0xff;
    c[2] = (u32 >>  8) & 0xff;
    c[3] =  u32 & 0xff;
}


/* Map user names to passwords */
/* Use arrays for username and p. The password or public key can
 * be hashed and the hash stored here. Then I won't need the type. */
typedef struct PwMap {
    uint8_t type;
    uint8_t username[32];
    uint32_t usernameSz;
    uint8_t p[SHA256_DIGEST_SIZE];
    struct PwMap* next;
} PwMap;


typedef struct PwMapList {
    PwMap* head;
} PwMapList;


static PwMap* PwMapNew(PwMapList* list, uint8_t type, const uint8_t* username,
                       uint32_t usernameSz, const uint8_t* p, uint32_t pSz)
{
    PwMap* map;

    map = (PwMap*)malloc(sizeof(PwMap));
    if (map != NULL) {
        Sha256 sha;
        uint8_t flatSz[4];

        map->type = type;
        if (usernameSz >= sizeof(map->username))
            usernameSz = sizeof(map->username) - 1;
        memcpy(map->username, username, usernameSz + 1);
        map->username[usernameSz] = 0;
        map->usernameSz = usernameSz;

        wc_InitSha256(&sha);
        c32toa(pSz, flatSz);
        wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
        wc_Sha256Update(&sha, p, pSz);
        wc_Sha256Final(&sha, map->p);

        map->next = list->head;
        list->head = map;
    }

    return map;
}


static void PwMapListDelete(PwMapList* list)
{
    if (list != NULL) {
        PwMap* head = list->head;

        while (head != NULL) {
            PwMap* cur = head;
            head = head->next;
            memset(cur, 0, sizeof(PwMap));
            free(cur);
        }
    }
}


static const char samplePasswordBuffer[] =
    "jill:upthehill\n"
    "jack:fetchapail\n";


static const char samplePublicKeyBuffer[] =
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQC9P3ZFowOsONXHD5MwWiCciXytBRZGho"
    "MNiisWSgUs5HdHcACuHYPi2W6Z1PBFmBWT9odOrGRjoZXJfDDoPi+j8SSfDGsc/hsCmc3G"
    "p2yEhUZUEkDhtOXyqjns1ickC9Gh4u80aSVtwHRnJZh9xPhSq5tLOhId4eP61s+a5pwjTj"
    "nEhBaIPUJO2C/M0pFnnbZxKgJlX7t1Doy7h5eXxviymOIvaCZKU+x5OopfzM/wFkey0EPW"
    "NmzI5y/+pzU5afsdeEWdiQDIQc80H6Pz8fsoFPvYSG+s4/wz0duu7yeeV1Ypoho65Zr+pE"
    "nIf7dO0B8EblgWt+ud+JI8wrAhfE4x hansel\n"
    "ssh-rsa AAAAB3NzaC1yc2EAAAADAQABAAABAQCqDwRVTRVk/wjPhoo66+Mztrc31KsxDZ"
    "+kAV0139PHQ+wsueNpba6jNn5o6mUTEOrxrz0LMsDJOBM7CmG0983kF4gRIihECpQ0rcjO"
    "P6BSfbVTE9mfIK5IsUiZGd8SoE9kSV2pJ2FvZeBQENoAxEFk0zZL9tchPS+OCUGbK4SDjz"
    "uNZl/30Mczs73N3MBzi6J1oPo7sFlqzB6ecBjK2Kpjus4Y1rYFphJnUxtKvB0s+hoaadru"
    "biE57dK6BrH5iZwVLTQKux31uCJLPhiktI3iLbdlGZEctJkTasfVSsUizwVIyRjhVKmbdI"
    "RGwkU38D043AR1h0mUoGCPIKuqcFMf gretel\n";


static int LoadPasswordBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    char* username;
    char* password;

    /* Each line of passwd.txt is in the format
     *     username:password\n
     * This function modifies the passed-in buffer. */

    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        delimiter = strchr(str, ':');
        username = str;
        *delimiter = 0;
        password = delimiter + 1;
        str = strchr(password, '\n');
        *str = 0;
        str++;
        if (PwMapNew(list, WOLFSSH_USERAUTH_PASSWORD,
                     (uint8_t*)username, (uint32_t)strlen(username),
                     (uint8_t*)password, (uint32_t)strlen(password)) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int LoadPublicKeyBuffer(uint8_t* buf, uint32_t bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    uint8_t* publicKey64;
    uint32_t publicKey64Sz;
    uint8_t* username;
    uint32_t usernameSz;
    uint8_t  publicKey[300];
    uint32_t publicKeySz;

    /* Each line of passwd.txt is in the format
     *     ssh-rsa AAAB3BASE64ENCODEDPUBLICKEYBLOB username\n
     * This function modifies the passed-in buffer. */
    if (list == NULL)
        return -1;

    if (buf == NULL || bufSz == 0)
        return 0;

    while (*str != 0) {
        /* Skip the public key type. This example will always be ssh-rsa. */
        delimiter = strchr(str, ' ');
        str = delimiter + 1;
        delimiter = strchr(str, ' ');
        publicKey64 = (uint8_t*)str;
        *delimiter = 0;
        publicKey64Sz = (uint32_t)(delimiter - str);
        str = delimiter + 1;
        delimiter = strchr(str, '\n');
        username = (uint8_t*)str;
        *delimiter = 0;
        usernameSz = (uint32_t)(delimiter - str);
        str = delimiter + 1;
        publicKeySz = sizeof(publicKey);

        if (Base64_Decode(publicKey64, publicKey64Sz,
                          publicKey, &publicKeySz) != 0) {

            return -1;
        }

        if (PwMapNew(list, WOLFSSH_USERAUTH_PUBLICKEY,
                     username, usernameSz,
                     publicKey, publicKeySz) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int wsUserAuth(uint8_t authType,
                      const WS_UserAuthData* authData,
                      void* ctx)
{
    PwMapList* list;
    PwMap* map;
    uint8_t authHash[SHA256_DIGEST_SIZE];

    if (ctx == NULL) {
        fprintf(stderr, "wsUserAuth: ctx not set");
        return WOLFSSH_USERAUTH_FAILURE;
    }

    if (authType != WOLFSSH_USERAUTH_PASSWORD &&
        authType != WOLFSSH_USERAUTH_PUBLICKEY) {

        return WOLFSSH_USERAUTH_FAILURE;
    }

    /* Hash the password or public key with its length. */
    {
        Sha256 sha;
        uint8_t flatSz[4];
        wc_InitSha256(&sha);
        if (authType == WOLFSSH_USERAUTH_PASSWORD) {
            c32toa(authData->sf.password.passwordSz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                            authData->sf.password.password,
                            authData->sf.password.passwordSz);
        }
        else if (authType == WOLFSSH_USERAUTH_PUBLICKEY) {
            c32toa(authData->sf.publicKey.publicKeySz, flatSz);
            wc_Sha256Update(&sha, flatSz, sizeof(flatSz));
            wc_Sha256Update(&sha,
                            authData->sf.publicKey.publicKey,
                            authData->sf.publicKey.publicKeySz);
        }
        wc_Sha256Final(&sha, authHash);
    }

    list = (PwMapList*)ctx;
    map = list->head;

    while (map != NULL) {
        if (authData->usernameSz == map->usernameSz &&
            memcmp(authData->username, map->username, map->usernameSz) == 0) {

            if (authData->type == map->type) {
                if (memcmp(map->p, authHash, SHA256_DIGEST_SIZE) == 0) {
                    return WOLFSSH_USERAUTH_SUCCESS;
                }
                else {
                    return (authType == WOLFSSH_USERAUTH_PASSWORD ?
                            WOLFSSH_USERAUTH_INVALID_PASSWORD :
                            WOLFSSH_USERAUTH_INVALID_PUBLICKEY);
                }
            }
            else {
                return WOLFSSH_USERAUTH_INVALID_AUTHTYPE;
            }
        }
        map = map->next;
    }

    return WOLFSSH_USERAUTH_INVALID_USER;
}


int main(void)
{
    WOLFSSH_CTX* ctx = NULL;
    PwMapList pwMapList;
    SOCKET_T listenFd = 0;
    uint32_t defaultHighwater = EXAMPLE_HIGHWATER_MARK;
    uint32_t threadCount = 0;

    #ifdef DEBUG_WOLFSSH
        wolfSSH_Debugging_ON();
    #endif

    if (wolfSSH_Init() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't initialize wolfSSH.\n");
        exit(EXIT_FAILURE);
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Couldn't allocate SSH CTX data.\n");
        exit(EXIT_FAILURE);
    }

    memset(&pwMapList, 0, sizeof(pwMapList));
    wolfSSH_SetUserAuth(ctx, wsUserAuth);
    wolfSSH_CTX_SetBanner(ctx, echoserverBanner);

    {
        uint8_t buf[SCRATCH_BUFFER_SIZE];
        uint32_t bufSz;

        bufSz = load_file("./keys/server-key.der", buf, SCRATCH_BUFFER_SIZE);
        if (bufSz == 0) {
            fprintf(stderr, "Couldn't load key file.\n");
            exit(EXIT_FAILURE);
        }
        if (wolfSSH_CTX_UsePrivateKey_buffer(ctx,
                                         buf, bufSz, WOLFSSH_FORMAT_ASN1) < 0) {
            fprintf(stderr, "Couldn't use key buffer.\n");
            exit(EXIT_FAILURE);
        }

        bufSz = (uint32_t)strlen((char*)samplePasswordBuffer);
        memcpy(buf, samplePasswordBuffer, bufSz);
        buf[bufSz] = 0;
        LoadPasswordBuffer(buf, bufSz, &pwMapList);

        bufSz = (uint32_t)strlen((char*)samplePublicKeyBuffer);
        memcpy(buf, samplePublicKeyBuffer, bufSz);
        buf[bufSz] = 0;
        LoadPublicKeyBuffer(buf, bufSz, &pwMapList);
    }

    tcp_bind(&listenFd, SERVER_PORT_NUMBER, 0);

    if (listen(listenFd, 5) != 0)
        err_sys("tcp listen failed");

    for (;;) {
        SOCKET_T      clientFd = 0;
        SOCKADDR_IN_T clientAddr;
        SOCKLEN_T     clientAddrSz = sizeof(clientAddr);
        THREAD_TYPE   thread;
        WOLFSSH*      ssh;
        thread_ctx_t* threadCtx;

        threadCtx = (thread_ctx_t*)malloc(sizeof(thread_ctx_t));
        if (threadCtx == NULL) {
            fprintf(stderr, "Couldn't allocate thread context data.\n");
            exit(EXIT_FAILURE);
        }

        ssh = wolfSSH_new(ctx);
        if (ssh == NULL) {
            fprintf(stderr, "Couldn't allocate SSH data.\n");
            exit(EXIT_FAILURE);
        }
        wolfSSH_SetUserAuthCtx(ssh, &pwMapList);
        /* Use the session object for its own highwater callback ctx */
        if (defaultHighwater > 0) {
            wolfSSH_SetHighwaterCtx(ssh, (void*)ssh);
            wolfSSH_SetHighwater(ssh, defaultHighwater);
        }

        clientFd = accept(listenFd, (struct sockaddr*)&clientAddr,
                                                                 &clientAddrSz);
        if (clientFd == -1)
            err_sys("tcp accept failed");

        wolfSSH_set_fd(ssh, clientFd);

        threadCtx->ssh = ssh;
        threadCtx->fd = clientFd;
        threadCtx->id = threadCount++;

        pthread_create(&thread, 0, server_worker, threadCtx);
        pthread_detach(thread);
    }

    PwMapListDelete(&pwMapList);
    wolfSSH_CTX_free(ctx);
    if (wolfSSH_Cleanup() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't clean up wolfSSH.\n");
        exit(EXIT_FAILURE);
    }

    return 0;
}
