/* agent.c
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


#ifdef HAVE_CONFIG_H
    #include <config.h>
#endif


#ifdef WOLFSSH_AGENT

#include <wolfssl/options.h>
#include <wolfssl/wolfcrypt/signature.h>
#include <wolfssl/wolfcrypt/asn.h>
#include <wolfssh/agent.h>
#include <wolfssh/internal.h>
#include <wolfssh/log.h>


#ifdef NO_INLINE
    #include <wolfssh/misc.h>
#else
    #define WOLFSSH_MISC_INCLUDED
    #include "src/misc.c"
#endif


#define WLOG_ENTER() do { \
    WLOG(WS_LOG_AGENT, "Entering %s()", __func__); \
} while (0)

#define WLOG_LEAVE(x) do { \
    WLOG(WS_LOG_AGENT, "Leaving %s(), ret = %d", __func__, (x)); \
} while (0)

#ifdef DEBUG_WOLFSSH
    #define DUMP(x,y) do { DumpOctetString((x),(y)); } while (0)
#else
    #define DUMP(x,y)
#endif


/* payloadSz is an estimate, but it shall be greater-than/equal-to
 * the actual value. */
static int PrepareMessage(WOLFSSH_AGENT_CTX* agent, word32 payloadSz)
{
    int ret = WS_SUCCESS;
    byte* msg = NULL;

    WLOG_ENTER();

    if (agent == NULL || payloadSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        if (agent->msg != NULL)
            WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        payloadSz += LENGTH_SZ;
        msg = (byte*)WMALLOC(payloadSz, agent->heap, DYNTYPE_AGENT_BUFFER);
        if (msg == NULL)
            ret = WS_MEMORY_E;
    }

    if (ret == WS_SUCCESS) {
        agent->msg = msg;
        agent->msgSz = payloadSz;
        agent->msgIdx = LENGTH_SZ;
    }

    WLOG_LEAVE(ret);
    return ret;
}


/* payloadSz is the actual payload size. */
static int BundleMessage(WOLFSSH_AGENT_CTX* agent, word32 payloadSz)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL || payloadSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        if (LENGTH_SZ + payloadSz > agent->msgSz ||
                agent->msgIdx > agent->msgSz)

            ret = WS_BUFFER_E;
    }

    if (ret == WS_SUCCESS) {
        c32toa(payloadSz, agent->msg);
        agent->msgSz = LENGTH_SZ + payloadSz;
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int SendFailure(WOLFSSH_AGENT_CTX* agent)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = PrepareMessage(agent, MSG_ID_SZ);

    if (ret == WS_SUCCESS) {
        agent->msg[agent->msgIdx++] = MSGID_AGENT_FAILURE;
        ret = BundleMessage(agent, MSG_ID_SZ);
    }

    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int SendSuccess(WOLFSSH_AGENT_CTX* agent)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = PrepareMessage(agent, MSG_ID_SZ);

    if (ret == WS_SUCCESS) {
        agent->msg[agent->msgIdx++] = MSGID_AGENT_SUCCESS;
        ret = BundleMessage(agent, MSG_ID_SZ);
    }

    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);

        WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        agent->msg = NULL;
        agent->msgSz = 0;
    }

    WLOG_LEAVE(ret);
    return ret;
}


#if 0
static int SendRequestIdentities(WOLFSSH_AGENT_CTX* agent)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = PrepareMessage(agent, MSG_ID_SZ);

    if (ret == WS_SUCCESS) {
        agent->msg[agent->msgIdx++] = MSGID_AGENT_REQUEST_IDENTITIES;
        ret = BundleMessage(agent, MSG_ID_SZ);
    }

    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);
    }

    WLOG_LEAVE(ret);
    return ret;
}
#endif


static int SendIdentitiesAnswer(WOLFSSH_AGENT_CTX* agent)
{
    WOLFSSH_AGENT_ID* id;
    word32 msgSz, idCount = 0;
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        msgSz = MSG_ID_SZ + UINT32_SZ;
        id = agent->idList;
        while (id != NULL) {
            idCount++;
            msgSz += LENGTH_SZ * 2 + id->keyBlobSz + id->commentSz;
            id = id->next;
        }

        ret = PrepareMessage(agent, msgSz);
    }

    if (ret == WS_SUCCESS) {
        byte* msg = agent->msg;
        word32 idx = agent->msgIdx;

        msg[idx++] = MSGID_AGENT_IDENTITIES_ANSWER;
        c32toa(idCount, msg + idx);
        idx += UINT32_SZ;

        id = agent->idList;
        while (id != NULL) {
            c32toa(id->keyBlobSz, msg + idx);
            idx += LENGTH_SZ;
            WMEMCPY(msg + idx, id->keyBlob, id->keyBlobSz);
            idx += id->keyBlobSz;
            c32toa(id->commentSz, msg + idx);
            idx += LENGTH_SZ;
            WMEMCPY(msg + idx, id->comment, id->commentSz);
            idx += id->commentSz;
            id = id->next;
        }

        agent->msgIdx = idx;
        ret = BundleMessage(agent, msgSz);
    }

    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);

        WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        agent->msg = NULL;
        agent->msgSz = 0;
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int SendSignRequest(WOLFSSH_AGENT_CTX* agent,
        const byte* data, word32 dataSz,
        const byte* keyBlob, word32 keyBlobSz,
        word32 flags)
{
    word32 msgSz;
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        msgSz = MSG_ID_SZ + (LENGTH_SZ * 2) + UINT32_SZ + dataSz + keyBlobSz;
        ret = PrepareMessage(agent, msgSz);
    }

    if (ret == WS_SUCCESS) {
        byte* msg = agent->msg;
        word32 idx = agent->msgIdx;

        msg[idx++] = MSGID_AGENT_SIGN_REQUEST;
        c32toa(keyBlobSz, msg + idx);
        idx += LENGTH_SZ;
        WMEMCPY(msg + idx, keyBlob, keyBlobSz);
        idx += keyBlobSz;
        c32toa(dataSz, msg + idx);
        idx += LENGTH_SZ;
        WMEMCPY(msg + idx, data, dataSz);
        idx += dataSz;
        c32toa(flags, msg + idx);
        idx += UINT32_SZ;

        agent->msgIdx = idx;
        ret = BundleMessage(agent, msgSz);
    }

        /*
    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);

        WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        agent->msg = NULL;
        agent->msgSz = 0;
    }
        */

    WLOG_LEAVE(ret);
    return ret;
}


static int SendSignResponse(WOLFSSH_AGENT_CTX* agent,
        const byte* sig, word32 sigSz)
{
    int ret = WS_SUCCESS;
    word32 msgSz;
    WLOG_ENTER();

    if (agent == NULL || sig == NULL || sigSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        msgSz = MSG_ID_SZ + LENGTH_SZ + sigSz;
        ret = PrepareMessage(agent, msgSz);
    }

    if (ret == WS_SUCCESS) {
        agent->msg[agent->msgIdx++] = MSGID_AGENT_SIGN_RESPONSE;
        c32toa(sigSz, agent->msg + agent->msgIdx);
        agent->msgIdx += LENGTH_SZ;
        WMEMCPY(agent->msg + agent->msgIdx, sig, sigSz);
        agent->msgIdx += sigSz;

        ret = BundleMessage(agent, msgSz);
    }

    if (ret == WS_SUCCESS) {
        DUMP(agent->msg, agent->msgSz);

        WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        agent->msg = 0;
        agent->msgSz = 0;
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int PostFailure(WOLFSSH_AGENT_CTX* agent)
{
    WLOG(WS_LOG_AGENT, "Posting failure to agent %p", agent);
    agent->requestFailure = 1;
    return WS_SUCCESS;
}


static int PostSuccess(WOLFSSH_AGENT_CTX* agent)
{
    WLOG(WS_LOG_AGENT, "Posting success to agent %p", agent);
    agent->requestSuccess = 1;
    return WS_SUCCESS;
}


static int PostLock(WOLFSSH_AGENT_CTX* agent,
        const byte* passphrase, word32 passphraseSz)
{
    char pp[32];
    word32 ppSz;

    WLOG(WS_LOG_AGENT, "Posting lock to agent %p", agent);

    ppSz = sizeof(pp) - 1;
    if (passphraseSz < ppSz)
        ppSz = passphraseSz;

    WMEMCPY(pp, passphrase, passphraseSz);
    pp[ppSz] = 0;
    WLOG(WS_LOG_AGENT, "Locking with passphrase '%s'", pp);

    return WS_SUCCESS;
}


static int PostUnlock(WOLFSSH_AGENT_CTX* agent,
        const byte* passphrase, word32 passphraseSz)
{
    char pp[32];
    word32 ppSz;

    WLOG(WS_LOG_AGENT, "Posting unlock to agent %p", agent);

    ppSz = sizeof(pp) - 1;
    if (passphraseSz < ppSz)
        ppSz = passphraseSz;

    WMEMCPY(pp, passphrase, passphraseSz);
    pp[ppSz] = 0;
    WLOG(WS_LOG_AGENT, "Unlocking with passphrase '%s'", pp);

    return WS_SUCCESS;
}


#ifndef WOLFSSH_NO_RSA
static int PostAddRsaId(WOLFSSH_AGENT_CTX* agent,
        byte keyType, byte* key, word32 keySz,
        word32 nSz, word32 eSz, word32 dSz,
        word32 iqmpSz, word32 pSz, word32 qSz,
        word32 commentSz)
{
    WOLFSSH_AGENT_ID* id = NULL;
    wc_Sha256 sha;
    int ret = WS_SUCCESS;

    WLOG_ENTER();
    DUMP(key, keySz);

    id = wolfSSH_AGENT_ID_new(keyType, keySz, agent->heap);
    if (id == NULL)
        ret = WS_MEMORY_E;

    if (ret == WS_SUCCESS) {
        ret = wc_InitSha256(&sha);
        if (ret == 0)
            ret = wc_Sha256Update(&sha, key, nSz + eSz + (LENGTH_SZ * 2));
        if (ret == 0)
            ret = wc_Sha256Final(&sha, id->id);

        if (ret == 0)
            ret = WS_SUCCESS;
        else
            ret = WS_CRYPTO_FAILED;
    }

    if (ret == WS_SUCCESS) {
        WOLFSSH_AGENT_KEY_RSA* rsa = &id->key.rsa;
        word32 keyIdx = 0;

        WMEMCPY(id->keyBuffer, key, keySz);
        id->keyBlob = id->keyBuffer;
        id->keyBlobSz = nSz + eSz + (LENGTH_SZ * 2);

        keyIdx += LENGTH_SZ;

        rsa->n = id->keyBuffer + keyIdx;
        rsa->nSz = nSz;
        keyIdx += nSz + LENGTH_SZ;

        rsa->e = id->keyBuffer + keyIdx;
        rsa->eSz = eSz;
        keyIdx += eSz + LENGTH_SZ;

        rsa->d = id->keyBuffer + keyIdx;
        rsa->dSz = dSz;
        keyIdx += dSz + LENGTH_SZ;

        rsa->iqmp = id->keyBuffer + keyIdx;
        rsa->iqmpSz = iqmpSz;
        keyIdx += iqmpSz + LENGTH_SZ;

        rsa->p = id->keyBuffer + keyIdx;
        rsa->pSz = pSz;
        keyIdx += pSz + LENGTH_SZ;

        rsa->q = id->keyBuffer + keyIdx;
        rsa->qSz = qSz;
        keyIdx += qSz + LENGTH_SZ;

        id->comment = id->keyBuffer + keyIdx;
        id->commentSz = commentSz;

        id->next = agent->idList;
        agent->idList = id;
        agent->idListSz++;
        WLOG(WS_LOG_AGENT, "idListSz = %u", agent->idListSz);
    }
    else if (id != NULL)
        wolfSSH_AGENT_ID_free(id, agent->heap);

    WLOG_LEAVE(ret);
    return ret;
}
#endif


#ifndef WOLFSSH_NO_ECDSA
static int PostAddEcdsaId(WOLFSSH_AGENT_CTX* agent,
        byte keyType, byte* key, word32 keySz,
        word32 curveNameSz, word32 qSz, word32 dSz,
        word32 commentSz)
{
    WOLFSSH_AGENT_ID* id = NULL;
    wc_Sha256 sha;
    int ret = WS_SUCCESS;

    WLOG_ENTER();
    DUMP(key, keySz);

    id = wolfSSH_AGENT_ID_new(keyType, keySz, agent->heap);
    if (id == NULL)
        ret = WS_MEMORY_E;

    if (ret == WS_SUCCESS) {
        ret = wc_InitSha256(&sha);
        if (ret == 0)
            ret = wc_Sha256Update(&sha, key,
                    curveNameSz + qSz + (LENGTH_SZ * 2));
        if (ret == 0)
            ret = wc_Sha256Final(&sha, id->id);

        if (ret == 0)
            ret = WS_SUCCESS;
        else
            ret = WS_CRYPTO_FAILED;
    }

    if (ret == WS_SUCCESS) {
        WOLFSSH_AGENT_KEY_ECDSA* ecdsa = &id->key.ecdsa;
        word32 keyIdx = 0;

        WMEMCPY(id->keyBuffer, key, keySz);
        id->keyBlob = id->keyBuffer;
        id->keyBlobSz = curveNameSz + qSz + (LENGTH_SZ * 2);

        keyIdx += LENGTH_SZ;

        ecdsa->curveName = id->keyBuffer + keyIdx;
        ecdsa->curveNameSz = curveNameSz;
        keyIdx += curveNameSz + LENGTH_SZ;

        ecdsa->q = id->keyBuffer + keyIdx;
        ecdsa->qSz = qSz;
        keyIdx += qSz + LENGTH_SZ;

        ecdsa->d = id->keyBuffer + keyIdx;
        ecdsa->dSz = dSz;
        keyIdx += dSz + LENGTH_SZ;

        id->comment = id->keyBuffer + keyIdx;
        id->commentSz = commentSz;

        id->next = agent->idList;
        agent->idList = id;
        agent->idListSz++;
        WLOG(WS_LOG_AGENT, "idListSz = %u", agent->idListSz);
    }
    else if (id != NULL)
        wolfSSH_AGENT_ID_free(id, agent->heap);

    WLOG_LEAVE(ret);
    return ret;
}
#endif


static int PostRemoveId(WOLFSSH_AGENT_CTX* agent,
        const byte* keyBlob, word32 keyBlobSz)
{
    WOLFSSH_AGENT_ID* cur = NULL;
    WOLFSSH_AGENT_ID* prev = NULL;
    byte id[WC_SHA256_DIGEST_SIZE];

    int ret = WS_SUCCESS;

    if (agent == NULL || keyBlob == NULL || keyBlobSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        wc_Sha256 sha;

        ret = wc_InitSha256(&sha);
        if (ret == 0)
            ret = wc_Sha256Update(&sha, keyBlob, keyBlobSz);
        if (ret == 0)
            ret = wc_Sha256Final(&sha, id);
        ret = (ret == 0) ? WS_SUCCESS : WS_CRYPTO_FAILED;
    }

    if (ret == WS_SUCCESS) {
        cur = agent->idList;

        while (cur != NULL) {
            int match;

            match = WMEMCMP(id, cur->id, WC_SHA256_DIGEST_SIZE);
            if (!match) {
                prev = cur;
                cur = cur->next;
            }
            else {
                if (prev != NULL)
                    prev->next = cur->next;
                else
                    agent->idList = NULL;

                wolfSSH_AGENT_ID_free(cur, agent->heap);
                cur = NULL;
                agent->idListSz--;
                WLOG(WS_LOG_AGENT, "idListSz = %u", agent->idListSz);
            }
        }
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int PostRemoveAllIds(WOLFSSH_AGENT_CTX* agent)
{
    WLOG_ENTER();

    wolfSSH_AGENT_ID_list_free(agent->idList, agent->heap);
    agent->idList = NULL;
    agent->idListSz = 0;
    WLOG(WS_LOG_AGENT, "idListSz = %u", agent->idListSz);

    WLOG_LEAVE(WS_SUCCESS);
    return(WS_SUCCESS);
}


static int PostRequestIds(WOLFSSH_AGENT_CTX* agent)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS)
        ret = SendIdentitiesAnswer(agent);

    WLOG_LEAVE(ret);
    return ret;
}


static WOLFSSH_AGENT_ID* FindKeyId(WOLFSSH_AGENT_ID* id,
        const byte* keyBlob, word32 keyBlobSz)
{
    int ret;
    wc_Sha256 sha;
    byte digest[WC_SHA256_DIGEST_SIZE];

    ret = wc_InitSha256(&sha);
    if (ret == 0)
        ret = wc_Sha256Update(&sha, keyBlob, keyBlobSz);
    if (ret == 0) {
        ret = wc_Sha256Final(&sha, digest);
        ret = (ret == 0) ? WS_SUCCESS : WS_CRYPTO_FAILED;
    }

    if (ret == WS_SUCCESS) {
        while (id != NULL &&
                WMEMCMP(digest, id, WC_SHA256_DIGEST_SIZE) != 0 &&
                WMEMCMP(keyBlob, id->keyBlob, keyBlobSz)) {
            id = id->next;
        }
    }

    return id;
}


static int SignHashRsa(WOLFSSH_AGENT_KEY_RSA* rawKey, enum wc_HashType hashType,
        const byte* digest, word32 digestSz, byte* sig, word32* sigSz,
        WC_RNG* rng, void* heap)
{
    RsaKey key;
    byte encSig[MAX_ENCODED_SIG_SZ];
    int encSigSz;
    int ret = 0;

    wc_InitRsaKey(&key, heap);
    mp_read_unsigned_bin(&key.n, rawKey->n, rawKey->nSz);
    mp_read_unsigned_bin(&key.e, rawKey->e, rawKey->eSz);
    mp_read_unsigned_bin(&key.d, rawKey->d, rawKey->dSz);
    mp_read_unsigned_bin(&key.p, rawKey->p, rawKey->pSz);
    mp_read_unsigned_bin(&key.q, rawKey->q, rawKey->qSz);
    mp_read_unsigned_bin(&key.u, rawKey->iqmp, rawKey->iqmpSz);

    encSigSz = wc_EncodeSignature(encSig, digest, digestSz,
            wc_HashGetOID(hashType));
    if (encSigSz <= 0) {
        WLOG(WS_LOG_DEBUG, "Bad Encode Sig");
        ret = WS_CRYPTO_FAILED;
    }
    else {
        WLOG(WS_LOG_INFO, "Signing hash with RSA.");
        *sigSz = wc_RsaSSL_Sign(encSig, encSigSz, sig, *sigSz, &key, rng);
        if (*sigSz <= 0) {
            WLOG(WS_LOG_DEBUG, "Bad RSA Sign");
            ret = WS_RSA_E;
        }
    }

    wc_FreeRsaKey(&key);
    if (ret != 0)
        ret = WS_RSA_E;

    return ret;
}


static int SignHashEcc(WOLFSSH_AGENT_KEY_ECDSA* rawKey, int curveId,
        const byte* digest, word32 digestSz,
        byte* sig, word32* sigSz, WC_RNG* rng)
{
    ecc_key key;
    int ret;

    ret = wc_ecc_import_private_key_ex(rawKey->d, rawKey->dSz,
            rawKey->q, rawKey->qSz, &key, curveId);

    if (ret == 0) {
        ret = wc_ecc_sign_hash(digest, digestSz, sig, sigSz, rng, &key);
    }
    if (ret != 0)  {
        WLOG(WS_LOG_DEBUG, "SendKexDhReply: Bad ECDSA Sign");
        ret = WS_ECC_E;
    }
    else {
        byte r[MAX_ECC_BYTES + ECC_MAX_PAD_SZ];
        word32 rSz = sizeof(r);
        byte rPad;
        byte s[MAX_ECC_BYTES + ECC_MAX_PAD_SZ];
        word32 sSz = sizeof(s);
        byte sPad;
        int idx;

        ret = wc_ecc_sig_to_rs(sig, *sigSz, r, &rSz, s, &sSz);
        if (ret == 0) {
            idx = 0;
            rPad = (r[0] & 0x80) ? 1 : 0;
            sPad = (s[0] & 0x80) ? 1 : 0;
            *sigSz = (LENGTH_SZ * 2) + rSz + rPad + sSz + sPad;

            c32toa(rSz + rPad, sig + idx);
            idx += LENGTH_SZ;
            if (rPad)
                sig[idx++] = 0;
            WMEMCPY(sig + idx, r, rSz);
            idx += rSz;
            c32toa(sSz + sPad, sig + idx);
            idx += LENGTH_SZ;
            if (sPad)
                sig[idx++] = 0;
            WMEMCPY(sig + idx, s, sSz);
        }
    }

    wc_ecc_free(&key);
    if (ret != 0)
        ret = WS_ECC_E;

    return ret;
}


static int PostSignRequest(WOLFSSH_AGENT_CTX* agent,
        byte* keyBlob, word32 keyBlobSz, byte* data, word32 dataSz,
        word32 flags)
{
    WOLFSSH_AGENT_ID* id = NULL;
    int ret = WS_SUCCESS;
    byte sig[256];
    byte digest[WC_MAX_DIGEST_SIZE];
    word32 sigSz = sizeof(sig);
    word32 digestSz = sizeof(digest);
    enum wc_HashType hashType;
    int curveId = 0, signRsa = 0, signEcc = 0;

    WLOG_ENTER();

    (void)flags;
    (void)curveId;

    if (agent == NULL || keyBlob == NULL || keyBlobSz == 0)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        if ((id = FindKeyId(agent->idList, keyBlob, keyBlobSz)) == NULL) {
            WLOG(WS_LOG_AGENT, "Sign: Key not found.");
            ret = WS_AGENT_NO_KEY_E;
        }
    }

    if (ret == WS_SUCCESS) {
        switch (id->keyType) {
            #if !defined(WOLFSSH_NO_SSH_RSA_SHA2_256) || \
                !defined(WOLFSSH_NO_SSH_RSA_SHA2_512)
            case ID_SSH_RSA:
                signRsa = 1;
                #ifndef WOLFSSH_NO_SSH_RSA_SHA2_256
                if (flags & AGENT_SIGN_RSA_SHA2_256)
                    hashType = WC_HASH_TYPE_SHA256;
                else
                #endif
                #ifndef WOLFSSH_NO_SSH_RSA_SHA2_512
                if (flags & AGENT_SIGN_RSA_SHA2_512)
                    hashType = WC_HASH_TYPE_SHA512;
                else
                #endif
                    ret = WS_INVALID_ALGO_ID;
                break;
            #endif
            #ifndef WOLFSSH_NO_ECDSA_SHA2_NISTP256
            case ID_ECDSA_SHA2_NISTP256:
                hashType = WC_HASH_TYPE_SHA256;
                curveId = ECC_SECP256R1;
                signEcc = 1;
                break;
            #endif
            #ifndef WOLFSSH_NO_ECDSA_SHA2_NISTP384
            case ID_ECDSA_SHA2_NISTP384:
                hashType = WC_HASH_TYPE_SHA384;
                curveId = ECC_SECP384R1;
                signEcc = 1;
                break;
            #endif
            #ifndef WOLFSSH_NO_ECDSA_SHA2_NISTP512
            case ID_ECDSA_SHA2_NISTP521:
                hashType = WC_HASH_TYPE_SHA512;
                curveId = ECC_SECP521R1;
                signEcc = 1;
                break;
            #endif
            default:
                ret = WS_INVALID_ALGO_ID;
        }
    }

    if (ret == WS_SUCCESS) {
        ret = wc_Hash(hashType, data, dataSz, digest, digestSz);
        if (ret != 0)
            ret = WS_CRYPTO_FAILED;
    }

    if (ret == WS_SUCCESS) {
#if !defined(WOLFSSH_NO_SSH_RSA_SHA2_256) || \
    !defined(WOLFSSH_NO_SSH_RSA_SHA2_512)
        if (signRsa)
            ret = SignHashRsa(&id->key.rsa, hashType,
                    digest, digestSz, sig, &sigSz, &agent->rng, agent->heap);
#endif
#if !defined(WOLFSSH_NO_ECDSA_SHA2_NISTP256) || \
    !defined(WOLFSSH_NO_ECDSA_SHA2_NISTP384) || \
    !defined(WOLFSSH_NO_ECDSA_SHA2_NISTP512)
        if (signEcc)
            ret = SignHashEcc(&id->key.ecdsa, curveId, digest, digestSz,
                    sig, &sigSz, &agent->rng);
#endif

        if (ret == WS_SUCCESS)
            ret = SendSignResponse(agent, sig, sigSz);
    }

    WLOG_LEAVE(ret);
    return ret;
}


static int DoFailure(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    WLOG(WS_LOG_AGENT, "Entering DoFailure()");

    (void)buf;
    (void)idx;

    if (len != 0)
        ret = WS_PARSE_E;

    if (ret == WS_SUCCESS)
        ret = PostFailure(agent);

    WLOG(WS_LOG_AGENT, "Leaving DoFailure(), ret = %d", ret);
    return ret;
}


static int DoSuccess(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    (void)buf;
    (void)idx;

    if (len != 0)
        ret = WS_PARSE_E;

    if (ret == WS_SUCCESS)
        ret = PostSuccess(agent);

    WLOG_LEAVE(ret);
    return ret;
}


static int DoRequestIdentities(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    (void)buf;
    (void)idx;

    if (len != 0)
        ret = WS_PARSE_E;

    if (ret == WS_SUCCESS)
        ret = PostRequestIds(agent);

    WLOG_LEAVE(ret);
    return ret;
}


static int DoIdentitiesAnswer(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    WLOG(WS_LOG_AGENT, "Entering DoIdentitiesAnswer()");
    (void)agent;
    (void)buf;
    (void)len;
    (void)idx;
    DUMP(buf + *idx, len);
    WLOG(WS_LOG_AGENT, "Leaving DoIdentitiesAnswer(), ret = %d", ret);
    return ret;
}


static int DoSignRequest(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    byte* keyBlob;
    byte* data;
    word32 begin, keyBlobSz, dataSz, flags;
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        WLOG(WS_LOG_AGENT, "len = %u, idx = %u", len, *idx);
        begin = *idx;

        ret = GetStringRef(&keyBlobSz, &keyBlob, buf, len, &begin);
    }

    if (ret == WS_SUCCESS)
        ret = GetStringRef(&dataSz, &data, buf, len, &begin);

    if (ret == WS_SUCCESS)
        ret = GetUint32(&flags, buf, len, &begin);

    if (ret == WS_SUCCESS)
        ret = PostSignRequest(agent, keyBlob, keyBlobSz, data, dataSz, flags);

    if (ret == WS_SUCCESS)
        *idx = begin;

    WLOG_LEAVE(ret);
    return ret;
}


static int DoSignResponse(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    byte* sig;
    word32 sigSz;
    int ret = WS_SUCCESS;
    WLOG(WS_LOG_AGENT, "Entering DoSignResponse()");

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if ( ret == WS_SUCCESS)
        ret = GetStringRef(&sigSz, &sig, buf, len, idx);

    if (ret == WS_SUCCESS) {
        agent->msg = sig;
        agent->msgSz = sigSz;
    }

    WLOG(WS_LOG_AGENT, "Leaving DoSignResponse(), ret = %d", ret);
    return ret;
}


static int DoAddIdentity(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    word32 begin;
    word32 sz;

    WLOG_ENTER();

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        WLOG(WS_LOG_AGENT, "len = %u, idx = %u", len, *idx);
        begin = *idx;

        if (begin + LENGTH_SZ > len) {
            ret = WS_PARSE_E;
        }
    }

    if (ret == WS_SUCCESS) {
        ato32(buf + begin, &sz);
        begin += LENGTH_SZ;

        if (begin + sz > len)
            ret = WS_PARSE_E;
    }

    if (ret == WS_SUCCESS) {
        byte keyType = NameToId((char*)buf + begin, sz);

        begin += sz;
        if (keyType == ID_SSH_RSA) {
#ifndef WOLFSSH_NO_RSA
            byte* key;
            byte* scratch;
            word32 keySz, nSz, eSz, dSz, iqmpSz, pSz, qSz, commentSz;

            key = buf + begin;

            /* n */
            ret = GetMpint(&nSz, &scratch, buf, len, &begin);

            /* e */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&eSz, &scratch, buf, len, &begin);

            /* d */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&dSz, &scratch, buf, len, &begin);

            /* iqmp */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&iqmpSz, &scratch, buf, len, &begin);

            /* p */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&pSz, &scratch, buf, len, &begin);

            /* q */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&qSz, &scratch, buf, len, &begin);

            /* comment */
            if (ret == WS_SUCCESS)
                ret = GetStringRef(&commentSz, &scratch, buf, len, &begin);

            if (ret == WS_SUCCESS) {
                (void)scratch;

                keySz = nSz + eSz + dSz + iqmpSz + pSz + qSz + commentSz +
                        (LENGTH_SZ * 7);
                ret = PostAddRsaId(agent, keyType, key, keySz,
                        nSz, eSz, dSz, iqmpSz, pSz, qSz, commentSz);
            }
#endif
        }
        else if (keyType == ID_ECDSA_SHA2_NISTP256 ||
                keyType == ID_ECDSA_SHA2_NISTP384 ||
                keyType == ID_ECDSA_SHA2_NISTP521) {
#ifndef WOLFSSH_NO_ECDSA
            byte* key;
            byte* scratch;
            word32 keySz, curveNameSz, qSz, dSz, commentSz;

            key = buf + begin;

            /* curve name */
            if (ret == WS_SUCCESS)
                ret = GetStringRef(&curveNameSz, &scratch, buf, len, &begin);

            /* Q */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&qSz, &scratch, buf, len, &begin);

            /* d */
            if (ret == WS_SUCCESS)
                ret = GetMpint(&dSz, &scratch, buf, len, &begin);

            /* comment */
            if (ret == WS_SUCCESS)
                ret = GetStringRef(&commentSz, &scratch, buf, len, &begin);

            if (ret == WS_SUCCESS) {
                keySz = curveNameSz + qSz + dSz + commentSz +
                        (LENGTH_SZ * 4);
                ret = PostAddEcdsaId(agent, keyType, key, keySz,
                        curveNameSz, qSz, dSz, commentSz);
            }
#endif
        }
        else {
            ret = WS_PARSE_E;
        }
    }

    if (ret == WS_SUCCESS)
        *idx = begin;

    WLOG_LEAVE(ret);
    return ret;
}


static int DoRemoveIdentity(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    byte* keyBlob = NULL;
    word32 keyBlobSz = 0;
    word32 begin;

    WLOG_ENTER();
    DUMP(buf + *idx, len);

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        WLOG(WS_LOG_AGENT, "len = %u, idx = %u", len, *idx);
        begin = *idx;

        if (begin + LENGTH_SZ > len) {
            ret = WS_PARSE_E;
        }
    }

    if (ret == WS_SUCCESS)
        ret = GetStringRef(&keyBlobSz, &keyBlob, buf, len, &begin);

    if (ret == WS_SUCCESS)
        ret = PostRemoveId(agent, keyBlob, keyBlobSz);

    if (ret == WS_SUCCESS)
        *idx = begin;

    WLOG_LEAVE(ret);
    return ret;
}


static int DoRemoveAllIdentities(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;

    WLOG_ENTER();

    if (buf == NULL || len != 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        DUMP(buf + *idx, len);
    }

    if (ret == WS_SUCCESS)
        ret = PostRemoveAllIds(agent);

    WLOG_LEAVE(ret);
    return ret;
}


static int DoAgentLock(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    word32 begin;
    byte* passphrase;
    word32 passphraseSz;

    WLOG(WS_LOG_AGENT, "Entering DoAgentLock()");

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        WLOG(WS_LOG_AGENT, "len = %u, idx = %u", len, *idx);
        begin = *idx;

        if (begin + LENGTH_SZ > len) {
            ret = WS_PARSE_E;
        }
    }

    if (ret == WS_SUCCESS) {
        ato32(buf + begin, &passphraseSz);
        begin += LENGTH_SZ;

        if (begin + passphraseSz > len) {
            ret = WS_PARSE_E;
        }
    }

    if (ret == WS_SUCCESS) {
        passphrase = buf + begin;
        begin += passphraseSz;

        ret = PostLock(agent, passphrase, passphraseSz);
    }

    if (ret == WS_SUCCESS)
        *idx = begin;

    WLOG(WS_LOG_AGENT, "Leaving DoAgentLock(), ret = %d", ret);
    return ret;
}


static int DoAgentUnlock(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    word32 begin;
    byte* passphrase;
    word32 passphraseSz;

    WLOG(WS_LOG_AGENT, "Entering DoAgentUnlock()");

    if (agent == NULL || buf == NULL || len == 0 || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        begin = *idx;

        if (begin + LENGTH_SZ > len)
            ret = WS_PARSE_E;
    }

    if (ret == WS_SUCCESS) {
        ato32(buf + begin, &passphraseSz);
        begin += LENGTH_SZ;

        if (begin + passphraseSz > len)
            ret = WS_PARSE_E;
    }

    if (ret == WS_SUCCESS) {
        passphrase = buf + begin;
        begin += passphraseSz;

        ret = PostUnlock(agent, passphrase, passphraseSz);
    }

    if (ret == WS_SUCCESS)
        *idx = begin;

    WLOG(WS_LOG_AGENT, "Leaving DoAgentUnlock(), ret = %d", ret);
    return ret;
}


static int DoUnimplemented(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    WLOG_ENTER();

    if (agent == NULL || idx == NULL)
        ret = WS_BAD_ARGUMENT;

    (void)buf;
    DUMP(buf + *idx, len);

    /* Just skip the message. */
    if (ret == WS_SUCCESS) {
        *idx = len;
        ret = SendFailure(agent);
    }

    if (ret == WS_SUCCESS)
        ret = WS_UNIMPLEMENTED_E;

    WLOG_LEAVE(ret);
    return ret;
}


/*
   len is the length of the blob of received data
   idx is in the index value into buf
*/
static int DoMessage(WOLFSSH_AGENT_CTX* agent,
        byte* buf, word32 len, word32* idx)
{
    int ret = WS_SUCCESS;
    word32 payloadSz, payloadIdx;
    word32 begin;
    byte msg;

    WLOG(WS_LOG_AGENT, "Entering DoMessage()");

    if (agent == NULL)
        ret = WS_SSH_NULL_E; /* WS_AGENT_NULL_E */

    if (ret == WS_SUCCESS) {
        if (buf == NULL || idx == NULL || len == 0)
            ret = WS_BAD_ARGUMENT;
    }

    if (ret == WS_SUCCESS) {
        WLOG(WS_LOG_AGENT, "len = %u, idx = %u", len, *idx);
        begin = *idx;
        if (begin > len) {
            ret = WS_OVERFLOW_E;
        }
    }

    if (ret == WS_SUCCESS) {
        if (LENGTH_SZ + MSG_ID_SZ + begin > len) {
            ret = WS_OVERFLOW_E;
        }
    }

    if (ret == WS_SUCCESS) {
        ato32(buf + begin, &payloadSz);
        WLOG(WS_LOG_AGENT, "payloadSz = %u", payloadSz);
        begin += LENGTH_SZ;
        if (payloadSz > len - begin) {
            ret = WS_OVERFLOW_E;
        }
    }

    if (ret == WS_SUCCESS) {
        msg = buf[begin++];
        payloadIdx = 0;
        switch (msg) {
            case MSGID_AGENT_FAILURE:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_FAILURE");
                ret = DoFailure(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_SUCCESS:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_SUCCESS");
                ret = DoSuccess(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_REQUEST_IDENTITIES:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_REQUEST_IDENTITIES");
                ret = DoRequestIdentities(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_IDENTITIES_ANSWER:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_IDENTITIES_ANSWER");
                ret = DoIdentitiesAnswer(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_SIGN_REQUEST:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_SIGN_REQUEST");
                ret = DoSignRequest(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_SIGN_RESPONSE:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_SIGN_RESPONSE");
                ret = DoSignResponse(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_ADD_IDENTITY:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_ADD_IDENTITY");
                ret = DoAddIdentity(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_REMOVE_IDENTITY:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_REMOVE_IDENTITY");
                ret = DoRemoveIdentity(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_REMOVE_ALL_IDENTITIES:
                WLOG(WS_LOG_AGENT,
                        "Decoding MSGID_AGENT_REMOVE_ALL_IDENTITIES");
                ret = DoRemoveAllIdentities(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_LOCK:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_LOCK");
                ret = DoAgentLock(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_UNLOCK:
                WLOG(WS_LOG_AGENT, "Decoding MSGID_AGENT_UNLOCK");
                ret = DoAgentUnlock(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
                break;

            case MSGID_AGENT_ADD_ID_CONSTRAINED:
            case MSGID_AGENT_ADD_SMARTCARD_KEY_CONSTRAINED:
            case MSGID_AGENT_EXTENSION:
            case MSGID_AGENT_EXTENSION_FAILURE:
            case MSGID_AGENT_ADD_SMARTCARD_KEY:
            case MSGID_AGENT_REMOVE_SMARTCARD_KEY:
            default:
                WLOG(WS_LOG_AGENT, "Unimplemented message ID (%d)", msg);
                ret = DoUnimplemented(agent,
                        buf + begin, payloadSz - 1, &payloadIdx);
        }
    }

    if (ret == WS_SUCCESS) {
        begin += payloadIdx;
        *idx = begin;
    }

    WLOG(WS_LOG_AGENT, "Leaving DoMessage(), ret = %d", ret);
    return ret;
}


static WOLFSSH_AGENT_CTX* AgentInit(WOLFSSH_AGENT_CTX* agent, void* heap)
{
    WLOG(WS_LOG_AGENT, "Entering AgentInit()");

    if (agent != NULL) {
        int ret;

        WMEMSET(agent, 0, sizeof(WOLFSSH_AGENT_CTX));
        agent->heap = heap;
        agent->state = AGENT_STATE_INIT;

        ret = wc_InitRng(&agent->rng);
        if (ret != 0) {
            WMEMSET(agent, 0, sizeof(WOLFSSH_AGENT_CTX));
            WFREE(agent, heap, DYNTYPE_AGENT);
            agent = NULL;
        }
    }

    WLOG(WS_LOG_AGENT, "Leaving AgentInit(), agent = %p", agent);
    return agent;
}


WOLFSSH_AGENT_CTX* wolfSSH_AGENT_new(void* heap)
{
    WOLFSSH_AGENT_CTX* agent;

    WLOG(WS_LOG_AGENT, "Entering wolfSSH_AGENT_new()");

    agent = (WOLFSSH_AGENT_CTX*)WMALLOC(sizeof(*agent), heap, DYNTYPE_AGENT);
    agent = AgentInit(agent, heap);

    WLOG(WS_LOG_AGENT, "Leaving wolfSSH_AGENT_new(), agent = %p", agent);
    return agent;
}


void wolfSSH_AGENT_free(WOLFSSH_AGENT_CTX* agent)
{
    void* heap = agent->heap;

    WLOG(WS_LOG_AGENT, "Entering wolfSSH_AGENT_free()");

    if (agent != NULL) {
        if (agent->msg != NULL)
            WFREE(agent->msg, agent->heap, DYNTYPE_AGENT_BUFFER);
        wc_FreeRng(&agent->rng);
        wolfSSH_AGENT_ID_list_free(agent->idList, heap);
        WMEMSET(agent, 0, sizeof(*agent));
        WFREE(agent, heap, DYNTYPE_AGENT);
    }

    WLOG(WS_LOG_AGENT, "Leaving wolfSSH_AGENT_free()");
}


WOLFSSH_AGENT_ID* wolfSSH_AGENT_ID_new(byte keyType, word32 keySz, void* heap)
{
    WOLFSSH_AGENT_ID* id = NULL;
    byte* buffer = NULL;

    WLOG(WS_LOG_AGENT, "Entering wolfSSH_AGENT_ID_new()");

    id = (WOLFSSH_AGENT_ID*)WMALLOC(sizeof(WOLFSSH_AGENT_ID), heap,
            DYNTYPE_AGENT_ID);

    if (id != NULL) {
        buffer = (byte*)WMALLOC(keySz, heap, DYNTYPE_STRING);
        if (buffer == NULL) {
            WFREE(id, heap, DYNTYPE_AGENT_ID);
            id = NULL;
        }
    }

    if (id != NULL && buffer != NULL) {
        WMEMSET(id, 0, sizeof(WOLFSSH_AGENT_ID));
        WMEMSET(buffer, 0, keySz);
        id->keyType = keyType;
        id->keyBuffer = buffer;
        id->keyBufferSz = keySz;
    }

    WLOG(WS_LOG_AGENT, "Leaving wolfSSH_AGENT_ID_new(), id = %p", id);
    return id;
}


void wolfSSH_AGENT_ID_free(WOLFSSH_AGENT_ID* id, void* heap)
{
    WLOG(WS_LOG_AGENT, "Entering wolfSSH_AGENT_ID_free()");
    (void)heap;

    if (id != NULL) {
        if (id->keyBuffer != NULL) {
            WMEMSET(id->keyBuffer, 0, id->keyBufferSz);
            WFREE(id->keyBuffer, heap, DYNTYPE_STRING);
        }
        WMEMSET(id, 0, sizeof(WOLFSSH_AGENT_ID));
        WFREE(id, heap, DYNATYPE_AGENT_ID);
    }

    WLOG(WS_LOG_AGENT, "Leaving wolfSSH_AGENT_ID_free()");
}


void wolfSSH_AGENT_ID_list_free(WOLFSSH_AGENT_ID* id, void* heap)
{
    WLOG(WS_LOG_AGENT, "Entering wolfSSH_AGENT_ID_list_free()");

    while (id != NULL) {
        WOLFSSH_AGENT_ID* next = id->next;
        wolfSSH_AGENT_ID_free(id, heap);
        id = next;
    }

    WLOG(WS_LOG_AGENT, "Leaving wolfSSH_AGENT_ID_list_free()");
}


int wolfSSH_CTX_set_agent_cb(WOLFSSH_CTX* ctx,
        WS_CallbackAgent agentCb, WS_CallbackAgentIO agentIoCb)
{
    int ret = WS_SUCCESS;

    if (ctx == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        ctx->agentCb = agentCb;
        ctx->agentIoCb = agentIoCb;
    }

    return ret;
}


int wolfSSH_set_agent_cb_ctx(WOLFSSH* ssh, void* ctx)
{
    int ret = WS_SUCCESS;

    if (ssh == NULL)
        ret = WS_BAD_ARGUMENT;

    if (ret == WS_SUCCESS) {
        ssh->agentCbCtx = ctx;
    }

    return ret;
}


int wolfSSH_CTX_AGENT_enable(WOLFSSH_CTX* ctx, byte isEnabled)
{
    int ret = WS_SUCCESS;

    WLOG_ENTER();

    if (ctx == NULL)
        ret = WS_SSH_CTX_NULL_E;

    if (ret == WS_SUCCESS)
        ctx->agentEnabled = isEnabled;

    WLOG_LEAVE(ret);
    return ret;
}


int wolfSSH_AGENT_enable(WOLFSSH* ssh, byte isEnabled)
{
    int ret = WS_SUCCESS;

    WLOG_ENTER();

    if (ssh == NULL)
        ret = WS_SSH_NULL_E;

    if (ret == WS_SUCCESS)
        ssh->agentEnabled = isEnabled;

    WLOG_LEAVE(ret);
    return ret;
}


int wolfSSH_AGENT_worker(WOLFSSH* ssh)
{
    int ret = WS_SUCCESS;

    WLOG_ENTER();

    if (ssh == NULL)
        ret = WS_SSH_NULL_E;

    while (1) {
        if (ret == WS_SUCCESS) {

            SendSuccess(NULL);
            DoMessage(NULL, NULL, 0, NULL);
            ssh->agent->state = AGENT_STATE_DONE;
            break;
        }
    }

    WLOG_LEAVE(ret);
    return ret;
}


int wolfSSH_AGENT_Relay(WOLFSSH* ssh,
        const byte* msg, word32* msgSz, byte* rsp, word32* rspSz)
{
    WOLFSSH_AGENT_CTX* agent = NULL;
    int ret = WS_SUCCESS, sz;

    WLOG_ENTER();

    if (ssh == NULL)
        ret = WS_SSH_NULL_E;

    if (ret == WS_SUCCESS) {
        if (ssh->agent == NULL)
            ret = WS_AGENT_NULL_E;
    }

    if (ret == WS_SUCCESS) {
        if (msg == NULL || msgSz == NULL || rsp == NULL || rspSz == NULL)
            ret = WS_BAD_ARGUMENT;
    }

    if (ret == WS_SUCCESS) {
        agent = ssh->agent;

        if (agent->state == AGENT_STATE_INIT) {
            if (ssh->ctx->agentCb) {
                ret = ssh->ctx->agentCb(WOLFSSH_AGENT_LOCAL_SETUP,
                        ssh->agentCbCtx);
                if (ret == WS_AGENT_SUCCESS)
                    agent->state = AGENT_STATE_CONNECTED;
                else
                    ret = WS_AGENT_CXN_FAIL;
            }

        }
    }

    if (ret == WS_SUCCESS) {
        /* Write msg to the agent socket. */
        sz = ssh->ctx->agentIoCb(WOLFSSH_AGENT_IO_WRITE,
                (byte*)msg, *msgSz, ssh->agentCbCtx);
        if (sz > 0) {
            *msgSz = (word32)sz;
        }
        else {
            if (ret == WS_CBIO_ERR_GENERAL) {
                if (ssh->ctx->agentCb) {
                    ret = ssh->ctx->agentCb(WOLFSSH_AGENT_LOCAL_SETUP,
                            ssh->agentCbCtx);
                    if (ret != WS_AGENT_SUCCESS)
                        ret = WS_AGENT_CXN_FAIL;
                }

                if (ret == WS_AGENT_SUCCESS) {
                    sz = ssh->ctx->agentIoCb(WOLFSSH_AGENT_IO_WRITE,
                            (byte*)msg, *msgSz, ssh->agentCbCtx);
                    if (sz > 0)
                        *msgSz = (word32)sz;
                    else
                        ret = WS_AGENT_CXN_FAIL;
                }
            }
        }
    }

    if (ret == WS_SUCCESS) {
        sz = ssh->ctx->agentIoCb(WOLFSSH_AGENT_IO_READ,
                rsp, *rspSz, ssh->agentCbCtx);
        if (sz > 0)
            *rspSz = (word32)sz;
        else
            ret = WS_AGENT_CXN_FAIL;

    }

    if (ret == WS_AGENT_SUCCESS)
        ret = WS_SUCCESS;
    else {
        if (agent != NULL)
            agent->error = ret;
        if (ssh != NULL)
            ssh->error = ret;
        ret = WS_ERROR;
    }

    WLOG_LEAVE(ret);
    return ret;
}


int wolfSSH_AGENT_SignRequest(WOLFSSH* ssh,
        const byte* digest, word32 digestSz,
        byte* sig, word32* sigSz,
        const byte* keyBlob, word32 keyBlobSz,
        word32 flags)
{
    int ret = WS_SUCCESS;
    byte rxBuf[512];
    int rxSz;
    word32 idx = 0;
    WOLFSSH_AGENT_CTX* agent = NULL;

    WLOG_ENTER();

    if (ssh == NULL)
        ret = WS_SSH_NULL_E;

    if (ret == WS_SUCCESS) {
        if (ssh->agent == NULL)
            ret = WS_AGENT_NULL_E;
    }

    if (ret == WS_SUCCESS) {
        if (sigSz == NULL)
            ret = WS_BAD_ARGUMENT;
    }

    if (ret == WS_SUCCESS) {
        agent = ssh->agent;
        if (ssh->ctx->agentCb)
            ret = ssh->ctx->agentCb(WOLFSSH_AGENT_LOCAL_SETUP, ssh->agentCbCtx);
    }

    if (ret == WS_SUCCESS)
        ret = SendSignRequest(agent, digest, digestSz,
                keyBlob, keyBlobSz, flags);

    if (ret == WS_SUCCESS)
        ret = ssh->ctx->agentIoCb(WOLFSSH_AGENT_IO_WRITE,
                agent->msg, agent->msgSz, ssh->agentCbCtx);

    if (ret > 0) ret = WS_SUCCESS;

    if (agent != NULL && agent->msg != NULL) {
        WFREE(ssh->agent->msg, ssh->agent->heap, DYNTYPE_AGENT_BUFFER);
        ssh->agent->msg = NULL;
        ssh->agent->msgSz = 0;
    }

    if (ret == WS_SUCCESS) {
        rxSz = ssh->ctx->agentIoCb(WOLFSSH_AGENT_IO_READ,
                rxBuf, sizeof(rxBuf), ssh->agentCbCtx);
        if (rxSz > 0) {
            ret = DoMessage(ssh->agent, rxBuf, rxSz, &idx);
            if (ssh->agent->requestFailure) {
                ssh->agent->requestFailure = 0;
                ret = WS_AGENT_NO_KEY_E;
            }
            else {
                WMEMCPY(sig, ssh->agent->msg, ssh->agent->msgSz);
                *sigSz = ssh->agent->msgSz;
            }
        }
        else ret = WS_AGENT_NO_KEY_E;
    }

    if (agent != NULL) {
        agent->msg = NULL;
        agent->msgSz = 0;
    }

    if (ssh != NULL && ssh->ctx != NULL && ssh->ctx->agentCb != NULL) {
        ssh->ctx->agentCb(WOLFSSH_AGENT_LOCAL_CLEANUP, ssh->agentCbCtx);
    }

    WLOG_LEAVE(ret);
    return ret;
}


#endif /* WOLFSSH_AGENT */
