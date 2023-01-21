set(srcs)
set(include_dirs)
set(idf_version "${IDF_VERSION_MAJOR}.${IDF_VERSION_MINOR}.${IDF_VERSION_PATCH}")

if (idf_version VERSION_LESS "4.0.0")
  message(STATUS "IDF Version: ${idf_version} - enabling strverscmp")
  list(APPEND srcs "src/strverscmp.c")
  list(APPEND include_dirs "src")
else ()
  message(STATUS "IDF Version: ${idf_version} - NOT enabling strverscmp")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       WHOLE_ARCHIVE)
