set(srcs)
set(include_dirs)
set(embed_files)

if (CONFIG_MG_ENABLE_SSL)
  list(APPEND srcs "src/ovms_tls.cpp")
  list(APPEND include_dirs "src")
  list(APPEND embed_files "trustedca/usertrust.crt" "trustedca/digicert_global.crt" "trustedca/digicert_g2.crt" "trustedca/starfield_class2.crt" "trustedca/baltimore_cybertrust.crt" "trustedca/isrg_x1.crt")
endif ()

# requirements can't depend on config
idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include_dirs}
                       PRIV_REQUIRES "main"
                       EMBED_FILES ${embed_files}
                       WHOLE_ARCHIVE)
