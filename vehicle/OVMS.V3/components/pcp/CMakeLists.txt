# requirements can't depend on config
idf_component_register(SRCS "./pcp.cpp"
                       INCLUDE_DIRS .
                       REQUIRES "main"
                       WHOLE_ARCHIVE)
