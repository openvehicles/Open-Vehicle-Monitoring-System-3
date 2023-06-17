set(srcs)

if (CONFIG_OVMS_SC_GPL_MONGOOSE AND CONFIG_OVMS_COMP_TELNET)
  list(APPEND srcs "src/console_telnet.cpp")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS src
                       PRIV_REQUIRES "main" "mongoose"
                       WHOLE_ARCHIVE)

