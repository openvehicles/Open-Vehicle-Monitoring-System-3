/* unit.c
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
#include <wolfssh/keygen.h>
#include <wolfssh/internal.h>


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
        byte b = in[inIdx] - 0x30;  /* 0 starts at 0x30 */

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
            *b1 = NULL;
            return -1;
        }
    }

    /* b2 */
    if (h2 && b2 && b2Sz) {
        *b2Sz = (word32)strlen(h2) / 2;
        *b2 = (byte*)malloc(*b2Sz);
        if (*b2 == NULL) {
            FreeBins(b1 ? *b1 : NULL, NULL, NULL, NULL);
            if (b1) *b1 = NULL;
            return -1;
        }
        ret = Base16_Decode((const byte*)h2, (word32)strlen(h2),
                            *b2, b2Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, *b2, NULL, NULL);
            if (b1) *b1 = NULL;
            *b2 = NULL;
            return -1;
        }
    }

    /* b3 */
    if (h3 && b3 && b3Sz) {
        *b3Sz = (word32)strlen(h3) / 2;
        *b3 = (byte*)malloc(*b3Sz);
        if (*b3 == NULL) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, NULL, NULL);
            if (b1) *b1 = NULL;
            if (b2) *b2 = NULL;
            return -1;
        }
        ret = Base16_Decode((const byte*)h3, (word32)strlen(h3),
                            *b3, b3Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, *b3, NULL);
            if (b1) *b1 = NULL;
            if (b2) *b2 = NULL;
            *b3 = NULL;
            return -1;
        }
    }

    /* b4 */
    if (h4 && b4 && b4Sz) {
        *b4Sz = (word32)strlen(h4) / 2;
        *b4 = (byte*)malloc(*b4Sz);
        if (*b4 == NULL) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, b3 ? *b3 : NULL, NULL);
            if (b1) *b1 = NULL;
            if (b2) *b2 = NULL;
            if (b3) *b3 = NULL;
            return -1;
        }
        ret = Base16_Decode((const byte*)h4, (word32)strlen(h4),
                            *b4, b4Sz);
        if (ret != 0) {
            FreeBins(b1 ? *b1 : NULL, b2 ? *b2 : NULL, b3 ? *b3 : NULL, *b4);
            if (b1) *b1 = NULL;
            if (b2) *b2 = NULL;
            if (b3) *b3 = NULL;
            *b4 = NULL;
            return -1;
        }
    }

    return 0;
}


/* Key Derivation Function (KDF) Unit Test */

typedef struct {
    byte hashId;
    byte keyId;
    const char* k;
    const char* h;
    const char* sessionId;
    const char* expectedKey;
} KdfTestVector;


/** Test Vector Set #1: SHA-1 **/
const char kdfTvSet1k[] =
    "35618FD3AABF980A5F766408961600D4933C60DD7B22D69EEB4D7A987C938F6F"
    "7BB2E60E0F638BB4289297B588E6109057325F010D021DF60EBF8BE67AD9C3E2"
    "6376A326A16210C7AF07B3FE562B8DD1DCBECB17AA7BFAF38708B0136120B2FC"
    "723E93EF4237AC3737BAE3A16EC03F605C7EEABFD526B38C826B506BBAECD2F7"
    "9932F1371AEABFBEB4F8222313506677330C714A2A6FDC70CB859B581AA18625"
    "ECCB6BA9DDEEAECF0E41D9E5076B899B477112E59DDADC4B4D9C13E9F07E1107"
    "B560FEFDC146B8ED3E73441D05345031C35F9E6911B00319481D80015855BE4D"
    "1C7D7ACC8579B1CC2E5F714109C0882C3B57529ABDA1F2255D2B27C4A83AE11E";
const char kdfTvSet1h[]         = "40555741F6DE70CDC4E740104A97E75473F49064";
const char kdfTvSet1sid[]       = "40555741F6DE70CDC4E740104A97E75473F49064";
const char kdfTvSet1a[]         = "B2EC4CF6943632C39972EE2801DC7393";
const char kdfTvSet1b[]         = "BC92238B6FA69ECC10B2B013C2FC9785";
const char kdfTvSet1c[]         = "9EF0E2053F66C56F3E4503DA1C2FBD6B";
const char kdfTvSet1d[]         = "47C8395B08277020A0645DA3959FA1A9";
const char kdfTvSet1e[]         = "EE436BFDABF9B0313224EC800E7390445E2F575E";
const char kdfTvSet1f[]         = "FB9FDEEC78B0FB258F1A4F47F6BCE166680994BB";

/** Test Vector Set #2: SHA-1 **/
const char kdfTvSet2k[] =
    "19FA2B7C7F4FE7DE61CDE17468C792CCEAB0E3F2CE37CDE2DAA0974BCDFFEDD4"
    "A29415CDB330FA6A97ECA742359DC1223B581D8AC61B43CFFDF66D20952840B0"
    "2593B48354E352E2A396BDF7F1C9D414FD31C2BF47E6EED306069C4F4F5F66C3"
    "003A90E85412A1FBE89CDFB457CDA0D832E8DA701627366ADEC95B70E8A8B7BF"
    "3F85775CCF36E40631B83B32CF643088F01A82C97C5C3A820EB4149F551CAF8C"
    "C98EE6B3065E6152FF877823F7C618C1CD93CE26DB9FAAFED222F1C93E8F4068"
    "BFDA4480432E14F98FFC821F05647693040B07D71DC273121D53866294434D46"
    "0E95CFA4AB4414705BF1F8224655F907A418A6A893F2A71019225869CB7FE988";
const char kdfTvSet2h[]         = "DFB748905CC8647684C3E0B7F26A3E8E7414AC51";
const char kdfTvSet2sid[]       = "DFB748905CC8647684C3E0B7F26A3E8E7414AC51";
const char kdfTvSet2a[]         = "52EDBFD5E414A3CC6C7F7A0F4EA60503";
const char kdfTvSet2b[]         = "926C6987696C5FFCC6511BFE34557878";
const char kdfTvSet2c[]         = "CB6D56EC5B9AFECD326D544DA2D22DED";
const char kdfTvSet2d[]         = "F712F6451F1BD6CE9BAA597AC87C5A24";
const char kdfTvSet2e[]         = "E42FC62C76B76B37818F78292D3C2226D0264760";
const char kdfTvSet2f[]         = "D14BE4DD0093A3E759580233C80BB8399CE4C4E7";

/** Test Vector Set #3: SHA-256 **/
const char kdfTvSet3k[] =
    "6AC382EAACA093E125E25C24BEBC84640C11987507344B5C739CEB84A9E0B222"
    "B9A8B51C839E5EBE49CFADBFB39599764ED522099DC912751950DC7DC97FBDC0"
    "6328B68F22781FD315AF568009A5509E5B87A11BF527C056DAFFD82AB6CBC25C"
    "CA37143459E7BC63BCDE52757ADEB7DF01CF12173F1FEF8102EC5AB142C213DD"
    "9D30696278A8D8BC32DDE9592D28C078C6D92B947D825ACAAB6494846A49DE24"
    "B9623F4889E8ADC38E8C669EFFEF176040AD945E90A7D3EEC15EFEEE78AE7104"
    "3C96511103A16BA7CAF0ACD0642EFDBE809934FAA1A5F1BD11043649B25CCD1F"
    "EE2E38815D4D5F5FC6B4102969F21C22AE1B0E7D3603A556A13262FF628DE222";
const char kdfTvSet3h[] =
    "7B7001185E256D4493445F39A55FB905E6321F4B5DD8BBF3100D51BA0BDA3D2D";
const char kdfTvSet3sid[] =
    "7B7001185E256D4493445F39A55FB905E6321F4B5DD8BBF3100D51BA0BDA3D2D";
const char kdfTvSet3a[]         = "81F0330EF6F05361B3823BFDED6E1DE9";
const char kdfTvSet3b[]         = "3F6FD2065EEB2B0B1D93195A1FED48A5";
const char kdfTvSet3c[]         = "C35471034E6FD6547613178E23435F21";
const char kdfTvSet3d[]         = "7E9D79032090D99F98B015634DD9F462";
const char kdfTvSet3e[] =
    "24EE559AD7CE712B685D0B2271E443C17AB1D1DCEB5A360569D25D5DC243002F";
const char kdfTvSet3f[] =
    "C3419C2B966235869D714BA5AC48DDB7D9E35C8C19AAC73422337A373453607E";

/** Test Vector Set #4: SHA-256 **/
const char kdfTvSet4k[] =
    "44708C76616F700BD31B0C155EF74E36390EEB39BC5C32CDC90E21922B0ED930"
    "B5B519C8AFEBEF0F4E4FB5B41B81D649D2127506620B594E9899F7F0D442ECDD"
    "D68308307B82F00065E9D75220A5A6F5641795772132215A236064EA965C6493"
    "C21F89879730EBBC3C20A22D8F5BFD07B525B194323B22D8A49944D1AA58502E"
    "756101EF1E8A91C9310E71F6DB65A3AD0A542CFA751F83721A99E89F1DBE5497"
    "1A3620ECFFC967AA55EED1A42D6E7A138B853557AC84689889F6D0C8553575FB"
    "89B4E13EAB5537DA72EF16F0D72F5E8505D97F110745193D550FA315FE88F672"
    "DB90D73843E97BA1F3D087BA8EB39025BBFFAD37589A6199227303D9D8E7F1E3";
const char kdfTvSet4h[] =
    "FE3727FD99A5AC7987C2CFBE062129E3027BF5E10310C6BCCDE9C916C8329DC2";
const char kdfTvSet4sid[] =
    "FFFA598BC0AD2AE84DC8DC05B1F72C5B0134025AE7EDF8A2E8DB11472E18E1FC";
const char kdfTvSet4a[]         = "36730BAE8DE5CB98898D6B4A00B37058";
const char kdfTvSet4b[]         = "5DFE446A83F40E8358D28CB97DF8F340";
const char kdfTvSet4c[]         = "495B7AFED0872B761437728E9E94E2B8";
const char kdfTvSet4d[]         = "C1474B3925BEC36F0B7F6CC698E949C8";
const char kdfTvSet4e[] =
    "B730F8DF6A0697645BE261169486C32A11612229276CBAC5D8B3669AFB2E4262";
const char kdfTvSet4f[] =
    "14A5EA98245FB058978B82A3CB092B1CCA7CE0109A4F98C16E1529579D58B819";

#define HASH_SHA WC_HASH_TYPE_SHA
#define HASH_SHA256 WC_HASH_TYPE_SHA256

static const KdfTestVector kdfTestVectors[] = {
    {HASH_SHA, 'A', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1a},
    {HASH_SHA, 'B', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1b},
    {HASH_SHA, 'C', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1c},
    {HASH_SHA, 'D', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1d},
    {HASH_SHA, 'E', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1e},
    {HASH_SHA, 'F', kdfTvSet1k, kdfTvSet1h, kdfTvSet1sid, kdfTvSet1f},
    {HASH_SHA, 'A', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2a},
    {HASH_SHA, 'B', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2b},
    {HASH_SHA, 'C', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2c},
    {HASH_SHA, 'D', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2d},
    {HASH_SHA, 'E', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2e},
    {HASH_SHA, 'F', kdfTvSet2k, kdfTvSet2h, kdfTvSet2sid, kdfTvSet2f},
    {HASH_SHA256, 'A', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3a},
    {HASH_SHA256, 'B', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3b},
    {HASH_SHA256, 'C', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3c},
    {HASH_SHA256, 'D', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3d},
    {HASH_SHA256, 'E', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3e},
    {HASH_SHA256, 'F', kdfTvSet3k, kdfTvSet3h, kdfTvSet3sid, kdfTvSet3f},
    {HASH_SHA256, 'A', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4a},
    {HASH_SHA256, 'B', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4b},
    {HASH_SHA256, 'C', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4c},
    {HASH_SHA256, 'D', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4d},
    {HASH_SHA256, 'E', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4e},
    {HASH_SHA256, 'F', kdfTvSet4k, kdfTvSet4h, kdfTvSet4sid, kdfTvSet4f}
};


static int test_KDF(void)
{
    int result = 0;
    word32 i;
    word32 tc = sizeof(kdfTestVectors)/sizeof(KdfTestVector);
    const KdfTestVector* tv = NULL;
    byte* k = NULL;
    byte* h = NULL;
    byte* sId = NULL;
    byte* eKey = NULL;
    word32 kSz = 0, hSz = 0, sIdSz = 0, eKeySz = 0;
    byte cKey[32]; /* Greater of SHA256_DIGEST_SIZE and AES_BLOCK_SIZE */
    /* sId - Session ID, eKey - Expected Key, cKey - Calculated Key */

    for (i = 0, tv = kdfTestVectors; i < tc; i++, tv++) {

        result = ConvertHexToBin(tv->k, &k, &kSz,
                                 tv->h, &h, &hSz,
                                 tv->sessionId, &sId, &sIdSz,
                                 tv->expectedKey, &eKey, &eKeySz);
        if (result != 0 || eKey == NULL) {
            printf("KDF: Could not convert test vector %u.\n", i);
            result = -100;
        }

        if (result == 0) {
            result = wolfSSH_KDF(tv->hashId, tv->keyId, cKey, eKeySz,
                    k, kSz, h, hSz, sId, sIdSz);

            if (result != 0) {
                printf("KDF: Could not derive key.\n");
                result = -101;
            }
        }

        if (result == 0) {
            if (memcmp(cKey, eKey, eKeySz) != 0) {
                printf("KDF: Calculated Key does not match Expected Key.\n");
                result = -102;
            }
        }

        FreeBins(k, h, sId, eKey);
        k = NULL;
        h = NULL;
        sId = NULL;
        eKey = NULL;

        if (result != 0) break;
    }

    return result;
}


/* Key Generation Unit Test */

#ifdef WOLFSSH_KEYGEN

static int test_RsaKeyGen(void)
{
    int result = 0;
    byte der[1200];
    int derSz;

    derSz = wolfSSH_MakeRsaKey(der, sizeof(der),
                               WOLFSSH_RSAKEY_DEFAULT_SZ,
                               WOLFSSH_RSAKEY_DEFAULT_E);
    if (derSz < 0) {
        printf("RsaKeyGen: MakeRsaKey failed\n");
        result = -103;
    }

    return result;
}

#endif


/* Error Code And Message Test */

static int test_Errors(void)
{
    const char* errStr;
    const char* unknownStr = wolfSSH_ErrorToName(1);
    int result = 0;

#ifdef NO_WOLFSSH_STRINGS
    /* Ensure a valid error code's string matches an invalid code's.
     * The string is that error strings are not available.
     */
    errStr = wolfSSH_ErrorToName(WS_BAD_ARGUMENT);
    if (errStr != unknownStr)
        result = -104;
#else
    int i, j = 0;
    /* Values that are not or no longer error codes. */
    int missing[] = { -1059 };
    int missingSz = (int)sizeof(missing)/sizeof(missing[0]);

    /* Check that all errors have a string and it's the same through the two
     * APIs. Check that the values that are not errors map to the unknown
     * string.  */
    for (i = WS_ERROR; i >= WS_LAST_E; i--) {
        errStr = wolfSSH_ErrorToName(i);

        if (j < missingSz && i == missing[j]) {
            j++;
            if (errStr != unknownStr) {
                result = -105;
                break;
            }
        }
        else {
            if (errStr == unknownStr) {
                result = -106;
                break;
            }
        }
    }

    /* Check if the next possible value has been given a string. */
    if (result == 0) {
        errStr = wolfSSH_ErrorToName(i);
        if (errStr != unknownStr)
            return -107;
    }
#endif

    return result;
}


int main(void)
{
    int testResult = 0, unitResult = 0;

    unitResult = test_Errors();
    printf("Errors: %s\n", (unitResult == 0 ? "SUCCESS" : "FAILED"));
    testResult = testResult || unitResult;

    unitResult = test_KDF();
    printf("KDF: %s\n", (unitResult == 0 ? "SUCCESS" : "FAILED"));
    testResult = testResult || unitResult;

#ifdef WOLFSSH_KEYGEN
    unitResult = test_RsaKeyGen();
    printf("RsaKeyGen: %s\n", (unitResult == 0 ? "SUCCESS" : "FAILED"));
    testResult = testResult || unitResult;
#endif

    return (testResult ? 1 : 0);
}

