/* wolfssh_demo.c
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
#include <stdio.h>
#include <string.h>
#include "r_t4_itcpip.h"

#define WOLFSSH_TEST_SERVER
#define WOLFSSH_TEST_THREADING

#ifdef WOLFSSL_USER_SETTINGS
    #include <wolfssl/wolfcrypt/settings.h>
#else
    #include <wolfssl/options.h>
#endif

#include <wolfssl/wolfcrypt/sha256.h>
#include <wolfssl/wolfcrypt/coding.h>
#include <wolfssh/ssh.h>
#include <wolfssh/certs_test.h>
#include <wolfssl/wolfcrypt/ecc.h>
#include "wolfssh_demo.h"

#ifdef NO_FILESYSTEM
    #include <wolfssh/certs_test.h>
#endif

#define FREQ 10000 /* Hz */

#ifdef __cplusplus
//#include <ios>                        // Remove the comment when you use ios
//_SINT ios_base::Init::init_cnt;       // Remove the comment when you use ios
#endif


static long tick;
static void timeTick(void *pdata)
{
    tick++;
}

#ifndef NO_WOLFSSH_SERVER

static const char serverBanner[] = "wolfSSH Example Server\n";

typedef struct {
    WOLFSSH* ssh;
    ID       fd;
} thread_ctx_t;


#ifndef EXAMPLE_HIGHWATER_MARK
    #define EXAMPLE_HIGHWATER_MARK 0x3FFF8000 /* 1GB - 32kB */
#endif
#ifndef EXAMPLE_BUFFER_SZ
    #define EXAMPLE_BUFFER_SZ 4096
#endif
#define SCRATCH_BUFFER_SZ 1200

static int my_IORecv(WOLFSSH* ssh, void* buff, word32 sz, void* ctx)
{
    int ret;
    ID  cepid;
    if(ctx != NULL)cepid = *(ID *)ctx;
    else return WS_CBIO_ERR_GENERAL;
    
    ret = tcp_rcv_dat(cepid, buff, sz, TMO_FEVR);
    
    return ret;
}

static int my_IOSend(WOLFSSH* ssh, void* buff, word32 sz, void* ctx)
{
    int ret;
    ID  cepid;
    
    if(ctx != NULL)cepid = *(ID *)ctx;
    else return WS_CBIO_ERR_GENERAL;

    ret = tcp_snd_dat(cepid, buff, sz, TMO_FEVR);
    
    return ret;
}

static byte find_char(const byte* str, const byte* buf, word32 bufSz)
{
    const byte* cur;

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
    word32 statsSz;
    word32 txCount, rxCount, seq, peerSeq;

    wolfSSH_GetStats(ctx->ssh, &txCount, &rxCount, &seq, &peerSeq);

    printf(stats, 
            "Statistics for Thread #%u:\r\n"
            "  txCount = %u\r\n  rxCount = %u\r\n"
            "  seq = %u\r\n  peerSeq = %u\r\n",
            0, txCount, rxCount, seq, peerSeq);
    statsSz = (word32)strlen(stats);

    fprintf(stderr, "%s", stats);
    return wolfSSH_stream_send(ctx->ssh, (byte*)stats, statsSz);
}



static int server_worker(void* vArgs)
{
    int ret;
    thread_ctx_t* threadCtx = (thread_ctx_t*)vArgs;
    
    ret = wolfSSH_accept(threadCtx->ssh);

    if (ret == WS_SUCCESS) {
        byte* buf = NULL;
        byte* tmpBuf;
        int bufSz, backlogSz = 0, rxSz, txSz, stop = 0, txSum;

        do {
            bufSz = EXAMPLE_BUFFER_SZ + backlogSz;

            tmpBuf = (byte*)realloc(buf, bufSz);
	   
            if (tmpBuf == NULL)
                stop = 1;
            else
                buf = tmpBuf;
	   
	    XMEMSET(buf, 0, bufSz);
	    
            if (!stop) {
                do {
                    rxSz = wolfSSH_stream_read(threadCtx->ssh,
                                               buf + backlogSz,
                                               EXAMPLE_BUFFER_SZ);
                    if (rxSz <= 0)
                        rxSz = wolfSSH_get_error(threadCtx->ssh);
                } while (rxSz == WS_WANT_READ || rxSz == WS_WANT_WRITE);

                if (rxSz > 0) {
                    backlogSz += rxSz;
                    txSum = 0;
                    txSz = 0;
		    printf("Client said = %s \n", buf);
                    while (backlogSz != txSum && txSz >= 0 && !stop) {
                        txSz = wolfSSH_stream_send(threadCtx->ssh,
                                                   buf + txSum,
                                                   backlogSz - txSum);

                        if (txSz > 0) {
                            byte c;
                            const byte matches[] = { 0x03, 0x05, 0x06, 0x00 };

                            c = find_char(matches, buf + txSum, txSz);
                            switch (c) {
                                case 0x03:
                                    stop = 1;
                                    break;
                                case 0x06:
                                    if (wolfSSH_TriggerKeyExchange(threadCtx->ssh)
                                            != WS_SUCCESS)
                                        stop = 1;
                                    break;
                                case 0x05:
                                    if (dump_stats(threadCtx) <= 0)
                                        stop = 1;
                                    break;
                            }
                            txSum += txSz;
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
    
    wolfSSH_stream_exit(threadCtx->ssh, 0);
    tcp_sht_cep(threadCtx->fd);
    wolfSSH_free(threadCtx->ssh);
    free(threadCtx);

    return 0;
}

/* returns buffer size on success */
static int load_key(byte isEcc, byte* buf, word32 bufSz)
{
    word32 sz = 0;

#ifndef NO_FILESYSTEM
    const char* bufName;
    bufName = isEcc ? "./keys/server-key-ecc.der" :
                       "./keys/server-key-rsa.der" ;
    sz = load_file(bufName, buf, bufSz);
#else
    /* using buffers instead */
    if (isEcc) {
        if (sizeof_ecc_key_der_256 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, ecc_key_der_256, sizeof_ecc_key_der_256);
        sz = sizeof_ecc_key_der_256;
    }
    else {
        if (sizeof_rsa_key_der_2048 > bufSz) {
            return 0;
        }
        WMEMCPY(buf, rsa_key_der_2048, sizeof_rsa_key_der_2048);
        sz = sizeof_rsa_key_der_2048;
    }
#endif

    return sz;
}


static INLINE void c32toa(word32 u32, byte* c)
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
    byte type;
    byte username[32];
    word32 usernameSz;
    byte p[SHA256_DIGEST_SIZE];
    struct PwMap* next;
} PwMap;


typedef struct PwMapList {
    PwMap* head;
} PwMapList;


static PwMap* PwMapNew(PwMapList* list, byte type, const byte* username,
                       word32 usernameSz, const byte* p, word32 pSz)
{
    PwMap* map;

    map = (PwMap*)malloc(sizeof(PwMap));
    if (map != NULL) {
        Sha256 sha;
        byte flatSz[4];

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


static const char samplePublicKeyEccBuffer[] =
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBNkI5JTP6D0lF42tbxX19cE87hztUS6FSDoGvPfiU0CgeNSbI+aFdKIzTP5CQEJSvm25"
    "qUzgDtH7oyaQROUnNvk= hansel\n"
    "ecdsa-sha2-nistp256 AAAAE2VjZHNhLXNoYTItbmlzdHAyNTYAAAAIbmlzdHAyNTYAAA"
    "BBBKAtH8cqaDbtJFjtviLobHBmjCtG56DMkP6A4M2H9zX2/YCg1h9bYS7WHd9UQDwXO1Hh"
    "IZzRYecXh7SG9P4GhRY= gretel\n";


static const char samplePublicKeyRsaBuffer[] =
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


static int LoadPasswordBuffer(byte* buf, word32 bufSz, PwMapList* list)
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
                     (byte*)username, (word32)strlen(username),
                     (byte*)password, (word32)strlen(password)) == NULL ) {

            return -1;
        }
    }

    return 0;
}


static int LoadPublicKeyBuffer(byte* buf, word32 bufSz, PwMapList* list)
{
    char* str = (char*)buf;
    char* delimiter;
    byte* publicKey64;
    word32 publicKey64Sz;
    byte* username;
    word32 usernameSz;
    byte  publicKey[300];
    word32 publicKeySz;

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
        publicKey64 = (byte*)str;
        *delimiter = 0;
        publicKey64Sz = (word32)(delimiter - str);
        str = delimiter + 1;
        delimiter = strchr(str, '\n');
        username = (byte*)str;
        *delimiter = 0;
        usernameSz = (word32)(delimiter - str);
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


static int wsUserAuth(byte authType,
                      WS_UserAuthData* authData,
                      void* ctx)
{
    PwMapList* list;
    PwMap* map;
    byte authHash[SHA256_DIGEST_SIZE];

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
        byte flatSz[4];
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


int server_test(void)
{
    WOLFSSH_CTX* ctx = NULL;
    PwMapList pwMapList;
    word32 defaultHighwater = EXAMPLE_HIGHWATER_MARK;
    int useEcc = 0;
    int multipleConnections = 0;
    ID cepid = 1;
    ID repid = 1;
    ER ercd;
    T_IPV4EP dst_addr = {0, 0};
        
    printf("Start server_test\n");
    if (wolfSSH_Init() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't initialize wolfSSH.\n");
        return(EXIT_FAILURE);
    }

    ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL);
    if (ctx == NULL) {
        fprintf(stderr, "Couldn't allocate SSH CTX data.\n");
        return(EXIT_FAILURE);
    }

    memset(&pwMapList, 0, sizeof(pwMapList));
    wolfSSH_SetUserAuth(ctx, wsUserAuth);
    wolfSSH_CTX_SetBanner(ctx, serverBanner);

    {
        const char* bufName;
        byte buf[SCRATCH_BUFFER_SZ];
        word32 bufSz;

        bufSz = load_key(useEcc, buf, SCRATCH_BUFFER_SZ);
        if (bufSz == 0) {
            fprintf(stderr, "Couldn't load key.\n");
            return(EXIT_FAILURE);
        }
        if (wolfSSH_CTX_UsePrivateKey_buffer(ctx, buf, bufSz,
                                             WOLFSSH_FORMAT_ASN1) < 0) {
            fprintf(stderr, "Couldn't use key buffer.\n");
            return(EXIT_FAILURE);
        }

        bufSz = (word32)strlen(samplePasswordBuffer);
        memcpy(buf, samplePasswordBuffer, bufSz);
        buf[bufSz] = 0;
        LoadPasswordBuffer(buf, bufSz, &pwMapList);

        bufName = useEcc ? samplePublicKeyEccBuffer :
                           samplePublicKeyRsaBuffer;
        bufSz = (word32)strlen(bufName);
        memcpy(buf, bufName, bufSz);
        buf[bufSz] = 0;
        LoadPublicKeyBuffer(buf, bufSz, &pwMapList);
    }
    
    /* Register callbacks */
    wolfSSH_SetIORecv(ctx, my_IORecv);
    wolfSSH_SetIOSend(ctx, my_IOSend);
    
    do {

        WOLFSSH*      ssh;
        thread_ctx_t* threadCtx;

        threadCtx = (thread_ctx_t*)malloc(sizeof(thread_ctx_t));
        if (threadCtx == NULL) {
            fprintf(stderr, "Couldn't allocate thread context data.\n");
            return(EXIT_FAILURE);
        }

        ssh = wolfSSH_new(ctx);
        if (ssh == NULL) {
            fprintf(stderr, "Couldn't allocate SSH data.\n");
            return(EXIT_FAILURE);
        }
        wolfSSH_SetUserAuthCtx(ssh, &pwMapList);
        /* Use the session object for its own highwater callback ctx */
        if (defaultHighwater > 0) {
            wolfSSH_SetHighwaterCtx(ssh, (void*)ssh);
            wolfSSH_SetHighwater(ssh, defaultHighwater);
        }
        printf("Waiting connection from client\n");
        if((ercd = tcp_acp_cep(cepid, repid, &dst_addr, TMO_FEVR)) != E_OK) {
            printf("ERROR TCP Accept: %d\n", ercd);
            return -1;
        }
    	wolfSSH_SetIOReadCtx(ssh, (void *)&cepid);
        wolfSSH_SetIOWriteCtx(ssh, (void *)&cepid);

	threadCtx->ssh = ssh;
        threadCtx->fd = cepid;
        server_worker(threadCtx);
        
    } while (multipleConnections);

    PwMapListDelete(&pwMapList);
    wolfSSH_CTX_free(ctx);
    if (wolfSSH_Cleanup() != WS_SUCCESS) {
        fprintf(stderr, "Couldn't clean up wolfSSH.\n");
        return(EXIT_FAILURE);
    }
#if defined(HAVE_ECC) && defined(FP_ECC) && defined(HAVE_THREAD_LS)
    wc_ecc_fp_free();  /* free per thread cache */
#endif
    printf("End server_test\n");
    return 0;
}

#endif /* NO_WOLFSSH_SERVER */

void wolfSSH_init(void)
{
    uint32_t channel;
    R_CMT_CreatePeriodic(FREQ, &timeTick, &channel);
#if defined(DEBUG_WOLFSSH)
    wolfSSH_Debugging_ON();
#endif

    server_test();
    
}

#ifdef __cplusplus
void abort(void)
{

}
#endif