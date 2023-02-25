set(srcs)
set(include_dirs)

if (CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE)
  list(APPEND srcs "src/duktape.c")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       # WHOLE_ARCHIVE
                       )

if (CONFIG_OVMS_SC_JAVASCRIPT_DUKTAPE)
  component_compile_definitions(DUK_USE_GET_MONOTONIC_TIME_CLOCK_GETTIME)
endif ()
