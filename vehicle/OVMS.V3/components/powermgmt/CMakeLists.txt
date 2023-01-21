# requirements can't depend on config
idf_component_register(SRCS "./powermgmt.cpp" "./powermgmt_web.cpp"
                       INCLUDE_DIRS .
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)

