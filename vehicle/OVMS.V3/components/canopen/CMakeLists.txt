set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_CANOPEN)
  list(APPEND srcs "src/canopen.cpp" "src/canopen_client.cpp" "src/canopen_shell.cpp" "src/canopen_worker.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main" "can"
                       WHOLE_ARCHIVE)
