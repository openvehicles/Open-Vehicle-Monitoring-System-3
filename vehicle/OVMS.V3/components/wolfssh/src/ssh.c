/* ssh.c
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
 * The ssh module contains the public API for wolfSSH.
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>
#include <wolfssh/internal.h>
#include <wolfssh/log.h>
#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssl/wolfcrypt/wc_port.h>

#ifdef NO_INLINE
    #include <wolfssh/misc.h>
#else
    #define WOLFSSH_MISC_INCLUDED
    #include "src/misc.c"
#endif


int wolfSSH_Init(void)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_Init()");
    if (wolfCrypt_Init() != 0)
        ret = WS_CRYPTO_FAILED;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_Init(), returning %d", ret);
    return ret;
}


int wolfSSH_Cleanup(void)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_Cleanup()");
    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_Cleanup(), returning %d", WS_SUCCESS);
    return WS_SUCCESS;
}


WOLFSSH_CTX* wolfSSH_CTX_new(uint8_t side, void* heap)
{
    WOLFSSH_CTX* ctx;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_new()");

    if (side != WOLFSSH_ENDPOINT_SERVER && side != WOLFSSH_ENDPOINT_CLIENT) {
        WLOG(WS_LOG_DEBUG, "Invalid endpoint type");
        return NULL;
    }

    ctx = (WOLFSSH_CTX*)WMALLOC(sizeof(WOLFSSH_CTX), heap, DYNTYPE_CTX);
    ctx = CtxInit(ctx, heap);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_CTX_new(), ctx = %p", ctx);

    return ctx;
}


void wolfSSH_CTX_free(WOLFSSH_CTX* ctx)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_free()");

    if (ctx) {
        CtxResourceFree(ctx);
        WFREE(ctx, ctx->heap, DYNTYPE_CTX);
    }
}


WOLFSSH* wolfSSH_new(WOLFSSH_CTX* ctx)
{
    WOLFSSH* ssh;
    void*    heap = NULL;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_new()");

    if (ctx)
        heap = ctx->heap;
    else {
        WLOG(WS_LOG_ERROR, "Trying to init a wolfSSH w/o wolfSSH_CTX");
        return NULL;
    }

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_new()");

    ssh = (WOLFSSH*)WMALLOC(sizeof(WOLFSSH), heap, DYNTYPE_SSH);
    ssh = SshInit(ssh, ctx);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_new(), ssh = %p", ssh);

    return ssh;
}


void wolfSSH_free(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_free()");

    if (ssh) {
        void* heap = ssh->ctx ? ssh->ctx->heap : NULL;
        SshResourceFree(ssh, heap);
        WFREE(ssh, heap, DYNTYPE_SSH);
    }
}


int wolfSSH_set_fd(WOLFSSH* ssh, int fd)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_set_fd()");

    if (ssh) {
        ssh->rfd = fd;
        ssh->wfd = fd;

        ssh->ioReadCtx  = &ssh->rfd;
        ssh->ioWriteCtx = &ssh->wfd;

        return WS_SUCCESS;
    }
    return WS_BAD_ARGUMENT;
}


int wolfSSH_get_fd(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_fd()");

    if (ssh)
        return ssh->rfd;

    return WS_BAD_ARGUMENT;
}


int wolfSSH_SetHighwater(WOLFSSH* ssh, uint32_t highwater)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetHighwater()");

    if (ssh) {
        ssh->highwaterMark = highwater;

        return WS_SUCCESS;
    }

    return WS_BAD_ARGUMENT;
}


uint32_t wolfSSH_GetHighwater(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetHighwater()");

    if (ssh)
        return ssh->highwaterMark;

    return 0;
}


void wolfSSH_SetHighwaterCb(WOLFSSH_CTX* ctx, uint32_t highwater,
                            WS_CallbackHighwater cb)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetHighwaterCb()");

    if (ctx) {
        ctx->highwaterMark = highwater;
        ctx->highwaterCb = cb;
    }
}


void wolfSSH_SetHighwaterCtx(WOLFSSH* ssh, void* ctx)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetHighwaterCtx()");

    if (ssh)
        ssh->highwaterCtx = ctx;
}


void* wolfSSH_GetHighwaterCtx(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetHighwaterCtx()");

    if (ssh)
        return ssh->highwaterCtx;

    return NULL;
}


int wolfSSH_get_error(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_error()");

    if (ssh)
        return ssh->error;

    return WS_BAD_ARGUMENT;
}


const char* wolfSSH_get_error_name(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_error_name()");

    if (ssh)
        return GetErrorString(ssh->error);

    return NULL;
}


const char acceptError[] = "accept error: %s, %d";
const char acceptState[] = "accept state: %s";


int wolfSSH_accept(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_accept()");

    switch (ssh->acceptState) {

        case ACCEPT_BEGIN:
            if ( (ssh->error = SendServerVersion(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, acceptError, "BEGIN", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->acceptState = ACCEPT_SERVER_VERSION_SENT;
            WLOG(WS_LOG_DEBUG, acceptState, "SERVER_VERSION_SENT");

        case ACCEPT_SERVER_VERSION_SENT:
            while (ssh->clientState < CLIENT_VERSION_DONE) {
                if ( (ssh->error = ProcessClientVersion(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "SERVER_VERSION_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_VERSION_DONE;
            WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_VERSION_DONE");

        case ACCEPT_CLIENT_VERSION_DONE:
            while (ssh->keyingState < KEYING_KEYED) {
                if ( (ssh->error = DoReceive(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "CLIENT_VERSION_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_KEYED;
            WLOG(WS_LOG_DEBUG, acceptState, "KEYED");

        case ACCEPT_KEYED:
            while (ssh->clientState < CLIENT_USERAUTH_REQUEST_DONE) {
                if ( (ssh->error = DoReceive(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "KEYED", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_USERAUTH_REQUEST_DONE;
            WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_USERAUTH_REQUEST_DONE");

        case ACCEPT_CLIENT_USERAUTH_REQUEST_DONE:
            if ( (ssh->error = SendServiceAccept(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, acceptError,
                     "CLIENT_USERAUTH_REQUEST_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->acceptState = ACCEPT_SERVER_USERAUTH_ACCEPT_SENT;
            WLOG(WS_LOG_DEBUG, acceptState,
                 "ACCEPT_SERVER_USERAUTH_ACCEPT_SENT");

        case ACCEPT_SERVER_USERAUTH_ACCEPT_SENT:
            while (ssh->clientState < CLIENT_USERAUTH_DONE) {
                if ( (ssh->error = DoReceive(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "SERVER_USERAUTH_ACCEPT_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_USERAUTH_DONE;
            WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_USERAUTH_DONE");

        case ACCEPT_CLIENT_USERAUTH_DONE:
            if ( (ssh->error = SendUserAuthSuccess(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, acceptError,
                     "CLIENT_USERAUTH_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->acceptState = ACCEPT_SERVER_USERAUTH_SENT;
            WLOG(WS_LOG_DEBUG, acceptState, "SERVER_USERAUTH_SENT");

        case ACCEPT_SERVER_USERAUTH_SENT:
            while (ssh->clientState < CLIENT_CHANNEL_OPEN_DONE) {
                if ( (ssh->error = DoReceive(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "SERVER_USERAUTH_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_CHANNEL_REQUEST_DONE;
            WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_CHANNEL_REQUEST_DONE");

        case ACCEPT_CLIENT_CHANNEL_REQUEST_DONE:
            if ( (ssh->error = SendChannelOpenConf(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, acceptError,
                     "CLIENT_CHANNEL_REQUEST_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->acceptState = ACCEPT_SERVER_CHANNEL_ACCEPT_SENT;
            WLOG(WS_LOG_DEBUG, acceptState, "SERVER_CHANNEL_ACCEPT_SENT");

        case ACCEPT_SERVER_CHANNEL_ACCEPT_SENT:
            while (ssh->clientState < CLIENT_DONE) {
                if ( (ssh->error = DoReceive(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "SERVER_CHANNEL_ACCEPT_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->acceptState = ACCEPT_CLIENT_SESSION_ESTABLISHED;
            WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_SESSION_ESTABLISHED");

    }

    return WS_SUCCESS;
}


int wolfSSH_TriggerKeyExchange(WOLFSSH* ssh)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_TriggerKeyExchange()");
    if (ssh == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = SendKexInit(ssh);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_TriggerKeyExchange(), ret = %d", ret);
    return ret;
}


int wolfSSH_stream_read(WOLFSSH* ssh, uint8_t* buf, uint32_t bufSz)
{
    Buffer* inputBuffer;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_stream_read()");

    if (ssh == NULL || buf == NULL || bufSz == 0 || ssh->channelList == NULL)
        return WS_BAD_ARGUMENT;

    inputBuffer = &ssh->channelList->inputBuffer;

    while (inputBuffer->length - inputBuffer->idx == 0) {
        int ret = DoReceive(ssh);
        if (ssh->channelList == NULL || ssh->channelList->receivedEof)
            ret = WS_EOF;
        if (ret < 0) {
            WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_read(), ret = %d", ret);
            return ret;
        }
    }

    bufSz = min(bufSz, inputBuffer->length - inputBuffer->idx);
    WMEMCPY(buf, inputBuffer->buffer + inputBuffer->idx, bufSz);
    inputBuffer->idx += bufSz;

    if (ssh->keyingState == KEYING_KEYED &&
        (inputBuffer->length > inputBuffer->bufferSz / 2)) {

        uint32_t usedSz = inputBuffer->length - inputBuffer->idx;
        uint32_t bytesToAdd = inputBuffer->idx;
        int sendResult;

        WLOG(WS_LOG_DEBUG, "Making more room: %u", usedSz);
        if (usedSz) {
            WLOG(WS_LOG_DEBUG, "  ...moving data down");
            WMEMMOVE(inputBuffer->buffer,
                     inputBuffer->buffer + bytesToAdd, usedSz);
        }

        sendResult = SendChannelWindowAdjust(ssh,
                                             ssh->channelList->peerChannel,
                                             bytesToAdd);
        if (sendResult != WS_SUCCESS)
            bufSz = sendResult;

        WLOG(WS_LOG_INFO, "  bytesToAdd = %u", bytesToAdd);
        WLOG(WS_LOG_INFO, "  windowSz = %u", ssh->channelList->windowSz);
        ssh->channelList->windowSz += bytesToAdd;
        WLOG(WS_LOG_INFO, "  update windowSz = %u", ssh->channelList->windowSz);

        inputBuffer->length = usedSz;
        inputBuffer->idx = 0;
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_read(), rxd = %d", bufSz);
    return bufSz;
}


int wolfSSH_stream_send(WOLFSSH* ssh, uint8_t* buf, uint32_t bufSz)
{
    int bytesTxd = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_stream_send()");

    if (ssh == NULL || buf == NULL || ssh->channelList == NULL)
        return WS_BAD_ARGUMENT;

    bytesTxd = SendChannelData(ssh, ssh->channelList->peerChannel, buf, bufSz);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_send(), txd = %d", bytesTxd);
    return bytesTxd;
}


int wolfSSH_stream_exit(WOLFSSH* ssh, int status)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_stream_exit(), status = %d", status);

    if (ssh == NULL || ssh->channelList == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = SendChannelExit(ssh, ssh->channelList->peerChannel, status);

    if (ret == WS_SUCCESS)
        ret = SendChannelEow(ssh, ssh->channelList->peerChannel);

    if (ret == WS_SUCCESS)
        ret = SendChannelEof(ssh, ssh->channelList->peerChannel);

    if (ret == WS_SUCCESS)
        ret = SendChannelClose(ssh, ssh->channelList->peerChannel);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_exit()");
    return ret;
}


void wolfSSH_SetUserAuth(WOLFSSH_CTX* ctx, WS_CallbackUserAuth cb)
{
    if (ctx != NULL) {
        ctx->userAuthCb = cb;
    }
}


void wolfSSH_SetUserAuthCtx(WOLFSSH* ssh, void* userAuthCtx)
{
    if (ssh != NULL) {
        ssh->userAuthCtx = userAuthCtx;
    }
}


void* wolfSSH_GetUserAuthCtx(WOLFSSH* ssh)
{
    if (ssh != NULL) {
        return ssh->userAuthCtx;
    }
    return NULL;
}


static int ProcessBuffer(WOLFSSH_CTX* ctx, const uint8_t* in, uint32_t inSz,
                                                           int format, int type)
{
    int dynamicType;
    void* heap;
    uint8_t* der;
    uint32_t derSz;

    if (ctx == NULL || in == NULL || inSz == 0)
        return WS_BAD_ARGUMENT;

    if (format != WOLFSSH_FORMAT_ASN1 && format != WOLFSSH_FORMAT_PEM &&
                                         format != WOLFSSH_FORMAT_RAW)
        return WS_BAD_FILETYPE_E;

    if (type == BUFTYPE_CA)
        dynamicType = DYNTYPE_CA;
    else if (type == BUFTYPE_CERT)
        dynamicType = DYNTYPE_CERT;
    else if (type == BUFTYPE_PRIVKEY)
        dynamicType = DYNTYPE_PRIVKEY;
    else
        return WS_BAD_ARGUMENT;

    heap = ctx->heap;

    if (format == WOLFSSH_FORMAT_PEM)
        return WS_UNIMPLEMENTED_E;
    else {
        /* format is ASN1 or RAW */
        der = (uint8_t*)WMALLOC(inSz, heap, dynamicType);
        if (der == NULL)
            return WS_MEMORY_E;
        WMEMCPY(der, in, inSz);
        derSz = inSz;
    }

    /* Maybe decrypt */

    if (type == BUFTYPE_PRIVKEY) {
        if (ctx->privateKey)
            WFREE(ctx->privateKey, heap, dynamicType);
        ctx->privateKey = der;
        ctx->privateKeySz = derSz;
    }
    else {
        WFREE(der, heap, dynamicType);
        return WS_UNIMPLEMENTED_E;
    }

    if (type == BUFTYPE_PRIVKEY && format != WOLFSSH_FORMAT_RAW) {
        /* Check RSA key */
        RsaKey key;
        uint32_t scratch = 0;

        if (wc_InitRsaKey(&key, NULL) < 0)
            return WS_RSA_E;

        if (wc_RsaPrivateKeyDecode(der, &scratch, &key, derSz) < 0)
            return WS_BAD_FILE_E;

        wc_FreeRsaKey(&key);
    }

    return WS_SUCCESS;
}


int wolfSSH_CTX_SetBanner(WOLFSSH_CTX* ctx,
                          const char* newBanner)
{
    uint32_t newBannerSz = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_SetBanner()");

    if (ctx == NULL)
        return WS_BAD_ARGUMENT;

    if (newBanner != NULL) {
        WLOG(WS_LOG_INFO, "  setting banner to: \"%s\"", newBanner);
        newBannerSz = (uint32_t)WSTRLEN(newBanner);
    }

    ctx->banner = newBanner;
    ctx->bannerSz = newBannerSz;

    return WS_SUCCESS;
}


int wolfSSH_CTX_UsePrivateKey_buffer(WOLFSSH_CTX* ctx,
                                   const uint8_t* in, uint32_t inSz, int format)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_UsePrivateKey_buffer()");
    return ProcessBuffer(ctx, in, inSz, format, BUFTYPE_PRIVKEY); 
}


void wolfSSH_GetStats(WOLFSSH* ssh, uint32_t* txCount, uint32_t* rxCount,
                      uint32_t* seq, uint32_t* peerSeq)
{
    uint32_t rTxCount = 0;
    uint32_t rRxCount = 0;
    uint32_t rSeq = 0;
    uint32_t rPeerSeq = 0;

    if (ssh != NULL) {
        rTxCount = ssh->txCount;
        rRxCount = ssh->rxCount;
        rSeq = ssh->seq;
        rPeerSeq = ssh->peerSeq;
    }

    if (txCount != NULL)
        *txCount = rTxCount;
    if (rxCount != NULL)
        *rxCount = rRxCount;
    if (seq != NULL)
        *seq = rSeq;
    if (peerSeq != NULL)
        *peerSeq = rPeerSeq;
}


int wolfSSH_KDF(uint8_t hashId, uint8_t keyId,
                uint8_t* key, uint32_t keySz,
                const uint8_t* k, uint32_t kSz,
                const uint8_t* h, uint32_t hSz,
                const uint8_t* sessionId, uint32_t sessionIdSz)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_KDF()");
    return GenerateKey(hashId, keyId, key, keySz, k, kSz, h, hSz,
                       sessionId, sessionIdSz);
}

