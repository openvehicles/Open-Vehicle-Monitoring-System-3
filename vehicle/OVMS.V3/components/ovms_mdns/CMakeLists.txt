set(srcs)
set(include_dirs)

if (CONFIG_OVMS_COMP_MDNS)
  list(APPEND srcs "src/ovms_mdns.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "mdns"
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
