set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_TELNET)
  list(APPEND srcs "src/libtelnet.c")
  list(APPEND include_dirs "include")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       )
