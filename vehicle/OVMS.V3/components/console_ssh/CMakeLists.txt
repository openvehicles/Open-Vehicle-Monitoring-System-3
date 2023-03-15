set(srcs)

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_SC_GPL_WOLF AND CONFIG_OVMS_COMP_SSH)
  list(APPEND srcs "src/console_ssh.cpp")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "freertos" "main" "wolfssl"
                       WHOLE_ARCHIVE)

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_SC_GPL_WOLF AND CONFIG_OVMS_COMP_SSH)
  component_compile_definitions(WOLFSSL_USER_SETTINGS)
endif ()