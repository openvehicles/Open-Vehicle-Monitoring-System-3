set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_RENAULTZOE_PH2)
  list(APPEND srcs "src/BCM_pids.cpp" "src/EVC_pids.cpp" "src/HVAC_pids.cpp" "src/INV_pids.cpp" "src/LBC_pids.cpp" "src/UCM_pids.cpp" "src/rz2_web.cpp" "src/vehicle_renaultzoe_ph2.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
