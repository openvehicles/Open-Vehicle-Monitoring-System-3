set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_VWEUP)
  list(APPEND srcs "src/poll_reply_helper.cpp" "src/vehicle_vweup.cpp" "src/vweup_obd.cpp" "src/vweup_shell.cpp" "src/vweup_t26.cpp" "src/vweup_web.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
