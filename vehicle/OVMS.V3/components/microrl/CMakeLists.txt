# requirements can't depend on config
idf_component_register(SRCS "./microrl.c"
                       INCLUDE_DIRS .
                       WHOLE_ARCHIVE)

