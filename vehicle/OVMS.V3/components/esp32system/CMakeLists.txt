# requirements can't depend on config
idf_component_register(SRCS "./esp32system.cpp"
                       INCLUDE_DIRS .
                       PRIV_REQUIRES "pcp"
                       WHOLE_ARCHIVE)
