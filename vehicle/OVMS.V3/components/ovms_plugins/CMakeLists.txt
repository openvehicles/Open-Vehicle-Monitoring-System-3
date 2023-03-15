set(srcs)
set(include_dirs)

if (CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE AND CONFIG_OVMS_COMP_PLUGINS)
  list(APPEND srcs "src/ovms_plugins.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       REQUIRES "json" "ovms_webserver"
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
