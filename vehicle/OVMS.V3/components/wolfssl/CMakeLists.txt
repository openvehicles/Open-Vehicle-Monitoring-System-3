set(srcs)
set(include_dirs)
set(priv_include_dirs)

if (CONFIG_OVMS_SC_GPL_WOLF)
  list(APPEND srcs "wolfssl/src/crl.c" "wolfssl/src/internal.c" "wolfssl/src/keys.c" "wolfssl/src/ocsp.c" "wolfssl/src/sniffer.c" "wolfssl/src/ssl.c" "wolfssl/src/tls.c" "wolfssl/src/tls13.c" "wolfssl/src/wolfio.c" "wolfssl/wolfcrypt/src/aes.c" "wolfssl/wolfcrypt/src/arc4.c" "wolfssl/wolfcrypt/src/asm.c" "wolfssl/wolfcrypt/src/asn.c" "wolfssl/wolfcrypt/src/blake2b.c" "wolfssl/wolfcrypt/src/camellia.c" "wolfssl/wolfcrypt/src/chacha.c" "wolfssl/wolfcrypt/src/chacha20_poly1305.c" "wolfssl/wolfcrypt/src/cmac.c" "wolfssl/wolfcrypt/src/coding.c" "wolfssl/wolfcrypt/src/compress.c" "wolfssl/wolfcrypt/src/cpuid.c" "wolfssl/wolfcrypt/src/curve25519.c" "wolfssl/wolfcrypt/src/des3.c" "wolfssl/wolfcrypt/src/dh.c" "wolfssl/wolfcrypt/src/dsa.c" "wolfssl/wolfcrypt/src/ecc.c" "wolfssl/wolfcrypt/src/ecc_fp.c" "wolfssl/wolfcrypt/src/ed25519.c" "wolfssl/wolfcrypt/src/error.c" "wolfssl/wolfcrypt/src/fe_low_mem.c" "wolfssl/wolfcrypt/src/fe_operations.c" "wolfssl/wolfcrypt/src/ge_low_mem.c" "wolfssl/wolfcrypt/src/ge_operations.c" "wolfssl/wolfcrypt/src/hash.c" "wolfssl/wolfcrypt/src/hc128.c" "wolfssl/wolfcrypt/src/hmac.c" "wolfssl/wolfcrypt/src/idea.c" "wolfssl/wolfcrypt/src/integer.c" "wolfssl/wolfcrypt/src/logging.c" "wolfssl/wolfcrypt/src/md2.c" "wolfssl/wolfcrypt/src/md4.c" "wolfssl/wolfcrypt/src/md5.c" "wolfssl/wolfcrypt/src/memory.c" "wolfssl/wolfcrypt/src/pkcs12.c" "wolfssl/wolfcrypt/src/pkcs7.c" "wolfssl/wolfcrypt/src/poly1305.c" "wolfssl/wolfcrypt/src/port/Espressif/esp32_aes.c" "wolfssl/wolfcrypt/src/port/Espressif/esp32_mp.c" "wolfssl/wolfcrypt/src/port/Espressif/esp32_sha.c" "wolfssl/wolfcrypt/src/port/Espressif/esp32_util.c" "wolfssl/wolfcrypt/src/pwdbased.c" "wolfssl/wolfcrypt/src/rabbit.c" "wolfssl/wolfcrypt/src/random.c" "wolfssl/wolfcrypt/src/ripemd.c" "wolfssl/wolfcrypt/src/rsa.c" "wolfssl/wolfcrypt/src/sha.c" "wolfssl/wolfcrypt/src/sha256.c" "wolfssl/wolfcrypt/src/sha3.c" "wolfssl/wolfcrypt/src/sha512.c" "wolfssl/wolfcrypt/src/signature.c" "wolfssl/wolfcrypt/src/srp.c" "wolfssl/wolfcrypt/src/wc_encrypt.c" "wolfssl/wolfcrypt/src/wc_port.c" "wolfssl/wolfcrypt/src/wolfevent.c" "wolfssl/wolfcrypt/src/wolfmath.c")
  list(APPEND include_dirs "port" "wolfssl")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       REQUIRES "freertos"
                       # PRIV_REQUIRES "freertos"
                       WHOLE_ARCHIVE)

if (CONFIG_OVMS_SC_GPL_WOLF)
  component_compile_definitions(WOLFSSL_USER_SETTINGS)
  component_compile_options("-Wno-cpp" "-Wno-char-subscripts")
  set_source_files_properties(wolfssl/src/ssl.c PROPERTIES COMPILE_FLAGS "-Wno-format-truncation -Wno-char-subscripts")
  set_source_files_properties(wolfssl/wolfcrypt/src/random.c PROPERTIES COMPILE_FLAGS "-Wno-implicit-function-declaration")
  set_source_files_properties(wolfssl/wolfcrypt/src/port/Espressif/esp32_aes.c PROPERTIES COMPILE_FLAGS "-Wno-incompatible-pointer-types")
endif ()
