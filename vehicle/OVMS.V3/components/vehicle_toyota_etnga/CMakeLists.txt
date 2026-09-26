set(srcs)
set(include_dirs)

# The e-TNGA platform is a shared base, not a directly selectable vehicle: it builds
# whenever any vehicle derived from it is selected. Mirrors the condition in component.mk.
if (CONFIG_OVMS_VEHICLE_SUBARU_SOLTERRA OR CONFIG_OVMS_VEHICLE_TOYOTA_BZ4X)
  list(APPEND srcs "src/etnga_can_processor.cpp" "src/etnga_charge_dump.cpp" "src/etnga_charge_io.cpp" "src/etnga_charge_report.cpp" "src/etnga_metrics.cpp" "src/etnga_poll_processor.cpp" "src/etnga_poll_states.cpp" "src/etnga_tpms.cpp" "src/etnga_web.cpp" "src/vehicle_toyota_etnga.cpp")
  list(APPEND include_dirs "src")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
