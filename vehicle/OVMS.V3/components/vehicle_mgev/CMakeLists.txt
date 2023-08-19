set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_MG_EV)
  list(APPEND srcs "src/mg_auth.cpp" "src/mg_bcm.cpp" "src/mg_can_handler.cpp" "src/mg_configuration.cpp" "src/mg_gwm.cpp" "src/mg_poll_atc.cpp" "src/mg_poll_bcm.cpp" "src/mg_poll_bms.cpp" "src/mg_poll_dcdc.cpp" "src/mg_poll_evcc.cpp" "src/mg_poll_peps.cpp" "src/mg_poll_tpms.cpp" "src/mg_poll_vcu.cpp" "src/mg_software_versions.cpp" "src/mg_web.cpp" "src/vehicle_mgev.cpp" "src/vehicle_mgev_a.cpp" "src/vehicle_mgev_b.cpp" "src/vehicle_mg5.cpp" "src/vehicle_mgzsev2.cpp" )
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
