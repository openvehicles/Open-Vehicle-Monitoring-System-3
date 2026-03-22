set(srcs)
set(include_dirs)

if (CONFIG_OVMS_VEHICLE_RENAULTZOE_PH2)
  list(APPEND srcs
  "src/rz2_pids_BCM.cpp"
  "src/rz2_pids_EVC.cpp"
  "src/rz2_pids_HVAC.cpp"
  "src/rz2_pids_INV.cpp"
  "src/rz2_pids_LBC.cpp"
  "src/rz2_pids_UCM.cpp"
  "src/rz2_commands_ddt.cpp"
  "src/rz2_commands_shell.cpp"
  "src/rz2_commands_vehicle.cpp"
  "src/rz2_dcdc.cpp"
  "src/rz2_lighting.cpp"
  "src/rz2_metrics.cpp"
  "src/rz2_vcan.cpp"
  "src/rz2_web.cpp"
  "src/vehicle_renaultzoe_ph2.cpp"
)
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
