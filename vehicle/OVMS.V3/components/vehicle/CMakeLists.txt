# requirements can't depend on config
idf_component_register(SRCS "./vehicle.cpp" "./vehicle_bms.cpp" "./vehicle_duktape.cpp" "./vehicle_poller.cpp" "./vehicle_poller_isotp.cpp" "./vehicle_poller_vwtp.cpp" "./vehicle_shell.cpp"
                       INCLUDE_DIRS .
                       REQUIRES "ovms_webserver"
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
