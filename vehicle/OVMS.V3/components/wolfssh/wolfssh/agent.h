/* agent.h
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
 */


#ifndef _WOLFSSH_AGENT_H_
#define _WOLFSSH_AGENT_H_

#include <wolfssh/settings.h>
#include <wolfssh/ssh.h>

#include <wolfssl/wolfcrypt/hash.h>
#include <wolfssl/wolfcrypt/random.h>
#include <wolfssl/wolfcrypt/aes.h>
#include <wolfssl/wolfcrypt/dh.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include <wolfssl/wolfcrypt/rsa.h>


#ifdef __cplusplus
extern "C" {
#endif


enum WS_AGENT_MessageIds {
    MSGID_AGENT_FAILURE = 5,
    MSGID_AGENT_SUCCESS = 6,
    MSGID_AGENT_REQUEST_IDENTITIES = 11,
    MSGID_AGENT_IDENTITIES_ANSWER = 12,
    MSGID_AGENT_SIGN_REQUEST = 13,
    MSGID_AGENT_SIGN_RESPONSE = 14,
    MSGID_AGENT_ADD_IDENTITY = 17,
    MSGID_AGENT_REMOVE_IDENTITY = 18,
    MSGID_AGENT_REMOVE_ALL_IDENTITIES = 19,
    MSGID_AGENT_ADD_SMARTCARD_KEY = 20,
    MSGID_AGENT_REMOVE_SMARTCARD_KEY = 21,
    MSGID_AGENT_LOCK = 22,
    MSGID_AGENT_UNLOCK = 23,
    MSGID_AGENT_ADD_ID_CONSTRAINED = 25,
    MSGID_AGENT_ADD_SMARTCARD_KEY_CONSTRAINED = 26,
    MSGID_AGENT_EXTENSION = 27,
    MSGID_AGENT_EXTENSION_FAILURE = 28,
};


enum WS_AGENT_ConstraintIds {
    AGENT_CONSTRAIN_LIFETIME = 1,
    AGENT_CONSTRAIN_CONFIRM = 2,
    AGENT_CONSTRAIN_EXTENSION = 3,
};


struct WOLFSSH_AGENT_KEY_RSA {
    byte* n;
    byte* e;
    byte* d;
    byte* iqmp;
    byte* p;
    byte* q;
    word32 nSz, eSz, dSz, iqmpSz, pSz, qSz;
};
typedef struct WOLFSSH_AGENT_KEY_RSA WOLFSSH_AGENT_KEY_RSA;


struct WOLFSSH_AGENT_KEY_ECDSA {
    byte* curveName;
    byte* q;
    byte* d;
    word32 curveNameSz, qSz, dSz;
};
typedef struct WOLFSSH_AGENT_KEY_ECDSA WOLFSSH_AGENT_KEY_ECDSA;


union WOLFSSH_AGENT_KEY {
    struct WOLFSSH_AGENT_KEY_RSA rsa;
    struct WOLFSSH_AGENT_KEY_ECDSA ecdsa;
};
typedef union WOLFSSH_AGENT_KEY WOLFSSH_AGENT_KEY;


struct WOLFSSH_AGENT_ID {
    struct WOLFSSH_AGENT_ID* next;
    byte* keyBuffer;
    byte* keyBlob;
    byte* comment;
    WOLFSSH_AGENT_KEY key;
    word32 keyBufferSz;
    word32 keyBlobSz;
    word32 commentSz;
    byte id[WC_SHA256_DIGEST_SIZE];
    byte keyType;
};
typedef struct WOLFSSH_AGENT_ID WOLFSSH_AGENT_ID;


/* Bit fields for the MSGID_AGENT_SIGN_REQUEST flag. */
#define AGENT_SIGN_RSA_SHA2_256 0x02
#define AGENT_SIGN_RSA_SHA2_512 0x04


enum AgentStates {
    AGENT_STATE_INIT,
    AGENT_STATE_LISTEN,
    AGENT_STATE_CONNECTED,
    AGENT_STATE_SIGNED,
    AGENT_STATE_DONE,
};


struct WOLFSSH_AGENT_CTX {
    void* heap;
    byte* msg;
    WC_RNG rng;
    word32 msgSz;
    word32 msgIdx;
    word32 channel;
    WOLFSSH_AGENT_ID* idList;
    word32 idListSz;
    int error;
    enum AgentStates state;
    int requestSuccess;
    int requestFailure;
};
typedef struct WOLFSSH_AGENT_CTX WOLFSSH_AGENT_CTX;


typedef enum WS_AgentCbAction {
    WOLFSSH_AGENT_LOCAL_SETUP,
    WOLFSSH_AGENT_LOCAL_CLEANUP,
} WS_AgentCbAction;

typedef enum WS_AgentIoCbAction {
    WOLFSSH_AGENT_IO_WRITE,
    WOLFSSH_AGENT_IO_READ,
} WS_AgentIoCbAction;


typedef enum WS_AgentCbError {
    WS_AGENT_SUCCESS,
    WS_AGENT_SETUP_E,
    WS_AGENT_NOT_AVAILABLE,
    WS_AGENT_INVALID_ACTION,
    WS_AGENT_PEER_E,
} WS_AgentCbError;


typedef int (*WS_CallbackAgent)(WS_AgentCbAction, void*);
typedef int (*WS_CallbackAgentIO)(WS_AgentIoCbAction, void*, word32, void*);


WOLFSSH_API WOLFSSH_AGENT_CTX* wolfSSH_AGENT_new(void*);
WOLFSSH_API void wolfSSH_AGENT_free(WOLFSSH_AGENT_CTX*);
WOLFSSH_LOCAL WOLFSSH_AGENT_ID* wolfSSH_AGENT_ID_new(byte, word32, void*);
WOLFSSH_LOCAL void wolfSSH_AGENT_ID_free(WOLFSSH_AGENT_ID*, void*);
WOLFSSH_LOCAL void wolfSSH_AGENT_ID_list_free(WOLFSSH_AGENT_ID*, void*);
WOLFSSH_API int wolfSSH_CTX_set_agent_cb(WOLFSSH_CTX*,
        WS_CallbackAgent, WS_CallbackAgentIO);
WOLFSSH_API int wolfSSH_set_agent_cb_ctx(WOLFSSH*, void*);
WOLFSSH_API int wolfSSH_CTX_AGENT_enable(WOLFSSH_CTX*, byte);
WOLFSSH_API int wolfSSH_AGENT_enable(WOLFSSH*, byte);
WOLFSSH_LOCAL int wolfSSH_AGENT_worker(WOLFSSH*);
WOLFSSH_API int wolfSSH_AGENT_Relay(WOLFSSH*,
        const byte*, word32*, byte*, word32*);
WOLFSSH_API int wolfSSH_AGENT_SignRequest(WOLFSSH*, const byte*, word32,
        byte*, word32*, const byte*, word32, word32);


#ifdef __cplusplus
}
#endif

#endif /* _WOLFSSH_AGENT_H_ */

