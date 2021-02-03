#
# Main component makefile.
#
# This Makefile can be left empty. By default, it will take the sources in the
# src/ directory, compile them and link them into lib(subdirectory_name).a
# in the build directory. This behaviour is entirely configurable,
# please read the ESP-IDF documents if you need to do this.
#

ifdef CONFIG_OVMS_SC_GPL_WOLF
COMPONENT_ADD_INCLUDEDIRS := .
COMPONENT_EXTRA_INCLUDES := ${IDF_PATH}/components/freertos/include/freertos
COMPONENT_SRCDIRS := wolfcrypt/src
COMPONENT_SRCDIRS += wolfcrypt/src/port/Espressif
COMPONENT_OBJS := wolfcrypt/src/aes.o
COMPONENT_OBJS += wolfcrypt/src/arc4.o
COMPONENT_OBJS += wolfcrypt/src/asm.o
COMPONENT_OBJS += wolfcrypt/src/asn.o
COMPONENT_OBJS += wolfcrypt/src/blake2b.o
COMPONENT_OBJS += wolfcrypt/src/camellia.o
COMPONENT_OBJS += wolfcrypt/src/chacha.o
COMPONENT_OBJS += wolfcrypt/src/chacha20_poly1305.o
COMPONENT_OBJS += wolfcrypt/src/cmac.o
COMPONENT_OBJS += wolfcrypt/src/coding.o
COMPONENT_OBJS += wolfcrypt/src/compress.o
COMPONENT_OBJS += wolfcrypt/src/cpuid.o
COMPONENT_OBJS += wolfcrypt/src/curve25519.o
COMPONENT_OBJS += wolfcrypt/src/des3.o
COMPONENT_OBJS += wolfcrypt/src/dh.o
COMPONENT_OBJS += wolfcrypt/src/dsa.o
COMPONENT_OBJS += wolfcrypt/src/ecc.o
COMPONENT_OBJS += wolfcrypt/src/ecc_fp.o
COMPONENT_OBJS += wolfcrypt/src/ed25519.o
COMPONENT_OBJS += wolfcrypt/src/error.o
COMPONENT_OBJS += wolfcrypt/src/fe_low_mem.o
COMPONENT_OBJS += wolfcrypt/src/fe_operations.o
COMPONENT_OBJS += wolfcrypt/src/ge_low_mem.o
COMPONENT_OBJS += wolfcrypt/src/ge_operations.o
COMPONENT_OBJS += wolfcrypt/src/hash.o
COMPONENT_OBJS += wolfcrypt/src/hc128.o
COMPONENT_OBJS += wolfcrypt/src/hmac.o
COMPONENT_OBJS += wolfcrypt/src/idea.o
COMPONENT_OBJS += wolfcrypt/src/integer.o
COMPONENT_OBJS += wolfcrypt/src/logging.o
COMPONENT_OBJS += wolfcrypt/src/md2.o
COMPONENT_OBJS += wolfcrypt/src/md4.o
COMPONENT_OBJS += wolfcrypt/src/md5.o
COMPONENT_OBJS += wolfcrypt/src/memory.o
COMPONENT_OBJS += wolfcrypt/src/pkcs12.o
COMPONENT_OBJS += wolfcrypt/src/pkcs7.o
COMPONENT_OBJS += wolfcrypt/src/poly1305.o
COMPONENT_OBJS += wolfcrypt/src/port/Espressif/esp32_aes.o
COMPONENT_OBJS += wolfcrypt/src/port/Espressif/esp32_mp.o
COMPONENT_OBJS += wolfcrypt/src/port/Espressif/esp32_sha.o
COMPONENT_OBJS += wolfcrypt/src/port/Espressif/esp32_util.o
COMPONENT_OBJS += wolfcrypt/src/pwdbased.o
COMPONENT_OBJS += wolfcrypt/src/rabbit.o
COMPONENT_OBJS += wolfcrypt/src/random.o
COMPONENT_OBJS += wolfcrypt/src/ripemd.o
COMPONENT_OBJS += wolfcrypt/src/rsa.o
COMPONENT_OBJS += wolfcrypt/src/sha.o
COMPONENT_OBJS += wolfcrypt/src/sha256.o
COMPONENT_OBJS += wolfcrypt/src/sha3.o
COMPONENT_OBJS += wolfcrypt/src/sha512.o
COMPONENT_OBJS += wolfcrypt/src/signature.o
COMPONENT_OBJS += wolfcrypt/src/srp.o
COMPONENT_OBJS += wolfcrypt/src/tfm.o
COMPONENT_OBJS += wolfcrypt/src/wc_encrypt.o
COMPONENT_OBJS += wolfcrypt/src/wc_port.o
COMPONENT_OBJS += wolfcrypt/src/wolfevent.o
COMPONENT_OBJS += wolfcrypt/src/wolfmath.o

CFLAGS += -DWOLFSSL_USER_SETTINGS
endif
