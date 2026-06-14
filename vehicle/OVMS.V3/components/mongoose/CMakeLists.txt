set(srcs)
set(include_dirs)
set(priv_includedirs)

if (CONFIG_OVMS_SC_GPL_MONGOOSE)
  list(APPEND srcs "mongoose/mongoose.c" "src/mongoose_client.cpp")
  list(APPEND include_dirs "include" "mongoose" "src")

  set_property(DIRECTORY APPEND PROPERTY CMAKE_CONFIGURE_DEPENDS "${srcs}" "mongoose/mongoose.h")

if (CONFIG_MG_SSL_IF_WOLFSSL)
  list(APPEND priv_includedirs "../wolfssl/wolfssl" "../wolfssl/wolfssl/wolfssl")
endif ()

set(MG_VERSION_COMPONENTS)
execute_process(
  COMMAND cat "${COMPONENT_DIR}/mongoose/mongoose.h"
  COMMAND grep "#define MG_VERSION"
  COMMAND cut "-d" " " "-f3"
  COMMAND sed -e s/\"//g
  OUTPUT_VARIABLE MG_VERSION
  OUTPUT_STRIP_TRAILING_WHITESPACE
)
# include(CMakePrintHelpers)
# cmake_print_variables(MG_VERSION)
message(STATUS "Compiled Mongoose version: ${MG_VERSION}" )

string(REPLACE "." ";" MG_VERSION_COMPONENTS ${MG_VERSION})
# cmake_print_variables(MG_VERSION_COMPONENTS)

set(MG_VERSION_MAJOR 0)
set(MG_VERSION_MINOR 0)
set(MG_VERSION_PATCH 0)
list(LENGTH MG_VERSION_COMPONENTS MG_VERSION_COMPONENTS_LENGTH)
# cmake_print_variables(MG_VERSION_COMPONENTS_LENGTH)

if (MG_VERSION_COMPONENTS_LENGTH GREATER "0")
  list(POP_FRONT MG_VERSION_COMPONENTS MG_VERSION_MAJOR)
  if (MG_VERSION_COMPONENTS_LENGTH GREATER "1")
    list(POP_FRONT MG_VERSION_COMPONENTS MG_VERSION_MINOR)
    if (MG_VERSION_COMPONENTS_LENGTH GREATER "2")
      list(POP_FRONT MG_VERSION_COMPONENTS MG_VERSION_PATCH)
    endif ()
  endif ()
endif ()
# cmake_print_variables(MG_VERSION_MAJOR MG_VERSION_MINOR MG_VERSION_PATCH)

configure_file("${COMPONENT_DIR}/mg_version.h.in" "${COMPONENT_DIR}/include/mg_version.h" @ONLY)

endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "lwip" "mbedtls" "wolfssl"
                       PRIV_REQUIRES "main" "freertos"
                       PRIV_INCLUDE_DIRS ${priv_includedirs}
                       # WHOLE_ARCHIVE
                       )

if (CONFIG_OVMS_SC_GPL_MONGOOSE)

  component_compile_definitions("MG_ARCH=MG_ARCH_FREERTOS" "MG_ENABLE_LINES=1" "MG_ENABLE_LWIP=1" "MG_ENABLE_IPV6=1" "MG_ENABLE_ASSERT=1")

if (CONFIG_MG_SSL_IF_MBEDTLS)
  component_compile_definitions("MG_ENABLE_MBEDTLS=1")
endif ()

endif ()
