/* ssh.c
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


/*
 * The ssh module contains the public API for wolfSSH.
 */


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif

#include <wolfssh/ssh.h>
#include <wolfssh/internal.h>
#include <wolfssh/log.h>
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
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_Cleanup()");

    if (wolfCrypt_Cleanup() != 0)
        ret = WS_CRYPTO_FAILED;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_Cleanup(), returning %d", ret);
    return ret;
}


WOLFSSH_CTX* wolfSSH_CTX_new(byte side, void* heap)
{
    WOLFSSH_CTX* ctx;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_new()");

    if (side != WOLFSSH_ENDPOINT_SERVER && side != WOLFSSH_ENDPOINT_CLIENT) {
        WLOG(WS_LOG_DEBUG, "Invalid endpoint type");
        return NULL;
    }

    ctx = (WOLFSSH_CTX*)WMALLOC(sizeof(WOLFSSH_CTX), heap, DYNTYPE_CTX);
    ctx = CtxInit(ctx, side, heap);

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

    (void)heap;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_new()");

    if (ctx)
        heap = ctx->heap;
    else {
        WLOG(WS_LOG_ERROR, "Trying to init a wolfSSH w/o wolfSSH_CTX");
        return NULL;
    }

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
    #ifdef WOLFSSH_SFTP
        if (wolfSSH_SFTP_free(ssh) != WS_SUCCESS) {
            WLOG(WS_LOG_SFTP, "Error cleaning up SFTP connection");
        }
    #endif
    #ifdef WOLFSSH_AGENT
        if (ssh->agent != NULL)
            wolfSSH_AGENT_free(ssh->agent);
    #endif /* WOLFSSH_AGENT */
        SshResourceFree(ssh, heap);
        WFREE(ssh, heap, DYNTYPE_SSH);
    }
}


int wolfSSH_set_fd(WOLFSSH* ssh, WS_SOCKET_T fd)
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


WS_SOCKET_T wolfSSH_get_fd(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_fd()");

    if (ssh)
        return ssh->rfd;

#ifdef USE_WINDOWS_API
    return INVALID_SOCKET;
#else
    return WS_BAD_ARGUMENT;
#endif
}


int wolfSSH_SetFilesystemHandle(WOLFSSH* ssh, void* handle)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetFilesystemHandle()");

    if (ssh) {
        ssh->fs = handle;

        return WS_SUCCESS;
    }

    return WS_BAD_ARGUMENT;
}


void* wolfSSH_GetFilesystemHandle(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetFilesystemHandle()");

    if (ssh)
        return ssh->fs;

    return NULL;
}


int wolfSSH_SetHighwater(WOLFSSH* ssh, word32 highwater)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetHighwater()");

    if (ssh) {
        ssh->highwaterMark = highwater;

        return WS_SUCCESS;
    }

    return WS_BAD_ARGUMENT;
}


word32 wolfSSH_GetHighwater(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetHighwater()");

    if (ssh)
        return ssh->highwaterMark;

    return 0;
}


void wolfSSH_SetHighwaterCb(WOLFSSH_CTX* ctx, word32 highwater,
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

void wolfSSH_SetGlobalReq(WOLFSSH_CTX *ctx, WS_CallbackGlobalReq cb)
{
    if (ctx)
        ctx->globalReqCb = cb;
}

void wolfSSH_SetReqSuccess(WOLFSSH_CTX *ctx, WS_CallbackReqSuccess cb)
{
    if (ctx)
        ctx->reqSuccessCb = cb;
}

void wolfSSH_SetReqFailure(WOLFSSH_CTX *ctx, WS_CallbackReqSuccess cb)
{
    if (ctx)
        ctx->reqFailureCb = cb;
}

void wolfSSH_SetGlobalReqCtx(WOLFSSH* ssh, void *ctx)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetGlobalReqCtx()");

    if (ssh)
        ssh->globalReqCtx = ctx;
}

void *wolfSSH_GetGlobalReqCtx(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetGlobalReqCtx()");

    if (ssh)
        return ssh->globalReqCtx;

    return NULL;
}

void wolfSSH_SetReqSuccessCtx(WOLFSSH *ssh, void *ctx)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetReqSuccessCtx()");

    if (ssh)
        ssh->reqSuccessCtx = ctx;
}

void *wolfSSH_GetReqSuccessCtx(WOLFSSH *ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetReqSuccessCtx()");

    if (ssh)
        return ssh->reqSuccessCtx;

    return NULL;
}

void wolfSSH_SetReqFailureCtx(WOLFSSH *ssh, void *ctx)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SetReqFailureCtx()");

    if (ssh)
        ssh->reqFailureCtx = ctx;
}

void *wolfSSH_GetReqFailureCtx(WOLFSSH *ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetReqFailureCtx()");

    if (ssh)
        return ssh->reqFailureCtx;

    return NULL;
}

int wolfSSH_get_error(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_error()");

    if (ssh)
        return ssh->error;

    return WS_SSH_NULL_E;
}


const char* wolfSSH_get_error_name(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_get_error_name()");

    if (ssh)
        return GetErrorString(ssh->error);

    return GetErrorString(WS_SSH_NULL_E);
}


const char* wolfSSH_ErrorToName(int err)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ErrorToName()");

    return GetErrorString(err);
}


#ifndef NO_WOLFSSH_SERVER

const char acceptError[] = "accept error: %s, %d";
const char acceptState[] = "accept state: %s";


int wolfSSH_accept(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_accept()");

    if (ssh == NULL)
        return WS_BAD_ARGUMENT;

    /* check if data pending to be sent */
    if (ssh->outputBuffer.length > 0 &&
            ssh->acceptState < ACCEPT_CLIENT_SESSION_ESTABLISHED) {
        if ((ssh->error = wolfSSH_SendPacket(ssh)) == WS_SUCCESS) {
            WLOG(WS_LOG_DEBUG, "Sent pending packet");

            /* adjust state, a couple of them use multiple sends */
            if (ssh->acceptState != ACCEPT_SERVER_VERSION_SENT &&
                ssh->acceptState != ACCEPT_SERVER_USERAUTH_ACCEPT_SENT &&
                ssh->acceptState != ACCEPT_SERVER_KEXINIT_SENT &&
                ssh->acceptState != ACCEPT_KEYED &&
                ssh->acceptState != ACCEPT_SERVER_CHANNEL_ACCEPT_SENT) {
                WLOG(WS_LOG_DEBUG, "Advancing accept state");
                ssh->acceptState++;
            }

            /* handle in process reply state */
            if (ssh->processReplyState == PROCESS_PACKET) {
                WLOG(WS_LOG_DEBUG, "PR3: peerMacSz = %u", ssh->peerMacSz);
                ssh->inputBuffer.idx += ssh->peerMacSz;
                WLOG(WS_LOG_DEBUG, "PR4: Shrinking input buffer");
                ShrinkBuffer(&ssh->inputBuffer, 1);
                ssh->processReplyState = PROCESS_INIT;

                WLOG(WS_LOG_DEBUG, "PR5: txCount = %u, rxCount = %u",
                    ssh->txCount, ssh->rxCount);
            }
        }
        else {
            return WS_FATAL_ERROR;
        }
    }

    while (ssh->acceptState != ACCEPT_CLIENT_SESSION_ESTABLISHED) {
        switch (ssh->acceptState) {

            case ACCEPT_BEGIN:
                if ( (ssh->error = SendProtoId(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError, "BEGIN", ssh->error);
                    return WS_FATAL_ERROR;
                }
                ssh->acceptState = ACCEPT_SERVER_VERSION_SENT;
                WLOG(WS_LOG_DEBUG, acceptState, "SERVER_VERSION_SENT");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_SERVER_VERSION_SENT:
                while (ssh->clientState < CLIENT_VERSION_DONE) {
                    if ( (ssh->error = DoProtoId(ssh)) < WS_SUCCESS) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "SERVER_VERSION_SENT", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }
                ssh->acceptState = ACCEPT_CLIENT_VERSION_DONE;
                WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_VERSION_DONE");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_CLIENT_VERSION_DONE:
                if ( (ssh->error = SendKexInit(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "CLIENT_VERSION_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
                ssh->acceptState = ACCEPT_SERVER_KEXINIT_SENT;
                WLOG(WS_LOG_DEBUG, acceptState, "SERVER_KEXINIT_SENT");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_SERVER_KEXINIT_SENT:
                while (ssh->isKeying) {
                    if (DoReceive(ssh) < WS_SUCCESS) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "SERVER_KEXINIT_SENT", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }
                ssh->acceptState = ACCEPT_KEYED;
                WLOG(WS_LOG_DEBUG, acceptState, "KEYED");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_KEYED:
                while (ssh->clientState < CLIENT_USERAUTH_REQUEST_DONE) {
                    if (DoReceive(ssh) < 0) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "KEYED", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }
                ssh->acceptState = ACCEPT_CLIENT_USERAUTH_REQUEST_DONE;
                WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_USERAUTH_REQUEST_DONE");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_CLIENT_USERAUTH_REQUEST_DONE:
                if ( (ssh->error = SendServiceAccept(ssh,
                                ID_SERVICE_USERAUTH)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "CLIENT_USERAUTH_REQUEST_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
                ssh->acceptState = ACCEPT_SERVER_USERAUTH_ACCEPT_SENT;
                WLOG(WS_LOG_DEBUG, acceptState,
                     "ACCEPT_SERVER_USERAUTH_ACCEPT_SENT");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_SERVER_USERAUTH_ACCEPT_SENT:
                while (ssh->clientState < CLIENT_USERAUTH_DONE) {
                    if (DoReceive(ssh) < 0) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "SERVER_USERAUTH_ACCEPT_SENT", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }
                ssh->acceptState = ACCEPT_CLIENT_USERAUTH_DONE;
                WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_USERAUTH_DONE");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_CLIENT_USERAUTH_DONE:
                if ( (ssh->error = SendUserAuthSuccess(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, acceptError,
                         "CLIENT_USERAUTH_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
                ssh->acceptState = ACCEPT_SERVER_USERAUTH_SENT;
                WLOG(WS_LOG_DEBUG, acceptState, "SERVER_USERAUTH_SENT");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_SERVER_USERAUTH_SENT:
                while (ssh->clientState < CLIENT_CHANNEL_OPEN_DONE) {
                    if (DoReceive(ssh) < 0) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "SERVER_USERAUTH_SENT", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }
                ssh->acceptState = ACCEPT_SERVER_CHANNEL_ACCEPT_SENT;
                WLOG(WS_LOG_DEBUG, acceptState, "SERVER_CHANNEL_ACCEPT_SENT");
                FALL_THROUGH;
                /* no break */

            case ACCEPT_SERVER_CHANNEL_ACCEPT_SENT:
                while (ssh->clientState < CLIENT_DONE) {
                    if (DoReceive(ssh) < 0) {
                        WLOG(WS_LOG_DEBUG, acceptError,
                             "SERVER_CHANNEL_ACCEPT_SENT", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                }

#ifdef WOLFSSH_SCP
                if (ChannelCommandIsScp(ssh)) {
                    ssh->acceptState = ACCEPT_INIT_SCP_TRANSFER;
                    WLOG(WS_LOG_DEBUG, acceptState, "ACCEPT_INIT_SCP_TRANSFER");
                    continue;
                }
#endif
#if defined(WOLFSSH_SFTP) && !defined(NO_WOLFSSH_SERVER)
                {
                    const char* cmd = wolfSSH_GetSessionCommand(ssh);
                    if (cmd != NULL &&
                        WOLFSSH_SESSION_SUBSYSTEM == wolfSSH_GetSessionType(ssh)
                        && (WSTRNCMP(cmd, "sftp", 4) == 0)) {
                        ssh->acceptState = ACCEPT_INIT_SFTP;
                        return wolfSSH_SFTP_accept(ssh);
                    }
                }
#endif /* WOLFSSH_SFTP and !NO_WOLFSSH_SERVER */
#ifdef WOLFSSH_AGENT
                if (ssh->useAgent) {
                    WOLFSSH_AGENT_CTX* newAgent;
                    WOLFSSH_CHANNEL* newChannel;

                    WLOG(WS_LOG_AGENT, "Starting agent channel");

                    newAgent = wolfSSH_AGENT_new(ssh->ctx->heap);
                    if (newAgent == NULL) {
                        ssh->error = WS_MEMORY_E;
                        WLOG(WS_LOG_DEBUG, acceptError,
                            "SERVER_USERAUTH_ACCEPT_DONE", ssh->error);
                        return WS_ERROR;
                    }

                    newChannel = ChannelNew(ssh, ID_CHANTYPE_AUTH_AGENT,
                            ssh->ctx->windowSz, ssh->ctx->maxPacketSz);
                    if (newChannel == NULL) {
                        wolfSSH_AGENT_free(newAgent);
                        ssh->error = WS_MEMORY_E;
                        WLOG(WS_LOG_DEBUG, acceptError,
                            "SERVER_USERAUTH_ACCEPT_DONE", ssh->error);
                        return WS_FATAL_ERROR;
                    }

                    ssh->error = SendChannelOpenSession(ssh, newChannel);
                    if (ssh->error < WS_SUCCESS) {
                        if (ssh->error == WS_WANT_WRITE ||
                                ssh->error == WS_WANT_READ) {
                            ChannelAppend(ssh, newChannel);
                        }
                        else {
                            ChannelDelete(newChannel, ssh->ctx->heap);
                            wolfSSH_AGENT_free(newAgent);
                        }
                        WLOG(WS_LOG_DEBUG, acceptError,
                            "SERVER_USERAUTH_ACCEPT_DONE", ssh->error);
                        return WS_FATAL_ERROR;
                    }
                    ChannelAppend(ssh, newChannel);
                    newAgent->channel = newChannel->channel;
                    if (ssh->ctx->agentCb) {
                        ssh->ctx->agentCb(WOLFSSH_AGENT_LOCAL_SETUP,
                                ssh->agentCbCtx);
                    }
                    if (ssh->agent != NULL)
                        wolfSSH_AGENT_free(ssh->agent);
                    ssh->agent = newAgent;
                }
#endif /* WOLFSSH_AGENT */
                ssh->acceptState = ACCEPT_CLIENT_SESSION_ESTABLISHED;
                WLOG(WS_LOG_DEBUG, acceptState, "CLIENT_SESSION_ESTABLISHED");
                break;

#ifdef WOLFSSH_SCP
            case ACCEPT_INIT_SCP_TRANSFER:
                if ( (ssh->error = DoScpRequest(ssh)) < 0) {
                    WLOG(WS_LOG_DEBUG, acceptError, "INIT_SCP_TRANSFER",
                         ssh->error);
                    return WS_FATAL_ERROR;
                }
                return WS_SCP_COMPLETE;
#endif
#ifdef WOLFSSH_SFTP
            case ACCEPT_INIT_SFTP:
                return wolfSSH_SFTP_accept(ssh);
#endif

        }
    } /* end while */

    return WS_SUCCESS;
}

#endif /* NO_WOLFSSH_SERVER */


#ifndef NO_WOLFSSH_CLIENT

const char connectError[] = "connect error: %s, %d";
const char connectState[] = "connect state: %s";


int wolfSSH_connect(WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_connect()");

    if (ssh == NULL)
        return WS_BAD_ARGUMENT;

    /* check if data pending to be sent */
    if (ssh->outputBuffer.length > 0 &&
            ssh->connectState < CONNECT_SERVER_CHANNEL_REQUEST_DONE) {
        if ((ssh->error = wolfSSH_SendPacket(ssh)) == WS_SUCCESS) {
            WLOG(WS_LOG_DEBUG, "Sent pending packet");

            /* adjust state, a couple of them use multiple sends */
            if (ssh->connectState != CONNECT_CLIENT_VERSION_SENT &&
                ssh->connectState != CONNECT_CLIENT_KEXINIT_SENT &&
                ssh->connectState != CONNECT_CLIENT_KEXDH_INIT_SENT &&
                ssh->connectState != CONNECT_CLIENT_USERAUTH_REQUEST_SENT &&
                ssh->connectState != CONNECT_CLIENT_USERAUTH_SENT &&
                ssh->connectState != CONNECT_CLIENT_CHANNEL_OPEN_SESSION_SENT &&
                ssh->connectState != CONNECT_CLIENT_CHANNEL_REQUEST_SENT) {
                WLOG(WS_LOG_DEBUG, "Advancing connect state");
                ssh->connectState++;
            }

            /* handle in process reply state */
            if (ssh->processReplyState == PROCESS_PACKET) {
                WLOG(WS_LOG_DEBUG, "PR3: peerMacSz = %u", ssh->peerMacSz);
                ssh->inputBuffer.idx += ssh->peerMacSz;
                WLOG(WS_LOG_DEBUG, "PR4: Shrinking input buffer");
                ShrinkBuffer(&ssh->inputBuffer, 1);
                ssh->processReplyState = PROCESS_INIT;

                WLOG(WS_LOG_DEBUG, "PR5: txCount = %u, rxCount = %u",
                    ssh->txCount, ssh->rxCount);
            }
        }
        else {
            return WS_FATAL_ERROR;
        }
    }

    switch (ssh->connectState) {

        case CONNECT_BEGIN:
            if ( (ssh->error = SendProtoId(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError, "BEGIN", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_VERSION_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_VERSION_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_VERSION_SENT:
            while (ssh->serverState < SERVER_VERSION_DONE) {
                if ( (ssh->error = DoProtoId(ssh)) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_VERSION_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_VERSION_DONE;
            WLOG(WS_LOG_DEBUG, connectState, "SERVER_VERSION_DONE");
            FALL_THROUGH;
            /* no break */

        case CONNECT_SERVER_VERSION_DONE:
            if ( (ssh->error = SendKexInit(ssh)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError,
                     "SERVER_VERSION_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_KEXINIT_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_KEXINIT_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_KEXINIT_SENT:
            while (ssh->serverState < SERVER_KEXINIT_DONE) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_KEXINIT_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_KEXINIT_DONE;
            WLOG(WS_LOG_DEBUG, connectState, "SERVER_KEXINIT_DONE");
            FALL_THROUGH;
            /* no break */

        case CONNECT_SERVER_KEXINIT_DONE:
            if (ssh->handshake == NULL) {
                return WS_FATAL_ERROR;
            }

            if (ssh->handshake->kexId == ID_DH_GEX_SHA256) {
#ifndef WOLFSSH_NO_DH
                ssh->error = SendKexDhGexRequest(ssh);
#endif
            }
            else
                ssh->error = SendKexDhInit(ssh);
            if (ssh->error < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError,
                     "SERVER_KEXINIT_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_KEXDH_INIT_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_KEXDH_INIT_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_KEXDH_INIT_SENT:
            while (ssh->isKeying) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_KEXDH_INIT_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_KEYED;
            WLOG(WS_LOG_DEBUG, connectState, "KEYED");
            FALL_THROUGH;
            /* no break */

        case CONNECT_KEYED:
            if ( (ssh->error = SendServiceRequest(ssh, ID_SERVICE_USERAUTH)) <
                                                                  WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError, "KEYED", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_USERAUTH_REQUEST_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_USERAUTH_REQUEST_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_USERAUTH_REQUEST_SENT:
            while (ssh->serverState < SERVER_USERAUTH_REQUEST_DONE) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_USERAUTH_REQUEST_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_USERAUTH_REQUEST_DONE;
            WLOG(WS_LOG_DEBUG, connectState, "SERVER_USERAUTH_REQUEST_DONE");
            FALL_THROUGH;
            /* no break */

        case CONNECT_SERVER_USERAUTH_REQUEST_DONE:
            #ifdef WOLFSSH_AGENT
                if (ssh->agentEnabled) {
                    ssh->agent = wolfSSH_AGENT_new(ssh->ctx->heap);
                    if (ssh->agent == NULL) {
                        ssh->agentEnabled = 0;
                        WLOG(WS_LOG_INFO, "Unable to create agent. Disabling.");
                    }
                }
            #endif

            if ( (ssh->error = SendUserAuthRequest(ssh, ID_NONE, 0)) <
                                                                  WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError,
                     "SERVER_USERAUTH_REQUEST_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_USERAUTH_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_USERAUTH_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_USERAUTH_SENT:
            while (ssh->serverState < SERVER_USERAUTH_ACCEPT_DONE) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_USERAUTH_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_USERAUTH_ACCEPT_DONE;
            WLOG(WS_LOG_DEBUG, connectState, "SERVER_USERAUTH_ACCEPT_DONE");
            FALL_THROUGH;
            /* no break */

        case CONNECT_SERVER_USERAUTH_ACCEPT_DONE:
            {
                WOLFSSH_CHANNEL* newChannel;

                newChannel = ChannelNew(ssh, ID_CHANTYPE_SESSION,
                        ssh->ctx->windowSz, ssh->ctx->maxPacketSz);
                if (newChannel == NULL) {
                    ssh->error = WS_MEMORY_E;
                    WLOG(WS_LOG_DEBUG, connectError,
                        "SERVER_USERAUTH_ACCEPT_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
                if ( (ssh->error =
                        SendChannelOpenSession(ssh, newChannel)) < WS_SUCCESS) {
                    if (ssh->error == WS_WANT_WRITE ||
                            ssh->error == WS_WANT_READ) {
                        ChannelAppend(ssh, newChannel);
                    }
                    else {
                        ChannelDelete(newChannel, ssh->ctx->heap);
                    }
                    WLOG(WS_LOG_DEBUG, connectError,
                        "SERVER_USERAUTH_ACCEPT_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
                ChannelAppend(ssh, newChannel);
            }
            ssh->connectState = CONNECT_CLIENT_CHANNEL_OPEN_SESSION_SENT;
            WLOG(WS_LOG_DEBUG, connectState,
                 "CLIENT_CHANNEL_OPEN_SESSION_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_CHANNEL_OPEN_SESSION_SENT:
            while (ssh->serverState < SERVER_CHANNEL_OPEN_DONE) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_CHANNEL_OPEN_SESSION_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_CHANNEL_OPEN_SESSION_DONE;
            WLOG(WS_LOG_DEBUG, connectState,
                 "SERVER_CHANNEL_OPEN_SESSION_DONE");
            FALL_THROUGH;
            /* no break */

        case CONNECT_SERVER_CHANNEL_OPEN_SESSION_DONE:
        #ifdef WOLFSSH_AGENT
            if (ssh->agentEnabled) {
                if ( (ssh->error = SendChannelAgentRequest(ssh))
                        < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                     "SERVER_CHANNEL_OPEN_SESSION_DONE", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
        #endif
            WLOG(WS_LOG_DEBUG, connectState,
                    "CLIENT_CHANNEL_AGENT_REQUEST_SENT");
            ssh->connectState = CONNECT_CLIENT_CHANNEL_AGENT_REQUEST_SENT;
            FALL_THROUGH;

        case CONNECT_CLIENT_CHANNEL_AGENT_REQUEST_SENT:
        #if defined(WOLFSSH_TERM) && !defined(NO_FILESYSTEM)
            if (ssh->sendTerminalRequest) {
                if ( (ssh->error = SendChannelTerminalRequest(ssh))
                        < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                            "CLIENT_CHANNEL_AGENT_REQUEST_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
        #endif
            WLOG(WS_LOG_DEBUG, connectState,
                    "CLIENT_CHANNEL_TERMINAL_REQUEST_SENT");
            ssh->connectState = CONNECT_CLIENT_CHANNEL_TERMINAL_REQUEST_SENT;
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_CHANNEL_TERMINAL_REQUEST_SENT:
            if ( (ssh->error = SendChannelRequest(ssh, ssh->channelName,
                            ssh->channelNameSz)) < WS_SUCCESS) {
                WLOG(WS_LOG_DEBUG, connectError,
                     "SERVER_CHANNEL_OPEN_SESSION_DONE", ssh->error);
                return WS_FATAL_ERROR;
            }
            ssh->connectState = CONNECT_CLIENT_CHANNEL_REQUEST_SENT;
            WLOG(WS_LOG_DEBUG, connectState, "CLIENT_CHANNEL_REQUEST_SENT");
            FALL_THROUGH;
            /* no break */

        case CONNECT_CLIENT_CHANNEL_REQUEST_SENT:
            while (ssh->serverState < SERVER_DONE) {
                if (DoReceive(ssh) < WS_SUCCESS) {
                    WLOG(WS_LOG_DEBUG, connectError,
                         "CLIENT_CHANNEL_REQUEST_SENT", ssh->error);
                    return WS_FATAL_ERROR;
                }
            }
            ssh->connectState = CONNECT_SERVER_CHANNEL_REQUEST_DONE;
            WLOG(WS_LOG_DEBUG, connectState,
                 "SERVER_CHANNEL_REQUEST_DONE");
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_connect()");
    return WS_SUCCESS;
}

#endif /* NO_WOLFSSH_CLIENT */


int wolfSSH_shutdown(WOLFSSH* ssh)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_shutdown()");

    if (ssh == NULL || ssh->channelList == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = SendChannelEof(ssh, ssh->channelList->peerChannel);

    if (ret == WS_SUCCESS)
        ret = SendChannelExit(ssh, ssh->channelList->peerChannel, 0);

    if (ret == WS_SUCCESS)
        ret = SendChannelClose(ssh, ssh->channelList->peerChannel);

    if (ssh != NULL && ssh->channelList == NULL) {
        WLOG(WS_LOG_DEBUG, "channel list was already removed");
        ret = WS_SUCCESS;
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_shutdown(), ret = %d", ret);
    return ret;
}


int wolfSSH_TriggerKeyExchange(WOLFSSH* ssh)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_TriggerKeyExchange()");
    if (ssh == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = ssh->error = SendKexInit(ssh);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_TriggerKeyExchange(), ret = %d", ret);
    return ret;
}


/* gets current input buffer if any without advancing the internal index.
 * returns number of bytes was able to peek at on success */
int wolfSSH_stream_peek(WOLFSSH* ssh, byte* buf, word32 bufSz)
{
    Buffer* inputBuffer;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_stream_peek()");

    if (ssh == NULL || ssh->channelList == NULL)
        return WS_BAD_ARGUMENT;

    inputBuffer = &ssh->channelList->inputBuffer;
    bufSz = min(bufSz, inputBuffer->length - inputBuffer->idx);
    if (buf != NULL) {
        WMEMCPY(buf, inputBuffer->buffer + inputBuffer->idx, bufSz);
    }
    return bufSz;
}


int wolfSSH_stream_read(WOLFSSH* ssh, byte* buf, word32 bufSz)
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
        if (ret < 0 && ret != WS_CHAN_RXD) {
            WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_read(), ret = %d", ret);
            return ret;
        }
        if (ssh->error == WS_CHAN_RXD &&
                ssh->lastRxId != ssh->channelList->channel) {
            return WS_ERROR;
        }
    }

    bufSz = min(bufSz, inputBuffer->length - inputBuffer->idx);
    WMEMCPY(buf, inputBuffer->buffer + inputBuffer->idx, bufSz);
    inputBuffer->idx += bufSz;

    if (!ssh->isKeying && (inputBuffer->length > inputBuffer->bufferSz / 2)) {

        word32 usedSz = inputBuffer->length - inputBuffer->idx;
        word32 bytesToAdd = inputBuffer->idx;
        int sendResult;

        WLOG(WS_LOG_DEBUG, "Making more room: %u", usedSz);
        if (usedSz) {
            WLOG(WS_LOG_DEBUG, "  ...moving data down");
            WMEMMOVE(inputBuffer->buffer,
                     inputBuffer->buffer + bytesToAdd, usedSz);
        }

        sendResult = SendChannelWindowAdjust(ssh, ssh->channelList->channel,
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


int wolfSSH_ChannelIdRead(WOLFSSH* ssh, word32 channelId,
        byte* buf, word32 bufSz)
{
    WOLFSSH_CHANNEL* chan = NULL;
    Buffer* inputBuffer;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelIdRead()");

    if (ssh == NULL || buf == NULL)
        return WS_BAD_ARGUMENT;

    chan = ChannelFind(ssh, channelId, WS_CHANNEL_ID_SELF);
    if (chan == NULL)
        return WS_INVALID_CHANID;

    inputBuffer = &chan->inputBuffer;

    bufSz = min(bufSz, inputBuffer->length - inputBuffer->idx);
    WMEMCPY(buf, inputBuffer->buffer + inputBuffer->idx, bufSz);
    inputBuffer->idx += bufSz;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelIdRead(), rxd = %d", bufSz);
    return bufSz;
}


int wolfSSH_stream_send(WOLFSSH* ssh, byte* buf, word32 bufSz)
{
    int bytesTxd = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_stream_send()");

    if (ssh == NULL || buf == NULL || ssh->channelList == NULL)
        return WS_BAD_ARGUMENT;

    /* case of WANT WRITE and data stored in output buffer */
    if (ssh->outputBuffer.plainSz && ssh->outputBuffer.length != 0) {
        int ret;

        bytesTxd = ssh->outputBuffer.plainSz;
        WLOG(WS_LOG_DEBUG, "Trying to resend %d bytes", bytesTxd);
        ssh->error = WS_SUCCESS;
        ret = wolfSSH_SendPacket(ssh);

        /* return the amount sent on success otherwise return error found */
        return (ret == WS_SUCCESS)? bytesTxd : ret;
    }

    bytesTxd = SendChannelData(ssh, ssh->channelList->channel, buf, bufSz);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_stream_send(), txd = %d", bytesTxd);
    return bytesTxd;
}


int wolfSSH_ChannelIdSend(WOLFSSH* ssh, word32 channelId,
        byte* buf, word32 bufSz)
{
    int bytesTxd = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelIdSend()");

    if (ssh == NULL || buf == NULL)
        return WS_BAD_ARGUMENT;

    /* case of WANT WRITE and data stored in output buffer */
    if (ssh->outputBuffer.plainSz && ssh->outputBuffer.length != 0) {
        int ret;

        bytesTxd = ssh->outputBuffer.plainSz;
        WLOG(WS_LOG_DEBUG, "Trying to resend %d bytes", bytesTxd);
        ssh->error = WS_SUCCESS;
        ret = wolfSSH_SendPacket(ssh);

        /* return the amount sent on success otherwise return error found */
        return (ret == WS_SUCCESS)? bytesTxd : ret;
    }

    bytesTxd = SendChannelData(ssh, channelId, buf, bufSz);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelIdSend(), txd = %d", bytesTxd);
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

int wolfSSH_global_request(WOLFSSH *ssh, const unsigned char* data, word32 dataSz, int reply)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_global_request");
    if (ssh == NULL || data == NULL)
        return WS_BAD_ARGUMENT;
    if (reply != 0 && reply != 1)
        return WS_BAD_ARGUMENT;
    return SendGlobalRequest(ssh, data, dataSz, reply);
}


/* Reads pending data from extended data buffer. Currently can be used to get
 * STDERR information sent across the channel.
 * Returns the number of bytes read on success */
int wolfSSH_extended_data_read(WOLFSSH* ssh, byte* out, word32 outSz)
{
    byte*  buf;
    word32 bufSz;

    if (ssh == NULL || out == NULL) {
        return WS_BAD_ARGUMENT;
    }

    /* sanity check to make sure idx is not in a bad state */
    if (ssh->extDataBuffer.idx > ssh->extDataBuffer.length) {
        WLOG(WS_LOG_ERROR, "Bad internal state for buffer index");
        return WS_INVALID_STATE_E;
    }
    bufSz = min(outSz, ssh->extDataBuffer.length - ssh->extDataBuffer.idx);
    buf = ssh->extDataBuffer.buffer + ssh->extDataBuffer.idx;
    WMEMCPY(out, buf, bufSz);
    ssh->extDataBuffer.idx += bufSz;
    return bufSz;
}


int wolfSSH_SendIgnore(WOLFSSH* ssh, const byte* buf, word32 bufSz)
{
    byte scratch[128];

    (void)buf;
    (void)bufSz;
    WMEMSET(scratch, 0, sizeof(scratch));

    return SendIgnore(ssh, scratch, sizeof(scratch));
}


int wolfSSH_SendDisconnect(WOLFSSH *ssh, word32 reason)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_SendDisconnect");
    return SendDisconnect(ssh, reason);
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


void wolfSSH_CTX_SetPublicKeyCheck(WOLFSSH_CTX* ctx,
        WS_CallbackPublicKeyCheck cb)
{
    if (ctx != NULL) {
        ctx->publicKeyCheckCb = cb;
    }
}


void wolfSSH_SetPublicKeyCheckCtx(WOLFSSH* ssh, void* publicKeyCheckCtx)
{
    if (ssh != NULL) {
        ssh->publicKeyCheckCtx = publicKeyCheckCtx;
    }
}


void* wolfSSH_GetPublicKeyCheckCtx(WOLFSSH* ssh)
{
    if (ssh != NULL) {
        return ssh->publicKeyCheckCtx;
    }
    return NULL;
}


/* Used to set the channel request type sent in wolfSSH connect. The default
 * type set is shell if this function is not called.
 *
 * type     channel type i.e. WOLFSSH_SESSION_SUBSYSTEM
 * name     name or command in the case of subsystem and exec channel types
 * nameSz   size of name buffer
 *
 * returns WS_SUCCESS on success
 */
int wolfSSH_SetChannelType(WOLFSSH* ssh, byte type, byte* name, word32 nameSz)
{
    if (ssh == NULL) {
        return WS_BAD_ARGUMENT;
    }

    switch (type) {
        case WOLFSSH_SESSION_SHELL:
            ssh->connectChannelId = type;
            break;

        case WOLFSSH_SESSION_EXEC:
            if (ssh->ctx->side == WOLFSSH_ENDPOINT_SERVER) {
                WLOG(WS_LOG_DEBUG, "Server side exec unsupported");
                return WS_BAD_ARGUMENT;
            }
            FALL_THROUGH;
            /* no break */

        case WOLFSSH_SESSION_SUBSYSTEM:
            ssh->connectChannelId = type;
            if (name != NULL && nameSz < WOLFSSH_MAX_CHN_NAMESZ) {
                WMEMCPY(ssh->channelName, name, nameSz);
                ssh->channelNameSz = nameSz;
            }
            else {
                WLOG(WS_LOG_DEBUG, "No subsystem name or name was too large");
            }
            break;

#ifdef WOLFSSH_TERM
        case WOLFSSH_SESSION_TERMINAL:
            /* send a pseudo-terminal request and shell channel */
            ssh->sendTerminalRequest = 1;
            ssh->connectChannelId = WOLFSSH_SESSION_SHELL;
            break;
#endif

        default:
            WLOG(WS_LOG_DEBUG, "Unknown channel type");
            return WS_BAD_ARGUMENT;
    }

    return WS_SUCCESS;
}


int wolfSSH_SetUsernameRaw(WOLFSSH* ssh,
        const byte* username, word32 usernameSz)
{
    char* newUsername = NULL;
    int ret = WS_SUCCESS;

    if (ssh == NULL)
        ret = WS_BAD_ARGUMENT;
    if (username == NULL || usernameSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        newUsername = (char*)WMALLOC(usernameSz + 1,
                ssh->ctx->heap, DYNTYPE_STRING);
        if (newUsername == NULL)
            ret = WS_MEMORY_E;
        else {
            WMEMCPY(newUsername, username, usernameSz);
            newUsername[usernameSz] = 0;
            if (ssh->userName != NULL)
                WFREE(ssh->userName, ssh->ctx->heap, DYNTYPE_STRING);
            ssh->userName = newUsername;
            ssh->userNameSz = usernameSz;
        }
    }

    return ret;
}


int wolfSSH_SetUsername(WOLFSSH* ssh, const char* username)
{
    int ret = WS_SUCCESS;

    if (ssh == NULL || username == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        ret = wolfSSH_SetUsernameRaw(ssh,
                (const byte*)username, (word32)WSTRLEN(username));
    }

    return ret;
}


char* wolfSSH_GetUsername(WOLFSSH* ssh)
{
    char* name = NULL;

    if (ssh != NULL)
        name = ssh->userName;

    return name;
}


#include <wolfssl/wolfcrypt/rsa.h>
#include <wolfssl/wolfcrypt/asn_public.h>
#include <wolfssl/wolfcrypt/coding.h>

/* Reads a key from the buffer in to out. If the out buffer doesn't exist
   it is created. The type of key is stored in outType. It'll be a pointer
   to a constant string. Format indicates the format of the key, currently
   either SSH format (a public key) or ASN.1 in DER format (a private key). */
int wolfSSH_ReadKey_buffer(const byte* in, word32 inSz, int format,
        byte** out, word32* outSz, const byte** outType, word32* outTypeSz,
        void* heap)
{
    int ret = WS_SUCCESS;

    (void)heap;

    if (in == NULL || inSz == 0 || out == NULL || outSz == NULL ||
            outType == NULL || outTypeSz == NULL)
        return WS_BAD_ARGUMENT;

    if (format == WOLFSSH_FORMAT_SSH) {
        char* c;
        char* last;
        char* type;
        char* key;

        /*
           SSH format is:
           type AAAABASE64ENCODEDKEYDATA comment
        */
        c = WSTRDUP((const char*)in, heap, DYNTYPE_STRING);
        type = WSTRTOK(c, " \n", &last);
        key = WSTRTOK(NULL, " \n", &last);

        if (type != NULL && key != NULL) {
            const char* name;
            word32 typeSz;
            byte nameId;

            typeSz = (word32)WSTRLEN(type);

            nameId = NameToId(type, typeSz);
            name = IdToName(nameId);
            *outType = (const byte*)name;
            *outTypeSz = typeSz;

            ret = Base64_Decode((byte*)key, (word32)WSTRLEN(key), *out, outSz);
        }

        if (ret != 0)
            ret = WS_ERROR;

        WFREE(c, heap, DYNTYPE_STRING);
    }
    else if (format == WOLFSSH_FORMAT_ASN1) {
        byte* newKey;
        union {
            #ifndef WOLFSSH_NO_RSA
                RsaKey rsa;
            #endif
            #ifndef WOLFSSH_NO_ECDSA
                ecc_key ecc;
            #endif
        } testKey;
        word32 scratch = 0;

        if (*out == NULL) {
            newKey = (byte*)WMALLOC(inSz, heap, DYNTYPE_PRIVKEY);
            if (newKey == NULL)
                return WS_MEMORY_E;
            *out = newKey;
        }
        else {
            if (*outSz < inSz)
                return WS_ERROR;
            newKey = *out;
        }
        *outSz = inSz;
        WMEMCPY(newKey, in, inSz);
#ifndef WOLFSSH_NO_RSA
        /* TODO: This is copied and modified from a function in src/internal.c.
           This and that code should be combined into a single function. */
        if (wc_InitRsaKey(&testKey.rsa, heap) < 0)
            return WS_RSA_E;

        ret = wc_RsaPrivateKeyDecode(in, &scratch, &testKey.rsa, inSz);

        wc_FreeRsaKey(&testKey.rsa);

        if (ret == 0) {
            *outType = (const byte*)IdToName(ID_SSH_RSA);
            *outTypeSz = (word32)WSTRLEN((const char*)*outType);
        }
        else {
#endif
#ifndef WOLFSSH_NO_ECDSA
            byte curveId = ID_UNKNOWN;

            /* Couldn't decode as RSA testKey. Try decoding as ECC testKey. */
            scratch = 0;
            if (wc_ecc_init_ex(&testKey.ecc, heap, INVALID_DEVID) != 0)
                return WS_ECC_E;

            ret = wc_EccPrivateKeyDecode(in, &scratch,
                                         &testKey.ecc, inSz);
            switch (wc_ecc_get_curve_id(testKey.ecc.idx)) {
                case ECC_SECP256R1:
                    curveId = ID_ECDSA_SHA2_NISTP256;
                    break;
                case ECC_SECP384R1:
                    curveId = ID_ECDSA_SHA2_NISTP384;
                    break;
                case ECC_SECP521R1:
                    curveId = ID_ECDSA_SHA2_NISTP521;
                    break;
            }
            wc_ecc_free(&testKey.ecc);

            if (ret == 0) {
                *outType = (const byte*)IdToName(curveId);
                *outTypeSz = (word32)WSTRLEN((const char*)*outType);
            }
            else
                return WS_BAD_FILE_E;
#endif
#ifndef WOLFSSH_NO_RSA
        }
#endif
    }
    else
        ret = WS_ERROR;

    return ret;
}


#ifndef NO_FILESYSTEM

/* Reads a key from the file name into a buffer. If the key starts with the
   string "ssh-rsa" or "ecdsa-sha2-nistp256", it is considered an SSH format
   public key, otherwise it is considered an ASN.1 private key. The buffer
   is passed to wolfSSH_ReadKey_buffer() for processing. */
int wolfSSH_ReadKey_file(const char* name,
        byte** out, word32* outSz, const byte** outType, word32* outTypeSz,
        byte* isPrivate, void* heap)
{
    WFILE* file;
    byte* in;
    word32 inSz;
    int format;
    int ret;

    if (name == NULL)
        return WS_BAD_FILE_E;

    if (out == NULL || outSz == NULL || outType == NULL || outTypeSz == NULL ||
            isPrivate == NULL)
        return WS_BAD_ARGUMENT;

    ret = WFOPEN(&file, name, "rb");
    if (ret != 0 || file == WBADFILE) return WS_BAD_FILE_E;
    if (WFSEEK(file, 0, WSEEK_END) != 0) {
        WFCLOSE(file);
        return WS_BAD_FILE_E;
    }
    inSz = (word32)WFTELL(file);
    WREWIND(file);

    if (inSz > WOLFSSH_MAX_FILE_SIZE || inSz == 0) {
        WFCLOSE(file);
        return WS_BAD_FILE_E;
    }

    in = (byte*)WMALLOC(inSz + 1, heap, DYNTYPE_FILE);
    if (in == NULL) {
        WFCLOSE(file);
        return WS_MEMORY_E;
    }

    ret = (int)XFREAD(in, 1, inSz, file);
    if (ret <= 0 || (word32)ret != inSz) {
        ret = WS_BAD_FILE_E;
    }
    else {
        if (WSTRNSTR((const char*)in,
                    "ssh-rsa", inSz) == (const char*)in ||
                WSTRNSTR((const char*)in,
                    "ecdsa-sha2-nistp", inSz) == (const char*)in) {
            *isPrivate = 0;
            format = WOLFSSH_FORMAT_SSH;
            in[inSz] = 0;
        }
        else {
            *isPrivate = 1;
            format = WOLFSSH_FORMAT_ASN1;
        }

        ret = wolfSSH_ReadKey_buffer(in, inSz, format,
                out, outSz, outType, outTypeSz, heap);
    }

    WFCLOSE(file);
    WFREE(in, heap, DYNTYPE_FILE);

    return ret;
}

#endif

int wolfSSH_CTX_SetBanner(WOLFSSH_CTX* ctx,
                          const char* newBanner)
{
    word32 newBannerSz = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_SetBanner()");

    if (ctx == NULL)
        return WS_BAD_ARGUMENT;

    if (newBanner != NULL) {
        WLOG(WS_LOG_INFO, "  setting banner to: \"%s\"", newBanner);
        newBannerSz = (word32)WSTRLEN(newBanner);
    }

    ctx->banner = newBanner;
    ctx->bannerSz = newBannerSz;

    return WS_SUCCESS;
}


int wolfSSH_CTX_UsePrivateKey_buffer(WOLFSSH_CTX* ctx,
                                   const byte* in, word32 inSz, int format)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_UsePrivateKey_buffer()");
    return wolfSSH_ProcessBuffer(ctx, in, inSz, format, BUFTYPE_PRIVKEY);
}


int wolfSSH_CTX_SetWindowPacketSize(WOLFSSH_CTX* ctx,
                                    word32 windowSz, word32 maxPacketSz)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_CTX_SetWindowPacketSize()");

    if (ctx == NULL)
        return WS_BAD_ARGUMENT;

    if (windowSz == 0)
        windowSz = DEFAULT_WINDOW_SZ;
    if (maxPacketSz == 0)
        maxPacketSz = DEFAULT_MAX_PACKET_SZ;

    ctx->windowSz = windowSz;
    ctx->maxPacketSz = maxPacketSz;

    return WS_SUCCESS;
}


void wolfSSH_GetStats(WOLFSSH* ssh, word32* txCount, word32* rxCount,
                      word32* seq, word32* peerSeq)
{
    word32 rTxCount = 0;
    word32 rRxCount = 0;
    word32 rSeq = 0;
    word32 rPeerSeq = 0;

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


int wolfSSH_KDF(byte hashId, byte keyId,
                byte* key, word32 keySz,
                const byte* k, word32 kSz,
                const byte* h, word32 hSz,
                const byte* sessionId, word32 sessionIdSz)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_KDF()");
    return GenerateKey(hashId, keyId, key, keySz, k, kSz, h, hSz,
                       sessionId, sessionIdSz);
}


WS_SessionType wolfSSH_GetSessionType(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetSessionType()");

    if (ssh && ssh->channelList)
        return (WS_SessionType)ssh->channelList->sessionType;

    return WOLFSSH_SESSION_UNKNOWN;
}


const char* wolfSSH_GetSessionCommand(const WOLFSSH* ssh)
{
    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_GetSessionCommand()");

    if (ssh && ssh->channelList)
        return ssh->channelList->command;

    return NULL;
}


int wolfSSH_worker(WOLFSSH* ssh, word32* channelId)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_worker()");

    if (ssh == NULL)
        ret = WS_BAD_ARGUMENT;

    /* Attempt to send any data pending in the outputBuffer. */
    if (ret == WS_SUCCESS) {
        if (ssh->outputBuffer.length != 0)
            ret = wolfSSH_SendPacket(ssh);
    }

    /* Attempt to receive data from the peer. */
    if (ret == WS_SUCCESS)
        ret = DoReceive(ssh);

    if (ret == WS_CHAN_RXD) {
        if (channelId != NULL)
            *channelId = ssh->lastRxId;
    }

    if (ret == WS_CHAN_RXD)
        WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_worker(), "
                           "data received on channel %u", ssh->lastRxId);
    else
        WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_worker(), ret = %d", ret);
    return ret;
}


int wolfSSH_GetLastRxId(WOLFSSH* ssh, word32* channelId)
{
    int ret = WS_SUCCESS;

    if (ssh == NULL || channelId == NULL)
        ret = WS_ERROR;

    if (ret == WS_SUCCESS)
        *channelId = ssh->lastRxId;

    return ret;
}


#ifdef WOLFSSH_FWD

WOLFSSH_CHANNEL* wolfSSH_ChannelFwdNew(WOLFSSH* ssh,
        const char* host, word32 hostPort,
        const char* origin, word32 originPort)
{
    WOLFSSH_CHANNEL* newChannel = NULL;
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelFwdNew()");

    if (ssh == NULL || ssh->ctx == NULL || host == NULL || origin == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        newChannel = ChannelNew(ssh, ID_CHANTYPE_TCPIP_DIRECT,
                ssh->ctx->windowSz, ssh->ctx->maxPacketSz);
        if (newChannel == NULL)
            ret = WS_MEMORY_E;
    }
    if (ret == WS_SUCCESS)
        ret = ChannelUpdateForward(newChannel,
                host, hostPort, origin, originPort);
    if (ret == WS_SUCCESS)
        ret = SendChannelOpenForward(ssh, newChannel);

    if (ret != WS_SUCCESS) {
        void* heap = (ssh != NULL && ssh->ctx != NULL) ? ssh->ctx->heap : NULL;
        ChannelDelete(newChannel, heap);
        newChannel = NULL;
    }

    if (newChannel != NULL)
        ChannelAppend(ssh, newChannel);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelFwdNew(), newChannel = %p",
            newChannel);
    return newChannel;
}

#endif /* WOLFSSH_FWD */


int wolfSSH_ChannelFree(WOLFSSH_CHANNEL* channel)
{
    int ret;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelFree()");

    if (channel != NULL) {
        ret = ChannelRemove(channel->ssh,
                channel->channel, WS_CHANNEL_ID_SELF);
    }
    else
        ret = WS_BAD_ARGUMENT;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelFree(), ret = %d", ret);
    return ret;
}


int wolfSSH_ChannelGetId(WOLFSSH_CHANNEL* channel, word32* id, byte peer)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelGetId()");

    if (channel == NULL || id == NULL)
        ret = WS_BAD_ARGUMENT;
    else {
        *id = (peer == WS_CHANNEL_ID_SELF) ?
            channel->channel : channel->peerChannel;
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelGetId(), ret = %d", ret);
    return ret;
}


WOLFSSH_CHANNEL* wolfSSH_ChannelFind(WOLFSSH* ssh, word32 id, byte peer)
{
    return ChannelFind(ssh, id, peer);
}


#ifdef WOLFSSH_FWD

int wolfSSH_ChannelSetFwdFd(WOLFSSH_CHANNEL* channel, int fwdFd)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelSetFwdFd()");

    if (channel != NULL)
        channel->fwdFd = fwdFd;
    else
        ret = WS_BAD_ARGUMENT;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelSetFwdFd(), ret = %d", ret);
    return ret;
}


int wolfSSH_ChannelGetFwdFd(const WOLFSSH_CHANNEL* channel)
{
    int fwdFd = -1;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelGetFwdFd()");

    if (channel != NULL)
        fwdFd = channel->fwdFd;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelGetFwdFd(), ret = %d", fwdFd);
    return fwdFd;
}

#endif /* WOLFSSH_FWD */


int wolfSSH_ChannelRead(WOLFSSH_CHANNEL* channel, byte* buf, word32 bufSz)
{
    Buffer* inputBuffer;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelRead()");

    if (channel == NULL || buf == NULL || bufSz == 0)
        return WS_BAD_ARGUMENT;

    inputBuffer = &channel->inputBuffer;
    bufSz = min(bufSz, inputBuffer->length - inputBuffer->idx);
    WMEMCPY(buf, inputBuffer->buffer + inputBuffer->idx, bufSz);
    inputBuffer->idx += bufSz;

    if (!channel->ssh->isKeying &&
        (inputBuffer->length > inputBuffer->bufferSz / 2)) {

        word32 usedSz = inputBuffer->length - inputBuffer->idx;
        word32 bytesToAdd = inputBuffer->idx;
        int sendResult;

        WLOG(WS_LOG_DEBUG, "Making more room: %u", usedSz);
        if (usedSz) {
            WLOG(WS_LOG_DEBUG, "  ...moving data down");
            WMEMMOVE(inputBuffer->buffer,
                     inputBuffer->buffer + bytesToAdd, usedSz);
        }

        sendResult = SendChannelWindowAdjust(channel->ssh, channel->channel,
                                             bytesToAdd);
        if (sendResult != WS_SUCCESS)
            bufSz = sendResult;

        WLOG(WS_LOG_INFO, "  bytesToAdd = %u", bytesToAdd);
        WLOG(WS_LOG_INFO, "  windowSz = %u", channel->windowSz);
        channel->windowSz += bytesToAdd;
        WLOG(WS_LOG_INFO, "  update windowSz = %u", channel->windowSz);

        inputBuffer->length = usedSz;
        inputBuffer->idx = 0;
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelRead(), bytesRxd = %d",
            bufSz);
    return bufSz;
}


int wolfSSH_ChannelSend(WOLFSSH_CHANNEL* channel,
        const byte* buf, word32 bufSz)
{
    int bytesTxd = 0;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelSend(), ID = %d, peerID = %d",
            channel->channel, channel->peerChannel);

#ifdef DEBUG_WOLFSSH
    DumpOctetString(buf, bufSz);
#endif
    if (channel == NULL || buf == NULL)
        bytesTxd = WS_BAD_ARGUMENT;
    else if (!channel->openConfirmed) {
        WLOG(WS_LOG_DEBUG, "Channel not confirmed yet.");
        bytesTxd = WS_CHANNEL_NOT_CONF;
    }
    else {
        WLOG(WS_LOG_DEBUG, "Sending data.");
        bytesTxd = SendChannelData(channel->ssh, channel->channel,
                (byte*)buf, bufSz);
    }

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelSend(), bytesTxd = %d",
            bytesTxd);
    return bytesTxd;
}


int wolfSSH_ChannelExit(WOLFSSH_CHANNEL* channel)
{
    int ret = WS_SUCCESS;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelExit()");

    if (channel == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = SendChannelEof(channel->ssh, channel->peerChannel);

    if (ret == WS_SUCCESS)
        ret = SendChannelClose(channel->ssh, channel->peerChannel);

    if (ret == WS_SUCCESS)
        ret = ChannelRemove(channel->ssh,
                channel->peerChannel, WS_CHANNEL_ID_PEER);

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelExit(), ret = %d", ret);
    return ret;
}


WOLFSSH_CHANNEL* wolfSSH_ChannelNext(WOLFSSH* ssh, WOLFSSH_CHANNEL* channel)
{
    WOLFSSH_CHANNEL* nextChannel = NULL;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelNext()");

    if (ssh != NULL && channel == NULL)
        nextChannel = ssh->channelList;
    else if (channel != NULL)
        nextChannel = channel->next;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelNext(), %s",
            nextChannel == NULL ? "none" : "NEXT!");
    return nextChannel;
}


int wolfSSH_ChannelGetEof(WOLFSSH_CHANNEL* channel)
{
    int eof = 1;

    WLOG(WS_LOG_DEBUG, "Entering wolfSSH_ChannelGetEof()");

    if (channel)
        eof = (int)channel->receivedEof;

    WLOG(WS_LOG_DEBUG, "Leaving wolfSSH_ChannelGetEof(), %s",
            eof ? "true" : "false");
    return eof;
}


#ifdef WOLFSSH_SHOW_SIZES

void wolfSSH_ShowSizes(void)
{
    fprintf(stderr, "wolfSSH struct sizes:\n");
    fprintf(stderr, "  sizeof(struct %s) = %u\n", "WOLFSSH_CTX",
            (word32)sizeof(struct WOLFSSH_CTX));
    fprintf(stderr, "  sizeof(struct %s) = %u\n", "WOLFSSH",
            (word32)sizeof(struct WOLFSSH));
    fprintf(stderr, "  sizeof(struct %s) = %u\n", "HandshakeInfo",
            (word32)sizeof(struct HandshakeInfo));
    fprintf(stderr, "  sizeof(struct %s) = %u\n", "WOLFSSH_CHANNEL",
            (word32)sizeof(struct WOLFSSH_CHANNEL));
    fprintf(stderr, "  sizeof(struct %s) = %u\n", "Buffer",
            (word32)sizeof(struct Buffer));
    #ifdef WOLFSSH_SFTP
        wolfSSH_SFTP_ShowSizes();
    #endif
}

#endif /* WOLFSSH_SHOW_SIZES */
