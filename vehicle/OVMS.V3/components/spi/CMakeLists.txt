# requirements can't depend on config
idf_component_register(SRCS "./spi.cpp"
                       INCLUDE_DIRS .
                       PRIV_REQUIRES "main"
                       WHOLE_ARCHIVE)
