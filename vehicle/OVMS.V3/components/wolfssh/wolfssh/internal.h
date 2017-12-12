/* internal.h
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
 * The internal module contains the private data and functions. The public
 * API calls into this module to do the work of processing the connections.
 */


#pragma once

#include <wolfssh/ssh.h>
#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/dh.h>
#include <wolfssl/wolfcrypt/aes.h>


#if !defined (ALIGN16)
    #if defined (__GNUC__)
        #define ALIGN16 __attribute__ ( (aligned (16)))
    #elif defined(_MSC_VER)
        /* disable align warning, we want alignment ! */
        #pragma warning(disable: 4324)
        #define ALIGN16 __declspec (align (16))
    #else
        #define ALIGN16
    #endif
#endif


#ifdef __cplusplus
extern "C" {
#endif


WOLFSSH_LOCAL const char* GetErrorString(int);


enum {
    /* Any of the items can be none. */
    ID_NONE,

    /* Encryption IDs */
    ID_AES128_CBC,
    ID_AES128_CTR,
    ID_AES128_GCM_WOLF,

    /* Integrity IDs */
    ID_HMAC_SHA1,
    ID_HMAC_SHA1_96,
    ID_HMAC_SHA2_256,

    /* Key Exchange IDs */
    ID_DH_GROUP1_SHA1,
    ID_DH_GROUP14_SHA1,
    ID_DH_GEX_SHA256,

    /* Public Key IDs */
    ID_SSH_RSA,

    /* UserAuth IDs */
    ID_USERAUTH_PASSWORD,
    ID_USERAUTH_PUBLICKEY,

    /* Channel Type IDs */
    ID_CHANTYPE_SESSION,

    ID_UNKNOWN
};


#define MAX_ENCRYPTION   3
#define MAX_INTEGRITY    2
#define MAX_KEY_EXCHANGE 2
#define MAX_PUBLIC_KEY   1
#define MAX_HMAC_SZ      SHA256_DIGEST_SIZE
#define MIN_BLOCK_SZ     8
#define COOKIE_SZ        16
#define LENGTH_SZ        4
#define PAD_LENGTH_SZ    1
#define MIN_PAD_LENGTH   4
#define BOOLEAN_SZ       1
#define MSG_ID_SZ        1
#define SHA1_96_SZ       12
#define UINT32_SZ        4
#define SSH_PROTO_SZ     7    /* "SSH-2.0" */
#define SSH_PROTO_EOL_SZ 2    /* Just the CRLF */
#ifndef DEFAULT_HIGHWATER_MARK
    #define DEFAULT_HIGHWATER_MARK ((1024 * 1024 * 1024) - (32 * 1024))
#endif
#ifndef DEFAULT_WINDOW_SZ
    #define DEFAULT_WINDOW_SZ (1024 * 1024)
#endif
#ifndef DEFAULT_MAX_PACKET_SZ
    #define DEFAULT_MAX_PACKET_SZ (16 * 1024)
#endif
#define DEFAULT_NEXT_CHANNEL  13013


WOLFSSH_LOCAL uint8_t     NameToId(const char*, uint32_t);
WOLFSSH_LOCAL const char* IdToName(uint8_t);


#define STATIC_BUFFER_LEN AES_BLOCK_SIZE
/* This is one AES block size. We always grab one
 * block size first to decrypt to find the size of
 * the rest of the data. */


typedef struct Buffer {
    void*           heap;         /* Heap for allocations */
    uint32_t        length;       /* total buffer length used */
    uint32_t        idx;          /* idx to part of length already consumed */
    uint8_t*        buffer;       /* place holder for actual buffer */
    uint32_t        bufferSz;     /* current buffer size */
    ALIGN16 uint8_t staticBuffer[STATIC_BUFFER_LEN];
    uint8_t         dynamicFlag;  /* dynamic memory currently in use */
} Buffer;


WOLFSSH_LOCAL int BufferInit(Buffer*, uint32_t, void*);
WOLFSSH_LOCAL int GrowBuffer(Buffer*, uint32_t, uint32_t);
WOLFSSH_LOCAL void ShrinkBuffer(Buffer* buf, int);


/* our wolfSSH Context */
struct WOLFSSH_CTX {
    void*               heap;        /* heap hint */
    WS_CallbackIORecv   ioRecvCb;    /* I/O Receive Callback */
    WS_CallbackIOSend   ioSendCb;    /* I/O Send Callback */
    WS_CallbackUserAuth userAuthCb;  /* User Authentication Callback */
    WS_CallbackHighwater highwaterCb; /* Data Highwater Mark Callback */

    uint8_t*            privateKey;  /* Owned by CTX */
    uint32_t            privateKeySz;
    uint32_t            highwaterMark;
    const char*         banner;
    uint32_t            bannerSz;
};


typedef struct Ciphers {
    Aes aes;
} Ciphers;


typedef struct Keys {
    uint8_t        iv[AES_BLOCK_SIZE];
    uint8_t        ivSz;
    uint8_t        encKey[AES_BLOCK_SIZE];
    uint8_t        encKeySz;
    uint8_t        macKey[MAX_HMAC_SZ];
    uint8_t        macKeySz;
} Keys;


typedef struct HandshakeInfo {
    uint8_t        kexId;
    uint8_t        pubKeyId;
    uint8_t        encryptId;
    uint8_t        macId;
    uint8_t        hashId;
    uint8_t        kexPacketFollows;

    uint8_t        blockSz;
    uint8_t        macSz;

    Keys           clientKeys;
    Keys           serverKeys;
    wc_HashAlg     hash;
    uint8_t        e[257]; /* May have a leading zero, for unsigned. */
    uint32_t       eSz;
    uint8_t*       serverKexInit;
    uint32_t       serverKexInitSz;

    uint32_t       dhGexMinSz;
    uint32_t       dhGexPreferredSz;
    uint32_t       dhGexMaxSz;
} HandshakeInfo;


/* our wolfSSH session */
struct WOLFSSH {
    WOLFSSH_CTX*   ctx;            /* owner context */
    int            error;
    int            rfd;
    int            wfd;
    void*          ioReadCtx;      /* I/O Read  Context handle */
    void*          ioWriteCtx;     /* I/O Write Context handle */
    int            rflags;         /* optional read  flags */
    int            wflags;         /* optional write flags */
    uint32_t       txCount;
    uint32_t       rxCount;
    uint32_t       highwaterMark;
    uint8_t        highwaterFlag;  /* Set when highwater CB called */
    void*          highwaterCtx;
    uint32_t       curSz;
    uint32_t       seq;
    uint32_t       peerSeq;
    uint32_t       packetStartIdx; /* Current send packet start index */
    uint8_t        paddingSz;      /* Current send packet padding size */
    uint8_t        acceptState;
    uint8_t        clientState;
    uint8_t        processReplyState;
    uint8_t        keyingState;

    uint8_t        connReset;
    uint8_t        isClosed;
    uint8_t        clientOpenSSH;

    uint8_t        blockSz;
    uint8_t        encryptId;
    uint8_t        macId;
    uint8_t        macSz;
    uint8_t        peerBlockSz;
    uint8_t        peerEncryptId;
    uint8_t        peerMacId;
    uint8_t        peerMacSz;

    Ciphers        encryptCipher;
    Ciphers        decryptCipher;

    uint32_t       nextChannel;
    WOLFSSH_CHANNEL* channelList;
    uint32_t       channelListSz;
    uint32_t       defaultPeerChannelId;

    Buffer         inputBuffer;
    Buffer         outputBuffer;
    RNG*           rng;

    uint8_t        h[WC_MAX_DIGEST_SIZE];
    uint32_t       hSz;
    uint8_t        k[257]; /* May have a leading zero, for unsigned. */
    uint32_t       kSz;
    uint8_t        sessionId[WC_MAX_DIGEST_SIZE];
    uint32_t       sessionIdSz;

    Keys           clientKeys;
    Keys           serverKeys;
    HandshakeInfo* handshake;

    void*          userAuthCtx;
    uint8_t*       userName;
    uint32_t       userNameSz;
    uint8_t*       pkBlob;
    uint32_t       pkBlobSz;
    uint8_t*       clientId;   /* Save for rekey */
    uint32_t       clientIdSz;
};


struct WOLFSSH_CHANNEL {
    uint8_t  channelType;
    uint8_t  sessionType;
    uint8_t  closeSent;
    uint8_t  receivedEof;
    uint32_t channel;
    uint32_t windowSz;
    uint32_t maxPacketSz;
    uint32_t peerChannel;
    uint32_t peerWindowSz;
    uint32_t peerMaxPacketSz;
    Buffer   inputBuffer;
    char*    command;
    struct WOLFSSH* ssh;
    struct WOLFSSH_CHANNEL* next;
};


WOLFSSH_LOCAL WOLFSSH_CTX* CtxInit(WOLFSSH_CTX*, void*);
WOLFSSH_LOCAL void CtxResourceFree(WOLFSSH_CTX*);
WOLFSSH_LOCAL WOLFSSH* SshInit(WOLFSSH*, WOLFSSH_CTX*);
WOLFSSH_LOCAL void SshResourceFree(WOLFSSH*, void*);

WOLFSSH_LOCAL WOLFSSH_CHANNEL* ChannelNew(WOLFSSH*, uint8_t, uint32_t,
                                          uint32_t, uint32_t);
WOLFSSH_LOCAL void ChannelDelete(WOLFSSH_CHANNEL*, void*);
WOLFSSH_LOCAL WOLFSSH_CHANNEL* ChannelFind(WOLFSSH*, uint32_t, uint8_t);
WOLFSSH_LOCAL int ChannelRemove(WOLFSSH*, uint32_t, uint8_t);
WOLFSSH_LOCAL int ChannelPutData(WOLFSSH_CHANNEL*, uint8_t*, uint32_t);


#ifndef WOLFSSH_USER_IO

/* default I/O handlers */
WOLFSSH_LOCAL int wsEmbedRecv(WOLFSSH*, void*, uint32_t, void*);
WOLFSSH_LOCAL int wsEmbedSend(WOLFSSH*, void*, uint32_t, void*);

#endif /* WOLFSSH_USER_IO */


WOLFSSH_LOCAL int DoReceive(WOLFSSH*);
WOLFSSH_LOCAL int ProcessClientVersion(WOLFSSH*);
WOLFSSH_LOCAL int SendServerVersion(WOLFSSH*);
WOLFSSH_LOCAL int SendKexInit(WOLFSSH*);
WOLFSSH_LOCAL int SendKexDhReply(WOLFSSH*);
WOLFSSH_LOCAL int SendKexDhGexGroup(WOLFSSH*);
WOLFSSH_LOCAL int SendNewKeys(WOLFSSH*);
WOLFSSH_LOCAL int SendUnimplemented(WOLFSSH*);
WOLFSSH_LOCAL int SendDisconnect(WOLFSSH*, uint32_t);
WOLFSSH_LOCAL int SendIgnore(WOLFSSH*, const unsigned char*, uint32_t);
WOLFSSH_LOCAL int SendDebug(WOLFSSH*, byte, const char*);
WOLFSSH_LOCAL int SendServiceAccept(WOLFSSH*);
WOLFSSH_LOCAL int SendUserAuthSuccess(WOLFSSH*);
WOLFSSH_LOCAL int SendUserAuthFailure(WOLFSSH*, uint8_t);
WOLFSSH_LOCAL int SendUserAuthBanner(WOLFSSH*);
WOLFSSH_LOCAL int SendUserAuthPkOk(WOLFSSH*, const uint8_t*, uint32_t,
                                   const uint8_t*, uint32_t);
WOLFSSH_LOCAL int SendRequestSuccess(WOLFSSH*, int);
WOLFSSH_LOCAL int SendChannelOpenConf(WOLFSSH*);
WOLFSSH_LOCAL int SendChannelEof(WOLFSSH*, uint32_t);
WOLFSSH_LOCAL int SendChannelEow(WOLFSSH*, uint32_t);
WOLFSSH_LOCAL int SendChannelExit(WOLFSSH*, uint32_t, int);
WOLFSSH_LOCAL int SendChannelClose(WOLFSSH*, uint32_t);
WOLFSSH_LOCAL int SendChannelData(WOLFSSH*, uint32_t, uint8_t*, uint32_t);
WOLFSSH_LOCAL int SendChannelWindowAdjust(WOLFSSH*, uint32_t, uint32_t);
WOLFSSH_LOCAL int SendChannelSuccess(WOLFSSH*, uint32_t, int);
WOLFSSH_LOCAL int GenerateKey(uint8_t, uint8_t, uint8_t*, uint32_t,
                              const uint8_t*, uint32_t,
                              const uint8_t*, uint32_t,
                              const uint8_t*, uint32_t);


enum KeyingStates {
    KEYING_UNKEYED = 0,

    KEYING_KEXINIT_SENT,
    KEYING_KEXINIT_RECV,
    KEYING_KEXINIT_DONE,

    KEYING_KEXDH_INIT_RECV,
    KEYING_KEXDH_DONE,

    KEYING_USING_KEYS_SENT,
    KEYING_USING_KEYS_RECV,
    KEYING_KEYED
};


enum AcceptStates {
    ACCEPT_BEGIN = 0,
    ACCEPT_SERVER_VERSION_SENT,
    ACCEPT_CLIENT_VERSION_DONE,
    ACCEPT_KEYED,
    ACCEPT_CLIENT_USERAUTH_REQUEST_DONE,
    ACCEPT_SERVER_USERAUTH_ACCEPT_SENT,
    ACCEPT_CLIENT_USERAUTH_DONE,
    ACCEPT_SERVER_USERAUTH_SENT,
    ACCEPT_CLIENT_CHANNEL_REQUEST_DONE,
    ACCEPT_SERVER_CHANNEL_ACCEPT_SENT,
    ACCEPT_CLIENT_SESSION_ESTABLISHED
};


enum ClientStates {
    CLIENT_BEGIN = 0,
    CLIENT_VERSION_DONE,
    CLIENT_KEXINIT_DONE,
    CLIENT_KEXDH_INIT_DONE,
    CLIENT_USING_KEYS,
    CLIENT_USERAUTH_REQUEST_DONE,
    CLIENT_USERAUTH_DONE,
    CLIENT_CHANNEL_OPEN_DONE,
    CLIENT_DONE
};


enum ProcessReplyStates {
    PROCESS_INIT,
    PROCESS_PACKET_LENGTH,
    PROCESS_PACKET_FINISH,
    PROCESS_PACKET
};


enum WS_MessageIds {
    MSGID_DISCONNECT      = 1,
    MSGID_IGNORE          = 2,
    MSGID_UNIMPLEMENTED   = 3,
    MSGID_DEBUG           = 4,
    MSGID_SERVICE_REQUEST = 5,
    MSGID_SERVICE_ACCEPT  = 6,

    MSGID_KEXINIT         = 20,
    MSGID_NEWKEYS         = 21,

    MSGID_KEXDH_INIT      = 30,
    MSGID_KEXDH_REPLY     = 31,

    MSGID_KEXDH_GEX_REQUEST = 34,
    MSGID_KEXDH_GEX_GROUP = 31,
    MSGID_KEXDH_GEX_INIT  = 32,
    MSGID_KEXDH_GEX_REPLY = 33,

    MSGID_USERAUTH_REQUEST = 50,
    MSGID_USERAUTH_FAILURE = 51,
    MSGID_USERAUTH_SUCCESS = 52,
    MSGID_USERAUTH_BANNER  = 53,
    MSGID_USERAUTH_PK_OK   = 60, /* Public Key OK */
    MSGID_USERAUTH_PW_CHRQ = 60, /* Password Change Request */

    MSGID_GLOBAL_REQUEST    = 80,
    MSGID_REQUEST_SUCCESS   = 81,
    MSGID_REQUEST_FAILURE   = 82,

    MSGID_CHANNEL_OPEN      = 90,
    MSGID_CHANNEL_OPEN_CONF = 91,
    MSGID_CHANNEL_WINDOW_ADJUST = 93,
    MSGID_CHANNEL_DATA      = 94,
    MSGID_CHANNEL_EOF       = 96,
    MSGID_CHANNEL_CLOSE     = 97,
    MSGID_CHANNEL_REQUEST   = 98,
    MSGID_CHANNEL_SUCCESS   = 99,
    MSGID_CHANNEL_FAILURE   = 100
};


/* dynamic memory types */
enum WS_DynamicTypes {
    DYNTYPE_CTX,
    DYNTYPE_SSH,
    DYNTYPE_CHANNEL,
    DYNTYPE_BUFFER,
    DYNTYPE_ID,
    DYNTYPE_HS,
    DYNTYPE_CA,
    DYNTYPE_CERT,
    DYNTYPE_PRIVKEY,
    DYNTYPE_PUBKEY,
    DYNTYPE_DH,
    DYNTYPE_RNG,
    DYNTYPE_STRING
};


enum WS_BufferTypes {
    BUFTYPE_CA,
    BUFTYPE_CERT,
    BUFTYPE_PRIVKEY,
    BUFTYPE_PUBKEY
};


WOLFSSH_LOCAL void DumpOctetString(const uint8_t*, uint32_t);


#ifdef __cplusplus
}
#endif

