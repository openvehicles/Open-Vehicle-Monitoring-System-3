set(srcs)
set(srcdirs)
set(srcexclude)
set(include_dirs)
set(priv_include_dirs)

if (CONFIG_OVMS_SC_GPL_WOLF)
  # Cf https://github.com/gojimmypi/wolfssl/blob/Espressif_No_Install/IDE/Espressif/ESP-IDF/examples/wolfssl_server/components/wolfssl/CMakeLists.txt
  # get a list of all wolfcrypt assembly files; we'll exclude them as they don't target Xtensa
  FILE(GLOB EXCLUDE_ASM *.S)
  file(GLOB_RECURSE EXCLUDE_ASM ${CMAKE_SOURCE_DIR} "wolfssl/wolfcrypt/src/*.S")
  message(STATUS "wolfssl : EXCLUDE_ASM = ${EXCLUDE_ASM}")

  list(APPEND include_dirs "port" "wolfssl")
  list(APPEND srcdirs "wolfssl/src/"
                      "wolfssl/wolfcrypt/src"
                      "wolfssl/wolfcrypt/src/port/Espressif/"
                      "wolfssl/wolfcrypt/src/port/atmel/"
                      )
  list(APPEND srcexclude
      "wolfssl/src/bio.c"
      "wolfssl/src/conf.c"
      "wolfssl/src/misc.c"
      "wolfssl/src/pk.c"
      "wolfssl/src/ssl_misc.c" # included by ssl.c
      "wolfssl/src/x509.c"
      "wolfssl/src/x509_str.c"
      "wolfssl/wolfcrypt/src/evp.c"
      "wolfssl/wolfcrypt/src/misc.c"
      "${EXCLUDE_ASM}"
      )
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       SRC_DIRS ${srcdirs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_INCLUDE_DIRS ${priv_include_dirs}
                       REQUIRES "freertos" "lwip"
                       EXCLUDE_SRCS ${srcexclude}
                       # PRIV_REQUIRES "freertos"
                       WHOLE_ARCHIVE)

if (CONFIG_OVMS_SC_GPL_WOLF)
  component_compile_definitions("WOLFSSL_USER_SETTINGS")
  component_compile_options("-Wno-cpp" "-Wno-char-subscripts")
  set_source_files_properties(wolfssl/src/ssl.c PROPERTIES COMPILE_FLAGS "-Wno-format-truncation -Wno-char-subscripts")
  set_source_files_properties(wolfssl/wolfcrypt/src/random.c PROPERTIES COMPILE_FLAGS "-Wno-implicit-function-declaration")
  set_source_files_properties(wolfssl/wolfcrypt/src/port/Espressif/esp32_aes.c PROPERTIES COMPILE_FLAGS "-Wno-incompatible-pointer-types")
endif ()
