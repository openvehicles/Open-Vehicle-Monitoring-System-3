/* api.c
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
#include <wolfssh/ssh.h>
#include <wolfssh/internal.h>
#ifdef WOLFSSH_SCP
    #include <wolfssh/wolfscp.h>
#endif
#ifdef WOLFSSH_SFTP
    #define WOLFSSH_TEST_LOCKING
    #define WOLFSSH_TEST_THREADING

    #define WOLFSSH_TEST_SERVER
    #define WOLFSSH_TEST_ECHOSERVER
    #include <wolfssh/test.h>

    #include "examples/echoserver/echoserver.h"

#endif

/* for echoserver test cases */
int myoptind = 0;
char* myoptarg = NULL;


#define Fail(description, result) do {                                         \
    printf("\nERROR - %s line %d failed with:", __FILE__, __LINE__);           \
    printf("\n    expected: "); printf description;                            \
    printf("\n    result:   "); printf result; printf("\n\n");                 \
    abort();                                                                   \
} while(0)

#define Assert(test, description, result) if (!(test)) Fail(description, result)

#define AssertTrue(x)    Assert( (x), ("%s is true",     #x), (#x " => FALSE"))
#define AssertFalse(x)   Assert(!(x), ("%s is false",    #x), (#x " => TRUE"))
#define AssertNotNull(x) Assert( (x), ("%s is not null", #x), (#x " => NULL"))

#define AssertNull(x) do {                                                     \
    void* _x = (void *) (x);                                                   \
                                                                               \
    Assert(!_x, ("%s is null", #x), (#x " => %p", _x));                        \
} while(0)

#define AssertInt(x, y, op, er) do {                                           \
    int _x = x;                                                                \
    int _y = y;                                                                \
                                                                               \
    Assert(_x op _y, ("%s " #op " %s", #x, #y), ("%d " #er " %d", _x, _y));    \
} while(0)

#define AssertIntEQ(x, y) AssertInt(x, y, ==, !=)
#define AssertIntNE(x, y) AssertInt(x, y, !=, ==)
#define AssertIntGT(x, y) AssertInt(x, y,  >, <=)
#define AssertIntLT(x, y) AssertInt(x, y,  <, >=)
#define AssertIntGE(x, y) AssertInt(x, y, >=,  <)
#define AssertIntLE(x, y) AssertInt(x, y, <=,  >)

#define AssertStr(x, y, op, er) do {                                           \
    const char* _x = x;                                                        \
    const char* _y = y;                                                        \
    int   _z = strcmp(_x, _y);                                                 \
                                                                               \
    Assert(_z op 0, ("%s " #op " %s", #x, #y),                                 \
                                            ("\"%s\" " #er " \"%s\"", _x, _y));\
} while(0)

#define AssertStrEQ(x, y) AssertStr(x, y, ==, !=)
#define AssertStrNE(x, y) AssertStr(x, y, !=, ==)
#define AssertStrGT(x, y) AssertStr(x, y,  >, <=)
#define AssertStrLT(x, y) AssertStr(x, y,  <, >=)
#define AssertStrGE(x, y) AssertStr(x, y, >=,  <)
#define AssertStrLE(x, y) AssertStr(x, y, <=,  >)


/* Utility functions */

#define BAD 0xFF

const byte hexDecode[] =
{
    0, 1, 2, 3, 4, 5, 6, 7, 8, 9,
    BAD, BAD, BAD, BAD, BAD, BAD, BAD,
    10, 11, 12, 13, 14, 15,  /* upper case A-F */
    BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD,
    BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD,
    BAD, BAD, BAD, BAD, BAD, BAD, BAD, BAD,
    BAD, BAD,  /* G - ` */
    10, 11, 12, 13, 14, 15   /* lower case a-f */
};  /* A starts at 0x41 not 0x3A */


static int Base16_Decode(const byte* in, word32 inLen,
                         byte* out, word32* outLen)
{
    word32 inIdx = 0;
    word32 outIdx = 0;

    if (inLen == 1 && *outLen && in) {
        byte b = in[inIdx++] - 0x30;  /* 0 starts at 0x30 */

        /* sanity check */
        if (b >=  sizeof(hexDecode)/sizeof(hexDecode[0]))
            return -1;

        b  = hexDecode[b];

        if (b == BAD)
            return -1;

        out[outIdx++] = b;

        *outLen = outIdx;
        return 0;
    }

    if (inLen % 2)
        return -1;

    if (*outLen < (inLen / 2))
        return -1;

    while (inLen) {
        byte b = in[inIdx++] - 0x30;  /* 0 starts at 0x30 */
        byte b2 = in[inIdx++] - 0x30;

        /* sanity checks */
        if (b >=  sizeof(hexDecode)/sizeof(hexDecode[0]))
            return -1;
        if (b2 >= sizeof(hexDecode)/sizeof(hexDecode[0]))
            return -1;

        b  = hexDecode[b];
        b2 = hexDecode[b2];

        if (b == BAD || b2 == BAD)
            return -1;

        out[outIdx++] = (byte)((b << 4) | b2);
        inLen -= 2;
    }

    *outLen = outIdx;
    return 0;
}


static void FreeBins(byte* b1, byte* b2, byte* b3, byte* b4)
{
    if (b1 != NULL) free(b1);
    if (b2 != NULL) free(b2);
    if (b3 != NULL) free(b3);
    if (b4 != NULL) free(b4);
}


/* convert hex string to binary, store size, 0 success (free mem on failure) */
static int ConvertHexToBin(const char* h1, byte** b1, word32* b1Sz,
                           const char* h2, byte** b2, word32* b2Sz,
                           const char* h3, byte** b3, word32* b3Sz,
                           const char* h4, byte** b4, word32* b4Sz)
{
    int ret;

    /* b1 */
    if (h1 && b1 && b1Sz) {
        *b1Sz = (word32)strlen(h1) / 2;
        *b1 = (byte*)malloc(*b1Sz);
        if (*b1 == NULL)
            return -1;
        ret = Base16_Decode((const byte*)h1, (word32)strlen(h1),
                            *b1, b1Sz);
        if (ret != 0) {
            FreeBins(*b1, NULL, NULL, NULL);
            return -1;
        }
    }

    /* b2 */
    if (h2 && b2 && b2Sz) {
        *b2Sz = (word32)strlen(h2) / 2;
        *b2 = (byte*)malloc(*b2Sz);
        if (*b2 == NULL) {
            FreeBins(b1 ? *b1 : NULL, NULL, NULL, NULL);
            return -1;
        }
        ret = Base16_Decode((const byte*)h2, (word32)strlen(h2),
                            *b2, b2Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, *b2, NULL, NULL);
            return -1;
        }
    }

    /* b3 */
    if (h3 && b3 && b3Sz) {
        *b3Sz = (word32)strlen(h3) / 2;
        *b3 = (byte*)malloc(*b3Sz);
        if (*b3 == NULL) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, NULL, NULL);
            return -1;
        }
        ret = Base16_Decode((const byte*)h3, (word32)strlen(h3),
                            *b3, b3Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, *b3, NULL);
            return -1;
        }
    }

    /* b4 */
    if (h4 && b4 && b4Sz) {
        *b4Sz = (word32)strlen(h4) / 2;
        *b4 = (byte*)malloc(*b4Sz);
        if (*b4 == NULL) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, b3 ? *b3 : NULL, NULL);
            return -1;
        }
        ret = Base16_Decode((const byte*)h4, (word32)strlen(h4),
                            *b4, b4Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, b3 ? *b3 : NULL, *b4);
            return -1;
        }
    }

    return 0;
}

#ifdef WOLFSSH_SFTP
byte userPassword[256];
static int sftpUserAuth(byte authType, WS_UserAuthData* authData, void* ctx)
{
    int ret = WOLFSSH_USERAUTH_INVALID_AUTHTYPE;

    if (authType == WOLFSSH_USERAUTH_PASSWORD) {
        const char* defaultPassword = (const char*)ctx;
        word32 passwordSz;

        ret = WOLFSSH_USERAUTH_SUCCESS;
        if (defaultPassword != NULL) {
            passwordSz = (word32)strlen(defaultPassword);
            memcpy(userPassword, defaultPassword, passwordSz);
        }
        else {
            printf("Expecting password set for test cases\n");
            return ret;
        }

        if (ret == WOLFSSH_USERAUTH_SUCCESS) {
            authData->sf.password.password = userPassword;
            authData->sf.password.passwordSz = passwordSz;
        }
    }
    return ret;
}

/* preforms connection to port, sets WOLFSSH_CTX and WOLFSSH on success
 * caller needs to free ctx and ssh when done
 */
static void sftp_client_connect(WOLFSSH_CTX** ctx, WOLFSSH** ssh, int port)
{
    SOCKET_T sockFd = WOLFSSH_SOCKET_INVALID;
    SOCKADDR_IN_T clientAddr;
    socklen_t clientAddrSz = sizeof(clientAddr);
    int ret;
    char* host = (char*)wolfSshIp;
    const char* username = "jill";
    const char* password = "upthehill";

    if (ctx == NULL || ssh == NULL) {
        return;
    }

    *ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL);
    if (*ctx == NULL) {
        return;
    }

    wolfSSH_SetUserAuth(*ctx, sftpUserAuth);
    *ssh = wolfSSH_new(*ctx);
    if (*ssh == NULL) {
        wolfSSH_CTX_free(*ctx);
        *ctx = NULL;
        return;
    }

    build_addr(&clientAddr, host, port);
    tcp_socket(&sockFd);
    ret = connect(sockFd, (const struct sockaddr *)&clientAddr, clientAddrSz);
    if (ret != 0){
        wolfSSH_free(*ssh);
        wolfSSH_CTX_free(*ctx);
        *ctx = NULL;
        *ssh = NULL;
        return;
    }

    wolfSSH_SetUserAuthCtx(*ssh, (void*)password);
    ret = wolfSSH_SetUsername(*ssh, username);
    if (ret == WS_SUCCESS)
        ret = wolfSSH_set_fd(*ssh, (int)sockFd);

    if (ret == WS_SUCCESS)
        ret = wolfSSH_SFTP_connect(*ssh);

    if (ret != WS_SUCCESS){
        wolfSSH_free(*ssh);
        wolfSSH_CTX_free(*ctx);
        *ctx = NULL;
        *ssh = NULL;
        return;
    }
}
#endif /* WOLFSSH_SFTP */


enum WS_TestEndpointTypes {
    TEST_GOOD_ENDPOINT_SERVER = WOLFSSH_ENDPOINT_SERVER,
    TEST_GOOD_ENDPOINT_CLIENT = WOLFSSH_ENDPOINT_CLIENT,
    TEST_BAD_ENDPOINT_NEXT,
    TEST_BAD_ENDPOINT_LAST = 255
};

static void test_wolfSSH_CTX_new(void)
{
    WOLFSSH_CTX* ctx;

    AssertNull(ctx = wolfSSH_CTX_new(TEST_BAD_ENDPOINT_NEXT, NULL));
    wolfSSH_CTX_free(ctx);

    AssertNull(ctx = wolfSSH_CTX_new(TEST_BAD_ENDPOINT_LAST, NULL));
    wolfSSH_CTX_free(ctx);

    AssertNotNull(ctx = wolfSSH_CTX_new(TEST_GOOD_ENDPOINT_SERVER, NULL));
    wolfSSH_CTX_free(ctx);

    AssertNotNull(ctx = wolfSSH_CTX_new(TEST_GOOD_ENDPOINT_CLIENT, NULL));
    wolfSSH_CTX_free(ctx);
}


static void test_server_wolfSSH_new(void)
{
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;

    AssertNull(ssh = wolfSSH_new(NULL));
    wolfSSH_free(ssh);

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
}


static void test_client_wolfSSH_new(void)
{
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;

    AssertNull(ssh = wolfSSH_new(NULL));
    wolfSSH_free(ssh);

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
}


static void test_wolfSSH_set_fd(void)
{
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;
    WS_SOCKET_T fd = 23, check;

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));

    AssertIntNE(WS_SUCCESS, wolfSSH_set_fd(NULL, fd));
    check = wolfSSH_get_fd(NULL);
    AssertFalse(WS_SUCCESS == check);

    AssertIntEQ(WS_SUCCESS, wolfSSH_set_fd(ssh, fd));
    check = wolfSSH_get_fd(ssh);
    AssertTrue(fd == check);
    AssertTrue(0 != check);

    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
}


static void test_wolfSSH_SetUsername(void)
{
#ifndef WOLFSSH_NO_CLIENT
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;
    const char username[] = "johnny";
    const char empty[] = "";
    const char* name = NULL;

    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsername(NULL, NULL));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsernameRaw(NULL, NULL, 0));

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));
    AssertIntEQ(WS_SUCCESS, wolfSSH_SetUsername(ssh, username));
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_CLIENT, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsername(ssh, NULL));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsername(ssh, empty));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsernameRaw(ssh, NULL, 0));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsernameRaw(ssh, NULL, 23));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsernameRaw(ssh,
                (const byte*)empty, 0));
    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsernameRaw(ssh,
                (const byte*)username, 0));
    wolfSSH_free(ssh);
    AssertNotNull(ssh = wolfSSH_new(ctx));
    AssertIntEQ(WS_SUCCESS, wolfSSH_SetUsername(ssh, username));
    AssertIntEQ(WS_SUCCESS, wolfSSH_SetUsernameRaw(ssh,
                (const byte*)username, (word32)strlen(username)));
    AssertNotNull((name = wolfSSH_GetUsername(ssh)));
    AssertIntEQ(0, strcmp(username, name));
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
#endif /* WOLFSSH_NO_CLIENT */
}


enum WS_TestFormatTypes {
    TEST_GOOD_FORMAT_ASN1 = WOLFSSH_FORMAT_ASN1,
    TEST_GOOD_FORMAT_PEM = WOLFSSH_FORMAT_PEM,
    TEST_GOOD_FORMAT_RAW = WOLFSSH_FORMAT_RAW,
    TEST_BAD_FORMAT_NEXT,
    TEST_BAD_FORMAT_LAST = 0xFFFF
};


#ifndef WOLFSSH_NO_ECDSA_SHA2_NISTP256
static const char serverKeyEccDer[] =
    "307702010104206109990b79d25f285a0f5d15cca15654f92b3987212da77d85"
    "7bb87f38c66dd5a00a06082a8648ce3d030107a144034200048113ffa42bb79c"
    "45747a834c61f33fad26cf22cda9a3bca561b47ce662d4c2f755439a31fb8011"
    "20b5124b24f578d7fd22ef4635f005586b5f63c8da1bc4f569";
static const int serverKeyEccCurveId = ECC_SECP256R1;
#elif defined(WOLFSSH_NO_ECDSA_SHA2_NISTP384)
static const char serverKeyEccDer[] =
    "3081a402010104303eadd2bbbf05a7be3a3f7c28151289de5bb3644d7011761d"
    "b56f2a0362fba64f98e64ff986dc4fb8efdb2d6b8da57142a00706052b810400"
    "22a1640362000438d62be418ff573fd0e020d48876c4e1121dfb2d6ebee4895d"
    "7724316d46a23105873f2986d5c712803a6f471ab86850eb063e108961349cf8"
    "b4c6a4cf5e97bd7e51e975e3e9217261506eb9cf3c493d3eb88d467b5f27ebab"
    "2161c00066febd";
static const int serverKeyEccCurveId = ECC_SECP384R1;
#elif defined(WOLFSSH_NO_ECDSA_SHA2_NISTP521)
static const char serverKeyEccDer[] =
    "3081dc0201010442004ca4d86428d9400e7b2df3912eb996c195895043af92e8"
    "6de70ae4df46f22a291a6bb2748aae82580df6c39f49b3ed82f1789ece1b657d"
    "45438cff156534354575a00706052b81040023a18189038186000401f8d0a7c3"
    "c58d841957969f213a94f3da550edf76d8dd171531f35bb069c8bc300d6f6b37"
    "d18046a9717f2c6f59519c827095b29a6313306218c235769400d0f96d000a19"
    "3ba346652beb409a9a45c597a3ed932dd5aaae96bf2f317e5a7ac7458b3c6cdb"
    "aa90c355382cdfcdca7377d92eb20a5e8c74237ca5a345b19e3f1a2290b154";
static const int serverKeyEccCurveId = ECC_SECP521R1;
#endif

#ifndef WOLFSSH_NO_SSH_RSA_SHA1
static const char serverKeyRsaDer[] =
    "308204a30201000282010100da5dad2514761559f340fd3cb86230b36dc0f9ec"
    "ec8b831e9e429cca416ad38ae15234e00d13627ed40fae5c4d04f18dfac5ad77"
    "aa5a05caeff88dabff8a29094c04c2f519cbed1fb1b429d3c36ca923dfa3a0e5"
    "08dead8c71f934886ced3bf06fa50fac59ff6b33f170fb8ca4b345228d9d777a"
    "e5295f8414d999eaeace2d51f3e358fa5b020fc9b52abcb25ed3c230bb3cb1c3"
    "ef58f35094288bc4654af700d997d96b4d8d95a18a6206b450112283b4ea2ae7"
    "d0a820474fff46aec513e1388bf854af3a4d2ff81fd78490d8930506c27d90db"
    "e39cd0c4655a03ad00ac5aa2cdda3f89583753bf2b467aac89412b5a2ee876e7"
    "5ee32985a363eae686607c2d02030100010281ff0f911e06c6aea45705405ccd"
    "3757c8a101f1ffdf23fdce1b20ad1f004c29916b1525071ff1ceaff6daa74386"
    "d0f6c94195df01bec62624c392d7e5419db5fbb6edf468f19025398248e8cf12"
    "899bf572d93e90f9c2e81cf72628ddd5dbee0d97d65dae005b6a19fa59fbf3f2"
    "d2caf4e2c1b5b80ecac76847c234c1043e38f4820159f28a6ef76b5b0abc05a9"
    "2737b9f9068054e8701ab432936bf526c786f4580543f9728fec42a03bba3562"
    "ccecf4b304a2ebae3c87408efe8fdd14bebd83c9c918ca817c06f9e3992eec29"
    "c52756ea1e93c6e80c44ca73684a7fae16251d1225142aec416925c35de6aee4"
    "59801dfabd9f3336939d88d688c95b277b0b6102818100de01abfa65d2fad26f"
    "fe3f576d757f8ce6bdfe08bdc71334620e87b27a2ca9cdca93d83191812dd668"
    "96aa25e3b87ea598a8e8153cc0cedef5ab80b1f5baafac9cc1b34334ae22f718"
    "418663a2448e1b419d2d756f0d5b10195d14aa801fee023ef8b6f6ec658e3889"
    "0d0b50e41149863982db73e53a0f1322abada0789b942102818100fbcd4c5249"
    "3f2c8094914a38ec0f4a7d3a8ebc0490152584fbd368bdefa047fece5bbf1d2a"
    "9427fc5170ffc9e9babe2ba05025d3e1a15733cc5cc77d09f6dcfb72943dca59"
    "5273e06c450ad9da30df2b33d752184101f0df1b01c1d3b79b26f81c8fffc819"
    "fd36d013a57242a3305957b4da2a09e5455a396d70220cba53268d02818100b1"
    "3cc270f093c43cf6be1311984882e11961bb0a7d800e3bf6c0c4e2df19032351"
    "44410829b2e8c650cf5fdd49f503deee86826a5a0b4fdcbe63022691184ea1ce"
    "aff18e88e330f4f5ff71ebdf233e145288ca3f03beb4e1a06e284e8a65735d85"
    "aa885f8f90f03f006352926cd1c4520d5e04177d7ca186545a9d0e0cdba02102"
    "818100eafe1b9e27b1876cb03a2f9493e9695119971facfa7261c38be92eb523"
    "aee7c1cb002089adb4fae4257559a22c3915454da5bec7d0a86be371739cd0fa"
    "bda25a20026cf02d1020086fc2b76fbc8b239b04148d0f098c302966e0eaed15"
    "4afcc14c96aed5263c042d88483d2c2773f5cd3e80e3febc334f128d29bafd39"
    "de63f9028181008b1f47a2904b823b892de96be128e5228783d0de1e0d8ccc84"
    "433d238d9d6cbcc4c6da44447920b63eefcf8ac438b0e5da45ac5acc7b62baa9"
    "731fba275c82f8ad311edef33772cb47d2cdf7f87f0039db8d2aca4ec1cee215"
    "89d63a61ae9da230a585ae38ea4674dc023aace95fa3c6734f73819056c3ce77"
    "5f5bba6c42f121";
#endif


static void test_wolfSSH_CTX_UsePrivateKey_buffer(void)
{
#ifndef WOLFSSH_NO_SERVER
    WOLFSSH_CTX* ctx;
#ifdef HAVE_ECC
    byte* eccKey;
    word32 eccKeySz;
#endif
#ifndef NO_RSA
    byte* rsaKey;
    word32 rsaKeySz;
#endif
    byte* lastKey = NULL;
    word32 lastKeySz = 0;

#ifdef HAVE_ECC
    AssertIntEQ(0,
            ConvertHexToBin(serverKeyEccDer, &eccKey, &eccKeySz,
                    NULL, NULL, NULL,
                    NULL, NULL, NULL,
                    NULL, NULL, NULL));
#endif
#ifndef NO_RSA
    AssertIntEQ(0,
            ConvertHexToBin(serverKeyRsaDer, &rsaKey, &rsaKeySz,
                    NULL, NULL, NULL,
                    NULL, NULL, NULL,
                    NULL, NULL, NULL));
#endif

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Fail: all NULL/BAD */
    AssertIntNE(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(NULL, NULL, 0, TEST_BAD_FORMAT_NEXT));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Fail: ctx set, others NULL/bad */
    AssertIntNE(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(ctx, NULL, 0, TEST_BAD_FORMAT_NEXT));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Fail: ctx set, key set, others bad */
    AssertIntNE(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(ctx,
                                         lastKey, 0, TEST_BAD_FORMAT_NEXT));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Fail: ctx set, keySz set, others NULL/bad */
    AssertIntNE(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(ctx, NULL, 1, TEST_BAD_FORMAT_NEXT));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Fail: ctx set, key set, keySz set, format invalid */
    AssertIntNE(WS_SUCCESS, wolfSSH_CTX_UsePrivateKey_buffer(ctx,
                lastKey, lastKeySz, TEST_GOOD_FORMAT_PEM));
    AssertNull(ctx->privateKey);
    AssertIntEQ(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    /* Pass */
#ifdef HAVE_ECC
    lastKey = ctx->privateKey;
    lastKeySz = ctx->privateKeySz;

    AssertIntEQ(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(ctx, eccKey, eccKeySz,
                                         TEST_GOOD_FORMAT_ASN1));
    AssertNotNull(ctx->privateKey);
    AssertIntNE(0, ctx->privateKeySz);
    AssertIntEQ(serverKeyEccCurveId, ctx->useEcc);

    AssertIntEQ(0, (lastKey == ctx->privateKey));
    AssertIntNE(lastKeySz, ctx->privateKeySz);
#endif

#ifndef NO_RSA
    lastKey = ctx->privateKey;
    lastKeySz = ctx->privateKeySz;

    AssertIntEQ(WS_SUCCESS,
        wolfSSH_CTX_UsePrivateKey_buffer(ctx, rsaKey, rsaKeySz,
                                         TEST_GOOD_FORMAT_ASN1));
    AssertNotNull(ctx->privateKey);
    AssertIntNE(0, ctx->privateKeySz);
    AssertIntEQ(0, ctx->useEcc);

    AssertIntEQ(0, (lastKey == ctx->privateKey));
    AssertIntNE(lastKeySz, ctx->privateKeySz);
#endif

    wolfSSH_CTX_free(ctx);
#ifdef HAVE_ECC
    FreeBins(eccKey, NULL, NULL, NULL);
#endif
#ifndef NO_RSA
    FreeBins(rsaKey, NULL, NULL, NULL);
#endif
#endif /* WOLFSSH_NO_SERVER */
}


#ifdef WOLFSSH_SCP
static int my_ScpRecv(WOLFSSH* ssh, int state, const char* basePath,
    const char* fileName, int fileMode, word64 mTime, word64 aTime,
    word32 totalFileSz, byte* buf, word32 bufSz, word32 fileOffset,
    void* ctx)
{
    (void)ssh;

    printf("calling scp recv cb with state %d\n", state);
    printf("\tbase path = %s\n", basePath);
    printf("\tfile name = %s\n", fileName);
    printf("\tfile mode = %d\n", fileMode);
    printf("\tfile size = %d\n", totalFileSz);
    printf("\tfile offset = %d\n", fileOffset);

    (void)mTime;
    (void)aTime;
    (void)buf;
    (void)bufSz;
    (void)ctx;

    return WS_SCP_ABORT; /* error out for test function */
}
#endif


static void test_wolfSSH_SCP_CB(void)
{
#ifdef WOLFSSH_SCP
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;
    int i = 3, j = 4; /* arbitrary value */
    const char err[] = "test setting error msg";

    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsername(NULL, NULL));

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    wolfSSH_SetScpRecv(ctx, my_ScpRecv);
    AssertNotNull(ssh = wolfSSH_new(ctx));

    wolfSSH_SetScpRecvCtx(ssh, (void*)&i);
    AssertIntEQ(i, *(int*)wolfSSH_GetScpRecvCtx(ssh));

    wolfSSH_SetScpSendCtx(ssh, (void*)&j);
    AssertIntEQ(j, *(int*)wolfSSH_GetScpSendCtx(ssh));
    AssertIntNE(j, *(int*)wolfSSH_GetScpRecvCtx(ssh));

    AssertIntEQ(wolfSSH_SetScpErrorMsg(ssh, err), WS_SUCCESS);

    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
#endif /* WOLFSSH_NO_CLIENT */
}

static void test_wolfSSH_SFTP_SendReadPacket(void)
{
#ifdef WOLFSSH_SFTP
    func_args ser;
    tcp_ready ready;
    int argsCount;

    const char* args[10];
    WOLFSSH_CTX* ctx = NULL;
    WOLFSSH*     ssh = NULL;

    THREAD_TYPE serThread;

    WMEMSET(&ser, 0, sizeof(func_args));

    argsCount = 0;
    args[argsCount++] = ".";
    args[argsCount++] = "-1";
#ifndef USE_WINDOWS_API
    args[argsCount++] = "-p";
    args[argsCount++] = "0";
#endif
    ser.argv   = (char**)args;
    ser.argc    = argsCount;
    ser.signal = &ready;
    InitTcpReady(ser.signal);
    ThreadStart(echoserver_test, (void*)&ser, &serThread);
    WaitTcpReady(&ser);

    sftp_client_connect(&ctx, &ssh, ready.port);
    AssertNotNull(ctx);
    AssertNotNull(ssh);

    {
        WS_SFTPNAME* tmp;
        WS_SFTPNAME* current;
        byte handle[WOLFSSH_MAX_HANDLE];
        word32 handleSz = WOLFSSH_MAX_HANDLE;
        const char* currentDir = ".";
        byte* out = NULL;
        int outSz = 18;
        const word32 ofst[2] = {0};

        current = wolfSSH_SFTP_LS(ssh, (char*)currentDir);
        tmp = current;
        while (tmp != NULL) {
            if ((tmp->atrb.sz[0] > 0) &&
                    (tmp->atrb.flags & WOLFSSH_FILEATRB_PERM) &&
                    !(tmp->atrb.per & 0x4000)) {
                break;
            }
            tmp = tmp->next;
        }

        if (tmp != NULL) {
            out = (byte*)malloc(tmp->atrb.sz[0]);
            AssertIntEQ(wolfSSH_SFTP_Open(ssh, tmp->fName, WOLFSSH_FXF_READ,
                        NULL, handle, &handleSz), WS_SUCCESS);

            /* read 18 bytes */
            if (tmp->atrb.sz[0] >= 18) {
                outSz = 18;
                AssertIntEQ(wolfSSH_SFTP_SendReadPacket(ssh, handle, handleSz,
                            ofst, out, outSz), outSz);
            }

            /* partial read */
            outSz = tmp->atrb.sz[0] / 2;
            AssertIntEQ(wolfSSH_SFTP_SendReadPacket(ssh, handle, handleSz, ofst,
                    out, outSz), outSz);

            /* read all */
            outSz = tmp->atrb.sz[0];
            AssertIntEQ(wolfSSH_SFTP_SendReadPacket(ssh, handle, handleSz, ofst,
                    out, outSz), outSz);

            free(out);
            wolfSSH_SFTP_Close(ssh, handle, handleSz);
            wolfSSH_SFTPNAME_list_free(current);
        }
    }

    AssertIntEQ(wolfSSH_shutdown(ssh), WS_SUCCESS);
    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
    ThreadJoin(serThread);
#endif
}



#ifdef USE_WINDOWS_API
static byte color_test[] = {
    0x1B, 0x5B, 0x34, 0x6D, 0x75, 0x6E, 0x64, 0x65,
    0x72, 0x6C, 0x69, 0x6E, 0x65, 0x1B, 0x1B, 0x5B,
    0x1B, 0x5B, 0x30, 0x6D, 0x0A, 0x1B, 0x5B, 0x33,
    0x31, 0x6D, 0x72, 0x65, 0x64, 0x0A, 0x1B, 0x5B,
    0x33, 0x32, 0x6D, 0x67, 0x72, 0x65, 0x65, 0x6E,
    0x0A, 0x1B, 0x5B, 0x33, 0x33, 0x6D, 0x79, 0x65,
    0x6C, 0x6C, 0x6F, 0x77, 0x0A, 0x1B, 0x5B, 0x32,
    0x32, 0x6D, 0x69, 0x6E, 0x74, 0x65, 0x6E, 0x73,
    0x65, 0x0A, 0x1B, 0x5B, 0x31, 0x6D, 0x62, 0x6F,
    0x6C, 0x64, 0x0A, 0x1B, 0x5B, 0x33, 0x34, 0x6D,
    0x62, 0x6C, 0x75, 0x65, 0x0A, 0x1B, 0x5B, 0x33,
    0x35, 0x6D, 0x6D, 0x61, 0x67, 0x65, 0x6E, 0x74,
    0x61, 0x0A, 0x1B, 0x5B, 0x33, 0x36, 0x6D, 0x63,
    0x79, 0x61, 0x6E, 0x0A, 0x1B, 0x5B, 0x33, 0x37,
    0x6D, 0x77, 0x68, 0x69, 0x74, 0x65, 0x0A, 0x1B,
    0x5B, 0x30, 0x6D, 0x6E, 0x6F, 0x72, 0x6D, 0x61,
    0x6C, 0x0A, 0x1B, 0x5B, 0x34, 0x30, 0x6D, 0x62,
    0x6C, 0x61, 0x63, 0x6B, 0x20, 0x62, 0x67, 0x0A,
    0x1B, 0x5B, 0x34, 0x31, 0x6D, 0x72, 0x65, 0x64,
    0x20, 0x62, 0x67, 0x0A, 0x1B, 0x5B, 0x34, 0x32,
    0x6D, 0x67, 0x72, 0x65, 0x65, 0x6E, 0x20, 0x62,
    0x67, 0x0A, 0x1B, 0x5B, 0x34, 0x33, 0x6D, 0x62,
    0x72, 0x6F, 0x77, 0x6E, 0x20, 0x62, 0x67, 0x0A,
    0x1B, 0x5B, 0x34, 0x34, 0x6D, 0x62, 0x6C, 0x75,
    0x65, 0x20, 0x62, 0x67, 0x0A, 0x1B, 0x5B, 0x34,
    0x35, 0x6D, 0x6D, 0x61, 0x67, 0x65, 0x6E, 0x74,
    0x61, 0x20, 0x62, 0x67, 0x0A, 0x1B, 0x5B, 0x34,
    0x36, 0x6D, 0x63, 0x79, 0x61, 0x6E, 0x20, 0x62,
    0x67, 0x0A, 0x1B, 0x5B, 0x34, 0x37, 0x6D, 0x77,
    0x68, 0x69, 0x74, 0x65, 0x20, 0x62, 0x67, 0x0A,
    0x1B, 0x5B, 0x34, 0x39, 0x6D, 0x64, 0x65, 0x66,
    0x61, 0x75, 0x6C, 0x74, 0x20, 0x62, 0x67, 0x0A,
};
#endif /* USE_WINDOWS_API */


static void test_wolfSSH_ConvertConsole(void)
{
#ifdef USE_WINDOWS_API
    WOLFSSH_CTX* ctx;
    WOLFSSH* ssh;
    int i = 3, j = 4; /* arbitrary value */
    const char err[] = "test setting error msg";
    HANDLE stdoutHandle = GetStdHandle(STD_OUTPUT_HANDLE);

    AssertIntNE(WS_SUCCESS, wolfSSH_SetUsername(NULL, NULL));

    AssertNotNull(ctx = wolfSSH_CTX_new(WOLFSSH_ENDPOINT_SERVER, NULL));
    AssertNotNull(ssh = wolfSSH_new(ctx));

    /* parameter tests */
    AssertIntEQ(wolfSSH_ConvertConsole(NULL, stdoutHandle, color_test,
                sizeof(color_test)), WS_BAD_ARGUMENT);
    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, NULL,
                sizeof(color_test)), WS_BAD_ARGUMENT);

    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, color_test, 1),
            WS_WANT_READ);
    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, color_test + 1, 1),
            WS_WANT_READ);
    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, color_test + 2,
                sizeof(color_test) - 2), WS_SUCCESS);

    /* bad esc esc command */
    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, color_test, 1),
            WS_WANT_READ);
    AssertIntEQ(wolfSSH_ConvertConsole(ssh, stdoutHandle, color_test, 1),
            WS_SUCCESS); /* should skip over unknown console code */

    wolfSSH_free(ssh);
    wolfSSH_CTX_free(ctx);
#endif /* USE_WINDOWS_API */
}


static void test_wstrcat(void)
{
#ifndef WSTRING_USER
    char dst[5];

    WSTRNCPY(dst, "12", sizeof(dst));
    AssertNull(wstrncat(dst, "345", sizeof(dst)));
    AssertStrEQ(dst, "12");
    AssertNotNull(wstrncat(dst, "67", sizeof(dst)));
    AssertStrEQ(dst, "1267");
#endif /* WSTRING_USER */
}


int main(void)
{
    AssertIntEQ(wolfSSH_Init(), WS_SUCCESS);

    test_wstrcat();
    test_wolfSSH_CTX_new();
    test_server_wolfSSH_new();
    test_client_wolfSSH_new();
    test_wolfSSH_set_fd();
    test_wolfSSH_SetUsername();
    test_wolfSSH_ConvertConsole();
    test_wolfSSH_CTX_UsePrivateKey_buffer();

    /* SCP tests */
    test_wolfSSH_SCP_CB();

    /* SFTP tests */
    test_wolfSSH_SFTP_SendReadPacket();


    AssertIntEQ(wolfSSH_Cleanup(), WS_SUCCESS);

    return 0;
}
