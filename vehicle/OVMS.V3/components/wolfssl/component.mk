#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_WOLF
COMPONENT_ADD_INCLUDEDIRS := port wolfssl
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
COMPONENT_ADD_LDFLAGS = -Wl,--whole-archive -l$(COMPONENT_NAME) -Wl,--no-whole-archive
COMPONENT_SRCDIRS := wolfssl/src wolfssl/wolfcrypt/src wolfssl/wolfcrypt/src/port/Espressif
COMPONENT_OBJS := wolfssl/src/crl.o
COMPONENT_OBJS += wolfssl/src/internal.o
COMPONENT_OBJS += wolfssl/src/keys.o
COMPONENT_OBJS += wolfssl/src/ocsp.o
COMPONENT_OBJS += wolfssl/src/sniffer.o
COMPONENT_OBJS += wolfssl/src/ssl.o
COMPONENT_OBJS += wolfssl/src/tls.o
COMPONENT_OBJS += wolfssl/src/tls13.o
COMPONENT_OBJS += wolfssl/src/wolfio.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/aes.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/arc4.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/asm.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/asn.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/blake2b.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/blake2s.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/camellia.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/chacha.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/chacha20_poly1305.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/cmac.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/coding.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/compress.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/cpuid.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/cryptocb.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/curve25519.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/curve448.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/des3.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/dh.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/dsa.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ecc.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ecc_fp.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/eccsi.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ed25519.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ed448.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/error.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/falcon.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/fe_448.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/fe_low_mem.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/fe_operations.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ge_448.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ge_low_mem.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ge_operations.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/hash.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/hmac.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/integer.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/kdf.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/logging.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/md2.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/md4.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/md5.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/memory.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/pkcs12.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/pkcs7.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/poly1305.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/port/Espressif/esp32_aes.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/port/Espressif/esp32_mp.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/port/Espressif/esp32_sha.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/port/Espressif/esp32_util.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/pwdbased.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/random.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/rc2.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/ripemd.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/rsa.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sakke.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sha.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sha256.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sha3.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sha512.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/signature.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/siphash.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_arm32.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_arm64.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_armthumb.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_c32.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_c64.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_cortexm.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_dsp32.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_int.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/sp_x86_64.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/srp.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/tfm.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wc_dsp.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wc_encrypt.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wc_pkcs11.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wc_port.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wolfevent.o
COMPONENT_OBJS += wolfssl/wolfcrypt/src/wolfmath.o

CFLAGS += -Wno-char-subscripts -DWOLFSSL_USER_SETTINGS
endif
