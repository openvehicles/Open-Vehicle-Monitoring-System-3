
// For compatibility of WolfSSH with ESP-IDF

#define BUILDING_WOLFSSH
#define WOLFSSH_LWIP
//#define DEFAULT_HIGHWATER_MARK (1024 * 4)
#define DEFAULT_HIGHWATER_MARK ((1024 * 1024 * 1024) - (32 * 1024))
#define DEFAULT_WINDOW_SZ (1024*2)
#define DEFAULT_MAX_PACKET_SZ (1024*2)
#define WOLFSSH_LOG_PRINTF

// For compatibility of WolfSSL with ESP-IDF

#define BUILDING_WOLFSSL
#define HAVE_VISIBILITY 1
#define WOLFCRYPT_ONLY
#define NO_DEV_RANDOM
#define NO_MAIN_DRIVER
#define FREERTOS
#define WOLFSSL_ESPIDF
#define WOLFSSL_ESPWROOM32
#define WOLFSSL_LWIP
#define WOLFSSL_KEY_GEN
#define SIZEOF_LONG 4
#define SIZEOF_LONG_LONG 8
#define HAVE_GETADDRINFO 1
#define HAVE_GMTIME_R 1

#define WOLFSSL_SMALL_STACK
#define USE_WOLFSSL_MEMORY
#define XMALLOC_USER
#define XMALLOC(s, h, t)     wolfSSL_Malloc(s)
#define XFREE(p, h, t)       wolfSSL_Free(p)
#define XREALLOC(p, n, h, t) wolfSSL_Realloc(p, n)

// Inclusion and exclusion of WolfSSL features, may be adjusted

#define NO_DES3
#define NO_DSA
#define NO_ERROR_STRINGS
#define NO_HC128
#define NO_MD4
#define NO_PSK
#define NO_PWDBASED
#define NO_RABBIT
#define NO_RC4
#define SMALL_SESSION_CACHE
#define ECC_SHAMIR
#define ECC_TIMING_RESISTANT
#define HAVE_WC_ECC_SET_RNG
#define HAVE_AESGCM
//#define HAVE_CHACHA
#define HAVE_DH
#define HAVE_ECC
#define HAVE_EXTENDED_MASTER
#define HAVE_HASHDRBG
#define HAVE_ONE_TIME_AUTH
//#define HAVE_POLY1305
#define HAVE_SUPPORTED_CURVES
#define HAVE_THREAD_LS
#define HAVE_TLS_EXTENSIONS
#define TFM_ECC256
#define TFM_TIMING_RESISTANT
#define WC_NO_ASYNC_THREADING
#define WC_RSA_BLINDING
#define WOLFSSL_BASE64_ENCODE
#define WOLFSSL_SHA224
//#define WOLFSSL_SHA3
#define WOLFSSL_SHA384
#define WOLFSSL_SHA512
