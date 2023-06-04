set(srcs)
set(include_dirs)

if (CONFIG_OVMS_SC_GPL_MONGOOSE)
  list(APPEND srcs "src/ovms_netconns.cpp" "src/ovms_nethttp.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
