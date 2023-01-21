set(srcs)
set(include_dirs)
set(priv_includedirs)

if (CONFIG_OVMS_SC_GPL_MONGOOSE)
  list(APPEND srcs "mongoose/mongoose.c")
  list(APPEND include_dirs "include" "mongoose")

if (CONFIG_MG_SSL_IF_WOLFSSL)
  list(APPEND priv_includedirs "../wolfssl" "../wolfssl/wolfssl")
endif ()

endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "lwip" "mbedtls"
                       PRIV_REQUIRES "main" "freertos"
                       PRIV_INCLUDE_DIRS ${priv_includedirs}
                       # WHOLE_ARCHIVE
                       )

if (CONFIG_OVMS_SC_GPL_MONGOOSE)

  component_compile_options("-mlongcalls")
  component_compile_definitions("MG_LOCALS" "MG_ENABLE_MBEDTLS=1")

endif ()
