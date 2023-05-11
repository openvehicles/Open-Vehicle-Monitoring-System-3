set(srcs)
set(include_dirs)

if (CONFIG_OVMS_SC_GPL_WOLF)
  list(APPEND srcs "wolfssh/src/internal.c" "wolfssh/src/io.c" "wolfssh/src/keygen.c" "wolfssh/src/log.c" "wolfssh/src/port.c" "wolfssh/src/ssh.c")
  list(APPEND include_dirs "wolfssh")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "freertos" "wolfssl"
                       # WHOLE_ARCHIVE
                       )

if (CONFIG_OVMS_SC_GPL_WOLF)
  component_compile_definitions(WOLFSSL_USER_SETTINGS)
  component_compile_options("-Wno-maybe-uninitialized")
endif ()
