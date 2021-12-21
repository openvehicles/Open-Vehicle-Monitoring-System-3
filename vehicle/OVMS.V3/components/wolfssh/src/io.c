/* io.c
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
 * The io module provides the default send and receive callbacks used
 * by the library to handle network I/O. By default they handle UNIX
 * style I/O.
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>
#include <wolfssh/internal.h>
#include <wolfssh/log.h>

#ifndef NULL
    #include <stddef.h>
#endif

#ifdef WOLFSSH_TEST_BLOCK
    #define WOLFSSH_TEST_SERVER
    #include "wolfssh/test.h"

    /* percent of time that forced want read/write is done */
    #ifndef WOLFSSH_BLOCK_PROB
        #define WOLFSSH_BLOCK_PROB 75
    #endif
#endif


/* allow I/O callback handlers whether user I/O or not */

/* install I/O recv callback */
void wolfSSH_SetIORecv(WOLFSSH_CTX* ctx, WS_CallbackIORecv cb)
{
    if (ctx)
        ctx->ioRecvCb = cb;
}


/* install I/O send callback */
void wolfSSH_SetIOSend(WOLFSSH_CTX* ctx, WS_CallbackIOSend cb)
{
    if (ctx)
        ctx->ioSendCb = cb;
}


/* install I/O read context */
void wolfSSH_SetIOReadCtx(WOLFSSH* ssh, void *ctx)
{
    if (ssh)
        ssh->ioReadCtx = ctx;
}


/* install I/O read context */
void wolfSSH_SetIOWriteCtx(WOLFSSH* ssh, void *ctx)
{
    if (ssh)
        ssh->ioWriteCtx = ctx;
}


/* get I/O read context */
void* wolfSSH_GetIOReadCtx(WOLFSSH* ssh)
{
    if (ssh)
        return ssh->ioReadCtx;

    return NULL;
}


/* get I/O write context */
void* wolfSSH_GetIOWriteCtx(WOLFSSH* ssh)
{
    if (ssh)
        return ssh->ioWriteCtx;

    return NULL;
}

#ifndef WOLFSSH_USER_IO

/* default I/O callbacks, use BSD style sockets */


#ifndef USE_WINDOWS_API
    #ifdef WOLFSSH_LWIP
        /* lwIP needs to be configured to use sockets API in this mode */
        /* LWIP_SOCKET 1 in lwip/opt.h or in build */
        #include "lwip/sockets.h"
        #ifndef LWIP_PROVIDE_ERRNO
            #include <errno.h>
            #define LWIP_PROVIDE_ERRNO 1
        #endif
    #elif defined(FREESCALE_MQX)
        #include <posix.h>
        #include <rtcs.h>
    #elif defined(WOLFSSH_MDK_ARM)
        #if defined(WOLFSSH_MDK5)
            #include "cmsis_os.h"
            #include "rl_fs.h" 
            #include "rl_net.h" 
        #else
            #include <rtl.h>
        #endif
        #undef RNG
        #include "WOLFSSH_MDK_ARM.h"
        #undef RNG
        #define RNG CyaSSL_RNG 
        /* for avoiding name conflict in "stm32f2xx.h" */
        static int errno;
    #elif defined(MICROCHIP_MPLAB_HARMONY)
        #include "tcpip/tcpip.h"
        #include "sys/errno.h"
        #include <errno.h>
    #elif defined(WOLFSSL_NUCLEUS)
        #include "nucleus.h"
        #include "networking/nu_networking.h"
        #include <errno.h>
    #else
        #include <sys/types.h>
        #include <errno.h>
        #ifndef EBSNET
            #include <unistd.h>
        #endif
        #include <fcntl.h>
        #if !(defined(DEVKITPRO) || defined(HAVE_RTP_SYS) || defined(EBSNET))
            #include <sys/socket.h>
            #include <arpa/inet.h>
            #include <netinet/in.h>
            #include <netdb.h>
            #ifdef __PPU
                #include <netex/errno.h>
            #else
                #include <sys/ioctl.h>
            #endif
        #endif
        #ifdef HAVE_RTP_SYS
            #include <socket.h>
        #endif
        #ifdef EBSNET
            #include "rtipapi.h"  /* errno */
            #include "socket.h"
        #endif
    #endif
#endif /* USE_WINDOWS_API */

#ifdef __sun
    #include <sys/filio.h>
#endif

#ifdef USE_WINDOWS_API 
    /* no epipe yet */
    #ifndef WSAEPIPE
        #define WSAEPIPE       -12345
    #endif
    #define SOCKET_EWOULDBLOCK WSAEWOULDBLOCK
    #define SOCKET_EAGAIN      WSAETIMEDOUT
    #define SOCKET_ECONNRESET  WSAECONNRESET
    #define SOCKET_EINTR       WSAEINTR
    #define SOCKET_EPIPE       WSAEPIPE
    #define SOCKET_ECONNREFUSED WSAENOTCONN
    #define SOCKET_ECONNABORTED WSAECONNABORTED
    #define close(s) closesocket(s)
#elif defined(__PPU)
    #define SOCKET_EWOULDBLOCK SYS_NET_EWOULDBLOCK
    #define SOCKET_EAGAIN      SYS_NET_EAGAIN
    #define SOCKET_ECONNRESET  SYS_NET_ECONNRESET
    #define SOCKET_EINTR       SYS_NET_EINTR
    #define SOCKET_EPIPE       SYS_NET_EPIPE
    #define SOCKET_ECONNREFUSED SYS_NET_ECONNREFUSED
    #define SOCKET_ECONNABORTED SYS_NET_ECONNABORTED
#elif defined(FREESCALE_MQX)
    /* RTCS doesn't have an EWOULDBLOCK error */
    #define SOCKET_EWOULDBLOCK EAGAIN
    #define SOCKET_EAGAIN      EAGAIN
    #define SOCKET_ECONNRESET  RTCSERR_TCP_CONN_RESET
    #define SOCKET_EINTR       EINTR
    #define SOCKET_EPIPE       EPIPE
    #define SOCKET_ECONNREFUSED RTCSERR_TCP_CONN_REFUSED
    #define SOCKET_ECONNABORTED RTCSERR_TCP_CONN_ABORTED
#elif defined(WOLFSSH_MDK_ARM)
    #if defined(WOLFSSH_MDK5)
        #define SOCKET_EWOULDBLOCK BSD_ERROR_WOULDBLOCK
        #define SOCKET_EAGAIN      BSD_ERROR_LOCKED
        #define SOCKET_ECONNRESET  BSD_ERROR_CLOSED
        #define SOCKET_EINTR       BSD_ERROR
        #define SOCKET_EPIPE       BSD_ERROR
        #define SOCKET_ECONNREFUSED BSD_ERROR
        #define SOCKET_ECONNABORTED BSD_ERROR
    #else
        #define SOCKET_EWOULDBLOCK SCK_EWOULDBLOCK
        #define SOCKET_EAGAIN      SCK_ELOCKED
        #define SOCKET_ECONNRESET  SCK_ECLOSED
        #define SOCKET_EINTR       SCK_ERROR
        #define SOCKET_EPIPE       SCK_ERROR
        #define SOCKET_ECONNREFUSED SCK_ERROR
        #define SOCKET_ECONNABORTED SCK_ERROR
    #endif
#elif defined(WOLFSSL_NUCLEUS)
    #define SOCKET_EWOULDBLOCK NU_WOULD_BLOCK
    #define SOCKET_EAGAIN      NU_WOULD_BLOCK
    #define SOCKET_ECONNRESET  NU_NOT_CONNECTED
    #define SOCKET_EINTR       NU_NOT_CONNECTED
    #define SOCKET_EPIPE       NU_NOT_CONNECTED
    #define SOCKET_ECONNREFUSED NU_CONNECTION_REFUSED
    #define SOCKET_ECONNABORTED NU_NOT_CONNECTED
#else
    #define SOCKET_EWOULDBLOCK EWOULDBLOCK
    #define SOCKET_EAGAIN      EAGAIN
    #define SOCKET_ECONNRESET  ECONNRESET
    #define SOCKET_EINTR       EINTR
    #define SOCKET_EPIPE       EPIPE
    #define SOCKET_ECONNREFUSED ECONNREFUSED
    #define SOCKET_ECONNABORTED ECONNABORTED
#endif /* USE_WINDOWS_API */


#ifdef DEVKITPRO
    /* from network.h */
    int net_send(int, const void*, int, unsigned int);
    int net_recv(int, void*, int, unsigned int);
    #define SEND_FUNCTION net_send
    #define RECV_FUNCTION net_recv
#elif defined(WOLFSSH_LWIP)
    #define SEND_FUNCTION lwip_send
    #define RECV_FUNCTION lwip_recv
#elif defined(MICROCHIP_MPLAB_HARMONY)
    #define SEND_FUNCTION(socket,buf,sz,flags) \
            TCPIP_TCP_ArrayPut((socket),(uint8_t*)(buf),(sz))
    #define RECV_FUNCTION(socket,buf,sz,flags) \
            TCPIP_TCP_ArrayGet((socket),(uint8_t*)(buf),(sz))
#elif defined(WOLFSSL_NUCLEUS)
    #define SEND_FUNCTION NU_Send
    #define RECV_FUNCTION NU_Recv
#else
    #define SEND_FUNCTION send
    #define RECV_FUNCTION recv
#endif


/* Translates return codes returned from send() and recv() if need be. */
static INLINE int TranslateReturnCode(int old, WS_SOCKET_T sd)
{
    (void)sd;

#ifdef FREESCALE_MQX
    if (old == 0) {
        errno = SOCKET_EWOULDBLOCK;
        return -1;  /* convert to BSD style wouldblock as error */
    }

    if (old < 0) {
        errno = RTCS_geterror(sd);
        if (errno == RTCSERR_TCP_CONN_CLOSING)
            return 0;   /* convert to BSD style closing */
    }
#endif

#ifdef MICROCHIP_MPLAB_HARMONY
    if (old == 0) {
        /* check is still connected */
        if (!TCPIP_TCP_IsConnected(sd))
        {
            return 0;
        }

        errno = SOCKET_EWOULDBLOCK;
        return -1;
    }
#endif
    return old;
}

static INLINE int LastError(void)
{
#ifdef USE_WINDOWS_API
    return WSAGetLastError();
#elif defined(EBSNET)
    return xn_getlasterror();
#else
    return errno;
#endif
}

/* The receive embedded callback
 *  return : nb bytes read, or error
 */
int wsEmbedRecv(WOLFSSH* ssh, void* data, word32 sz, void* ctx)
{
    int recvd;
    int err;
    WS_SOCKET_T sd = *(WS_SOCKET_T*)ctx;
    char* buf = (char*)data;

#ifdef WOLFSSH_TEST_BLOCK
    if (tcp_select(sd, 1) == WS_SELECT_RECV_READY &&
            (rand() % 100) < WOLFSSH_BLOCK_PROB) {
        printf("Forced read block\n");
        return WS_CBIO_ERR_WANT_READ;
    }
#endif

    recvd = (int)RECV_FUNCTION(sd, buf, sz, ssh->rflags);

    recvd = TranslateReturnCode(recvd, sd);

    if (recvd < 0) {
        err = LastError();
        WLOG(WS_LOG_DEBUG,"Embed Receive error");

        if (err == SOCKET_EWOULDBLOCK || err == SOCKET_EAGAIN) {
            WLOG(WS_LOG_DEBUG,"    Would block");
            return WS_CBIO_ERR_WANT_READ;
        }
        else if (err == SOCKET_ECONNRESET) {
            WLOG(WS_LOG_DEBUG,"    Connection reset");
            return WS_CBIO_ERR_CONN_RST;
        }
        else if (err == SOCKET_EINTR) {
            WLOG(WS_LOG_DEBUG,"    Socket interrupted");
            return WS_CBIO_ERR_ISR;
        }
        else if (err == SOCKET_ECONNREFUSED) {
            WLOG(WS_LOG_DEBUG,"    Connection refused");
            return WS_CBIO_ERR_WANT_READ;
        }
        else if (err == SOCKET_ECONNABORTED) {
            WLOG(WS_LOG_DEBUG,"    Connection aborted");
            return WS_CBIO_ERR_CONN_CLOSE;
        }
        else {
            WLOG(WS_LOG_DEBUG,"    General error");
            return WS_CBIO_ERR_GENERAL;
        }
    }
    else if (recvd == 0) {
        WLOG(WS_LOG_DEBUG,"Embed receive connection closed");
        return WS_CBIO_ERR_CONN_CLOSE;
    }

    return recvd;
}


/* The send embedded callback
 *  return : nb bytes sent, or error
 */
int wsEmbedSend(WOLFSSH* ssh, void* data, word32 sz, void* ctx)
{
    WS_SOCKET_T sd = *(WS_SOCKET_T*)ctx;
    int sent;
    int err;
    char* buf = (char*)data;

#ifdef WOLFSSH_TEST_BLOCK
    if ((rand() % 100) < WOLFSSH_BLOCK_PROB) {
        printf("Forced write block\n");
        return WS_CBIO_ERR_WANT_WRITE;
    }
#endif

    WLOG(WS_LOG_DEBUG,"Embed Send trying to send %d", sz);

#ifdef MICROCHIP_MPLAB_HARMONY
    /* check is still connected */
    if (!TCPIP_TCP_IsConnected(sd))
    {
        return WS_CBIO_ERR_CONN_CLOSE;
    }

    /* not enough space to send */
    if ((sent = TCPIP_TCP_PutIsReady(sd)) < sz) {
        sz = sent;
    }
    if (sent == 0) {
        /* In the case that 0 is returned the main TCP loop needs to be called */
        return WS_CBIO_ERR_WANT_WRITE;
    }
#endif /* MICROCHIP_MPLAB_HARMONY */

    sent = (int)SEND_FUNCTION(sd, buf, sz, ssh->wflags);

    WLOG(WS_LOG_DEBUG,"Embed Send sent %d", sent);

    if (sent < 0) {
        err = LastError();
        WLOG(WS_LOG_DEBUG,"Embed Send error");

        if (err == SOCKET_EWOULDBLOCK || err == SOCKET_EAGAIN) {
            WLOG(WS_LOG_DEBUG,"    Would Block");
            return WS_CBIO_ERR_WANT_WRITE;
        }
        else if (err == SOCKET_ECONNRESET) {
            WLOG(WS_LOG_DEBUG,"    Connection reset");
            return WS_CBIO_ERR_CONN_RST;
        }
        else if (err == SOCKET_EINTR) {
            WLOG(WS_LOG_DEBUG,"    Socket interrupted");
            return WS_CBIO_ERR_ISR;
        }
        else if (err == SOCKET_EPIPE) {
            WLOG(WS_LOG_DEBUG,"    Socket EPIPE");
            return WS_CBIO_ERR_CONN_CLOSE;
        }
        else {
            WLOG(WS_LOG_DEBUG,"    General error");
            return WS_CBIO_ERR_GENERAL;
        }
    }
    return sent;
}



#endif /* WOLFSSH_USER_IO */

